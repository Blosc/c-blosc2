/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program showing how to operate with compressed buffers.

  To compile this program:

  $ gcc -fopenmp -O3 sum_openmp.c -o sum_openmp -lblosc

  To run:

  $ OMP_NUM_THREADS=8 ./sum_openmp
  Blosc version info: 2.0.0a4.dev ($Date:: 2016-08-04 #$)
  Sum for uncompressed data: 4999999950000000
  Sum time for uncompressed data: 0.0289 s, 26441.8 MB/s
  Compression ratio: 762.9 MB -> 8.4 MB (90.6x)
  Compression time: 0.46 s, 1659.9 MB/s
  Sum for *compressed* data: 4999999950000000
  Sum time for *compressed* data: 0.015 s, 50909.5 MB/s


*/

#include <stdio.h>
#include <assert.h>
#include "blosc.h"

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define N (100 * 1000 * 1000)
#define CHUNKSIZE (4 * 1000)
#define NCHUNKS (N / CHUNKSIZE)
#define NITER 5
#define NTHREADS 8
#define CLEVEL 9
#define CODEC BLOSC_BLOSCLZ
#define DTYPE int64_t


int main() {
  static DTYPE udata[N];
  static DTYPE chunk_buf[CHUNKSIZE];
  static DTYPE chunk[NTHREADS][CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(DTYPE);
  DTYPE sum, compressed_sum;
  int nchunks_thread = NCHUNKS / NTHREADS;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  blosc2_context* dctx[NTHREADS];
  int i, j, nchunk;
  blosc_timestamp_t last, current;
  double ttotal, itotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  // Fill the uncompressed dataset
  for (i = 0; i < N; i++) {
    udata[i] = i;
  }

  // Reduce uncompressed dataset
  ttotal = 1e10;
  for (int n = 0; n < NITER; n++) {
    sum = 0;
    blosc_set_timestamp(&last);
#pragma omp parallel for reduction (+:sum)
    for (i = 0; i < N; i++) {
      sum += udata[i];
    }
    blosc_set_timestamp(&current);
    itotal = blosc_elapsed_secs(last, current);
    if (itotal < ttotal) ttotal = itotal;
  }
  printf("Sum for uncompressed data: %10.0f\n", (double)sum);
  printf("Sum time for uncompressed data: %.3g s, %.1f MB/s\n",
         ttotal, (isize * NCHUNKS) / (ttotal * MB));

  // Create a super-chunk container for the compressed container
  cparams.typesize = sizeof(DTYPE);
  cparams.compcode = CODEC;
  cparams.clevel = CLEVEL;
  cparams.nthreads = 1;
  dparams.nthreads = 1;
  blosc_set_timestamp(&last);
  schunk = blosc2_new_schunk(cparams, dparams);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      chunk_buf[i] = i + nchunk * CHUNKSIZE;
    }
    blosc2_append_buffer(schunk, isize, chunk_buf);
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  // Reduce uncompressed dataset
  blosc_set_timestamp(&last);
  ttotal = 1e10;
  for (int n = 0; n < NITER; n++) {
    compressed_sum = 0;
    #pragma omp parallel for private(nchunk) reduction (+:compressed_sum)
    for (j = 0; j < NTHREADS; j++) {
      dctx[j] = blosc2_create_dctx(dparams);
      for (nchunk = 0; nchunk < nchunks_thread; nchunk++) {
        blosc2_decompress_ctx(dctx[j], schunk->data[j * nchunks_thread + nchunk], (void*)(chunk[j]), isize);
        for (i = 0; i < CHUNKSIZE; i++) {
          compressed_sum += chunk[j][i];
          //compressed_sum += i + (j * nchunks_thread + nchunk) * CHUNKSIZE;
        }
      }
    }
    blosc_set_timestamp(&current);
    itotal = blosc_elapsed_secs(last, current);
    if (itotal < ttotal) ttotal = itotal;
  }
  printf("Sum for *compressed* data: %10.0f\n", (double)compressed_sum);
  printf("Sum time for *compressed* data: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));
  assert(sum == compressed_sum);

  /* Free resources */
  blosc2_free_schunk(schunk);

  return 0;
}

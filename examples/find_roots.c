/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating how the different compression params affects
  the performance of root finding.

  To compile this program:

  $ gcc -O3 find_roots.c -o find_roots -lblosc2

  To run:

  $ ./find_roots
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Creation time for X values: 0.178 s, 4274.5 MB/s
  Compression for X values: 762.9 MB -> 27.3 MB (28.0x)
  Computing Y polynomial: 0.342 s, 4463.3 MB/s
  Compression for Y values: 762.9 MB -> 54.0 MB (14.1x)
  Roots found at: 1.350000023841858, 4.450000286102295, 8.500000953674316,
  Find root time:  0.401 s, 3806.8 MB/s

*/

#include <stdio.h>
#include "blosc2.h"

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)


#define NCHUNKS 500
#define CHUNKSIZE (200 * 1000)  // fits well in modern L3 caches
#define NTHREADS 4


void fill_buffer(double *x, int nchunk) {
  double incx = 10. / (NCHUNKS * CHUNKSIZE);

  for (int i = 0; i < CHUNKSIZE; i++) {
    x[i] = incx * (nchunk * CHUNKSIZE + i);
  }
}

void process_data(const double *x, double *y) {

  for (int i = 0; i < CHUNKSIZE; i++) {
    double xi = x[i];
    //y[i] = ((.25 * xi + .75) * xi - 1.5) * xi - 2;
    y[i] = (xi - 1.35) * (xi - 4.45) * (xi - 8.5);
  }
}

void find_root(const double *x, const double *y,
               const double prev_value) {
  double pv = prev_value;
  int last_root_idx = -1;

  for (int i = 0; i < CHUNKSIZE; i++) {
    double yi = y[i];
    if (((yi > 0) - (yi < 0)) != ((pv > 0) - (pv < 0))) {
      if (last_root_idx != (i - 1)) {
        printf("%.16g, ", x[i]);
        last_root_idx = i;  // avoid the last point (ULP effects)
      }
    }
    pv = yi;
  }
}


int compute_vectors(void) {
  static double buffer_x[CHUNKSIZE];
  static double buffer_y[CHUNKSIZE];
  const int32_t isize = CHUNKSIZE * sizeof(double);
  int dsize;
  long nbytes = 0;
  blosc2_schunk *sc_x, *sc_y;
  int nchunk;
  blosc_timestamp_t last, current;
  double ttotal;
  double prev_value;

  /* Create a super-chunk container for input (X values) */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(double);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.filters[0] = BLOSC_TRUNC_PREC;
  cparams.filters_meta[0] = 23;  // treat doubles as floats
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  sc_x = blosc2_schunk_new(&storage);

  /* Create a super-chunk container for output (Y values) */
  sc_y = blosc2_schunk_new(&storage);

  /* Now fill the buffer with even values between 0 and 10 */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    fill_buffer(buffer_x, nchunk);
    blosc2_schunk_append_buffer(sc_x, buffer_x, isize);
    nbytes += (long) isize;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Creation time for X values: %.3g s, %.1f MB/s\n",
         ttotal, (double) nbytes / (ttotal * MB));
  printf("Compression for X values: %.1f MB -> %.1f MB (%.1fx)\n",
         sc_x->nbytes / MB, sc_x->cbytes / MB,
         (1. * sc_x->nbytes) / sc_x->cbytes);

  /* Retrieve the chunks and compute the polynomial in another super-chunk */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(sc_x, nchunk, buffer_x, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    process_data(buffer_x, buffer_y);
    blosc2_schunk_append_buffer(sc_y, buffer_y, isize);
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Computing Y polynomial: %.3g s, %.1f MB/s\n",
         ttotal, 2. * (double) nbytes / (ttotal * MB));    // 2 super-chunks involved
  printf("Compression for Y values: %.1f MB -> %.1f MB (%.1fx)\n",
         sc_y->nbytes / MB, sc_y->cbytes / MB,
         (1. * sc_y->nbytes) / sc_y->cbytes);

  /* Find the roots of the polynomial */
  printf("Roots found at: ");
  blosc_set_timestamp(&last);
  prev_value = buffer_y[0];
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(sc_y, nchunk, (void *) buffer_y, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    dsize = blosc2_schunk_decompress_chunk(sc_x, nchunk, (void *) buffer_x, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    find_root(buffer_x, buffer_y, prev_value);
    prev_value = buffer_y[CHUNKSIZE - 1];
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("\n");
  printf("Find root time:  %.3g s, %.1f MB/s\n",
         ttotal, 2. * (double) nbytes / (ttotal * MB));    // 2 super-chunks involved

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_schunk_free(sc_x);
  blosc2_schunk_free(sc_y);
  return 0;
}


int main(void) {
  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  compute_vectors();

  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}

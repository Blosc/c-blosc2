/*********************************************************************
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Small benchmark for testing basic capabilities of Blosc.

  You can select different degrees of 'randomness' in input buffer, as
  well as external datafiles (uncomment the lines after "For data
  coming from a file" comment).

  For usage instructions of this benchmark, please see:

    https://www.blosc.org/pages/synthetic-benchmarks/

  I'm collecting speeds for different machines, so the output of your
  benchmarks and your processor specifications are welcome!

  Note: Compiling this with VS2008 does not work well with cmake.  Here
  it is a way to compile the benchmark (with added support for LZ4):

  > cl /arch:SSE2 /Ox /Febench.exe /Iblosc /Iinternal-complibs\lz4-1.7.0 bench\bench.c blosc\blosc.c blosc\blosclz.c blosc\shuffle.c blosc\shuffle-sse2.c blosc\shuffle-generic.c blosc\bitshuffle-generic.c blosc\bitshuffle-sse2.c internal-complibs\lz4-1.7.0\*.c

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "blosc2.h"

#define KB  1024u
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS (32*1024)       /* maximum number of chunks */


int nchunks = NCHUNKS;
int niter = 1;
int niter_c = 1;
int niter_d = 1;
/* default number of iterations */
double totalsize = 0.;          /* total compressed/decompressed size */

/* Define posix_memalign for Windows */
#if defined(_WIN32)
#include <malloc.h>

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    *memptr = _aligned_malloc(size, alignment);
    return 0;
}

/* Buffers allocated with _aligned_malloc need to be freed with _aligned_free. */
#define aligned_free(memptr) _aligned_free(memptr)
#else
/* If not using MSVC, aligned memory can be freed in the usual way. */
#define aligned_free(memptr) free(memptr)
#endif  /* defined(_WIN32) && !defined(__MINGW32__) */

/* Given two timeval stamps, return the time per chunk in usec */
double get_usec_chunk(blosc_timestamp_t last, blosc_timestamp_t current,
                      int niter_, int nchunks_) {
  double elapsed_usecs = 1e-3 * blosc_elapsed_nsecs(last, current);
  return elapsed_usecs / (double)(niter_ * nchunks_);
}


int get_value(int i, int rshift) {
  int v;

  v = (i << 26) ^ (i << 18) ^ (i << 11) ^ (i << 3) ^ i;
  if (rshift < 32) {
    v &= (1 << rshift) - 1;
  }
  return v;
}


void init_buffer(void* src, size_t size, int rshift) {
  int i;
  int* _src = (int*)src;

  /* To have reproducible results */
  srand(1);

  /* Initialize the original buffer */
  for (i = 0; i < (int) (size / sizeof(int)); ++i) {
    /* Choose one below */
    /* _src[i] = 0;
     * _src[i] = 0x01010101;
     * _src[i] = 0x01020304;
     * _src[i] = i * 1/.3;
     * _src[i] = i; */
    //_src[i] = rand() >> (32 - rshift);
    _src[i] = get_value(i, rshift);
  }
}


void do_bench(char* compressor, char* shuffle, int nthreads, int size_, int elsize,
              int rshift, FILE* ofile) {
  size_t size = (size_t)size_;
  void* src, *srccpy;
  void* dest[NCHUNKS], *dest2;
  int nbytes = 0, cbytes = 0;
  int i, j, retcode;
  unsigned char* orig, * round;
  blosc_timestamp_t last, current;
  double tmemcpy, tshuf, tunshuf;
  int clevel, doshuffle = BLOSC_NOFILTER;

  if (strcmp(shuffle, "shuffle") == 0) {
    doshuffle = BLOSC_SHUFFLE;
  }
  else if (strcmp(shuffle, "bitshuffle") == 0) {
    doshuffle = BLOSC_BITSHUFFLE;
  }
  else if (strcmp(shuffle, "noshuffle") == 0) {
    doshuffle = BLOSC_NOSHUFFLE;
  }

  blosc2_set_nthreads((int16_t) nthreads);
  if (blosc1_set_compressor(compressor) < 0) {
    printf("Compiled w/o support for compressor: '%s', so sorry.\n",
           compressor);
    exit(1);
  }

  /* Initialize buffers */
  srccpy = malloc(size);
  retcode = posix_memalign(&src, 32, size);
  if (retcode != 0) {
    printf("Error in allocating memory!");
  }
  retcode = posix_memalign(&dest2, 32, size);
  if (retcode != 0) {
    printf("Error in allocating memory!");
  }

  /* zero src to initialize all bytes on it, and not only multiples of 4 */
  memset(src, 0, size);
  init_buffer(src, size, rshift);
  memcpy(srccpy, src, size);
  for (j = 0; j < nchunks; j++) {
    retcode = posix_memalign(&dest[j], 32, size + BLOSC2_MAX_OVERHEAD);
    if (retcode != 0) {
      printf("Error in allocating memory!");
    }
  }
  memset(dest2, 0, size);  // just to avoid some GCC compiler warnings

  fprintf(ofile, "--> %d, %d, %d, %d, %s, %s\n", nthreads, (int)size, elsize, rshift, compressor, shuffle);
  fprintf(ofile, "********************** Run info ******************************\n");
  fprintf(ofile, "Blosc version: %s (%s)\n", BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);
  fprintf(ofile, "Using synthetic data with %d significant bits (out of 32)\n", rshift);
  fprintf(ofile, "Dataset size: %d bytes\tType size: %d bytes\n", (int)size, elsize);
  fprintf(ofile, "Working set: %.1f MB\t\t", (float)(size * nchunks) / (float)MB);
  fprintf(ofile, "Number of threads: %d\n", nthreads);
  fprintf(ofile, "********************** Running benchmarks *********************\n");

  blosc_set_timestamp(&last);
  for (i = 0; i < niter; i++) {
    for (j = 0; j < nchunks; j++) {
      memcpy(dest[j], src, size);
    }
  }
  blosc_set_timestamp(&current);
  tmemcpy = get_usec_chunk(last, current, niter, nchunks);
  fprintf(ofile, "memcpy(write):\t\t %6.1f us, %.1f MB/s\n",
          tmemcpy, ((float)size * 1e6) / (tmemcpy * MB));

  blosc_set_timestamp(&last);
  for (i = 0; i < niter; i++) {
    for (j = 0; j < nchunks; j++) {
      memcpy(dest2, dest[j], size);
    }
  }
  blosc_set_timestamp(&current);
  tmemcpy = get_usec_chunk(last, current, niter, nchunks);
  fprintf(ofile, "memcpy(read):\t\t %6.1f us, %.1f MB/s\n",
          tmemcpy, ((float)size * 1e6) / (tmemcpy * MB));

  for (clevel = 0; clevel < 10; clevel++) {

    fprintf(ofile, "Compression level: %d\n", clevel);

    blosc_set_timestamp(&last);
    for (i = 0; i < niter_c; i++) {
      for (j = 0; j < nchunks; j++) {
        cbytes = blosc1_compress(clevel, doshuffle, (size_t)elsize, size, src,
                                 dest[j], size + BLOSC2_MAX_OVERHEAD);
      }
    }
    blosc_set_timestamp(&current);
    tshuf = get_usec_chunk(last, current, niter_c, nchunks);
    fprintf(ofile, "comp(write):\t %6.1f us, %.1f MB/s\t  ",
            tshuf, ((float)size * 1e6) / (tshuf * MB));
    fprintf(ofile, "Final bytes: %d  ", cbytes);
    if (cbytes > 0) {
      fprintf(ofile, "Ratio: %3.2f", (float)size / (float)cbytes);
    }
    fprintf(ofile, "\n");

    /* Compressor was unable to compress.  Copy the buffer manually. */
    if (cbytes == 0) {
      for (j = 0; j < nchunks; j++) {
        memcpy(dest[j], src, size);
      }
    }

    blosc_set_timestamp(&last);
    for (i = 0; i < niter_d; i++) {
      for (j = 0; j < nchunks; j++) {
        if (cbytes == 0) {
          memcpy(dest2, dest[j], size);
          nbytes = (int)size;
        }
        else {
          nbytes = blosc1_decompress(dest[j], dest2, size);
        }
      }
    }
    blosc_set_timestamp(&current);
    tunshuf = get_usec_chunk(last, current, niter_d, nchunks);
    fprintf(ofile, "decomp(read):\t %6.1f us, %.1f MB/s\t  ",
            tunshuf, (nbytes * 1e6) / (tunshuf * MB));
    if (nbytes < 0) {
      fprintf(ofile, "FAILED.  Error code: %d\n", nbytes);
    }
    /* fprintf(ofile, "Orig bytes: %d\tFinal bytes: %d\n", cbytes, nbytes); */

    /* Check if data has had a good roundtrip.
       Byte-by-byte comparison is slow, so use 'memcmp' to check whether the
       roundtripped data is correct. If not, fall back to the slow path to
       print diagnostic messages. */
    orig = (unsigned char*)srccpy;
    round = (unsigned char*)dest2;
    if (memcmp(orig, round, size) != 0) {
      for (i = 0; i < (int)size; ++i) {
        if (orig[i] != round[i]) {
          fprintf(ofile, "\nError: Original data and round-trip do not match in pos %d\n", i);
          fprintf(ofile, "Orig--> %x, round-trip--> %x\n", orig[i], round[i]);
          break;
        }
      }
    }
    else {
      i = (int)size;
    }

    if (i == (int)size) fprintf(ofile, "OK\n");

  } /* End clevel loop */


  /* To compute the totalsize, we should take into account the 10
     compression levels */
  totalsize += ((double)size * nchunks * niter * 10.);

  aligned_free(src);
  free(srccpy);
  aligned_free(dest2);
  for (i = 0; i < nchunks; i++) {
    aligned_free(dest[i]);
  }
}


/* Compute a sensible value for nchunks */
int get_nchunks(int size_, int ws) {
  int nchunks_;

  nchunks_ = ws / size_;
  if (nchunks_ > NCHUNKS) nchunks_ = NCHUNKS;
  if (nchunks_ < 1) nchunks_ = 1;
  return nchunks_;
}

void print_compress_info(void) {
  char* name = NULL, * version = NULL;
  int ret;

  printf("Blosc version: %s (%s)\n", BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  printf("List of supported compressors in this build: %s\n",
         blosc2_list_compressors());

  printf("Supported compression libraries:\n");
  ret = blosc2_get_complib_info("blosclz", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  free(name); free(version);
  ret = blosc2_get_complib_info("lz4", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  free(name); free(version);
  ret = blosc2_get_complib_info("zlib", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  free(name); free(version);
  ret = blosc2_get_complib_info("zstd", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  free(name); free(version);
}


int main(int argc, char* argv[]) {
  char compressor[32];
  char shuffle[32] = "shuffle";
  char bsuite[32];
  int single = 1;
  int suite = 0;
  int hard_suite = 0;
  int extreme_suite = 0;
  int debug_suite = 0;
  int nthreads = 8;                     /* The number of threads */
  int size = 8 * MB;                    /* Buffer size */
  int elsize = 4;                       /* Datatype size */
  int rshift = 19;                      /* Significant bits */
  int workingset = 256 * MB;            /* The maximum allocated memory */
  int nthreads_, size_, elsize_, rshift_, i;
  FILE* output_file = stdout;
  blosc_timestamp_t last, current;
  double totaltime;
  char usage[256];

  print_compress_info();

  strncpy(usage, "Usage: bench [blosclz | lz4 | lz4hc | zlib | zstd] "
      "[noshuffle | shuffle | bitshuffle] "
      "[single | suite | hardsuite | extremesuite | debugsuite] "
      "[nthreads] [bufsize(bytes)] [typesize] [sbits]", 255);

  if (argc < 1) {
    printf("%s\n", usage);
    exit(1);
  }

  if (argc >= 2) {
    strcpy(compressor, argv[1]);
  }
  else {
    strcpy(compressor, "blosclz");
  }

  if (strcmp(compressor, "blosclz") != 0 &&
      strcmp(compressor, "lz4") != 0 &&
      strcmp(compressor, "lz4hc") != 0 &&
      strcmp(compressor, "zlib") != 0 &&
      strcmp(compressor, "zstd") != 0) {
    printf("No such compressor: '%s'\n", compressor);
    printf("%s\n", usage);
    exit(2);
  }

  if (argc >= 3) {
    strcpy(shuffle, argv[2]);
    if (strcmp(shuffle, "shuffle") != 0 &&
        strcmp(shuffle, "bitshuffle") != 0 &&
        strcmp(shuffle, "noshuffle") != 0) {
      printf("No such shuffler: '%s'\n", shuffle);
      printf("%s\n", usage);
      exit(2);
    }
  }

  if (argc < 4)
    strcpy(bsuite, "single");
  else
    strcpy(bsuite, argv[3]);

  if (strcmp(bsuite, "single") == 0) {
    single = 1;
  }
  else if (strcmp(bsuite, "test") == 0) {
    single = 1;
    workingset /= 2;
  }
  else if (strcmp(bsuite, "suite") == 0) {
    suite = 1;
  }
  else if (strcmp(bsuite, "hardsuite") == 0) {
    hard_suite = 1;
    workingset /= 4;
    /* Values here are ending points for loops */
    nthreads = 2;
    size = 8 * MB;
    elsize = 32;
    rshift = 32;
  }
  else if (strcmp(bsuite, "extremesuite") == 0) {
    extreme_suite = 1;
    workingset /= 8;
    niter = 1;
    /* Values here are ending points for loops */
    nthreads = 4;
    size = 16 * MB;
    elsize = 32;
    rshift = 32;
  }
  else if (strcmp(bsuite, "debugsuite") == 0) {
    debug_suite = 1;
    workingset /= 8;
    niter = 1;
    /* Warning: values here are starting points for loops.  This is
       useful for debugging. */
    nthreads = 1;
    size = 16 * KB;
    elsize = 1;
    rshift = 0;
  }
  else {
    printf("%s\n", usage);
    exit(1);
  }

  printf("Using compressor: %s\n", compressor);
  printf("Using shuffle type: %s\n", shuffle);
  printf("Running suite: %s\n", bsuite);

  if (argc >= 5) {
    nthreads = (int)strtol(argv[4], NULL, 10);
  }
  if (argc >= 6) {
    size = (int)strtol(argv[5], NULL, 10);
  }
  if (argc >= 7) {
    elsize = (int)strtol(argv[6], NULL, 10);
  }
  if (argc >= 8) {
    rshift = (int)strtol(argv[7], NULL, 10);
  }

  if ((argc >= 9) || !(single || suite || hard_suite || extreme_suite)) {
    printf("%s\n", usage);
    exit(1);
  }

  nchunks = get_nchunks(size, workingset);
  blosc_set_timestamp(&last);

  blosc2_init();

  if (suite) {
    for (nthreads_ = 1; nthreads_ <= nthreads; nthreads_++) {
      do_bench(compressor, shuffle, nthreads_, size, elsize, rshift, output_file);
    }
  }
  else if (hard_suite) {
    /* Let's start the rshift loop by 4 so that 19 is visited.  This
       is to allow a direct comparison with the plain suite, that runs
       precisely at 19 significant bits. */
    for (rshift_ = 4; rshift_ <= rshift; rshift_ += 5) {
      for (elsize_ = 1; elsize_ <= elsize; elsize_ *= 2) {
        /* The next loop is for getting sizes that are not power of 2 */
        for (i = -elsize_; i <= elsize_; i += elsize_) {
          for (size_ = 32 * KB; size_ <= size; size_ *= 2) {
            nchunks = get_nchunks(size_ + i, workingset);
            niter = 1;
            for (nthreads_ = 1; nthreads_ <= nthreads; nthreads_++) {
              do_bench(compressor, shuffle, nthreads_, size_ + i, elsize_, rshift_, output_file);
              blosc_set_timestamp(&current);
              totaltime = blosc_elapsed_secs(last, current);
              printf("Elapsed time:\t %6.1f s.  Processed data: %.1f GB\n",
                     totaltime, totalsize / GB);
            }
          }
        }
      }
    }
  }
  else if (extreme_suite) {
    for (rshift_ = 0; rshift_ <= rshift; rshift_++) {
      for (elsize_ = 1; elsize_ <= elsize; elsize_++) {
        /* The next loop is for getting sizes that are not power of 2 */
        for (i = -elsize_ * 2; i <= elsize_ * 2; i += elsize_) {
          for (size_ = 32 * KB; size_ <= size; size_ *= 2) {
            nchunks = get_nchunks(size_ + i, workingset);
            for (nthreads_ = 1; nthreads_ <= nthreads; nthreads_++) {
              do_bench(compressor, shuffle, nthreads_, size_ + i, elsize_, rshift_, output_file);
              blosc_set_timestamp(&current);
              totaltime = blosc_elapsed_secs(last, current);
              printf("Elapsed time:\t %6.1f s.  Processed data: %.1f GB\n",
                     totaltime, totalsize / GB);
            }
          }
        }
      }
    }
  }
  else if (debug_suite) {
    for (rshift_ = rshift; rshift_ <= 32; rshift_++) {
      for (elsize_ = elsize; elsize_ <= 32; elsize_++) {
        /* The next loop is for getting sizes that are not power of 2 */
        for (i = -elsize_ * 2; i <= elsize_ * 2; i += elsize_) {
          for (size_ = size; size_ <= (int) (16 * MB); size_ *= 2) {
            nchunks = get_nchunks(size_ + i, workingset);
            for (nthreads_ = nthreads; nthreads_ <= 6; nthreads_++) {
              do_bench(compressor, shuffle, nthreads_, size_ + i, elsize_, rshift_, output_file);
              blosc_set_timestamp(&current);
              totaltime = blosc_elapsed_secs(last, current);
              printf("Elapsed time:\t %6.1f s.  Processed data: %.1f GB\n",
                     totaltime, totalsize / GB);
            }
          }
        }
      }
    }
  }
    /* Single mode */
  else {
    do_bench(compressor, shuffle, nthreads, size, elsize, rshift, output_file);
  }

  /* Print out some statistics */
  blosc_set_timestamp(&current);
  totaltime = (float)blosc_elapsed_secs(last, current);
  printf("\nRound-trip compr/decompr on %.1f GB\n", totalsize / GB);
  printf("Elapsed time:\t %6.1f s, %.1f MB/s\n",
         totaltime, totalsize * 2 * 1.1 / (MB * totaltime));

  /* Free blosc resources */
  blosc2_free_resources();
  blosc2_destroy();
  return 0;
}

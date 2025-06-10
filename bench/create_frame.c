/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Simple benchmark for frame creation.

  To run:

  $ ./create_frame

   ***  Creating uninitialized   ***
   ***  Using fill method!   ***

*** Creating *contiguous* super-chunk for *blosclz*
Compression ratio: 186.26 GB -> 0.17 KB (1162790697.7x)
Compression time: 8.5e-06 s, 21399.9 TB/s
Decompression time: 0.0628 s, 2965.1 GB/s

*** Creating *sparse* super-chunk for *blosclz*
Compression ratio: 186.26 GB -> 3906.29 KB (49999.5x)
Compression time: 0.00294 s, 61.8 TB/s
Decompression time: 0.00424 s, 43959.2 GB/s

*** Creating *contiguous* super-chunk for *lz4*
Compression ratio: 186.26 GB -> 0.17 KB (1162790697.7x)
Compression time: 4.71e-06 s, 38636.1 TB/s
Decompression time: 0.0701 s, 2656.7 GB/s

*** Creating *sparse* super-chunk for *lz4*
Compression ratio: 186.26 GB -> 3906.29 KB (49999.5x)
Compression time: 0.00311 s, 58.5 TB/s
Decompression time: 0.0101 s, 18516.4 GB/s

Process finished with exit code 0

 */

#include <stdio.h>
#include <blosc2.h>
#include <inttypes.h>

#define KB  (1024.)
#define MB  (1024 * KB)
#define GB  (1024 * MB)
#define TB  (1024 * GB)

#define CHUNKSHAPE (500 * 1000)
#define NCHUNKS 100000
#define NTHREADS 1  // curiously, using 1 single thread is better for the uninitialized values

// For exercising the optimized chunk creators (un)comment the lines below as you please
//#define CREATE_ZEROS
#define CREATE_FILL
//#define CREATE_LOOP

int create_cframe(const char* compname, bool contiguous) {
  int32_t isize = CHUNKSHAPE * sizeof(int32_t);
  int32_t* data = malloc(isize);
  int32_t* data_dest = malloc(isize);
  int32_t* data_dest2 = malloc(isize);
  int64_t nbytes, cbytes;
  int nchunk;
  blosc_timestamp_t last, current;
  double ttotal;
  int compcode = blosc2_compname_to_compcode(compname);
  printf("\n*** Creating *%s* super-chunk for *%s*\n",
         contiguous ? "contiguous" : "sparse", compname);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = compcode;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  //cparams.blocksize = 1024;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  char filename[64];
  sprintf(filename, "frame_simple-%s.b2frame", compname);
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath=NULL, .contiguous=contiguous};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

#ifdef CREATE_ZEROS
  // Precompute chunk of zeros
  int ret = blosc2_chunk_zeros(cparams, isize, data_dest, isize);
#else
  int ret = blosc2_chunk_uninit(cparams, isize, data_dest, isize);
#endif
  if (ret < 0) {
    printf("Creation error in special chunk.  Error code: %d\n", ret);
    return ret;
  }

  // Add some data
  blosc_set_timestamp(&last);

  int64_t nitems;
#ifdef CREATE_FILL
  // Make nitems a non-divisible number of CHUNKSHAPE
  nitems = (int64_t)NCHUNKS * CHUNKSHAPE + 1;
#ifdef CREATE_ZEROS
  // Precompute chunk of zeros
  int special_value = BLOSC2_SPECIAL_ZERO;
#else
  int special_value = BLOSC2_SPECIAL_UNINIT;
#endif
  int64_t rc = blosc2_schunk_fill_special(schunk, nitems, special_value, isize);
  if (rc < 0) {
    printf("Error in fill special.  Error code: %" PRId64 "\n", rc);
    return (int)rc;
  }
#else
  // In these methods, nitems can only be an actual multiple of CHUNKSHAPE
  nitems = (int64_t)NCHUNKS * CHUNKSHAPE;
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
#ifdef CREATE_LOOP
    int nchunks = blosc2_schunk_append_chunk(schunk, (uint8_t *) data_dest, true);
    if (nchunks != nchunk + 1) {
      printf("Compression error in append chunk.  Error code: %d\n", nchunks);
      return nchunk;
    }
#else
    for (int i = 0; i < CHUNKSHAPE; i++) {
      // Different data patterns
      // data[i] = i * nchunk;
      // data[i] = nchunk;
      data[i] = 0;
    }
    int nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
      printf("Compression error appending in schunk.  Error code: %d\n", nchunks);
      return nchunk;
    }
#endif
  }
#endif
  blosc_set_timestamp(&current);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = blosc2_schunk_frame_len(schunk);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.2f GB -> %.2f KB (%4.1fx)\n",
         (double)nbytes / GB, (double)cbytes / KB, (double)nbytes / (double)cbytes);
  printf("Compression time: %.3g s, %.1f TB/s\n",
         ttotal, (double)nbytes / (ttotal * TB));

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  blosc_set_timestamp(&last);
  int32_t leftover_bytes = (int32_t)((nitems % CHUNKSHAPE) * sizeof(int32_t));
  int32_t nchunks = leftover_bytes ? NCHUNKS + 1 : NCHUNKS;
  for (nchunk = 0; nchunk < nchunks; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk.  Error code: %d\n", dsize);
      return dsize;
    }
    if ((nchunk == nchunks - 1) && (leftover_bytes > 0)) {
      if (dsize != leftover_bytes) {
        printf("Wrong size for last chunk.  It is %d and should be: %d\n", dsize, leftover_bytes);
        return dsize;
      }
    }
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f GB/s\n",
         ttotal, (double)nbytes / (ttotal * GB));

  /* Free resources */
  blosc2_schunk_free(schunk);
  free(data);
  free(data_dest);
  free(data_dest2);

  return 0;
}


int main(void) {
  blosc2_init();

#ifdef CREATE_ZEROS
  printf("\n   ***  Creating zeros   ***\n");
#else
  printf("\n   ***  Creating uninitialized   ***\n");
#endif
#ifdef CREATE_FILL
  printf("   ***  Using fill method!   ***\n");
#elif defined(CREATE_LOOP)
  printf("   ***  Using loop method!   ***\n");
#else
  printf("   ***  Using not optimized method!   ***\n");
#endif

  create_cframe("blosclz", true);
  create_cframe("blosclz", false);
  create_cframe("lz4", true);
  create_cframe("lz4", false);

  blosc2_destroy();
}

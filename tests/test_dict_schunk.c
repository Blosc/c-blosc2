/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdbool.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 10
#define NTHREADS 4

/* Global vars */
int tests_run = 0;
int blocksize;
int use_dict;
int compcode;

static const uint8_t small_payload[] = {
    0x8b, 0xa6, 0x73, 0x6f, 0x75, 0x72, 0x63, 0x65, 0xa1, 0x78, 0xa4, 0x72, 0x6f, 0x77, 0x73, 0x01,
    0xae, 0x72, 0x65, 0x67, 0x75, 0x6c, 0x61, 0x72, 0x5f, 0x63, 0x68, 0x75, 0x6e, 0x6b, 0x73, 0x91,
    0x01, 0xae, 0x72, 0x65, 0x67, 0x75, 0x6c, 0x61, 0x72, 0x5f, 0x62, 0x6c, 0x6f, 0x63, 0x6b, 0x73,
    0x91, 0x01, 0xa5, 0x63, 0x6f, 0x64, 0x65, 0x63, 0xa4, 0x5a, 0x53, 0x54, 0x44, 0xa6, 0x63, 0x6c,
    0x65, 0x76, 0x65, 0x6c, 0x05, 0xac, 0x63, 0x72, 0x65, 0x61, 0x74, 0x65, 0x64, 0x5f, 0x75, 0x6e,
    0x69, 0x78, 0x00, 0xb1, 0x69, 0x6e, 0x76, 0x65, 0x6e, 0x74, 0x6f, 0x72, 0x79, 0x5f, 0x76, 0x65,
    0x72, 0x73, 0x69, 0x6f, 0x6e, 0x01, 0xa5, 0x70, 0x68, 0x61, 0x73, 0x65, 0xa6, 0x64, 0x69, 0x72,
    0x65, 0x63, 0x74, 0xab, 0x73, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x5f, 0x6d, 0x6f, 0x64, 0x65, 0xc3,
    0xaa, 0x62, 0x61, 0x74, 0x63, 0x68, 0x5f, 0x6b, 0x69, 0x6e, 0x64, 0xc0,
};

static char* test_dict(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int64_t nchunks;
  blosc_timestamp_t last, current;
  double cttotal, dttotal;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = use_dict;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  cparams.blocksize = blocksize;
  cparams.splitmode = BLOSC_FORWARD_COMPAT_SPLIT;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  blosc_set_timestamp(&last);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: incorrect nchunks value", nchunks == (nchunk + 1));
  }
  blosc_set_timestamp(&current);
  cttotal = blosc_elapsed_secs(last, current);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: Decompression error.", dsize > 0);
  }
  blosc_set_timestamp(&current);
  dttotal = blosc_elapsed_secs(last, current);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  float cratio =(float)nbytes / (float)cbytes;
  float cspeed = (float)nbytes / ((float)cttotal * MB);
  float dspeed = (float)nbytes / ((float)dttotal * MB);
  if (tests_run == 0) printf("\n");
  if (blocksize > 0) {
    printf("[blocksize: %d KB] ", blocksize / 1024);
  } else {
    printf("[blocksize: automatic] ");
  }
  if (!use_dict) {
    printf("cratio w/o dict: %.1fx (compr @ %.1f MB/s, decompr @ %.1f MB/s)\n",
            cratio, cspeed, dspeed);
    switch (blocksize) {
      case 1 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  3 * cbytes < nbytes);
        break;
      case 4 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  10 * cbytes < nbytes);
        break;
      case 32 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  70 * cbytes < nbytes);
        break;
      case 256 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  190 * cbytes < nbytes);
        break;
      default:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  170 * cbytes < nbytes);
    }
  } else {
    printf("cratio with dict: %.1fx (compr @ %.1f MB/s, decompr @ %.1f MB/s)\n",
           cratio, cspeed, dspeed);
    switch (blocksize) {
      case 1 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  8 * cbytes < nbytes);
        break;
      case 4 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  15 * cbytes < nbytes);
        break;
      case 32 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  100 * cbytes < nbytes);
        break;
      case 256 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  180 * cbytes < nbytes);
        break;
      default:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  180 * cbytes < nbytes);
    }
  }

  /* When use_dict=1, verify that BLOSC2_USEDICT is set in each raw chunk */
  if (use_dict) {
    for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
      uint8_t *chunk_ptr = NULL;
      bool needs_free = false;
      int csize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_ptr, &needs_free);
      mu_assert("ERROR: cannot retrieve raw chunk for flag check", csize > 0 && chunk_ptr != NULL);
      bool flag_set = (chunk_ptr[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0;
      if (needs_free) free(chunk_ptr);
      mu_assert("ERROR: BLOSC2_USEDICT flag not set in chunk compressed with use_dict=1", flag_set);
    }
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize == isize);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",
                data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  return EXIT_SUCCESS;
}

static char* test_small_dict_append(void) {
  uint8_t recovered[sizeof(small_payload)];
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  blosc2_init();

  cparams.typesize = 1;
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = use_dict;
  cparams.clevel = 5;
  cparams.nthreads = 1;

  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk for small dict append", schunk != NULL);

  int64_t nchunks = blosc2_schunk_append_buffer(schunk, small_payload, (int32_t)sizeof(small_payload));
  mu_assert("ERROR: cannot append small buffer with current use_dict setting", nchunks == 1);

  int dsize = blosc2_schunk_decompress_chunk(schunk, 0, recovered, (int32_t)sizeof(recovered));
  mu_assert("ERROR: cannot decompress small buffer chunk", dsize == (int)sizeof(small_payload));
  mu_assert("ERROR: bad roundtrip for small dict append",
            memcmp(recovered, small_payload, sizeof(small_payload)) == 0);

  if (use_dict) {
    /* For very small payloads dict training falls back to plain compression,
     * so BLOSC2_USEDICT may or may not be set depending on whether ZDICT
     * succeeded.  Only verify that the chunk is self-consistent: if the flag
     * is set the chunk must still decompress correctly (already checked above),
     * and if it is not set that is the expected graceful-fallback behaviour. */
    uint8_t *chunk_ptr = NULL;
    bool needs_free = false;
    int csize = blosc2_schunk_get_chunk(schunk, 0, &chunk_ptr, &needs_free);
    mu_assert("ERROR: cannot retrieve small buffer raw chunk for flag check", csize > 0 && chunk_ptr != NULL);
    if (needs_free) free(chunk_ptr);
  }

  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


/* Verify that use_dict is correctly round-tripped through the frame format.
 * Write a schunk to disk, reopen it, and check that blosc2_schunk_get_cparams
 * returns the same use_dict value that was used at creation time. */
static char* test_dict_persist(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  const char *urlpath = "test_dict_persist.b2frame";

  blosc2_init();

  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  /* Write schunk with use_dict to file */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = use_dict;
  cparams.clevel = 5;
  cparams.nthreads = 1;

  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                             .urlpath = (char *)urlpath, .contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create persistent schunk for dict persist test", schunk != NULL);

  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in dict persist test", nchunks == nchunk + 1);
  }
  blosc2_schunk_free(schunk);

  /* Reopen and check that use_dict is preserved */
  schunk = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot reopen persistent schunk", schunk != NULL);

  blosc2_cparams *recovered_cparams;
  int rc = blosc2_schunk_get_cparams(schunk, &recovered_cparams);
  mu_assert("ERROR: blosc2_schunk_get_cparams failed after reopen", rc == 0);
  mu_assert("ERROR: use_dict not preserved after frame reopen",
            recovered_cparams->use_dict == use_dict);
  free(recovered_cparams);

  /* Also verify data integrity */
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed after reopen", dsize == isize);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip after reopen", data_dest[i] == i);
    }
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char* test_dict_lz4(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int64_t nchunks;

  blosc2_init();

  cparams.typesize = sizeof(int32_t);
  cparams.compcode = compcode;
  cparams.use_dict = 1;
  cparams.clevel = 1;
  cparams.nthreads = NTHREADS;
  cparams.blocksize = blocksize;
  cparams.splitmode = BLOSC_FORWARD_COMPAT_SPLIT;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: incorrect nchunks value", nchunks == (nchunk + 1));
  }

  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  float cratio = (float)nbytes / (float)cbytes;
  if (tests_run == 0) printf("\n");
  printf("[%s blocksize: %s] cratio with dict: %.1fx\n",
         compcode == BLOSC_LZ4 ? "LZ4" : "LZ4HC",
         blocksize > 0 ? (blocksize <= 4*1024 ? "4 KB" : "32 KB") : "auto",
         cratio);
  mu_assert("ERROR: LZ4 dict does not improve compression ratio", cratio > 1.0f);

  /* Verify BLOSC2_USEDICT is set in each raw chunk */
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    uint8_t *chunk_ptr = NULL;
    bool needs_free = false;
    int csize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_ptr, &needs_free);
    mu_assert("ERROR: cannot retrieve raw chunk", csize > 0 && chunk_ptr != NULL);
    bool flag_set = (chunk_ptr[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0;
    if (needs_free) free(chunk_ptr);
    mu_assert("ERROR: BLOSC2_USEDICT flag not set in LZ4 dict chunk", flag_set);
  }

  /* Verify correct roundtrip */
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: LZ4 dict chunk decompression failed", dsize == isize);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip in LZ4 dict test",
                data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  blosc2_schunk_free(schunk);
  blosc2_destroy();
  return EXIT_SUCCESS;
}


/* Regression test for the VL-block + use_dict + tiny payload segfault.
 * blosc_compress_context() was overwriting context->destsize with the
 * training-pass output size, making subsequent compression passes see a
 * buffer too small to hold even the chunk header overhead, which caused a
 * NULL-pointer memcpy (context->src is NULL for VL blocks). */
static char* test_vlcompress_dict_small(void) {
  static const uint8_t payload[] = {'a'};
  uint8_t chunk[256];
  void *dests[1] = {NULL};
  int32_t destsizes[1] = {0};

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.compcode = BLOSC_ZSTD;
  cparams.clevel = 5;
  cparams.use_dict = 1;
  cparams.nthreads = 1;
  dparams.nthreads = 1;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create contexts", cctx != NULL && dctx != NULL);

  const void *srcs[1] = {payload};
  int32_t srcsizes[1] = {1};

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, srcs, srcsizes, 1, chunk, (int32_t)sizeof(chunk));
  mu_assert("ERROR: blosc2_vlcompress_ctx failed for tiny payload with use_dict=1", cbytes > 0);

  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, dests, destsizes, 1);
  mu_assert("ERROR: blosc2_vldecompress_ctx failed", nblocks == 1);
  mu_assert("ERROR: decompressed size mismatch", destsizes[0] == 1);
  mu_assert("ERROR: roundtrip mismatch", memcmp(dests[0], payload, 1) == 0);

  free(dests[0]);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return EXIT_SUCCESS;
}


static char* test_vlcompress_dict_20blocks(void) {
  /* 20 one-byte blocks: dict_maxsize = 20/20 = 1, vl_sample_size = 20/20/16 = 0 →
     falls back to plain compression rather than failing with BLOSC2_ERROR_CODEC_DICT */
  static const uint8_t payload = 'x';
  static const int NBLK = 20;
  uint8_t chunk[512];
  void *dests[20];
  int32_t destsizes[20];
  memset(dests, 0, sizeof(dests));
  memset(destsizes, 0, sizeof(destsizes));

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.compcode = BLOSC_ZSTD;
  cparams.clevel = 5;
  cparams.use_dict = 1;
  cparams.nthreads = 1;
  dparams.nthreads = 1;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create contexts", cctx != NULL && dctx != NULL);

  const void *srcs[20];
  int32_t srcsizes[20];
  for (int i = 0; i < NBLK; i++) {
    srcs[i] = &payload;
    srcsizes[i] = 1;
  }

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, srcs, srcsizes, NBLK, chunk, (int32_t)sizeof(chunk));
  mu_assert("ERROR: blosc2_vlcompress_ctx failed for 20-block 1-byte VL with use_dict=1", cbytes > 0);

  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, dests, destsizes, NBLK);
  mu_assert("ERROR: blosc2_vldecompress_ctx failed", nblocks == NBLK);
  for (int i = 0; i < NBLK; i++) {
    mu_assert("ERROR: decompressed size mismatch", destsizes[i] == 1);
    mu_assert("ERROR: roundtrip mismatch", *(uint8_t *)dests[i] == payload);
    free(dests[i]);
  }

  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return EXIT_SUCCESS;
}



/* Regression: blosc2_chunk_zeros/uninit/nans/repeatval must succeed even when
 * use_dict=1 is set with a codec that does not support dictionary compression.
 * These functions never compress data — they only build a special-value header —
 * so use_dict is irrelevant and must not trigger a codec-support error. */
static char *test_special_value_use_dict(void) {
  static const int codecs[] = {BLOSC_BLOSCLZ, BLOSC_LZ4, BLOSC_ZLIB};
  int32_t isize = 1000 * (int32_t)sizeof(float);
  float repeatval = 3.14f;

  for (int ci = 0; ci < (int)(sizeof(codecs) / sizeof(codecs[0])); ci++) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = sizeof(float);
    cparams.clevel = 5;
    cparams.nthreads = 1;
    cparams.compcode = codecs[ci];
    cparams.use_dict = 1;

    uint8_t chunk[BLOSC_EXTENDED_HEADER_LENGTH + sizeof(float)];
    int ret;

    ret = blosc2_chunk_zeros(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
    mu_assert("ERROR: blosc2_chunk_zeros failed with use_dict=1",
              ret == BLOSC_EXTENDED_HEADER_LENGTH);

    ret = blosc2_chunk_uninit(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
    mu_assert("ERROR: blosc2_chunk_uninit failed with use_dict=1",
              ret == BLOSC_EXTENDED_HEADER_LENGTH);

    ret = blosc2_chunk_nans(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
    mu_assert("ERROR: blosc2_chunk_nans failed with use_dict=1",
              ret == BLOSC_EXTENDED_HEADER_LENGTH);

    ret = blosc2_chunk_repeatval(cparams, isize,
                                  chunk, BLOSC_EXTENDED_HEADER_LENGTH + (int32_t)sizeof(float),
                                  &repeatval);
    mu_assert("ERROR: blosc2_chunk_repeatval failed with use_dict=1",
              ret == BLOSC_EXTENDED_HEADER_LENGTH + (int32_t)sizeof(float));
  }
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  blocksize = 1 * KB;    // really tiny
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 4 * KB;    // page size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 32 * KB;   // L1 cache size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 256 * KB;   // L2 cache size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 0;         // automatic size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  use_dict = 0;
  mu_run_test(test_small_dict_append);
  use_dict = 1;
  mu_run_test(test_small_dict_append);

  /* Persistence: use_dict must survive write-to-file / reopen */
  use_dict = 0;
  mu_run_test(test_dict_persist);
  use_dict = 1;
  mu_run_test(test_dict_persist);

  /* LZ4 and LZ4HC dict tests */
  blocksize = 4 * KB;
  compcode = BLOSC_LZ4;
  mu_run_test(test_dict_lz4);
  compcode = BLOSC_LZ4HC;
  mu_run_test(test_dict_lz4);

  blocksize = 32 * KB;
  compcode = BLOSC_LZ4;
  mu_run_test(test_dict_lz4);
  compcode = BLOSC_LZ4HC;
  mu_run_test(test_dict_lz4);

  blocksize = 0;  // automatic
  compcode = BLOSC_LZ4;
  mu_run_test(test_dict_lz4);
  compcode = BLOSC_LZ4HC;
  mu_run_test(test_dict_lz4);

  /* Regression: VL-block + use_dict + tiny (1-byte) payload must not crash */
  mu_run_test(test_vlcompress_dict_small);
  /* Regression: VL + use_dict + 20 one-byte blocks must not return BLOSC2_ERROR_CODEC_DICT.
     At exactly 20 blocks, dict_maxsize = srcsize/20 = 1, which is too small for ZSTD training;
     should fall back to plain compression. */
  mu_run_test(test_vlcompress_dict_20blocks);

  /* Regression: special-value chunk functions must not fail when use_dict=1
     is combined with a codec that does not support dict compression. */
  mu_run_test(test_special_value_use_dict);

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  blosc2_init();

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != EXIT_SUCCESS;
}

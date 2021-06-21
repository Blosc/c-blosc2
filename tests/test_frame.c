/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdbool.h>
#include "test_common.h"
#include "frame.h"

#if defined(_WIN32)
#define snprintf _snprintf
#endif

#define CHUNKSIZE (40 * 1000)
#define NTHREADS (4)

/* Global vars */
int nchunks_[] = {0, 1, 2, 5};
int tests_run = 0;
int nchunks;
int32_t blocksize_[] = {0, 20 * 1000};
int32_t blocksize;
bool multithread;
bool splits;
bool free_new;
bool sparse_schunk;
bool filter_pipeline;
bool metalayers;
bool vlmetalayers;
char *fname;
char buf[256];


static char* test_frame(void) {
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t *data = malloc(isize);
  int32_t *data_dest = malloc(isize);
  int dsize;
  int64_t nbytes, cbytes;
  uint8_t *buffer;
  bool buffer_needs_free;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  if (filter_pipeline) {
    cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
    cparams.filters_meta[BLOSC2_MAX_FILTERS - 2] = 0;
  }
  blosc2_schunk *schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a frame container */
  cparams.typesize = sizeof(int32_t);
  if (splits) {
    // Use a codec that splits blocks (important for lazy chunks).
    // Only BLOSCLZ is doing that.
    cparams.compcode = BLOSC_BLOSCLZ;
  } else {
    cparams.compcode = BLOSC_LZ4;
  }
  cparams.blocksize = blocksize;

  if (multithread) {
    cparams.nthreads = NTHREADS;
    dparams.nthreads = NTHREADS;
  } else {
    cparams.nthreads = 1;
    dparams.nthreads = 1;
  }
  blosc2_storage storage = {.contiguous=true, .urlpath=fname, .cparams=&cparams, .dparams=&dparams};
  if (fname != NULL) {
    if (strncmp(fname, "file:///", strlen("file:///")) == 0) {
      char *fname2 = fname + strlen("file:///");
      remove(fname2);
    }
    else {
      remove(fname);
    }
  }
  schunk = blosc2_schunk_new(&storage);
  mu_assert("blosc2_schunk_new() failed", schunk != NULL);
  char* content = "This is a pretty long string with a good number of chars";
  char* content2 = "This is a pretty long string with a good number of chars; longer than content";
  char* content3 = "This is a short string, and shorter than content";
  uint8_t* content_;
  size_t content_len = strlen(content);
  size_t content_len2 = strlen(content2);
  size_t content_len3 = strlen(content3);

  if (metalayers) {
    blosc2_meta_add(schunk, "metalayer1", (uint8_t *) "my metalayer1", sizeof("my metalayer1"));
    blosc2_meta_add(schunk, "metalayer2", (uint8_t *) "my metalayer1", sizeof("my metalayer1"));
  }

  if (vlmetalayers) {
    blosc2_vlmeta_add(schunk, "vlmetalayer", (uint8_t *) content, (int32_t) content_len, NULL);
  }

  if (!sparse_schunk) {
    if (free_new) {
      if (fname != NULL) {
        blosc2_schunk_free(schunk);
        schunk = blosc2_schunk_open(fname);
        mu_assert("blosc2_schunk_open() failed", schunk != NULL);
        mu_assert("storage is not recovered correctly",
                  schunk->storage->contiguous == true);
        mu_assert("cparams are not recovered correctly",
                  schunk->storage->cparams->clevel == BLOSC2_CPARAMS_DEFAULTS.clevel);
        mu_assert("dparams are not recovered correctly",
                  schunk->storage->dparams->nthreads == BLOSC2_DPARAMS_DEFAULTS.nthreads);
        mu_assert("blocksize is not recovered correctly",
                  schunk->storage->cparams->blocksize == cparams.blocksize);
      } else {
        // Dump the schunk into a buffer and regenerate it from there
        int64_t buffer_len = blosc2_schunk_to_buffer(schunk, &buffer, &buffer_needs_free);
        mu_assert("blosc2_schunk_to_buffer() failed", buffer_len > 0);
        blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(buffer, buffer_len, true);
        mu_assert("blosc2_schunk_from_buffer() failed", schunk2 != NULL);
        // We've made a copy, so it is safe to clean the original schunk up
        blosc2_schunk_free(schunk);
        schunk = schunk2;
        if (buffer_needs_free) {
          free(buffer);
        }
      }
    }
  }

  if (metalayers) {
    uint8_t* _content;
    uint32_t _content_len;
    blosc2_meta_get(schunk, "metalayer1", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_meta_get(schunk, "metalayer2", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
  }

  if (vlmetalayers) {
    uint32_t content_len_;
    blosc2_vlmeta_get(schunk, "vlmetalayer", &content_, &content_len_);
    mu_assert("ERROR: bad vlmetalayers length in frame", (size_t) content_len_ == content_len);
    mu_assert("ERROR: bad vlmetalayers data in frame", strncmp((char*)content_, content, content_len) == 0);
    free(content_);
    blosc2_vlmeta_update(schunk, "vlmetalayer", (uint8_t *) content2, (int32_t) content_len2, NULL);
  }

  // Feed it with data
  int _nchunks = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    _nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", _nchunks >= 0);
  }
  mu_assert("ERROR: wrong number of append chunks", _nchunks == nchunks);

  if (!sparse_schunk && schunk->frame != NULL) {
    blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
    mu_assert("ERROR: frame->len must be larger or equal than schunk->cbytes",
              frame->len >= schunk->cbytes + FRAME_HEADER_MINLEN);
  }

  if (metalayers) {
    uint8_t* _content;
    uint32_t _content_len;
    blosc2_meta_get(schunk, "metalayer1", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_meta_get(schunk, "metalayer2", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_meta_update(schunk, "metalayer2", (uint8_t *) "my metalayer2", sizeof("my metalayer2"));
  }

  if (vlmetalayers) {
    uint32_t content_len_;
    blosc2_vlmeta_get(schunk, "vlmetalayer", &content_, &content_len_);
    mu_assert("ERROR: bad vlmetalayers length in frame", (size_t) content_len_ == content_len2);
    mu_assert("ERROR: bad vlmetalayers data in frame", strncmp((char*)content_, content2, content_len2) == 0);
    free(content_);
    blosc2_vlmeta_update(schunk, "vlmetalayer", (uint8_t *) content3, (int32_t) content_len3, NULL);
  }

  if (!sparse_schunk) {
    if (free_new) {
      if (fname != NULL) {
        blosc2_schunk_free(schunk);
        schunk = blosc2_schunk_open(fname);
      } else {
        // Dump the schunk to a buffer and regenerate it from there
        int64_t buffer_len = blosc2_schunk_to_buffer(schunk, &buffer, &buffer_needs_free);
        mu_assert("blosc2_schunk_to_buffer() failed (2)", buffer_len > 0);
        blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(buffer, buffer_len, true);
        mu_assert("blosc2_schunk_from_buffer() failed (2)", schunk2 != NULL);
        // We've made a copy, so it is safe to clean the original schunk up
        blosc2_schunk_free(schunk);
        schunk = schunk2;
        if (buffer_needs_free) {
          free(buffer);
        }
      }
    }
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (nchunks > 0) {
    mu_assert("ERROR: bad compression ratio in frame", nbytes > 10 * cbytes);
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  if (metalayers) {
    uint8_t* _content;
    uint32_t _content_len;
    blosc2_meta_get(schunk, "metalayer1", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_meta_get(schunk, "metalayer2", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer2", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
  }

  if (vlmetalayers) {
    uint32_t content_len_;
    blosc2_vlmeta_get(schunk, "vlmetalayer", &content_, &content_len_);
    mu_assert("ERROR: bad vlmetalayers length in frame", (size_t) content_len_ == content_len3);
    mu_assert("ERROR: bad vlmetalayers data in frame", strncmp((char*)content_, content3, content_len3) == 0);
    free(content_);
  }

  /* Free resources */
  free(data_dest);
  free(data);
  blosc2_schunk_free(schunk);

  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {

  // Iterate over all different parameters
  for (int i = 0; i < (int)sizeof(nchunks_) / (int)sizeof(int); i++) {
    nchunks = nchunks_[i];
    for (int isplits = 0; isplits < 2; isplits++) {
      for (int imultithread = 0; imultithread < 2; imultithread++) {
        for (int ifree_new = 0; ifree_new < 2; ifree_new++) {
          for (int isparse_schunk = 0; isparse_schunk < 2; isparse_schunk++) {
            for (int ifilter_pipeline = 0; ifilter_pipeline < 2; ifilter_pipeline++) {
              for (int imetalayers = 0; imetalayers < 2; imetalayers++) {
                for (int ivlmetalayers = 0; ivlmetalayers < 2; ivlmetalayers++) {
                  for (int iblocksize = 0; iblocksize < sizeof(blocksize_) / sizeof(int32_t); ++iblocksize) {
                        blocksize = blocksize_[iblocksize];
                        splits = (bool) isplits;
                        multithread = (bool) imultithread;
                        sparse_schunk = (bool) isparse_schunk;
                        free_new = (bool) ifree_new;
                        filter_pipeline = (bool) ifilter_pipeline;
                        metalayers = (bool) imetalayers;
                        vlmetalayers = (bool) ivlmetalayers;
                        fname = NULL;
                        mu_run_test(test_frame);
                        // An easy way to test for file:/// prefix in some tests
                        if (splits) {
                          snprintf(buf, sizeof(buf), "test_frame_nc%d.b2frame", nchunks);
                        }
                        else {
                          snprintf(buf, sizeof(buf), "file:///test_frame_nc%d.b2frame", nchunks);
                        }
                        fname = buf;
                        mu_run_test(test_frame);
                    }
                }
              }
            }
          }
        }
      }
    }
  }

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  blosc_init();

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_destroy();

  return result != EXIT_SUCCESS;
}

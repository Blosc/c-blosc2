/*
  Copyright (C) 2019  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2019-08-06

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include <stdbool.h>
#include "test_common.h"
#include "frame.h"

#if defined(_WIN32)
#define snprintf _snprintf
#endif

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int nchunks_[] = {0, 1, 2, 10};
int tests_run = 0;
int nchunks;
bool free_new;
bool sparse_schunk;
bool filter_pipeline;
bool metalayers;
bool usermeta;
bool check_sframe;
char *fname;
char buf[256];


static char* test_frame(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  if (filter_pipeline) {
    cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
    cparams.filters_meta[BLOSC2_MAX_FILTERS - 2] = 0;
  }
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a frame container */
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.sequential=true, .path=fname, .cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(storage);
  mu_assert("blosc2_schunk_new() failed", schunk != NULL);
  char* content = "This is a pretty long string with a good number of chars";
  char* content2 = "This is a pretty long string with a good number of chars; longer than content";
  char* content3 = "This is a short string, and shorter than content";
  uint8_t* content_;
  size_t content_len = strlen(content);
  size_t content_len2 = strlen(content2);
  size_t content_len3 = strlen(content3);

  if (metalayers) {
    blosc2_add_metalayer(schunk, "metalayer1", (uint8_t*)"my metalayer1", sizeof("my metalayer1"));
    blosc2_add_metalayer(schunk, "metalayer2", (uint8_t*)"my metalayer1", sizeof("my metalayer1"));
  }

  if (usermeta) {
    blosc2_update_usermeta(schunk, (uint8_t *) content, (int32_t) content_len, BLOSC2_CPARAMS_DEFAULTS);
  }

  if (!sparse_schunk) {
    if (free_new) {
      if (fname != NULL) {
        blosc2_schunk_free(schunk);
        blosc2_storage storage2 = {.sequential=true, .path=fname};
        schunk = blosc2_schunk_open(storage2);
        mu_assert("blosc2_schunk_open() failed", schunk != NULL);
      } else {
        blosc2_frame* frame = schunk->frame;
        int64_t len = frame->len;
        uint8_t* sframe = malloc((size_t)len);
        memcpy(sframe, frame->sdata, (size_t)frame->len);
        blosc2_schunk_free(schunk);
        schunk = blosc2_schunk_from_memframe(sframe, len);
        mu_assert("blosc2_schunk_from_memframe() failed", schunk != NULL);
      }
    }
  }

  if (metalayers) {
    uint8_t* _content;
    uint32_t _content_len;
    blosc2_get_metalayer(schunk, "metalayer1", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_get_metalayer(schunk, "metalayer2", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
  }

  if (usermeta) {
    int content_len_ = blosc2_get_usermeta(schunk, &content_);
    mu_assert("ERROR: bad usermeta length in frame", (size_t) content_len_ == content_len);
    mu_assert("ERROR: bad usermeta data in frame", strncmp((char*)content_, content, content_len) == 0);
    free(content_);
    blosc2_update_usermeta(schunk, (uint8_t *) content2, (int32_t) content_len2, BLOSC2_CPARAMS_DEFAULTS);
  }

  // Feed it with data
  int _nchunks = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    _nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunk >= 0);
  }
  mu_assert("ERROR: wrong number of append chunks", _nchunks == nchunks);

  if (!sparse_schunk) {
    blosc2_frame* frame = schunk->frame;
    mu_assert("ERROR: frame->len must be larger or equal than schunk->cbytes",
              frame->len >= schunk->cbytes + FRAME_HEADER_MINLEN);
  }

  if (metalayers) {
    uint8_t* _content;
    uint32_t _content_len;
    blosc2_get_metalayer(schunk, "metalayer1", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_get_metalayer(schunk, "metalayer2", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_update_metalayer(schunk, "metalayer2", (uint8_t*)"my metalayer2", sizeof("my metalayer2"));
  }

  if (usermeta) {
    int content_len_ = blosc2_get_usermeta(schunk, &content_);
    mu_assert("ERROR: bad usermeta length in frame", (size_t) content_len_ == content_len2);
    mu_assert("ERROR: bad usermeta data in frame", strncmp((char*)content_, content2, content_len2) == 0);
    free(content_);
    blosc2_update_usermeta(schunk, (uint8_t *) content3, (int32_t) content_len3, BLOSC2_CPARAMS_DEFAULTS);
  }

  if (!sparse_schunk) {
    if (free_new) {
      if (fname != NULL) {
        blosc2_schunk_free(schunk);
        blosc2_storage storage2 = {.sequential=true, .path=fname};
        schunk = blosc2_schunk_open(storage2);
      } else {
        blosc2_frame* frame = schunk->frame;
        int64_t len = frame->len;
        uint8_t *sframe = malloc((size_t)len);
        memcpy(sframe, frame->sdata, (size_t)frame->len);
        blosc2_schunk_free(schunk);
        schunk = blosc2_schunk_from_memframe(sframe, len);
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
    blosc2_get_metalayer(schunk, "metalayer1", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer1", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
    blosc2_get_metalayer(schunk, "metalayer2", &_content, &_content_len);
    mu_assert("ERROR: bad metalayer content", strncmp((char*)_content, "my metalayer2", _content_len) == 0);
    if (_content != NULL) {
      free(_content);
    }
  }

  if (usermeta) {
    int content_len_ = blosc2_get_usermeta(schunk, &content_);
    mu_assert("ERROR: bad usermeta length in frame", (size_t) content_len_ == content_len3);
    mu_assert("ERROR: bad usermeta data in frame", strncmp((char*)content_, content3, content_len3) == 0);
    free(content_);
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {

  // Iterate over all different parameters
  for (int i = 0; i < (int)sizeof(nchunks_) / (int)sizeof(int); i++) {
    nchunks = nchunks_[i];
    for (int ifree_new = 0; ifree_new < 2; ifree_new++) {
      for (int isparse_schunk = 0; isparse_schunk < 2; isparse_schunk++) {
        for (int ifilter_pipeline = 0; ifilter_pipeline < 2; ifilter_pipeline++) {
          for (int imetalayers = 0; imetalayers < 2; imetalayers++) {
            for (int iusermeta = 0; iusermeta < 2; iusermeta++) {
              sparse_schunk = (bool) isparse_schunk;
              free_new = (bool) ifree_new;
              filter_pipeline = (bool) ifilter_pipeline;
              metalayers = (bool) imetalayers;
              usermeta = (bool) iusermeta;
              fname = NULL;
              mu_run_test(test_frame);
              snprintf(buf, sizeof(buf), "test_frame_nc%d.b2frame", nchunks);
              fname = buf;
              mu_run_test(test_frame);
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

/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Each error of blosc2_create_cctx() must return NULL and free the context with
  the allocator it came from (plain free() corrupts the heap on Windows).  Run
  under LeakSanitizer to see the leaks.
*/

#include <stdio.h>

#include "frame.h"
#include "test_common.h"

#define NITEMS 1000
#define USER_CODEC 200
#define USER_CODEC_NAME "test_create_ctx_codec"
#define TUNER_INIT_FAILS 170
#define TUNER_NOT_LOADABLE 171
#define TUNER_NOT_REGISTERED 250

int tests_run = 0;

static void unset_env(const char *name) {
#if defined(_WIN32)
  _putenv_s(name, "");  // an empty value removes the variable
#else
  unsetenv(name);
#endif
}

static int noop_prefilter(blosc2_prefilter_params *params) {
  (void) params;
  return 0;
}

static blosc2_prefilter_params preparams = {0};

/* With a prefilter, so that the later error paths also have its params to free */
static blosc2_cparams cparams_with_prefilter(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.prefilter = noop_prefilter;
  cparams.preparams = &preparams;
  return cparams;
}


static char* test_undefined_filter(void) {
  struct {
    uint8_t filter;
    char *message;
  } cases[] = {
    {BLOSC_LAST_FILTER, "A Blosc-defined filter id that is not defined must be rejected"},
    {BLOSC_LAST_REGISTERED_FILTER + 1, "A Blosc-registered filter id that is not registered must be rejected"},
  };
  for (int i = 0; i < (int) ARRAY_SIZE(cases); ++i) {
    blosc2_cparams cparams = cparams_with_prefilter();
    cparams.filters[0] = cases[i].filter;
    mu_assert(cases[i].message, blosc2_create_cctx(cparams) == NULL);
  }
  return EXIT_SUCCESS;
}


static char* test_user_codec_in_env(void) {
  blosc2_codec codec = {.compcode = USER_CODEC, .compname = USER_CODEC_NAME, .complib = USER_CODEC,
                        .version = 1};
  mu_assert("Cannot register the codec", blosc2_register_codec(&codec) == 0);
  setenv("BLOSC_COMPRESSOR", USER_CODEC_NAME, 1);
  blosc2_context *cctx = blosc2_create_cctx(cparams_with_prefilter());
  unset_env("BLOSC_COMPRESSOR");
  mu_assert("A user-defined codec in BLOSC_COMPRESSOR must be rejected", cctx == NULL);
  return EXIT_SUCCESS;
}


static int failing_tuner_init(void *config, blosc2_context *cctx, blosc2_context *dctx) {
  (void) config;
  (void) cctx;
  (void) dctx;
  return BLOSC2_ERROR_FAILURE;
}


static char* test_tuner_errors(void) {
  blosc2_tuner init_fails = {.init = failing_tuner_init, .id = TUNER_INIT_FAILS, .name = "test_create_ctx_tuner"};
  // Without an init function the tuner is looked up as a plugin library; this
  // name is refused before any library is searched for
  blosc2_tuner not_loadable = {.init = NULL, .id = TUNER_NOT_LOADABLE, .name = "not a plugin name"};
  mu_assert("Cannot register the tuner", blosc2_register_tuner(&init_fails) == 0);
  mu_assert("Cannot register the tuner", blosc2_register_tuner(&not_loadable) == 0);
  struct {
    int tuner_id;
    char *message;
  } cases[] = {
    {TUNER_NOT_REGISTERED, "A tuner that is not registered must be rejected"},
    {TUNER_INIT_FAILS, "A tuner whose init fails must be rejected"},
    {TUNER_NOT_LOADABLE, "A tuner that cannot be loaded must be rejected"},
  };
  for (int i = 0; i < (int) ARRAY_SIZE(cases); ++i) {
    blosc2_cparams cparams = cparams_with_prefilter();
    cparams.tuner_id = cases[i].tuner_id;
    mu_assert(cases[i].message, blosc2_create_cctx(cparams) == NULL);
  }
  return EXIT_SUCCESS;
}


static char* test_schunk_new(void) {
  blosc2_cparams cparams = cparams_with_prefilter();
  cparams.filters[0] = BLOSC_LAST_FILTER;
  blosc2_storage storage = {.cparams = &cparams};
  mu_assert("A super-chunk with a filter that is not defined must be rejected",
            blosc2_schunk_new(&storage) == NULL);
  return EXIT_SUCCESS;
}


/* A frame written with a filter this library does not know (e.g. one
   registered by a newer version) cannot be opened */
static char* test_frame_unknown_filter(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create the super-chunk", schunk != NULL);
  int32_t data[NITEMS];
  for (int i = 0; i < NITEMS; ++i) {
    data[i] = i;
  }
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, (int32_t) sizeof(data)) == 1);
  uint8_t *cframe;
  bool cframe_needs_free;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  blosc2_schunk_free(schunk);
  mu_assert("Cannot export the frame", cframe_len > FRAME_HEADER_MINLEN && cframe_needs_free);
  cframe[FRAME_FILTER_PIPELINE + 1] = BLOSC_LAST_REGISTERED_FILTER + 1;
  blosc2_schunk *opened = blosc2_schunk_from_buffer(cframe, cframe_len, false);
  free(cframe);
  mu_assert("A frame with a filter that is not defined must be rejected", opened == NULL);
  return EXIT_SUCCESS;
}


/* Compress and decompress with fresh contexts: the heap must still be fine */
static char* test_roundtrip_after_errors(void) {
  int32_t src[NITEMS];
  int32_t dest[NITEMS];
  uint8_t chunk[NITEMS * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD];
  for (int i = 0; i < NITEMS; ++i) {
    src[i] = i;
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("Cannot create a compression context", cctx != NULL);
  int csize = blosc2_compress_ctx(cctx, src, (int32_t) sizeof(src), chunk, (int32_t) sizeof(chunk));
  blosc2_free_ctx(cctx);
  mu_assert("Cannot compress", csize > 0);
  blosc2_context *dctx = blosc2_create_dctx(BLOSC2_DPARAMS_DEFAULTS);
  mu_assert("Cannot create a decompression context", dctx != NULL);
  int dsize = blosc2_decompress_ctx(dctx, chunk, csize, dest, (int32_t) sizeof(dest));
  blosc2_free_ctx(dctx);
  mu_assert("Cannot decompress", dsize == (int) sizeof(dest));
  mu_assert("The round trip changed the data", memcmp(src, dest, sizeof(src)) == 0);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_undefined_filter);
  mu_run_test(test_user_codec_in_env);
  mu_run_test(test_tuner_errors);
  mu_run_test(test_schunk_new);
  mu_run_test(test_frame_unknown_filter);
  mu_run_test(test_roundtrip_after_errors);
  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  blosc2_init();
  result = all_tests();
  blosc2_destroy();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  return result != EXIT_SUCCESS;
}

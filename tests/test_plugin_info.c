/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Dynamic plugins (filters, codecs and tuners registered without functions)
  are loaded on first use through the `info` symbol of their library.  A
  library without `info`, with an `info` that names no functions or with
  one that names a function the library lacks must give an error, every
  time it is used, and never a crash.  The broken plugins are built from
  plugin_info_lib.c.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include "blosc-private.h"

#define NBYTES (64 * 1024)
#define FIRST_ID 240

static char *plugins[] = {"plugin_info_none", "plugin_info_null", "plugin_info_missing"};

/* Else every use would fail to find it, and the test would pass without
   loading anything */
static bool library_found(char *plugin) {
  char libpath[PATH_MAX];
  void *lib = load_lib(plugin, libpath);
  if (lib == NULL) {
    printf("library of plugin %s not found\n", plugin);
    return false;
  }
  dlclose(lib);
  return true;
}

static int compress_with(uint8_t compcode, uint8_t filter, const uint8_t *src, uint8_t *dest) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.nthreads = 1;
  cparams.compcode = compcode;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    return BLOSC2_ERROR_NULL_POINTER;
  }
  int rc = blosc2_compress_ctx(cctx, src, NBYTES, dest, NBYTES + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  return rc;
}

static bool tuner_loads(int tuner_id) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 1;
  cparams.tuner_id = tuner_id;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    return false;
  }
  blosc2_free_ctx(cctx);
  return true;
}

int main(void) {
  int failures = 0;
  uint8_t *src = malloc(NBYTES);
  uint8_t *dest = malloc(NBYTES + BLOSC2_MAX_OVERHEAD);
  if (src == NULL || dest == NULL) {
    free(src);
    free(dest);
    return EXIT_FAILURE;
  }
  blosc_test_fill_seq(src, NBYTES);

  blosc2_init();
  for (int p = 0; p < (int)ARRAY_SIZE(plugins); p++) {
    if (!library_found(plugins[p])) {
      failures++;
      continue;
    }
    uint8_t id = (uint8_t)(FIRST_ID + p);
    blosc2_filter filter = {id, plugins[p], 1, NULL, NULL};
    blosc2_codec codec = {id, plugins[p], id, 1, NULL, NULL};
    blosc2_tuner tuner = {NULL, NULL, NULL, NULL, NULL, id, plugins[p]};
    if (blosc2_register_filter(&filter) < 0 || blosc2_register_codec(&codec) < 0 ||
        blosc2_register_tuner(&tuner) < 0) {
      printf("%s: cannot register\n", plugins[p]);
      failures++;
      continue;
    }
    /* Twice: a failed load must not leave anything behind */
    for (int attempt = 1; attempt <= 2; attempt++) {
      int rc = compress_with(BLOSC_BLOSCLZ, id, src, dest);
      if (rc != BLOSC2_ERROR_FILTER_PIPELINE) {
        printf("%s as a filter: got %d (attempt %d)\n", plugins[p], rc, attempt);
        failures++;
      }
      rc = compress_with(id, BLOSC_NOFILTER, src, dest);
      if (rc != BLOSC2_ERROR_CODEC_SUPPORT) {
        printf("%s as a codec: got %d (attempt %d)\n", plugins[p], rc, attempt);
        failures++;
      }
      if (tuner_loads(id)) {
        printf("%s as a tuner: no error (attempt %d)\n", plugins[p], attempt);
        failures++;
      }
    }
  }
  blosc2_destroy();

  free(src);
  free(dest);
  if (failures > 0) {
    printf("test_plugin_info: %d failures\n", failures);
    return EXIT_FAILURE;
  }
  printf("test_plugin_info: all broken plugins gave errors\n");
  return EXIT_SUCCESS;
}

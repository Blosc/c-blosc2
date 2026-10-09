/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating how to store chunks of different
  lengths inside a single variable-sized-chunk schunk.
*/

#include <stdio.h>

#include "blosc2.h"

// static const char short_string[] = "short";
// static const char longer_string[] = "a bit longer string";
// static const char longest_string[] = "this is the longest string stored in the chunk";

int main(void) {
  blosc2_schunk *schunk = NULL;
  const char *urlpath = "vschunks_example.b2frame";
  int rc = 1;

  blosc2_init();
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
    .contiguous = true,
    .urlpath = (char *)urlpath,
    .cparams = &cparams,
    .dparams = &dparams,
  };
  
  schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    fprintf(stderr, "cannot create frame-backed schunk\n");
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}

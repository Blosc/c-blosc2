/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "blosc2.h"

static const int nitems = 100;
static const size_t typesize = sizeof(int32_t);

int main(void) {
  int rc = 1;

  const char *urlpath = "sparse_coords.b2frame";
  uint8_t *dataset = NULL;
  blosc2_schunk *schunk = NULL;

  blosc2_init();
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
    .contiguous = true,
    .urlpath = (char *)urlpath,
    .cparams = &cparams,
    .dparams = &dparams,
  };

  dataset = malloc((size_t)nitems * (size_t)typesize);
  if (dataset == NULL) {
    fprintf(stderr, "failed to malloc dataset");
    goto cleanup;
  }

  for (int32_t i = 0; i < nitems; ++i) {
    memcpy(dataset + i * typesize, &i, (size_t)typesize);
  }

  schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    fprintf(stderr, "failed to create schunk\n");
    goto cleanup;
  }

  rc = 0;

cleanup:
  free(dataset);

  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}

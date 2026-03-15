/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating how to store strings of different
  lengths inside a single variable-length-block chunk.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"

static const char string0[] = "short";
static const char string1[] = "a bit longer string";
static const char string2[] = "this is the longest string stored in the chunk";

static const void *srcs[] = {
    string0,
    string1,
    string2,
};

static const int32_t srcsizes[] = {
    (int32_t)sizeof(string0),
    (int32_t)sizeof(string1),
    (int32_t)sizeof(string2),
};

static const char *labels[] = {
    "string0",
    "string1",
    "string2",
};

static int32_t total_nbytes(void) {
  return srcsizes[0] + srcsizes[1] + srcsizes[2];
}

static int print_strings(const char *title, void **buffers, const int32_t *sizes, int32_t nblocks) {
  printf("%s\n", title);
  for (int32_t i = 0; i < nblocks; ++i) {
    if (sizes[i] != srcsizes[i]) {
      fprintf(stderr, "unexpected size for %s: %d != %d\n", labels[i], sizes[i], srcsizes[i]);
      return -1;
    }
    if (memcmp(buffers[i], srcs[i], (size_t)srcsizes[i]) != 0) {
      fprintf(stderr, "content mismatch for %s\n", labels[i]);
      return -1;
    }
    printf("  %s (%d bytes): %s\n", labels[i], sizes[i], (char *)buffers[i]);
  }
  return 0;
}

int main(void) {
  const char *urlpath = "vlblocks_example.b2frame";
  uint8_t *buffers[3] = {NULL};
  int32_t sizes[3] = {0};
  uint8_t *chunk = NULL;
  blosc2_context *cctx = NULL;
  blosc2_context *dctx = NULL;
  blosc2_schunk *schunk = NULL;
  uint8_t *stored_chunk = NULL;
  bool needs_free = false;
  int rc = 1;

  blosc2_init();
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.nthreads = 2;
  cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    fprintf(stderr, "cannot create compression context\n");
    goto cleanup;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 2;
  dctx = blosc2_create_dctx(dparams);
  if (dctx == NULL) {
    fprintf(stderr, "cannot create decompression context\n");
    goto cleanup;
  }

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  chunk = malloc((size_t)destsize);
  if (chunk == NULL) {
    fprintf(stderr, "cannot allocate chunk buffer\n");
    goto cleanup;
  }

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, srcs, srcsizes, 3, chunk, destsize);
  if (cbytes <= 0) {
    fprintf(stderr, "VL-block compression failed: %d\n", cbytes);
    goto cleanup;
  }
  printf("stored %d strings in one VL-block chunk: %d -> %d bytes\n", 3, total_nbytes(), cbytes);

  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, (void **)buffers, sizes, 3);
  if (nblocks != 3) {
    fprintf(stderr, "VL-block decompression failed: %d\n", nblocks);
    goto cleanup;
  }
  if (print_strings("decompressed directly from the chunk:", (void **)buffers, sizes, nblocks) < 0) {
    goto cleanup;
  }

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
  if (blosc2_schunk_append_chunk(schunk, chunk, true) != 1) {
    fprintf(stderr, "cannot append VL-block chunk to schunk\n");
    goto cleanup;
  }
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open(urlpath);
  if (schunk == NULL) {
    fprintf(stderr, "cannot reopen frame-backed schunk\n");
    goto cleanup;
  }
  if (blosc2_schunk_get_chunk(schunk, 0, &stored_chunk, &needs_free) < 0) {
    fprintf(stderr, "cannot get stored VL-block chunk back from schunk\n");
    goto cleanup;
  }

  for (int i = 0; i < 3; ++i) {
    free(buffers[i]);
    buffers[i] = NULL;
    sizes[i] = 0;
  }
  nblocks = blosc2_vldecompress_ctx(dctx, stored_chunk, cbytes, (void **)buffers, sizes, 3);
  if (nblocks != 3) {
    fprintf(stderr, "cannot vldecompress reopened VL-block chunk: %d\n", nblocks);
    goto cleanup;
  }
  if (print_strings("recovered after reopening the frame:", (void **)buffers, sizes, nblocks) < 0) {
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (needs_free) {
    free(stored_chunk);
  }
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }
  if (dctx != NULL) {
    blosc2_free_ctx(dctx);
  }
  if (cctx != NULL) {
    blosc2_free_ctx(cctx);
  }
  for (int i = 0; i < 3; ++i) {
    free(buffers[i]);
  }
  free(chunk);
  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}

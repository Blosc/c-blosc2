/*
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Generate structured seed corpus inputs for fuzz_schunk_ops.c.

  Run:  ./gen_schunk_corpus <output-dir>

  Writes a few valid frames (with/without user-defined metalayers, several
  codecs and chunk counts) so libFuzzer starts from structurally valid input.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <blosc2.h>

static uint32_t pattern(int64_t i) {
  return (uint32_t)(i * 2654435761u);
}

static void write_seed(const char *dir, const char *name, uint8_t *cframe,
                       int64_t len) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    fprintf(stderr, "cannot open %s for writing\n", path);
    return;
  }
  fwrite(cframe, 1, (size_t)len, f);
  fclose(f);
  printf("Successfully created %s (%lld bytes)\n", name, (long long)len);
}

static void emit(const char *dir, const char *name, bool contiguous,
                 uint8_t compcode, uint8_t clevel, uint8_t typesize,
                 uint8_t filter, int nchunks, int chunk_items,
                 const char *meta_name, const char *meta_content) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = clevel;
  cparams.compcode = compcode;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                            .contiguous = contiguous};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return;
  }
  if (meta_name != NULL) {
    blosc2_meta_add(schunk, meta_name, (uint8_t *)meta_content,
                    (int32_t)strlen(meta_content));
  }
  int32_t bufsize = (int32_t)(chunk_items * typesize);
  uint32_t *data = (uint32_t *)malloc((size_t)bufsize);
  int64_t words = bufsize / 4;
  for (int i = 0; i < nchunks; i++) {
    for (int64_t k = 0; k < words; k++) {
      data[k] = pattern((int64_t)i * words + k);
    }
    blosc2_schunk_append_buffer(schunk, data, bufsize);
  }
  free(data);

  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  if (len > 0 && cframe != NULL) {
    write_seed(dir, name, cframe, len);
  }
  if (needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <output-dir>\n", argv[0]);
    return 1;
  }
  const char *dir = argv[1];

  blosc2_init();
  emit(dir, "schunk_meta_blosclz.b2frame", true, BLOSC_BLOSCLZ, 5, 4,
       BLOSC_SHUFFLE, 3, 1024, "fz", "seedmeta");
  emit(dir, "schunk_sparse_lz4.b2frame", false, BLOSC_LZ4, 6, 2,
       BLOSC_SHUFFLE, 2, 2048, NULL, NULL);
  emit(dir, "schunk_zstd_typesize8.b2frame", true, BLOSC_ZSTD, 9, 8,
       BLOSC_NOSHUFFLE, 2, 512, "b2nd", "\xa4" "b2nd");
  emit(dir, "schunk_many_chunks.b2frame", true, BLOSC_BLOSCLZ, 5, 1,
       BLOSC_BITSHUFFLE, 12, 256, NULL, NULL);

  blosc2_destroy();
  return 0;
}

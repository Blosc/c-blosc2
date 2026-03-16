/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Generator for VL-block cframes used in forward compatibility checks.

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc2.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BLOCKS 6
/* Each base block pattern is repeated this many times before compression */
#define BLOCK_REPEATS 10

typedef enum {
  OP_COMPRESS,
  OP_DECOMPRESS,
} op_t;

typedef struct {
  const char *blocks[MAX_BLOCKS];
  int32_t sizes[MAX_BLOCKS];
  int32_t nblocks;
} vl_chunk_data;

typedef struct {
  const vl_chunk_data *chunks;
  int32_t nchunks;
  bool variable_chunks;
} vl_frame_data;

static const vl_chunk_data fixed_chunks[] = {
    {   /* alphanumeric patterns */
        .blocks = {
            "abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuv",
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUV",
            "0123456789-0123456789-0123456789-0123456789-01234",
            "the-quick-brown-fox-jumps-over-the-lazy-dog-finis",
        },
        .sizes = {50, 50, 50, 50},
        .nblocks = 4,
    },
    {   /* space / astronomy */
        .blocks = {
            "mercury-venus-earth-mars-jupiter-saturn-uranus-ne",
            "sun-moon-milky-way-galaxy-star-pulsar-quasar-nova",
            "andromeda-galaxy-pegasus-cygnus-orion-lyra-aquila",
            "black-hole-neutron-star-white-dwarf-red-giant-sun",
        },
        .sizes = {50, 50, 50, 50},
        .nblocks = 4,
    },
    {   /* highly compressible (repeated characters) */
        .blocks = {
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            "ccccccccccccccccccccccccccccccccccccccccccccccccc",
            "ddddddddddddddddddddddddddddddddddddddddddddddddd",
        },
        .sizes = {50, 50, 50, 50},
        .nblocks = 4,
    },
    {   /* technical / numerical */
        .blocks = {
            "0xdeadbeef-0xcafebabe-0x01234567-0x89abcdef-0xfff",
            "3.14159265-2.71828182-1.41421356-1.73205080-3.141",
            "sha256-md5-sha1-sha512-hmac-aes256-rsa4096-ecdsa-",
            "blosc2-lz4-zstd-zlib-blosclz-lz4hc-snappy-gzip-bl",
        },
        .sizes = {50, 50, 50, 50},
        .nblocks = 4,
    },
    {   /* pangrams (natural language) */
        .blocks = {
            "pack-my-box-with-five-dozen-liquor-jugs-and-bread",
            "how-vexingly-quick-daft-zebras-jump-high-overhead",
            "sphinx-of-black-quartz-judge-my-vow-and-be-feared",
            "foxy-quincy-jumps-over-a-wilted-zinnia-bed-bright",
        },
        .sizes = {50, 50, 50, 50},
        .nblocks = 4,
    },
};

static const vl_chunk_data variable_chunks[] = {
    {   /* 2 blocks, total 100 bytes */
        .blocks = {
            "mercury-venus-earth-mars-jupiter-saturn-uranus-ne",
            "andromeda-galaxy-pegasus-cygnus-orion-lyra-aquila",
        },
        .sizes = {50, 50},
        .nblocks = 2,
    },
    {   /* 3 blocks, total 150 bytes */
        .blocks = {
            "blosc2-lz4-zstd-zlib-blosclz-lz4hc-snappy-gzip-bl",
            "3.14159265-2.71828182-1.41421356-1.73205080-3.141",
            "0xdeadbeef-0xcafebabe-0x01234567-0x89abcdef-0xfff",
        },
        .sizes = {50, 50, 50},
        .nblocks = 3,
    },
    {   /* 4 blocks, total 200 bytes */
        .blocks = {
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            "pack-my-box-with-five-dozen-liquor-jugs-and-bread",
            "how-vexingly-quick-daft-zebras-jump-high-overhead",
        },
        .sizes = {50, 50, 50, 50},
        .nblocks = 4,
    },
    {   /* 5 blocks, total 250 bytes */
        .blocks = {
            "abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuv",
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUV",
            "0123456789-0123456789-0123456789-0123456789-01234",
            "the-quick-brown-fox-jumps-over-the-lazy-dog-finis",
            "foxy-quincy-jumps-over-a-wilted-zinnia-bed-bright",
        },
        .sizes = {50, 50, 50, 50, 50},
        .nblocks = 5,
    },
    {   /* 6 blocks, total 300 bytes */
        .blocks = {
            "sun-moon-milky-way-galaxy-star-pulsar-quasar-nova",
            "black-hole-neutron-star-white-dwarf-red-giant-sun",
            "sphinx-of-black-quartz-judge-my-vow-and-be-feared",
            "ccccccccccccccccccccccccccccccccccccccccccccccccc",
            "ddddddddddddddddddddddddddddddddddddddddddddddddd",
            "sha256-md5-sha1-sha512-hmac-aes256-rsa4096-ecdsa-",
        },
        .sizes = {50, 50, 50, 50, 50, 50},
        .nblocks = 6,
    },
};

static const vl_frame_data fixed_frame = {
    .chunks = fixed_chunks,
    .nchunks = (int32_t)(sizeof(fixed_chunks) / sizeof(fixed_chunks[0])),
    .variable_chunks = false,
};

static const vl_frame_data variable_frame = {
    .chunks = variable_chunks,
    .nchunks = (int32_t)(sizeof(variable_chunks) / sizeof(variable_chunks[0])),
    .variable_chunks = true,
};

/* ---- Regular (non-VL-block) variable-size-chunk data ---- */
typedef struct {
  const char *pattern;   /* base pattern tiled to fill the chunk */
  int32_t     pattern_len;
  int32_t     size;      /* total chunk uncompressed size in bytes */
} regular_chunk_spec;

static const regular_chunk_spec regular_var_specs[] = {
    {"abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuv", 50, 1000},
    {"ABCDEFGHIJKLMNOPQRSTUVWXYZ-ABCDEFGHIJKLMNOPQRSTUV", 50,  500},
    {"0123456789-0123456789-0123456789-0123456789-01234", 50, 2000},
    {"the-quick-brown-fox-jumps-over-the-lazy-dog-finis", 50,  520},
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 50,  301},
};
#define REGULAR_VAR_NCHUNKS ((int32_t)(sizeof(regular_var_specs) / sizeof(regular_var_specs[0])))

static op_t parse_op(const char *op) {
  if (strcmp(op, "compress") == 0) {
    return OP_COMPRESS;
  }
  if (strcmp(op, "decompress") == 0) {
    return OP_DECOMPRESS;
  }

  printf("Unknown operation: %s\n", op);
  exit(EXIT_FAILURE);
}

static const vl_frame_data *get_frame_data(bool variable_chunks_) {
  return variable_chunks_ ? &variable_frame : &fixed_frame;
}

static int32_t total_nbytes(const vl_chunk_data *chunk) {
  int32_t total = 0;
  for (int i = 0; i < chunk->nblocks; ++i) {
    total += chunk->sizes[i];
  }
  return total;
}

/* Returns a heap-allocated buffer filled with `pattern` tiled to `total_size` bytes. */
static char *fill_pattern(const char *pattern, int32_t pattern_len, int32_t total_size) {
  char *buf = malloc((size_t)total_size);
  if (buf == NULL) return NULL;
  for (int32_t off = 0; off < total_size; off += pattern_len) {
    int32_t to_copy = (off + pattern_len <= total_size) ? pattern_len : total_size - off;
    memcpy(buf + off, pattern, (size_t)to_copy);
  }
  return buf;
}

/* Build a heap buffer of `unit_size * BLOCK_REPEATS` bytes by repeating `pattern`. */
static char *expand_block(const char *pattern, int32_t unit_size, int32_t *out_size) {
  *out_size = unit_size * BLOCK_REPEATS;
  char *buf = malloc((size_t)*out_size);
  if (buf == NULL) return NULL;
  for (int i = 0; i < BLOCK_REPEATS; i++) {
    memcpy(buf + i * unit_size, pattern, (size_t)unit_size);
  }
  return buf;
}

static int verify_split_buffers(char **ref_blocks, const int32_t *ref_sizes, int32_t ref_nblocks,
                                void **buffers, const int32_t *sizes, int32_t nblocks) {
  if (nblocks != ref_nblocks) {
    return -1;
  }

  for (int i = 0; i < nblocks; ++i) {
    if (sizes[i] != ref_sizes[i]) {
      return -2;
    }
    if (memcmp(buffers[i], ref_blocks[i], (size_t)ref_sizes[i]) != 0) {
      return -3;
    }
  }

  return 0;
}

static int verify_contiguous_buffer(char **ref_blocks, const int32_t *ref_sizes, int32_t ref_nblocks,
                                    const uint8_t *buffer, int32_t nbytes) {
  int32_t offset = 0;
  int32_t expected = 0;
  for (int i = 0; i < ref_nblocks; ++i) expected += ref_sizes[i];
  if (nbytes != expected) {
    return -1;
  }

  for (int i = 0; i < ref_nblocks; ++i) {
    if (memcmp(buffer + offset, ref_blocks[i], (size_t)ref_sizes[i]) != 0) {
      return -2;
    }
    offset += ref_sizes[i];
  }

  return 0;
}

static int compress_frame(const char *compname, const char *urlpath, bool variable_chunks_) {
  const vl_frame_data *frame_data = get_frame_data(variable_chunks_);
  int compcode = blosc2_compname_to_compcode(compname);
  if (compcode < 0) {
    printf("Unknown compressor: %s\n", compname);
    return compcode;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = (uint8_t)compcode;
  cparams.clevel = 9;
  cparams.typesize = 1;
  cparams.nthreads = 1;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;

  blosc2_storage storage = {
      .contiguous = true,
      .urlpath = (char *)urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_remove_urlpath(urlpath);
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    printf("Cannot create persistent VL-block schunk.\n");
    return EXIT_FAILURE;
  }

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    printf("Cannot create compression context.\n");
    blosc2_schunk_free(schunk);
    return EXIT_FAILURE;
  }

  for (int i = 0; i < frame_data->nchunks; ++i) {
    const vl_chunk_data *chunk_data = &frame_data->chunks[i];

    /* Expand each block by repeating its base pattern BLOCK_REPEATS times */
    char *expanded[MAX_BLOCKS] = {NULL};
    int32_t exp_sizes[MAX_BLOCKS] = {0};
    int32_t total_exp = 0;
    for (int j = 0; j < chunk_data->nblocks; ++j) {
      expanded[j] = expand_block(chunk_data->blocks[j], chunk_data->sizes[j], &exp_sizes[j]);
      if (expanded[j] == NULL) {
        printf("Cannot allocate expanded block buffer.\n");
        for (int k = 0; k < j; ++k) free(expanded[k]);
        blosc2_free_ctx(cctx);
        blosc2_schunk_free(schunk);
        return EXIT_FAILURE;
      }
      total_exp += exp_sizes[j];
    }

    int32_t destsize = total_exp + BLOSC2_MAX_OVERHEAD + chunk_data->nblocks * 16 + 128;
    uint8_t *chunk = malloc((size_t)destsize);
    if (chunk == NULL) {
      printf("Cannot allocate chunk buffer.\n");
      for (int j = 0; j < chunk_data->nblocks; ++j) free(expanded[j]);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    const void *srcs[MAX_BLOCKS];
    for (int j = 0; j < chunk_data->nblocks; ++j) {
      srcs[j] = expanded[j];
    }

    int32_t cbytes = blosc2_vlcompress_ctx(cctx, srcs, exp_sizes, chunk_data->nblocks, chunk, destsize);
    for (int j = 0; j < chunk_data->nblocks; ++j) free(expanded[j]);
    if (cbytes <= 0) {
      printf("VL-block compression error. Error code: %d\n", cbytes);
      free(chunk);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return cbytes;
    }

    if (blosc2_schunk_append_chunk(schunk, chunk, true) != i + 1) {
      printf("Cannot append VL-block chunk %d to frame.\n", i);
      free(chunk);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }
    free(chunk);
  }

  int32_t total_uncompressed = 0;
  int32_t total_blocks = 0;
  for (int i = 0; i < frame_data->nchunks; ++i) {
    total_uncompressed += total_nbytes(&frame_data->chunks[i]) * BLOCK_REPEATS;
    total_blocks += frame_data->chunks[i].nblocks;
  }
  printf("Wrote %s with %d VL-block chunks%s.\n", urlpath, frame_data->nchunks,
         frame_data->variable_chunks ? " and variable chunk sizes" : "");
  printf("  Uncompressed: %d bytes (%d blocks total, %d chunks)\n",
         total_uncompressed, total_blocks, frame_data->nchunks);
  printf("  Compressed:   %d bytes (ratio: %.2fx)\n",
         (int32_t)schunk->cbytes, (float)total_uncompressed / (float)schunk->cbytes);
  printf("  chunksize: %d (0 means variable)\n", schunk->chunksize);
  printf("  Blocks per chunk:");
  for (int i = 0; i < frame_data->nchunks; ++i) {
    printf(" %d", frame_data->chunks[i].nblocks);
  }
  printf("\n");
  blosc2_free_ctx(cctx);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}

static int decompress_frame(const char *urlpath) {
  const vl_frame_data *frame_data;
  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);
  if (schunk == NULL) {
    printf("Cannot open %s\n", urlpath);
    return EXIT_FAILURE;
  }

  frame_data = get_frame_data(schunk->chunksize == 0);

  if (schunk->nchunks != frame_data->nchunks) {
    printf("Unexpected number of chunks: %lld\n", (long long)schunk->nchunks);
    blosc2_schunk_free(schunk);
    return EXIT_FAILURE;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  if (dctx == NULL) {
    printf("Cannot create decompression context.\n");
    blosc2_schunk_free(schunk);
    return EXIT_FAILURE;
  }

  for (int i = 0; i < frame_data->nchunks; ++i) {
    const vl_chunk_data *chunk_data = &frame_data->chunks[i];

    /* Expand reference blocks for verification */
    char *expanded[MAX_BLOCKS] = {NULL};
    int32_t exp_sizes[MAX_BLOCKS] = {0};
    int32_t total_exp = 0;
    for (int j = 0; j < chunk_data->nblocks; ++j) {
      expanded[j] = expand_block(chunk_data->blocks[j], chunk_data->sizes[j], &exp_sizes[j]);
      if (expanded[j] == NULL) {
        printf("Cannot allocate expanded reference buffer.\n");
        for (int k = 0; k < j; ++k) free(expanded[k]);
        blosc2_free_ctx(dctx);
        blosc2_schunk_free(schunk);
        return EXIT_FAILURE;
      }
      total_exp += exp_sizes[j];
    }

    uint8_t *chunk = NULL;
    bool needs_free = false;
    int32_t cbytes = blosc2_schunk_get_chunk(schunk, i, &chunk, &needs_free);
    if (cbytes <= 0 || chunk == NULL) {
      printf("Cannot retrieve chunk %d from frame.\n", i);
      for (int j = 0; j < chunk_data->nblocks; ++j) free(expanded[j]);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t nbytes = total_exp;
    uint8_t *buffer = malloc((size_t)nbytes);
    if (buffer == NULL) {
      printf("Cannot allocate contiguous destination buffer.\n");
      for (int j = 0; j < chunk_data->nblocks; ++j) free(expanded[j]);
      if (needs_free) free(chunk);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t dsize = blosc2_decompress_ctx(dctx, chunk, cbytes, buffer, nbytes);
    if (dsize != nbytes ||
        verify_contiguous_buffer(expanded, exp_sizes, chunk_data->nblocks, buffer, dsize) != 0) {
      printf("Contiguous decompression mismatch in chunk %d.\n", i);
      free(buffer);
      for (int j = 0; j < chunk_data->nblocks; ++j) free(expanded[j]);
      if (needs_free) free(chunk);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }
    free(buffer);

    void *buffers[MAX_BLOCKS] = {NULL};
    int32_t sizes[MAX_BLOCKS] = {0};
    int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, buffers, sizes, chunk_data->nblocks);
    if (nblocks != chunk_data->nblocks ||
        verify_split_buffers(expanded, exp_sizes, chunk_data->nblocks, buffers, sizes, nblocks) != 0) {
      printf("VL-block decompression mismatch in chunk %d.\n", i);
      for (int j = 0; j < chunk_data->nblocks; ++j) {
        free(buffers[j]);
        free(expanded[j]);
      }
      if (needs_free) free(chunk);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    for (int j = 0; j < chunk_data->nblocks; ++j) {
      free(buffers[j]);
      free(expanded[j]);
    }
    if (needs_free) {
      free(chunk);
    }
  }

  printf("Successful VL-block frame roundtrip for %s\n", urlpath);
  printf("  chunksize: %d (0 means variable)\n", schunk->chunksize);
  printf("  Blocks per chunk:");
  for (int i = 0; i < frame_data->nchunks; ++i) {
    printf(" %d", frame_data->chunks[i].nblocks);
  }
  printf("\n");
  blosc2_free_ctx(dctx);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}

static int compress_regular_frame(const char *compname, const char *urlpath) {
  int compcode = blosc2_compname_to_compcode(compname);
  if (compcode < 0) {
    printf("Unknown compressor: %s\n", compname);
    return compcode;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = (uint8_t)compcode;
  cparams.clevel = 9;
  cparams.typesize = 1;
  cparams.nthreads = 1;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;

  blosc2_storage storage = {
      .contiguous = true,
      .urlpath = (char *)urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_remove_urlpath(urlpath);
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    printf("Cannot create persistent regular-chunk schunk.\n");
    return EXIT_FAILURE;
  }

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    printf("Cannot create compression context.\n");
    blosc2_schunk_free(schunk);
    return EXIT_FAILURE;
  }

  int32_t total_uncompressed = 0;
  for (int i = 0; i < REGULAR_VAR_NCHUNKS; ++i) {
    const regular_chunk_spec *spec = &regular_var_specs[i];
    char *src = fill_pattern(spec->pattern, spec->pattern_len, spec->size);
    if (src == NULL) {
      printf("Cannot allocate source buffer for chunk %d.\n", i);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t destsize = spec->size + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc((size_t)destsize);
    if (chunk == NULL) {
      printf("Cannot allocate chunk buffer.\n");
      free(src);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t cbytes = blosc2_compress_ctx(cctx, src, spec->size, chunk, destsize);
    free(src);
    if (cbytes <= 0) {
      printf("Compression error for chunk %d. Error code: %d\n", i, cbytes);
      free(chunk);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return cbytes;
    }

    if (blosc2_schunk_append_chunk(schunk, chunk, true) != i + 1) {
      printf("Cannot append chunk %d to frame.\n", i);
      free(chunk);
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }
    free(chunk);
    total_uncompressed += spec->size;
  }

  printf("Wrote %s with %d regular variable-size chunks.\n", urlpath, REGULAR_VAR_NCHUNKS);
  printf("  Uncompressed: %d bytes (%d chunks)\n", total_uncompressed, REGULAR_VAR_NCHUNKS);
  printf("  Compressed:   %d bytes (ratio: %.2fx)\n",
         (int32_t)schunk->cbytes, (float)total_uncompressed / (float)schunk->cbytes);
  printf("  chunksize: %d (0 means variable)\n", schunk->chunksize);
  printf("  Sizes per chunk:");
  for (int i = 0; i < REGULAR_VAR_NCHUNKS; ++i) {
    printf(" %d", regular_var_specs[i].size);
  }
  printf("\n");

  blosc2_free_ctx(cctx);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}

static int decompress_regular_frame(const char *urlpath) {
  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);
  if (schunk == NULL) {
    printf("Cannot open %s\n", urlpath);
    return EXIT_FAILURE;
  }

  if (schunk->nchunks != REGULAR_VAR_NCHUNKS) {
    printf("Unexpected number of chunks: %lld (expected %d)\n",
           (long long)schunk->nchunks, REGULAR_VAR_NCHUNKS);
    blosc2_schunk_free(schunk);
    return EXIT_FAILURE;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  if (dctx == NULL) {
    printf("Cannot create decompression context.\n");
    blosc2_schunk_free(schunk);
    return EXIT_FAILURE;
  }

  for (int i = 0; i < REGULAR_VAR_NCHUNKS; ++i) {
    const regular_chunk_spec *spec = &regular_var_specs[i];
    char *ref = fill_pattern(spec->pattern, spec->pattern_len, spec->size);
    if (ref == NULL) {
      printf("Cannot allocate reference buffer for chunk %d.\n", i);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    uint8_t *chunk = NULL;
    bool needs_free = false;
    int32_t cbytes = blosc2_schunk_get_chunk(schunk, i, &chunk, &needs_free);
    if (cbytes <= 0 || chunk == NULL) {
      printf("Cannot retrieve chunk %d from frame.\n", i);
      free(ref);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    char *buffer = malloc((size_t)spec->size);
    if (buffer == NULL) {
      printf("Cannot allocate decompression buffer for chunk %d.\n", i);
      free(ref);
      if (needs_free) free(chunk);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t dsize = blosc2_decompress_ctx(dctx, chunk, cbytes, buffer, spec->size);
    if (dsize != spec->size || memcmp(buffer, ref, (size_t)spec->size) != 0) {
      printf("Decompression mismatch in chunk %d.\n", i);
      free(buffer);
      free(ref);
      if (needs_free) free(chunk);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }
    free(buffer);
    free(ref);
    if (needs_free) free(chunk);
  }

  printf("Successful regular variable-chunk frame roundtrip for %s\n", urlpath);
  printf("  chunksize: %d (0 means variable)\n", schunk->chunksize);
  printf("  Sizes per chunk:");
  for (int i = 0; i < REGULAR_VAR_NCHUNKS; ++i) {
    printf(" %d", regular_var_specs[i].size);
  }
  printf("\n");

  blosc2_free_ctx(dctx);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}

/* Returns true if the first chunk in the frame uses VL-blocks encoding. */
static bool peek_is_vl_schunk(const char *urlpath) {
  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);
  if (schunk == NULL || schunk->nchunks == 0) {
    if (schunk) blosc2_schunk_free(schunk);
    return false;
  }
  uint8_t *chunk = NULL;
  bool needs_free = false;
  bool is_vl = false;
  if (blosc2_schunk_get_chunk(schunk, 0, &chunk, &needs_free) > 0 && chunk != NULL) {
    is_vl = (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS2] & BLOSC2_VL_BLOCKS) != 0;
    if (needs_free) free(chunk);
  }
  blosc2_schunk_free(schunk);
  return is_vl;
}

int main(int argc, char *argv[]) {
  int exit_code = EXIT_FAILURE;
  bool variable_chunks_ = false;
  bool regular_ = false;

  if (argc < 3 || argc > 5) {
    printf("Usage:\n");
    printf("%s compress <compressor> <output_file> [--variable-chunks|--regular]\n", argv[0]);
    printf("%s decompress <input_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  op_t operation = parse_op(argv[1]);
  if (operation == OP_COMPRESS && argc == 5) {
    if (strcmp(argv[4], "--variable-chunks") == 0) {
      variable_chunks_ = true;
    }
    else if (strcmp(argv[4], "--regular") == 0) {
      regular_ = true;
    }
    else {
      printf("Unknown flag: %s\n", argv[4]);
      return EXIT_FAILURE;
    }
  }
  if (operation == OP_DECOMPRESS && argc != 3) {
    printf("Usage:\n");
    printf("%s decompress <input_file>\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (operation == OP_COMPRESS && argc < 4) {
    printf("Usage:\n");
    printf("%s compress <compressor> <output_file> [--variable-chunks|--regular]\n", argv[0]);
    return EXIT_FAILURE;
  }

  printf("Blosc version info: %s\n", blosc2_get_version_string());
  blosc2_init();

  if (operation == OP_COMPRESS) {
    if (regular_) {
      exit_code = compress_regular_frame(argv[2], argv[3]);
    }
    else {
      exit_code = compress_frame(argv[2], argv[3], variable_chunks_);
    }
  }
  else {
    if (peek_is_vl_schunk(argv[2])) {
      exit_code = decompress_frame(argv[2]);
    }
    else {
      exit_code = decompress_regular_frame(argv[2]);
    }
  }

  blosc2_destroy();
  return exit_code;
}

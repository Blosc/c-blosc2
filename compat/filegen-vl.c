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

typedef enum {
  OP_COMPRESS,
  OP_DECOMPRESS,
} op_t;

typedef struct {
  const char *blocks[3];
  int32_t sizes[3];
  int32_t nblocks;
} vl_chunk_data;

typedef struct {
  const vl_chunk_data *chunks;
  int32_t nchunks;
  bool variable_chunks;
} vl_frame_data;

static const vl_chunk_data fixed_chunks[] = {
    {
        .blocks = {"red", "green-green", "blue-blue-blue-blue"},
        .sizes = {(int32_t)sizeof("red"), (int32_t)sizeof("green-green"), (int32_t)sizeof("blue-blue-blue-blue")},
        .nblocks = 3,
    },
    {
        .blocks = {"sun", "planet-1234", "galaxy-galaxy-spiral"},
        .sizes = {(int32_t)sizeof("sun"), (int32_t)sizeof("planet-1234"), (int32_t)sizeof("galaxy-galaxy-spiral")},
        .nblocks = 3,
    },
};

static const vl_chunk_data variable_chunks[] = {
    {
        .blocks = {"red", "green-green", "blue-blue-blue-blue"},
        .sizes = {(int32_t)sizeof("red"), (int32_t)sizeof("green-green"), (int32_t)sizeof("blue-blue-blue-blue")},
        .nblocks = 3,
    },
    {
        .blocks = {"tiny", "bravo-bravo-bravo", "charlie-charlie-charlie-charlie"},
        .sizes = {(int32_t)sizeof("tiny"), (int32_t)sizeof("bravo-bravo-bravo"),
                  (int32_t)sizeof("charlie-charlie-charlie-charlie")},
        .nblocks = 3,
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

static int verify_split_buffers(const vl_chunk_data *chunk, void **buffers, const int32_t *sizes, int32_t nblocks) {
  if (nblocks != chunk->nblocks) {
    return -1;
  }

  for (int i = 0; i < nblocks; ++i) {
    if (sizes[i] != chunk->sizes[i]) {
      return -2;
    }
    if (memcmp(buffers[i], chunk->blocks[i], (size_t)chunk->sizes[i]) != 0) {
      return -3;
    }
  }

  return 0;
}

static int verify_contiguous_buffer(const vl_chunk_data *chunk, const uint8_t *buffer, int32_t nbytes) {
  int32_t offset = 0;
  if (nbytes != total_nbytes(chunk)) {
    return -1;
  }

  for (int i = 0; i < chunk->nblocks; ++i) {
    if (memcmp(buffer + offset, chunk->blocks[i], (size_t)chunk->sizes[i]) != 0) {
      return -2;
    }
    offset += chunk->sizes[i];
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
    int32_t destsize = total_nbytes(chunk_data) + BLOSC2_MAX_OVERHEAD + 128;
    uint8_t *chunk = malloc((size_t)destsize);
    if (chunk == NULL) {
      printf("Cannot allocate chunk buffer.\n");
      blosc2_free_ctx(cctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    const void *srcs[] = {
        chunk_data->blocks[0],
        chunk_data->blocks[1],
        chunk_data->blocks[2],
    };

    int32_t cbytes = blosc2_vlcompress_ctx(cctx, srcs, chunk_data->sizes, chunk_data->nblocks, chunk, destsize);
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

  printf("Wrote %s with %d VL-block chunks%s.\n", urlpath, frame_data->nchunks,
         frame_data->variable_chunks ? " and variable chunk sizes" : "");
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
    uint8_t *chunk = NULL;
    bool needs_free = false;
    int32_t cbytes = blosc2_schunk_get_chunk(schunk, i, &chunk, &needs_free);
    if (cbytes <= 0 || chunk == NULL) {
      printf("Cannot retrieve chunk %d from frame.\n", i);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t nbytes = total_nbytes(chunk_data);
    uint8_t *buffer = malloc((size_t)nbytes);
    if (buffer == NULL) {
      printf("Cannot allocate contiguous destination buffer.\n");
      if (needs_free) {
        free(chunk);
      }
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    int32_t dsize = blosc2_decompress_ctx(dctx, chunk, cbytes, buffer, nbytes);
    if (dsize != nbytes || verify_contiguous_buffer(chunk_data, buffer, dsize) != 0) {
      printf("Contiguous decompression mismatch in chunk %d.\n", i);
      free(buffer);
      if (needs_free) {
        free(chunk);
      }
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }
    free(buffer);

    void *buffers[3] = {NULL};
    int32_t sizes[3] = {0};
    int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, buffers, sizes, chunk_data->nblocks);
    if (nblocks != chunk_data->nblocks || verify_split_buffers(chunk_data, buffers, sizes, nblocks) != 0) {
      printf("VL-block decompression mismatch in chunk %d.\n", i);
      for (int j = 0; j < chunk_data->nblocks; ++j) {
        free(buffers[j]);
      }
      if (needs_free) {
        free(chunk);
      }
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return EXIT_FAILURE;
    }

    for (int j = 0; j < chunk_data->nblocks; ++j) {
      free(buffers[j]);
    }
    if (needs_free) {
      free(chunk);
    }
  }

  printf("Successful VL-block frame roundtrip!\n");
  blosc2_free_ctx(dctx);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  int exit_code = EXIT_FAILURE;
  bool variable_chunks_ = false;

  if (argc < 3 || argc > 5) {
    printf("Usage:\n");
    printf("%s compress <compressor> <output_file> [--variable-chunks]\n", argv[0]);
    printf("%s decompress <input_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  op_t operation = parse_op(argv[1]);
  if (operation == OP_COMPRESS && argc == 5) {
    if (strcmp(argv[4], "--variable-chunks") != 0) {
      printf("Unknown flag: %s\n", argv[4]);
      return EXIT_FAILURE;
    }
    variable_chunks_ = true;
  }
  if (operation == OP_DECOMPRESS && argc != 3) {
    printf("Usage:\n");
    printf("%s decompress <input_file>\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (operation == OP_COMPRESS && argc < 4) {
    printf("Usage:\n");
    printf("%s compress <compressor> <output_file> [--variable-chunks]\n", argv[0]);
    return EXIT_FAILURE;
  }

  printf("Blosc version info: %s\n", blosc2_get_version_string());
  blosc2_init();

  if (operation == OP_COMPRESS) {
    exit_code = compress_frame(argv[2], argv[3], variable_chunks_);
  }
  else {
    exit_code = decompress_frame(argv[2]);
  }

  blosc2_destroy();
  return exit_code;
}

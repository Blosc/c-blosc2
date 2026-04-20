/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "frame.h"
#include "blosc2.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#endif


static char* sframe_make_index_path(const char* urlpath) {
  size_t path_len = strlen(urlpath);
  size_t suffix_len = strlen("/chunks.b2frame");
  if (path_len > SIZE_MAX - suffix_len - 1) {
    BLOSC_TRACE_ERROR("Index path length overflows size limits");
    return NULL;
  }

  char* index_path = malloc(path_len + suffix_len + 1);
  if (index_path == NULL) {
    return NULL;
  }

  int written = snprintf(index_path, path_len + suffix_len + 1, "%s/chunks.b2frame", urlpath);
  if (written < 0 || (size_t)written >= path_len + suffix_len + 1) {
    BLOSC_TRACE_ERROR("Error building index path");
    free(index_path);
    return NULL;
  }

  return index_path;
}


static char* sframe_make_chunk_path(const char* urlpath, int64_t nchunk) {
  if (nchunk < 0 || (uint64_t)nchunk > UINT32_MAX) {
    BLOSC_TRACE_ERROR("Chunk index (%" PRId64 ") is out of range for sframe filenames", nchunk);
    return NULL;
  }

  size_t path_len = strlen(urlpath);
  size_t suffix_len = strlen("/.chunk");
  size_t chunk_hex_len = 8;
  if (path_len > SIZE_MAX - suffix_len - chunk_hex_len - 1) {
    BLOSC_TRACE_ERROR("Chunk path length overflows size limits");
    return NULL;
  }

  size_t total_len = path_len + suffix_len + chunk_hex_len + 1;
  char* chunk_path = malloc(total_len);
  if (chunk_path == NULL) {
    return NULL;
  }

  int written = snprintf(chunk_path, total_len, "%s/%08" PRIX32 ".chunk", urlpath, (uint32_t)nchunk);
  if (written < 0 || (size_t)written >= total_len) {
    BLOSC_TRACE_ERROR("Error building chunk path for chunk index (%" PRId64 ")", nchunk);
    free(chunk_path);
    return NULL;
  }

  return chunk_path;
}


/* Open sparse frame index chunk */
void* sframe_open_index(const char* urlpath, const char* mode, const blosc2_io *io) {
  void* fp = NULL;
  char* index_path = sframe_make_index_path(urlpath);
  if (index_path) {
    blosc2_io_cb *io_cb = blosc2_get_io_cb(io->id);
    if (io_cb == NULL) {
      BLOSC_TRACE_ERROR("Error getting the input/output API");
      free(index_path);
      return NULL;
    }
    fp = io_cb->open(index_path, mode, io->params);
    if (fp == NULL)
      BLOSC_TRACE_ERROR("Error creating index path in: %s", index_path);
    free(index_path);
  }
  return fp;
}

/* Open directory/nchunk.chunk with 8 zeros of padding */
void* sframe_open_chunk(const char* urlpath, int64_t nchunk, const char* mode, const blosc2_io *io) {
  void* fp = NULL;
  char* chunk_path = sframe_make_chunk_path(urlpath, nchunk);
  if (chunk_path) {
    blosc2_io_cb *io_cb = blosc2_get_io_cb(io->id);
    if (io_cb == NULL) {
      BLOSC_TRACE_ERROR("Error getting the input/output API");
      free(chunk_path);
      return NULL;
    }
    fp = io_cb->open(chunk_path, mode, io->params);
    if (fp == NULL)
      BLOSC_TRACE_ERROR("Error opening chunk path in: %s", chunk_path);
    free(chunk_path);
  }
  return fp;
}

/* Append an existing chunk into a sparse frame. */
void* sframe_create_chunk(blosc2_frame_s* frame, uint8_t* chunk, int64_t nchunk, int64_t cbytes) {
  void* fpc = sframe_open_chunk(frame->urlpath, nchunk, "wb", frame->schunk->storage->io);
  if (fpc == NULL) {
    BLOSC_TRACE_ERROR("Cannot open the chunkfile.");
    return NULL;
  }
  blosc2_io_cb *io_cb = blosc2_get_io_cb(frame->schunk->storage->io->id);
  if (io_cb == NULL) {
    BLOSC_TRACE_ERROR("Error getting the input/output API");
    return NULL;
  }
  int64_t io_pos = 0;
  int64_t wbytes = io_cb->write(chunk, 1, cbytes, io_pos, fpc);
  io_cb->close(fpc);
  if (wbytes != cbytes) {
    BLOSC_TRACE_ERROR("Cannot write the full chunk.");
    return NULL;
  }

  return frame;
}

/* Append an existing chunk into a sparse frame. */
int sframe_delete_chunk(const char *urlpath, int64_t nchunk) {
  char* chunk_path = sframe_make_chunk_path(urlpath, nchunk);
  if (chunk_path) {
    int rc = remove(chunk_path);
    free(chunk_path);
    return rc;
  }
  return BLOSC2_ERROR_FILE_REMOVE;
}

/* Get chunk from sparse frame. */
int32_t sframe_get_chunk(blosc2_frame_s* frame, int64_t nchunk, uint8_t** chunk, bool* needs_free){
  void *fpc = sframe_open_chunk(frame->urlpath, nchunk, "rb", frame->schunk->storage->io);
  if(fpc == NULL){
    BLOSC_TRACE_ERROR("Cannot open the chunkfile.");
    return BLOSC2_ERROR_FILE_OPEN;
  }

  blosc2_io_cb *io_cb = blosc2_get_io_cb(frame->schunk->storage->io->id);
  if (io_cb == NULL) {
    BLOSC_TRACE_ERROR("Error getting the input/output API");
    return BLOSC2_ERROR_PLUGIN_IO;
  }

  int64_t chunk_cbytes = io_cb->size(fpc);

  if (io_cb->is_allocation_necessary) {
    *chunk = malloc((size_t)chunk_cbytes);
    *needs_free = true;
  }
  else {
    *needs_free = false;
  }

  int64_t io_pos = 0;
  int64_t rbytes = io_cb->read((void**)chunk, 1, chunk_cbytes, io_pos, fpc);
  io_cb->close(fpc);
  if (rbytes != chunk_cbytes) {
    BLOSC_TRACE_ERROR("Cannot read the chunk out of the chunkfile.");
    return BLOSC2_ERROR_FILE_READ;
  }

  return (int32_t)chunk_cbytes;
}

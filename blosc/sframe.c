/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
#include "frame.h"


/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#endif


/* Open sparse frame index chunk */
void* sframe_open_index(const char* urlpath, const char* mode, const blosc2_io *io) {
  void* fp = NULL;
  char* index_path = malloc(strlen(urlpath) + strlen("/chunks.b2frame") + 1);
  if (index_path) {
    sprintf(index_path, "%s/chunks.b2frame", urlpath);
    blosc2_io_cb *io_cb = blosc2_get_io_cb(io->id);
    if (io_cb == NULL) {
      BLOSC_TRACE_ERROR("Error getting the input/output API");
      return NULL;
    }
    fp = io_cb->open(index_path, mode, io->params);
    free(index_path);
  }
  return fp;
}

/* Open directory/nchunk.chunk with 8 zeros of padding */
void* sframe_open_chunk(const char* urlpath, int64_t nchunk, const char* mode, const blosc2_io *io) {
  void* fp = NULL;
  char* chunk_path = malloc(strlen(urlpath) + 1 + 8 + strlen(".chunk") + 1);
  if (chunk_path) {
    sprintf(chunk_path, "%s/%08X.chunk", urlpath, (unsigned int)nchunk);
    blosc2_io_cb *io_cb = blosc2_get_io_cb(io->id);
    if (io_cb == NULL) {
      BLOSC_TRACE_ERROR("Error getting the input/output API");
      return NULL;
    }
    fp = io_cb->open(chunk_path, mode, io->params);
    free(chunk_path);
  }
  return fp;
}

/* Append an existing chunk into a sparse frame. */
void* sframe_create_chunk(blosc2_frame_s* frame, uint8_t* chunk, int32_t nchunk, int64_t cbytes) {
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
  int64_t wbytes = io_cb->write(chunk, 1, cbytes, fpc);
  io_cb->close(fpc);
  if (wbytes != (size_t)cbytes) {
    BLOSC_TRACE_ERROR("Cannot write the full chunk.");
    return NULL;
  }

  return frame;
}

/* Append an existing chunk into a sparse frame. */
int sframe_delete_chunk(const char *urlpath, int32_t nchunk) {
  char* chunk_path = malloc(strlen(urlpath) + 1 + 8 + strlen(".chunk") + 1);
  if (chunk_path) {
    sprintf(chunk_path, "%s/%08X.chunk", urlpath, (unsigned int)nchunk);
    int rc = remove(chunk_path);
    free(chunk_path);
    return rc;
  }
  return BLOSC2_ERROR_FILE_REMOVE;
}

/* Get chunk from sparse frame. */
int sframe_get_chunk(blosc2_frame_s* frame, int32_t nchunk, uint8_t** chunk, bool* needs_free){
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

  io_cb->seek(fpc, 0L, SEEK_END);
  int64_t chunk_cbytes = io_cb->tell(fpc);
  *chunk = malloc((size_t)chunk_cbytes);

  io_cb->seek(fpc, 0L, SEEK_SET);
  int64_t rbytes = io_cb->read(*chunk, 1, (size_t)chunk_cbytes, fpc);
  io_cb->close(fpc);
  if (rbytes != (size_t)chunk_cbytes) {
    BLOSC_TRACE_ERROR("Cannot read the chunk out of the chunkfile.");
    return BLOSC2_ERROR_FILE_READ;
  }
  *needs_free = true;

  return chunk_cbytes;
}

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "frame.h"
#include "stune.h"
#include "blosc-private.h"
#include "blosc2/tuners-registry.h"
#include "blosc2.h"

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#include <malloc.h>
#define mkdir(D, M) _mkdir(D)
#endif  /* _WIN32 */

#include <sys/stat.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
#endif


/* Get the cparams associated with a super-chunk */
int blosc2_schunk_get_cparams(blosc2_schunk *schunk, blosc2_cparams **cparams) {
  *cparams = calloc(1, sizeof(blosc2_cparams));
  (*cparams)->schunk = schunk;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    (*cparams)->filters[i] = schunk->filters[i];
    (*cparams)->filters_meta[i] = schunk->filters_meta[i];
  }
  (*cparams)->compcode = schunk->compcode;
  (*cparams)->compcode_meta = schunk->compcode_meta;
  (*cparams)->clevel = schunk->clevel;
  (*cparams)->typesize = schunk->typesize;
  (*cparams)->blocksize = schunk->blocksize;
  (*cparams)->splitmode = schunk->splitmode;
  if (schunk->cctx == NULL) {
    (*cparams)->nthreads = blosc2_get_nthreads();
  }
  else {
    (*cparams)->nthreads = (int16_t)schunk->cctx->nthreads;
  }
  return 0;
}


/* Get the dparams associated with a super-chunk */
int blosc2_schunk_get_dparams(blosc2_schunk *schunk, blosc2_dparams **dparams) {
  *dparams = calloc(1, sizeof(blosc2_dparams));
  (*dparams)->schunk = schunk;
  if (schunk->dctx == NULL) {
    (*dparams)->nthreads = blosc2_get_nthreads();
  }
  else {
    (*dparams)->nthreads = schunk->dctx->nthreads;
  }
  return 0;
}


int update_schunk_properties(struct blosc2_schunk* schunk) {
  blosc2_cparams* cparams = schunk->storage->cparams;
  blosc2_dparams* dparams = schunk->storage->dparams;

  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    schunk->filters[i] = cparams->filters[i];
    schunk->filters_meta[i] = cparams->filters_meta[i];
  }
  schunk->compcode = cparams->compcode;
  schunk->compcode_meta = cparams->compcode_meta;
  schunk->clevel = cparams->clevel;
  schunk->splitmode = cparams->splitmode;
  schunk->typesize = cparams->typesize;
  schunk->blocksize = cparams->blocksize;
  schunk->chunksize = -1;
  schunk->tuner_params = cparams->tuner_params;
  schunk->tuner_id = cparams->tuner_id;
  if (cparams->tuner_id == BLOSC_BTUNE) {
    cparams->use_dict = 0;
  }
  /* The compression context */
  if (schunk->cctx != NULL) {
    blosc2_free_ctx(schunk->cctx);
  }
  cparams->schunk = schunk;
  schunk->cctx = blosc2_create_cctx(*cparams);
  if (schunk->cctx == NULL) {
    BLOSC_TRACE_ERROR("Could not create compression ctx");
    return BLOSC2_ERROR_NULL_POINTER;
  }

  /* The decompression context */
  if (schunk->dctx != NULL) {
    blosc2_free_ctx(schunk->dctx);
  }
  dparams->schunk = schunk;
  schunk->dctx = blosc2_create_dctx(*dparams);
  if (schunk->dctx == NULL) {
    BLOSC_TRACE_ERROR("Could not create decompression ctx");
    return BLOSC2_ERROR_NULL_POINTER;
  }

  return BLOSC2_ERROR_SUCCESS;
}


static bool file_exists (char *filename) {
  struct stat   buffer;
  return (stat (filename, &buffer) == 0);
}


/* Create a new super-chunk */
blosc2_schunk* blosc2_schunk_new(blosc2_storage *storage) {
  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));
  schunk->version = 0;     /* pre-first version */

  // Get the storage with proper defaults
  schunk->storage = get_new_storage(storage, &BLOSC2_CPARAMS_DEFAULTS, &BLOSC2_DPARAMS_DEFAULTS, &BLOSC2_IO_DEFAULTS);
  // Update the (local variable) storage
  storage = schunk->storage;

  char* tradeoff = getenv("BTUNE_TRADEOFF");
  if (tradeoff != NULL) {
    // If BTUNE_TRADEOFF passed, automatically use btune
    storage->cparams->tuner_id = BLOSC_BTUNE;
  }

  // ...and update internal properties
  if (update_schunk_properties(schunk) < 0) {
    BLOSC_TRACE_ERROR("Error when updating schunk properties");
    return NULL;
  }

  if (!storage->contiguous && storage->urlpath != NULL){
    char* urlpath;
    char last_char = storage->urlpath[strlen(storage->urlpath) - 1];
    urlpath = malloc(strlen(storage->urlpath) + 1);
    strcpy(urlpath, storage->urlpath);
    if (last_char == '\\' || last_char == '/') {
      urlpath[strlen(storage->urlpath) - 1] = '\0';
    }
    // Create directory
    if (mkdir(urlpath, 0777) == -1) {
      BLOSC_TRACE_ERROR("Error during the creation of the directory, maybe it already exists.");
      return NULL;
    }
    // We want a sparse (directory) frame as storage
    blosc2_frame_s* frame = frame_new(urlpath);
    free(urlpath);
    frame->sframe = true;
    // Initialize frame (basically, encode the header)
    frame->schunk = schunk;
    int64_t frame_len = frame_from_schunk(schunk, frame);
    if (frame_len < 0) {
      BLOSC_TRACE_ERROR("Error during the conversion of schunk to frame.");
      return NULL;
    }
    schunk->frame = (blosc2_frame*)frame;
  }
  if (storage->contiguous){
    // We want a contiguous frame as storage
    if (storage->urlpath != NULL) {
      if (file_exists(storage->urlpath)) {
        BLOSC_TRACE_ERROR("You are trying to overwrite an existing frame.  Remove it first!");
        return NULL;
      }
    }
    blosc2_frame_s* frame = frame_new(storage->urlpath);
    frame->sframe = false;
    // Initialize frame (basically, encode the header)
    frame->schunk = schunk;
    int64_t frame_len = frame_from_schunk(schunk, frame);
    if (frame_len < 0) {
      BLOSC_TRACE_ERROR("Error during the conversion of schunk to frame.");
      return NULL;
    }
    schunk->frame = (blosc2_frame*)frame;
  }

  return schunk;
}


/* Create a copy of a super-chunk */
blosc2_schunk* blosc2_schunk_copy(blosc2_schunk *schunk, blosc2_storage *storage) {
  if (schunk == NULL) {
    BLOSC_TRACE_ERROR("Can not copy a NULL `schunk`.");
    return NULL;
  }

  // Check if cparams are equals
  bool cparams_equal = true;
  blosc2_cparams cparams = {0};
  if (storage->cparams == NULL) {
    // When cparams are not specified, just use the same of schunk
    cparams.typesize = schunk->cctx->typesize;
    cparams.clevel = schunk->cctx->clevel;
    cparams.compcode = schunk->cctx->compcode;
    cparams.compcode_meta = schunk->cctx->compcode_meta;
    cparams.splitmode = schunk->cctx->splitmode;
    cparams.use_dict = schunk->cctx->use_dict;
    cparams.blocksize = schunk->cctx->blocksize;
    memcpy(cparams.filters, schunk->cctx->filters, BLOSC2_MAX_FILTERS);
    memcpy(cparams.filters_meta, schunk->cctx->filters_meta, BLOSC2_MAX_FILTERS);
    storage->cparams = &cparams;
  }
  else {
    cparams = *storage->cparams;
  }
  if (cparams.blocksize == 0) {
    // TODO: blocksize should be read from schunk->blocksize
    // For this, it should be updated during the first append
    // (or change API to make this a property during schunk creation).
    cparams.blocksize = schunk->cctx->blocksize;
  }

  if (cparams.typesize != schunk->cctx->typesize ||
      cparams.clevel != schunk->cctx->clevel ||
      cparams.compcode != schunk->cctx->compcode ||
      cparams.use_dict != schunk->cctx->use_dict ||
      cparams.blocksize != schunk->cctx->blocksize ||
      // In case of prefilters or postfilters, force their execution.
      schunk->cctx->prefilter != NULL ||
      schunk->dctx->postfilter != NULL) {
    cparams_equal = false;
  }
  for (int i = 0; i < BLOSC2_MAX_FILTERS; ++i) {
    if (cparams.filters[i] != schunk->cctx->filters[i] ||
        cparams.filters_meta[i] != schunk->cctx->filters_meta[i]) {
      cparams_equal = false;
    }
  }

  // Create new schunk
  blosc2_schunk *new_schunk = blosc2_schunk_new(storage);
  if (new_schunk == NULL) {
    BLOSC_TRACE_ERROR("Can not create a new schunk");
    return NULL;
  }

  // Copy metalayers
  for (int nmeta = 0; nmeta < schunk->nmetalayers; ++nmeta) {
    blosc2_metalayer *meta = schunk->metalayers[nmeta];
    if (blosc2_meta_add(new_schunk, meta->name, meta->content, meta->content_len) < 0) {
      BLOSC_TRACE_ERROR("Can not add %s `metalayer`.", meta->name);
      return NULL;
    }
  }

  // Copy chunks
  if (cparams_equal) {
    for (int nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
      uint8_t *chunk;
      bool needs_free;
      if (blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free) < 0) {
        BLOSC_TRACE_ERROR("Can not get the `chunk` %d.", nchunk);
        return NULL;
      }
      if (blosc2_schunk_append_chunk(new_schunk, chunk, !needs_free) < 0) {
        BLOSC_TRACE_ERROR("Can not append the `chunk` into super-chunk.");
        return NULL;
      }
    }
  } else {
    int32_t chunksize = schunk->chunksize == -1 ? 0 : schunk->chunksize;
    uint8_t *buffer = malloc(chunksize);
    for (int nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
      if (blosc2_schunk_decompress_chunk(schunk, nchunk, buffer, schunk->chunksize) < 0) {
        BLOSC_TRACE_ERROR("Can not decompress the `chunk` %d.", nchunk);
        return NULL;
      }
      if (blosc2_schunk_append_buffer(new_schunk, buffer, schunk->chunksize) < 0) {
        BLOSC_TRACE_ERROR("Can not append the `buffer` into super-chunk.");
        return NULL;
      }
    }
    free(buffer);
  }

  // Copy vlmetalayers
  for (int nmeta = 0; nmeta < schunk->nvlmetalayers; ++nmeta) {
    uint8_t *content;
    int32_t content_len;
    char* name = schunk->vlmetalayers[nmeta]->name;
    if (blosc2_vlmeta_get(schunk, name, &content, &content_len) < 0) {
      BLOSC_TRACE_ERROR("Can not get %s `vlmetalayer`.", name);
    }
    if (blosc2_vlmeta_add(new_schunk, name, content, content_len, NULL) < 0) {
      BLOSC_TRACE_ERROR("Can not add %s `vlmetalayer`.", name);
      return NULL;
    }
    free(content);
  }
  return new_schunk;
}


/* Open an existing super-chunk that is on-disk (no copy is made). */
blosc2_schunk* blosc2_schunk_open_udio(const char* urlpath, const blosc2_io *udio) {
  return blosc2_schunk_open_offset_udio(urlpath, 0, udio);
}

blosc2_schunk* blosc2_schunk_open_offset_udio(const char* urlpath, int64_t offset, const blosc2_io *udio) {
  if (urlpath == NULL) {
    BLOSC_TRACE_ERROR("You need to supply a urlpath.");
    return NULL;
  }

  blosc2_frame_s* frame = frame_from_file_offset(urlpath, udio, offset);
  if (frame == NULL) {
    blosc2_io_cb *io_cb = blosc2_get_io_cb(udio->id);
    if (io_cb == NULL) {
        BLOSC_TRACE_ERROR("Error getting the input/output API");
        return NULL;
    }
    int rc = io_cb->destroy(udio->params);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Cannot destroy the input/output object.");
    }
    return NULL;
  }
  blosc2_schunk* schunk = frame_to_schunk(frame, false, udio);

  // Set the storage with proper defaults
  size_t pathlen = strlen(urlpath);
  schunk->storage->urlpath = malloc(pathlen + 1);
  strcpy(schunk->storage->urlpath, urlpath);
  schunk->storage->contiguous = !frame->sframe;

  return schunk;
}

blosc2_schunk* blosc2_schunk_open(const char* urlpath) {
  return blosc2_schunk_open_udio(urlpath, &BLOSC2_IO_DEFAULTS);
}

blosc2_schunk* blosc2_schunk_open_offset(const char* urlpath, int64_t offset) {
  return blosc2_schunk_open_offset_udio(urlpath, offset, &BLOSC2_IO_DEFAULTS);
}

int64_t blosc2_schunk_to_buffer(blosc2_schunk* schunk, uint8_t** dest, bool* needs_free) {
  blosc2_frame_s* frame;
  int64_t cframe_len;

  // Initialize defaults in case of errors
  *dest = NULL;
  *needs_free = false;

  if ((schunk->storage->contiguous == true) && (schunk->storage->urlpath == NULL)) {
    frame =  (blosc2_frame_s*)(schunk->frame);
    *dest = frame->cframe;
    cframe_len = frame->len;
    *needs_free = false;
  }
  else {
    // Copy to a contiguous storage
    blosc2_storage frame_storage = {.contiguous=true};
    blosc2_schunk* schunk_copy = blosc2_schunk_copy(schunk, &frame_storage);
    if (schunk_copy == NULL) {
      BLOSC_TRACE_ERROR("Error during the conversion of schunk to buffer.");
      return BLOSC2_ERROR_SCHUNK_COPY;
    }
    frame = (blosc2_frame_s*)(schunk_copy->frame);
    *dest = frame->cframe;
    cframe_len = frame->len;
    *needs_free = true;
    frame->avoid_cframe_free = true;
    blosc2_schunk_free(schunk_copy);
  }

  return cframe_len;

}


/* Write an in-memory frame out to a file. */
int64_t frame_to_file(blosc2_frame_s* frame, const char* urlpath) {
  blosc2_io_cb *io_cb = blosc2_get_io_cb(frame->schunk->storage->io->id);
  if (io_cb == NULL) {
    BLOSC_TRACE_ERROR("Error getting the input/output API");
    return BLOSC2_ERROR_PLUGIN_IO;
  }
  void* fp = io_cb->open(urlpath, "wb", frame->schunk->storage->io);
  int64_t io_pos = 0;
  int64_t nitems = io_cb->write(frame->cframe, frame->len, 1, io_pos, fp);
  io_cb->close(fp);
  return nitems * frame->len;
}


/* Append an in-memory frame to a file. */
int64_t append_frame_to_file(blosc2_frame_s* frame, const char* urlpath) {
    blosc2_io_cb *io_cb = blosc2_get_io_cb(frame->schunk->storage->io->id);
    if (io_cb == NULL) {
        BLOSC_TRACE_ERROR("Error getting the input/output API");
        return BLOSC2_ERROR_PLUGIN_IO;
    }
    void* fp = io_cb->open(urlpath, "ab", frame->schunk->storage->io);

    int64_t io_pos = io_cb->size(fp);
    io_cb->write(frame->cframe, frame->len, 1, io_pos, fp);
    io_cb->close(fp);
    return io_pos;
}


/* Write super-chunk out to a file. */
int64_t blosc2_schunk_to_file(blosc2_schunk* schunk, const char* urlpath) {
  if (urlpath == NULL) {
    BLOSC_TRACE_ERROR("urlpath cannot be NULL");
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  // Accelerated path for in-memory frames
  if (schunk->storage->contiguous && schunk->storage->urlpath == NULL) {
    int64_t len = frame_to_file((blosc2_frame_s*)(schunk->frame), urlpath);
    if (len <= 0) {
      BLOSC_TRACE_ERROR("Error writing to file");
      return len;
    }
    return len;
  }

  // Copy to a contiguous file
  blosc2_storage frame_storage = {.contiguous=true, .urlpath=(char*)urlpath};
  blosc2_schunk* schunk_copy = blosc2_schunk_copy(schunk, &frame_storage);
  if (schunk_copy == NULL) {
    BLOSC_TRACE_ERROR("Error during the conversion of schunk to buffer.");
    return BLOSC2_ERROR_SCHUNK_COPY;
  }
  blosc2_frame_s* frame = (blosc2_frame_s*)(schunk_copy->frame);
  int64_t frame_len = frame->len;
  blosc2_schunk_free(schunk_copy);
  return frame_len;
}


/* Append a super-chunk to a file. */
int64_t blosc2_schunk_append_file(blosc2_schunk* schunk, const char* urlpath) {
    if (urlpath == NULL) {
        BLOSC_TRACE_ERROR("urlpath cannot be NULL");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    // Accelerated path for in-memory frames
    if (schunk->storage->contiguous && schunk->storage->urlpath == NULL) {
        int64_t offset = append_frame_to_file((blosc2_frame_s*)(schunk->frame), urlpath);
        if (offset <= 0) {
            BLOSC_TRACE_ERROR("Error writing to file");
            return offset;
        }
        return offset;
    }

    // Copy to a contiguous file
    blosc2_storage frame_storage = {.contiguous=true, .urlpath=NULL};
    blosc2_schunk* schunk_copy = blosc2_schunk_copy(schunk, &frame_storage);
    if (schunk_copy == NULL) {
        BLOSC_TRACE_ERROR("Error during the conversion of schunk to buffer.");
        return BLOSC2_ERROR_SCHUNK_COPY;
    }
    blosc2_frame_s* frame = (blosc2_frame_s*)(schunk_copy->frame);
    int64_t offset = append_frame_to_file(frame, urlpath);
    blosc2_schunk_free(schunk_copy);
    return offset;
}


/* Free all memory from a super-chunk. */
int blosc2_schunk_free(blosc2_schunk *schunk) {
  if (schunk->data != NULL) {
    for (int i = 0; i < schunk->nchunks; i++) {
      free(schunk->data[i]);
    }
    free(schunk->data);
  }
  if (schunk->cctx != NULL)
    blosc2_free_ctx(schunk->cctx);
  if (schunk->dctx != NULL)
    blosc2_free_ctx(schunk->dctx);
  if (schunk->blockshape != NULL)
    free(schunk->blockshape);

  if (schunk->nmetalayers > 0) {
    for (int i = 0; i < schunk->nmetalayers; i++) {
      if (schunk->metalayers[i] != NULL) {
        if (schunk->metalayers[i]->name != NULL)
          free(schunk->metalayers[i]->name);
        if (schunk->metalayers[i]->content != NULL)
          free(schunk->metalayers[i]->content);
        free(schunk->metalayers[i]);
      }
    }
    schunk->nmetalayers = 0;
  }

  if (schunk->storage != NULL) {
    blosc2_io_cb *io_cb = blosc2_get_io_cb(schunk->storage->io->id);
    if (io_cb != NULL) {
      int rc = io_cb->destroy(schunk->storage->io->params);
      if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not free the I/O resources.");
      }
    }

    if (schunk->storage->urlpath != NULL) {
      free(schunk->storage->urlpath);
    }
    free(schunk->storage->cparams);
    free(schunk->storage->dparams);
    free(schunk->storage->io);
    free(schunk->storage);
  }

  if (schunk->frame != NULL) {
    frame_free((blosc2_frame_s *) schunk->frame);
  }

  if (schunk->nvlmetalayers > 0) {
    for (int i = 0; i < schunk->nvlmetalayers; ++i) {
      if (schunk->vlmetalayers[i] != NULL) {
        if (schunk->vlmetalayers[i]->name != NULL)
          free(schunk->vlmetalayers[i]->name);
        if (schunk->vlmetalayers[i]->content != NULL)
          free(schunk->vlmetalayers[i]->content);
        free(schunk->vlmetalayers[i]);
      }
    }
  }

  free(schunk);

  return 0;
}


/* Create a super-chunk out of a contiguous frame buffer */
blosc2_schunk* blosc2_schunk_from_buffer(uint8_t *cframe, int64_t len, bool copy) {
  blosc2_frame_s* frame = frame_from_cframe(cframe, len, false);
  if (frame == NULL) {
    return NULL;
  }
  // Check that the buffer actually comes from a cframe
  char *magic_number = (char *)cframe;
  magic_number += FRAME_HEADER_MAGIC;
  if (strcmp(magic_number, "b2frame\0") != 0) {
    frame_free(frame);
    return NULL;
  }
  blosc2_schunk* schunk = frame_to_schunk(frame, copy, &BLOSC2_IO_DEFAULTS);
  if (schunk && copy) {
    // Super-chunk has its own copy of frame
    frame_free(frame);
  }
  return schunk;
}


/* Create a super-chunk out of a contiguous frame buffer */
void blosc2_schunk_avoid_cframe_free(blosc2_schunk *schunk, bool avoid_cframe_free) {
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  frame_avoid_cframe_free(frame, avoid_cframe_free);
}


/* Fill an empty frame with special values (fast path). */
int64_t blosc2_schunk_fill_special(blosc2_schunk* schunk, int64_t nitems, int special_value,
                               int32_t chunksize) {
  if (nitems == 0) {
    return 0;
  }

  int32_t typesize = schunk->typesize;

  if ((nitems * typesize / chunksize) > INT_MAX) {
    BLOSC_TRACE_ERROR("nitems is too large.  Try increasing the chunksize.");
    return BLOSC2_ERROR_SCHUNK_SPECIAL;
  }

  if ((schunk->nbytes > 0) || (schunk->cbytes > 0)) {
    BLOSC_TRACE_ERROR("Filling with special values only works on empty super-chunks");
    return BLOSC2_ERROR_FRAME_SPECIAL;
  }

  // Compute the number of chunks and the length of the offsets chunk
  int32_t chunkitems = chunksize / typesize;
  int64_t nchunks = nitems / chunkitems;
  int32_t leftover_items = (int32_t)(nitems % chunkitems);

  if (schunk->frame == NULL) {
    // Build the special chunks
    int32_t leftover_size = leftover_items * typesize;
    void* chunk = malloc(BLOSC_EXTENDED_HEADER_LENGTH);
    void* chunk2 = malloc(BLOSC_EXTENDED_HEADER_LENGTH);
    blosc2_cparams* cparams;
    blosc2_schunk_get_cparams(schunk, &cparams);
    int csize, csize2;
    switch (special_value) {
      case BLOSC2_SPECIAL_ZERO:
        csize = blosc2_chunk_zeros(*cparams, chunksize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        csize2 = blosc2_chunk_zeros(*cparams, leftover_size, chunk2, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case BLOSC2_SPECIAL_UNINIT:
        csize = blosc2_chunk_uninit(*cparams, chunksize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        csize2 = blosc2_chunk_uninit(*cparams, leftover_size, chunk2, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case BLOSC2_SPECIAL_NAN:
        csize = blosc2_chunk_nans(*cparams, chunksize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        csize2 = blosc2_chunk_nans(*cparams, leftover_size, chunk2, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      default:
        BLOSC_TRACE_ERROR("Only zeros, NaNs or non-initialized values are supported.");
        return BLOSC2_ERROR_SCHUNK_SPECIAL;
    }
    free(cparams);
    if (csize < 0 || csize2 < 0) {
      BLOSC_TRACE_ERROR("Error creating special chunks.");
      return BLOSC2_ERROR_SCHUNK_SPECIAL;
    }

    for (int nchunk = 0; nchunk < nchunks; nchunk++) {
      int64_t nchunk_ = blosc2_schunk_append_chunk(schunk, chunk, true);
      if (nchunk_ != nchunk + 1) {
        BLOSC_TRACE_ERROR("Error appending special chunks.");
        return BLOSC2_ERROR_SCHUNK_SPECIAL;
      }
    }

    if (leftover_items) {
      int64_t nchunk_ = blosc2_schunk_append_chunk(schunk, chunk2, true);
      if (nchunk_ != nchunks + 1) {
        BLOSC_TRACE_ERROR("Error appending last special chunk.");
        return BLOSC2_ERROR_SCHUNK_SPECIAL;
      }
    }
    free(chunk);
    free(chunk2);
  }
  else {
    /* Fill an empty frame with special values (fast path). */
    blosc2_frame_s *frame = (blosc2_frame_s *) schunk->frame;
    /* Update counters (necessary for the frame_fill_special() logic) */
    if (leftover_items) {
      nchunks += 1;
    }
    schunk->chunksize = chunksize;
    schunk->nchunks = nchunks;
    schunk->nbytes = nitems * typesize;
    int64_t frame_len = frame_fill_special(frame, nitems, special_value, chunksize, schunk);
    if (frame_len < 0) {
      BLOSC_TRACE_ERROR("Error creating special frame.");
      return frame_len;
    }
  }

  return schunk->nchunks;
}

/* Append an existing chunk into a super-chunk. */
int64_t blosc2_schunk_append_chunk(blosc2_schunk *schunk, uint8_t *chunk, bool copy) {
  int32_t chunk_nbytes;
  int32_t chunk_cbytes;
  int64_t nchunks = schunk->nchunks;

  int rc = blosc2_cbuffer_sizes(chunk, &chunk_nbytes, &chunk_cbytes, NULL);
  if (rc < 0) {
    return rc;
  }

  if (schunk->chunksize == -1) {
    schunk->chunksize = chunk_nbytes;  // The super-chunk is initialized now
  }
  if (chunk_nbytes > schunk->chunksize) {
    BLOSC_TRACE_ERROR("Appending chunks that have different lengths in the same schunk "
                      "is not supported yet: %d > %d.", chunk_nbytes, schunk->chunksize);
    return BLOSC2_ERROR_CHUNK_APPEND;
  }

  /* Update counters */
  schunk->current_nchunk = nchunks;
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += chunk_nbytes;
  if (schunk->frame == NULL) {
    schunk->cbytes += chunk_cbytes;
  } else {
    // A frame
    int special_value = (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] >> 4) & BLOSC2_SPECIAL_MASK;
    switch (special_value) {
      case BLOSC2_SPECIAL_ZERO:
      case BLOSC2_SPECIAL_NAN:
      case BLOSC2_SPECIAL_UNINIT:
        schunk->cbytes += 0;
        break;
      default:
        schunk->cbytes += chunk_cbytes;
    }
  }

  if (copy) {
    // Make a copy of the chunk
    uint8_t *chunk_copy = malloc(chunk_cbytes);
    memcpy(chunk_copy, chunk, chunk_cbytes);
    chunk = chunk_copy;
  }

  // Update super-chunk or frame
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame == NULL) {
    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 1) && (chunk_nbytes < schunk->chunksize)) {
      uint8_t* last_chunk = schunk->data[nchunks - 1];
      int32_t last_nbytes;
      rc = blosc2_cbuffer_sizes(last_chunk, &last_nbytes, NULL, NULL);
      if (rc < 0) {
        return rc;
      }
      if ((last_nbytes < schunk->chunksize) && (chunk_nbytes < schunk->chunksize)) {
        BLOSC_TRACE_ERROR(
                "Appending two consecutive chunks with a chunksize smaller than the schunk chunksize "
                "is not allowed yet: %d != %d.", chunk_nbytes, schunk->chunksize);
        return BLOSC2_ERROR_CHUNK_APPEND;
      }
    }

    if (!copy && (chunk_cbytes < chunk_nbytes)) {
      // We still want to do a shrink of the chunk
      chunk = realloc(chunk, chunk_cbytes);
    }

    /* Make space for appending the copy of the chunk and do it */
    if ((nchunks + 1) * sizeof(void *) > schunk->data_len) {
      // Extend the data pointer by one memory page (4k)
      schunk->data_len += 4096;  // must be a multiple of sizeof(void*)
      schunk->data = realloc(schunk->data, schunk->data_len);
    }
    schunk->data[nchunks] = chunk;
  }
  else {
    if (frame_append_chunk(frame, chunk, schunk) == NULL) {
      BLOSC_TRACE_ERROR("Problems appending a chunk.");
      return BLOSC2_ERROR_CHUNK_APPEND;
    }
  }
  return schunk->nchunks;
}


/* Insert an existing @p chunk in a specified position on a super-chunk */
int64_t blosc2_schunk_insert_chunk(blosc2_schunk *schunk, int64_t nchunk, uint8_t *chunk, bool copy) {
  int32_t chunk_nbytes;
  int32_t chunk_cbytes;
  int64_t nchunks = schunk->nchunks;

  int rc = blosc2_cbuffer_sizes(chunk, &chunk_nbytes, &chunk_cbytes, NULL);
  if (rc < 0) {
    return rc;
  }

  if (schunk->chunksize == -1) {
    schunk->chunksize = chunk_nbytes;  // The super-chunk is initialized now
  }

  if (chunk_nbytes > schunk->chunksize) {
    BLOSC_TRACE_ERROR("Inserting chunks that have different lengths in the same schunk "
                      "is not supported yet: %d > %d.", chunk_nbytes, schunk->chunksize);
    return BLOSC2_ERROR_CHUNK_INSERT;
  }

  /* Update counters */
  schunk->current_nchunk = nchunk;
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += chunk_nbytes;
  if (schunk->frame == NULL) {
    schunk->cbytes += chunk_cbytes;
  } else {
    // A frame
    int special_value = (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] >> 4) & BLOSC2_SPECIAL_MASK;
    switch (special_value) {
      case BLOSC2_SPECIAL_ZERO:
      case BLOSC2_SPECIAL_NAN:
      case BLOSC2_SPECIAL_UNINIT:
        schunk->cbytes += 0;
        break;
      default:
        schunk->cbytes += chunk_cbytes;
    }
  }

  if (copy) {
    // Make a copy of the chunk
    uint8_t *chunk_copy = malloc(chunk_cbytes);
    memcpy(chunk_copy, chunk, chunk_cbytes);
    chunk = chunk_copy;
  }

  // Update super-chunk or frame
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame == NULL) {
    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (chunk_nbytes < schunk->chunksize)) {
      uint8_t* last_chunk = schunk->data[nchunks - 1];
      int32_t last_nbytes;
      rc = blosc2_cbuffer_sizes(last_chunk, &last_nbytes, NULL, NULL);
      if (rc < 0) {
        return rc;
      }
      if ((last_nbytes < schunk->chunksize) && (chunk_nbytes < schunk->chunksize)) {
        BLOSC_TRACE_ERROR("Appending two consecutive chunks with a chunksize smaller "
                          "than the schunk chunksize is not allowed yet:  %d != %d",
                          chunk_nbytes, schunk->chunksize);
        return BLOSC2_ERROR_CHUNK_APPEND;
      }
    }

    if (!copy && (chunk_cbytes < chunk_nbytes)) {
      // We still want to do a shrink of the chunk
      chunk = realloc(chunk, chunk_cbytes);
    }

    // Make space for appending the copy of the chunk and do it
    if ((nchunks + 1) * sizeof(void *) > schunk->data_len) {
      // Extend the data pointer by one memory page (4k)
      schunk->data_len += 4096;  // must be a multiple of sizeof(void*)
      schunk->data = realloc(schunk->data, schunk->data_len);
    }

    // Reorder the offsets and insert the new chunk
    for (int64_t i = nchunks; i > nchunk; --i) {
      schunk->data[i] = schunk->data[i-1];
    }
    schunk->data[nchunk] = chunk;
  }

  else {
    if (frame_insert_chunk(frame, nchunk, chunk, schunk) == NULL) {
      BLOSC_TRACE_ERROR("Problems inserting a chunk in a frame.");
      return BLOSC2_ERROR_CHUNK_INSERT;
    }
  }
  return schunk->nchunks;
}


int64_t blosc2_schunk_update_chunk(blosc2_schunk *schunk, int64_t nchunk, uint8_t *chunk, bool copy) {
  int32_t chunk_nbytes;
  int32_t chunk_cbytes;

  int rc = blosc2_cbuffer_sizes(chunk, &chunk_nbytes, &chunk_cbytes, NULL);
  if (rc < 0) {
    return rc;
  }

  if (schunk->chunksize == -1) {
    schunk->chunksize = chunk_nbytes;  // The super-chunk is initialized now
  }

  if (schunk->chunksize != 0 && (chunk_nbytes > schunk->chunksize ||
      (chunk_nbytes < schunk->chunksize && nchunk != schunk->nchunks - 1))) {
    BLOSC_TRACE_ERROR("Updating chunks having different lengths in the same schunk "
                      "is not supported yet (unless it's the last one and smaller):"
                      " %d > %d.", chunk_nbytes, schunk->chunksize);
    return BLOSC2_ERROR_CHUNK_UPDATE;
  }

  bool needs_free;
  uint8_t *chunk_old;
  int err = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_old, &needs_free);
  if (err < 0) {
    BLOSC_TRACE_ERROR("%" PRId64 " chunk can not be obtained from schunk.", nchunk);
    return -1;
  }
  int32_t chunk_nbytes_old = 0;
  int32_t chunk_cbytes_old = 0;
  schunk->current_nchunk = nchunk;

  if (chunk_old != 0) {
    rc = blosc2_cbuffer_sizes(chunk_old, &chunk_nbytes_old, &chunk_cbytes_old, NULL);
    if (rc < 0) {
      return rc;
    }
    if (chunk_cbytes_old == BLOSC2_MAX_OVERHEAD) {
        chunk_cbytes_old = 0;
    }
  }
  if (needs_free) {
    free(chunk_old);
  }

  if (copy) {
    // Make a copy of the chunk
    uint8_t *chunk_copy = malloc(chunk_cbytes);
    memcpy(chunk_copy, chunk, chunk_cbytes);
    chunk = chunk_copy;
  }

  blosc2_frame_s* frame = (blosc2_frame_s*)(schunk->frame);
  if (schunk->frame == NULL) {
    /* Update counters */
    schunk->nbytes += chunk_nbytes;
    schunk->nbytes -= chunk_nbytes_old;
    schunk->cbytes += chunk_cbytes;
    schunk->cbytes -= chunk_cbytes_old;
  } else {
    // A frame
    int special_value = (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] >> 4) & BLOSC2_SPECIAL_MASK;
    switch (special_value) {
      case BLOSC2_SPECIAL_ZERO:
      case BLOSC2_SPECIAL_NAN:
      case BLOSC2_SPECIAL_UNINIT:
        schunk->nbytes += chunk_nbytes;
        schunk->nbytes -= chunk_nbytes_old;
        if (frame->sframe) {
          schunk->cbytes -= chunk_cbytes_old;
        }
        break;
      default:
        /* Update counters */
        schunk->nbytes += chunk_nbytes;
        schunk->nbytes -= chunk_nbytes_old;
        schunk->cbytes += chunk_cbytes;
        if (frame->sframe) {
          schunk->cbytes -= chunk_cbytes_old;
        }
        else {
          if (chunk_cbytes_old >= chunk_cbytes) {
            schunk->cbytes -= chunk_cbytes;
          }
        }
    }
  }

  // Update super-chunk or frame
  if (schunk->frame == NULL) {
    if (!copy && (chunk_cbytes < chunk_nbytes)) {
      // We still want to do a shrink of the chunk
      chunk = realloc(chunk, chunk_cbytes);
    }

    // Free old chunk and add reference to new chunk
    if (schunk->data[nchunk] != 0) {
      free(schunk->data[nchunk]);
    }
    schunk->data[nchunk] = chunk;
  }
  else {
    if (frame_update_chunk(frame, nchunk, chunk, schunk) == NULL) {
        BLOSC_TRACE_ERROR("Problems updating a chunk in a frame.");
        return BLOSC2_ERROR_CHUNK_UPDATE;
    }
  }

  return schunk->nchunks;
}

int64_t blosc2_schunk_delete_chunk(blosc2_schunk *schunk, int64_t nchunk) {
  int rc;
  if (schunk->nchunks < nchunk) {
    BLOSC_TRACE_ERROR("The schunk has not enough chunks (%" PRId64 ")!", schunk->nchunks);
  }

  bool needs_free;
  uint8_t *chunk_old;
  int err = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_old, &needs_free);
  if (err < 0) {
    BLOSC_TRACE_ERROR("%" PRId64 "chunk can not be obtained from schunk.", nchunk);
    return -1;
  }
  int32_t chunk_nbytes_old = 0;
  int32_t chunk_cbytes_old = 0;
  schunk->current_nchunk = nchunk;

  if (chunk_old != 0) {
    rc = blosc2_cbuffer_sizes(chunk_old, &chunk_nbytes_old, &chunk_cbytes_old, NULL);
    if (rc < 0) {
      return rc;
    }
    if (chunk_cbytes_old == BLOSC2_MAX_OVERHEAD) {
      chunk_cbytes_old = 0;
    }
  }
  if (needs_free) {
    free(chunk_old);
  }

  blosc2_frame_s* frame = (blosc2_frame_s*)(schunk->frame);
  schunk->nchunks -= 1;
  if (schunk->frame == NULL) {
    /* Update counters */
    schunk->nbytes -= chunk_nbytes_old;
    schunk->cbytes -= chunk_cbytes_old;
  } else {
    // A frame
    schunk->nbytes -= chunk_nbytes_old;
    if (frame->sframe) {
      schunk->cbytes -= chunk_cbytes_old;
    }
  }

  // Update super-chunk or frame
  if (schunk->frame == NULL) {
    // Free old chunk
    if (schunk->data[nchunk] != 0) {
      free(schunk->data[nchunk]);
    }
    // Reorder the offsets and insert the new chunk
    for (int64_t i = nchunk; i < schunk->nchunks; i++) {
      schunk->data[i] = schunk->data[i + 1];
    }
    schunk->data[schunk->nchunks] = NULL;

  }
  else {
    if (frame_delete_chunk(frame, nchunk, schunk) == NULL) {
      BLOSC_TRACE_ERROR("Problems deleting a chunk in a frame.");
      return BLOSC2_ERROR_CHUNK_UPDATE;
    }
  }
  return schunk->nchunks;
}


/* Append a data buffer to a super-chunk. */
int64_t blosc2_schunk_append_buffer(blosc2_schunk *schunk, const void *src, int32_t nbytes) {
  uint8_t* chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  schunk->current_nchunk = schunk->nchunks;
  /* Compress the src buffer using super-chunk context */
  int cbytes = blosc2_compress_ctx(schunk->cctx, src, nbytes, chunk,
                                   nbytes + BLOSC2_MAX_OVERHEAD);
  if (cbytes < 0) {
    free(chunk);
    return cbytes;
  }
  // We don't need a copy of the chunk, as it will be shrunk if necessary
  int64_t nchunks = blosc2_schunk_append_chunk(schunk, chunk, false);
  if (nchunks < 0) {
    BLOSC_TRACE_ERROR("Error appending a buffer in super-chunk");
    return nchunks;
  }

  return nchunks;
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_schunk_decompress_chunk(blosc2_schunk *schunk, int64_t nchunk,
                                   void *dest, int32_t nbytes) {
  int32_t chunk_nbytes;
  int32_t chunk_cbytes;
  int chunksize;
  int rc;
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;

  schunk->current_nchunk = nchunk;
  if (frame == NULL) {
    if (nchunk >= schunk->nchunks) {
      BLOSC_TRACE_ERROR("nchunk ('%" PRId64 "') exceeds the number of chunks "
                        "('%" PRId64 "') in super-chunk.", nchunk, schunk->nchunks);
      return BLOSC2_ERROR_INVALID_PARAM;
    }
    uint8_t* src = schunk->data[nchunk];
    if (src == 0) {
      return 0;
    }

    rc = blosc2_cbuffer_sizes(src, &chunk_nbytes, &chunk_cbytes, NULL);
    if (rc < 0) {
      return rc;
    }

    if (nbytes < chunk_nbytes) {
      BLOSC_TRACE_ERROR("Buffer size is too small for the decompressed buffer "
                        "('%d' bytes, but '%d' are needed).", nbytes, chunk_nbytes);
      return BLOSC2_ERROR_INVALID_PARAM;
    }

    chunksize = blosc2_decompress_ctx(schunk->dctx, src, chunk_cbytes, dest, nbytes);
    if (chunksize < 0 || chunksize != chunk_nbytes) {
      BLOSC_TRACE_ERROR("Error in decompressing chunk.");
      if (chunksize < 0)
        return chunksize;
      return BLOSC2_ERROR_FAILURE;
    }
  } else {
    chunksize = frame_decompress_chunk(schunk->dctx, frame, nchunk, dest, nbytes);
    if (chunksize < 0) {
      return chunksize;
    }
  }
  return chunksize;
}


/* Return a compressed chunk that is part of a super-chunk in the `chunk` parameter.
 * If the super-chunk is backed by a frame that is disk-based, a buffer is allocated for the
 * (compressed) chunk, and hence a free is needed.  You can check if the chunk requires a free
 * with the `needs_free` parameter.
 * If the chunk does not need a free, it means that a pointer to the location in the super-chunk
 * (or the backing in-memory frame) is returned in the `chunk` parameter.
 *
 * The size of the (compressed) chunk is returned.  If some problem is detected, a negative code
 * is returned instead.
*/
int blosc2_schunk_get_chunk(blosc2_schunk *schunk, int64_t nchunk, uint8_t **chunk, bool *needs_free) {
  if (schunk->dctx->threads_started > 1) {
    pthread_mutex_lock(&schunk->dctx->nchunk_mutex);
    schunk->current_nchunk = nchunk;
    pthread_mutex_unlock(&schunk->dctx->nchunk_mutex);
  }
  else {
    schunk->current_nchunk = nchunk;
  }
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame != NULL) {
    return frame_get_chunk(frame, nchunk, chunk, needs_free);
  }

  if (nchunk >= schunk->nchunks) {
    BLOSC_TRACE_ERROR("nchunk ('%" PRId64 "') exceeds the number of chunks "
                      "('%" PRId64 "') in schunk.", nchunk, schunk->nchunks);
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  *chunk = schunk->data[nchunk];
  if (*chunk == 0) {
    *needs_free = 0;
    return 0;
  }

  *needs_free = false;
  int32_t chunk_cbytes;
  int rc = blosc2_cbuffer_sizes(*chunk, NULL, &chunk_cbytes, NULL);
  if (rc < 0) {
    return rc;
  }
  return (int)chunk_cbytes;
}


/* Return a compressed chunk that is part of a super-chunk in the `chunk` parameter.
 * If the super-chunk is backed by a frame that is disk-based, a buffer is allocated for the
 * (compressed) chunk, and hence a free is needed.  You can check if the chunk requires a free
 * with the `needs_free` parameter.
 * If the chunk does not need a free, it means that a pointer to the location in the super-chunk
 * (or the backing in-memory frame) is returned in the `chunk` parameter.
 *
 * The size of the (compressed) chunk is returned.  If some problem is detected, a negative code
 * is returned instead.
*/
int blosc2_schunk_get_lazychunk(blosc2_schunk *schunk, int64_t nchunk, uint8_t **chunk, bool *needs_free) {
  if (schunk->dctx->threads_started > 1) {
    pthread_mutex_lock(&schunk->dctx->nchunk_mutex);
    schunk->current_nchunk = nchunk;
    pthread_mutex_unlock(&schunk->dctx->nchunk_mutex);
  }
  else {
    schunk->current_nchunk = nchunk;
  }
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (schunk->frame != NULL) {
    return frame_get_lazychunk(frame, nchunk, chunk, needs_free);
  }

  if (nchunk >= schunk->nchunks) {
    BLOSC_TRACE_ERROR("nchunk ('%" PRId64 "') exceeds the number of chunks "
                      "('%" PRId64 "') in schunk.", nchunk, schunk->nchunks);
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  *chunk = schunk->data[nchunk];
  if (*chunk == 0) {
    *needs_free = 0;
    return 0;
  }

  *needs_free = false;
  int32_t chunk_cbytes;
  int rc = blosc2_cbuffer_sizes(*chunk, NULL, &chunk_cbytes, NULL);
  if (rc < 0) {
    return rc;
  }
  return (int)chunk_cbytes;
}


int blosc2_schunk_get_slice_buffer(blosc2_schunk *schunk, int64_t start, int64_t stop, void *buffer) {
  int64_t byte_start = start * schunk->typesize;
  int64_t byte_stop = stop * schunk->typesize;
  int64_t nchunk_start = byte_start / schunk->chunksize;
  int32_t chunk_start = (int32_t) (byte_start % schunk->chunksize); // 0 indexed
  int32_t chunk_stop; // 0 indexed
  if (byte_stop >= (nchunk_start + 1) * schunk->chunksize) {
    chunk_stop = schunk->chunksize;
  }
  else {
    chunk_stop = (int32_t) (byte_stop % schunk->chunksize);
  }

  uint8_t *dst_ptr = (uint8_t *) buffer;
  bool needs_free;
  uint8_t *chunk;
  int32_t cbytes;
  int64_t nchunk = nchunk_start;
  int64_t nbytes_read = 0;
  int32_t nbytes;
  int32_t chunksize = schunk->chunksize;

  while (nbytes_read < ((stop - start) * schunk->typesize)) {
    cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &chunk, &needs_free);
    if (cbytes < 0) {
      BLOSC_TRACE_ERROR("Cannot get lazychunk ('%" PRId64 "').", nchunk);
      return BLOSC2_ERROR_FAILURE;
    }
    int32_t blocksize = sw32_(chunk + BLOSC2_CHUNK_BLOCKSIZE);

    int32_t nblock_start = (int32_t) (chunk_start / blocksize);
    int32_t nblock_stop = (int32_t) ((chunk_stop - 1) / blocksize);
    if (nchunk == (schunk->nchunks - 1) && schunk->nbytes % schunk->chunksize != 0) {
      chunksize = schunk->nbytes % schunk->chunksize;
    }
    int32_t nblocks = chunksize / blocksize;
    if (chunksize % blocksize != 0) {
      nblocks++;
    }

    if (chunk_start == 0 && chunk_stop == chunksize) {
      // Avoid memcpy
      nbytes = blosc2_decompress_ctx(schunk->dctx, chunk, cbytes, dst_ptr, chunksize);
      if (nbytes < 0) {
        BLOSC_TRACE_ERROR("Cannot decompress chunk ('%" PRId64 "').", nchunk);
        return BLOSC2_ERROR_FAILURE;
      }
    }
    else {
      // After extensive timing I have not been able to see lots of situations where
      // a maskout read is better than a getitem one.  Disabling for now.
      // if (nblock_start != nblock_stop) {
      if (false) {
        uint8_t *data = malloc(chunksize);
        /* We have more than 1 block to read, so use a masked read */
        bool *block_maskout = calloc(nblocks, 1);
        for (int32_t nblock = 0; nblock < nblocks; nblock++) {
          if ((nblock < nblock_start) || (nblock > nblock_stop)) {
            block_maskout[nblock] = true;
          }
        }
        if (blosc2_set_maskout(schunk->dctx, block_maskout, nblocks) < 0) {
          BLOSC_TRACE_ERROR("Cannot set maskout");
          return BLOSC2_ERROR_FAILURE;
        }

        nbytes = blosc2_decompress_ctx(schunk->dctx, chunk, cbytes, data, chunksize);
        if (nbytes < 0) {
          BLOSC_TRACE_ERROR("Cannot decompress chunk ('%" PRId64 "').", nchunk);
          return BLOSC2_ERROR_FAILURE;
        }
        nbytes = chunk_stop - chunk_start;
        memcpy(dst_ptr, &data[chunk_start], nbytes);
        free(block_maskout);
        free(data);
      }
      else {
        /* Less than 1 block to read; use a getitem call */
        nbytes = blosc2_getitem_ctx(schunk->dctx, chunk, cbytes, (int32_t) (chunk_start / schunk->typesize),
                                    (chunk_stop - chunk_start) / schunk->typesize, dst_ptr, chunksize);
        if (nbytes < 0) {
          BLOSC_TRACE_ERROR("Cannot get item from ('%" PRId64 "') chunk.", nchunk);
          return BLOSC2_ERROR_FAILURE;
        }
      }
    }

    dst_ptr += nbytes;
    nbytes_read += nbytes;
    nchunk++;

    if (needs_free) {
      free(chunk);
    }
    chunk_start = 0;
    if (byte_stop >= (nchunk + 1) * chunksize) {
      chunk_stop = chunksize;
    }
    else {
      chunk_stop = (int32_t)(byte_stop % chunksize);
    }
  }

  return BLOSC2_ERROR_SUCCESS;
}


int blosc2_schunk_set_slice_buffer(blosc2_schunk *schunk, int64_t start, int64_t stop, void *buffer) {
  int64_t byte_start = start * schunk->typesize;
  int64_t byte_stop = stop * schunk->typesize;
  int64_t nchunk_start = byte_start / schunk->chunksize;
  int32_t chunk_start = (int32_t) (byte_start % schunk->chunksize);
  int32_t chunk_stop;
  if (byte_stop >= (nchunk_start + 1) * schunk->chunksize) {
    chunk_stop = schunk->chunksize;
  }
  else {
    chunk_stop = (int32_t) (byte_stop % schunk->chunksize);
  }

  uint8_t *src_ptr = (uint8_t *) buffer;
  int64_t nchunk = nchunk_start;
  int64_t nbytes_written = 0;
  int32_t nbytes;
  uint8_t *data = malloc(schunk->chunksize);
  int64_t nchunks;
  int32_t chunksize = schunk->chunksize;

  while (nbytes_written < ((stop - start) * schunk->typesize)) {
    if (chunk_start == 0 &&
        (chunk_stop == schunk->chunksize || chunk_stop == schunk->nbytes % schunk->chunksize)) {
      if (chunk_stop == schunk->nbytes % schunk->chunksize) {
        chunksize = chunk_stop;
      }
      uint8_t *chunk = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
      if (blosc2_compress_ctx(schunk->cctx, src_ptr, chunksize, chunk, chunksize + BLOSC2_MAX_OVERHEAD) < 0) {
        BLOSC_TRACE_ERROR("Cannot compress data of chunk ('%" PRId64 "').", nchunk);
        return BLOSC2_ERROR_FAILURE;
      }
      nchunks = blosc2_schunk_update_chunk(schunk, nchunk, chunk, false);
      if (nchunks != schunk->nchunks) {
        BLOSC_TRACE_ERROR("Cannot update chunk ('%" PRId64 "').", nchunk);
        return BLOSC2_ERROR_CHUNK_UPDATE;
      }
    }
    else {
      nbytes = blosc2_schunk_decompress_chunk(schunk, nchunk, data, schunk->chunksize);
      if (nbytes < 0) {
        BLOSC_TRACE_ERROR("Cannot decompress chunk ('%" PRId64 "').", nchunk);
        return BLOSC2_ERROR_FAILURE;
      }
      memcpy(&data[chunk_start], src_ptr, chunk_stop - chunk_start);
      uint8_t *chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
      if (blosc2_compress_ctx(schunk->cctx, data, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD) < 0) {
        BLOSC_TRACE_ERROR("Cannot compress data of chunk ('%" PRId64 "').", nchunk);
        return BLOSC2_ERROR_FAILURE;
      }
      nchunks = blosc2_schunk_update_chunk(schunk, nchunk, chunk, false);
      if (nchunks != schunk->nchunks) {
        BLOSC_TRACE_ERROR("Cannot update chunk ('%" PRId64 "').", nchunk);
        return BLOSC2_ERROR_CHUNK_UPDATE;
      }
    }
    nchunk++;
    nbytes_written += chunk_stop - chunk_start;
    src_ptr += chunk_stop - chunk_start;
    chunk_start = 0;
    if (byte_stop >= (nchunk + 1) * schunk->chunksize) {
      chunk_stop = schunk->chunksize;
    }
    else {
      chunk_stop = (int32_t) (byte_stop % schunk->chunksize);
    }
  }
  free(data);

  return BLOSC2_ERROR_SUCCESS;
}


int schunk_get_slice_nchunks(blosc2_schunk *schunk, int64_t start, int64_t stop, int64_t **chunks_idx) {
  BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_NULL_POINTER);

  int64_t byte_start = start * schunk->typesize;
  int64_t byte_stop = stop * schunk->typesize;
  int64_t nchunk_start = byte_start / schunk->chunksize;
  int64_t nchunk_stop = byte_stop / schunk->chunksize;
  if (byte_stop % schunk->chunksize != 0) {
    nchunk_stop++;
  }
  int64_t nchunk = nchunk_start;
  int nchunks = (int)(nchunk_stop - nchunk_start);
  *chunks_idx = malloc(nchunks * sizeof(int64_t));
  int64_t *ptr = *chunks_idx;
  for (int64_t i = 0; i < nchunks; ++i) {
    ptr[i] = nchunk;
    nchunk++;
  }
  return nchunks;
}

/* Reorder the chunk offsets of an existing super-chunk. */
int blosc2_schunk_reorder_offsets(blosc2_schunk *schunk, int64_t *offsets_order) {
  // Check that the offsets order are correct
  bool *index_check = (bool *) calloc(schunk->nchunks, sizeof(bool));
  for (int i = 0; i < schunk->nchunks; ++i) {
    int64_t index = offsets_order[i];
    if (index >= schunk->nchunks) {
      BLOSC_TRACE_ERROR("Index is bigger than the number of chunks.");
      free(index_check);
      return BLOSC2_ERROR_DATA;
    }
    if (index_check[index] == false) {
      index_check[index] = true;
    } else {
      BLOSC_TRACE_ERROR("Index is yet used.");
      free(index_check);
      return BLOSC2_ERROR_DATA;
    }
  }
  free(index_check);

  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame != NULL) {
    return frame_reorder_offsets(frame, offsets_order, schunk);
  }
  uint8_t **offsets = schunk->data;

  // Make a copy of the chunk offsets and reorder it
  uint8_t **offsets_copy = malloc(schunk->data_len);
  memcpy(offsets_copy, offsets, schunk->data_len);

  for (int i = 0; i < schunk->nchunks; ++i) {
    offsets[i] = offsets_copy[offsets_order[i]];
  }
  free(offsets_copy);

  return 0;
}


// Get the length (in bytes) of the internal frame of the super-chunk
int64_t blosc2_schunk_frame_len(blosc2_schunk* schunk) {
  int64_t len;
  blosc2_frame_s* frame_s = (blosc2_frame_s*)(schunk->frame);
  if (frame_s != NULL) {
    len = frame_s->len;
  }
  else {
    // No attached frame, but we can still come with an estimate
    len = (int64_t) (schunk->cbytes + schunk->nchunks * sizeof(int64_t));
  }

  return len;
}


/**
 * @brief Flush metalayers content into a possible attached frame.
 *
 * @param schunk The super-chunk to which the flush should be applied.
 *
 * @return If successful, a 0 is returned. Else, return a negative value.
 */
// Initially, this was a public function, but as it is really meant to be used only
// in the schunk_add_metalayer(), I decided to convert it into private and call it
// implicitly instead of requiring the user to do so.  The only drawback is that
// each add operation requires a complete frame re-build, but as users should need
// very few metalayers, this overhead should be negligible in practice.
int metalayer_flush(blosc2_schunk* schunk) {
  int rc = BLOSC2_ERROR_SUCCESS;
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame == NULL) {
    return rc;
  }
  rc = frame_update_header(frame, schunk, true);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Unable to update metalayers into frame.");
    return rc;
  }
  rc = frame_update_trailer(frame, schunk);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Unable to update trailer into frame.");
    return rc;
  }
  return rc;
}


/* Add content into a new metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_meta_add(blosc2_schunk *schunk, const char *name, uint8_t *content, int32_t content_len) {
  int nmetalayer = blosc2_meta_exists(schunk, name);
  if (nmetalayer >= 0) {
    BLOSC_TRACE_ERROR("Metalayer \"%s\" already exists.", name);
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  // Add the metalayer
  blosc2_metalayer *metalayer = malloc(sizeof(blosc2_metalayer));
  char* name_ = malloc(strlen(name) + 1);
  strcpy(name_, name);
  metalayer->name = name_;
  uint8_t* content_buf = malloc((size_t)content_len);
  memcpy(content_buf, content, content_len);
  metalayer->content = content_buf;
  metalayer->content_len = content_len;
  schunk->metalayers[schunk->nmetalayers] = metalayer;
  schunk->nmetalayers += 1;

  int rc = metalayer_flush(schunk);
  if (rc < 0) {
    return rc;
  }

  return schunk->nmetalayers - 1;
}


/* Update the content of an existing metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_meta_update(blosc2_schunk *schunk, const char *name, uint8_t *content, int32_t content_len) {
  int nmetalayer = blosc2_meta_exists(schunk, name);
  if (nmetalayer < 0) {
    BLOSC_TRACE_ERROR("Metalayer \"%s\" not found.", name);
    return nmetalayer;
  }

  blosc2_metalayer *metalayer = schunk->metalayers[nmetalayer];
  if (content_len > metalayer->content_len) {
    BLOSC_TRACE_ERROR("`content_len` cannot exceed the existing size of %d bytes.", metalayer->content_len);
    return nmetalayer;
  }

  // Update the contents of the metalayer
  memcpy(metalayer->content, content, content_len);

  // Update the metalayers in frame (as size has not changed, we don't need to update the trailer)
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame != NULL) {
    int rc = frame_update_header(frame, schunk, false);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Unable to update meta info from frame.");
      return rc;
    }
  }

  return nmetalayer;
}


/* Find whether the schunk has a variable-length metalayer or not.
 *
 * If successful, return the index of the variable-length metalayer.  Else, return a negative value.
 */
int blosc2_vlmeta_exists(blosc2_schunk *schunk, const char *name) {
  if (strlen(name) > BLOSC2_METALAYER_NAME_MAXLEN) {
    BLOSC_TRACE_ERROR("Variable-length metalayer names cannot be larger than %d chars.", BLOSC2_METALAYER_NAME_MAXLEN);
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  for (int nvlmetalayer = 0; nvlmetalayer < schunk->nvlmetalayers; nvlmetalayer++) {
    if (strcmp(name, schunk->vlmetalayers[nvlmetalayer]->name) == 0) {
      return nvlmetalayer;
    }
  }
  return BLOSC2_ERROR_NOT_FOUND;
}

int vlmetalayer_flush(blosc2_schunk* schunk) {
  int rc = BLOSC2_ERROR_SUCCESS;
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame == NULL) {
    return rc;
  }
  rc = frame_update_header(frame, schunk, false);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Unable to update metalayers into frame.");
    return rc;
  }
  rc = frame_update_trailer(frame, schunk);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Unable to update trailer into frame.");
    return rc;
  }
  return rc;
}

/* Add content into a new variable-length metalayer.
 *
 * If successful, return the index of the new variable-length metalayer.  Else, return a negative value.
 */
int blosc2_vlmeta_add(blosc2_schunk *schunk, const char *name, uint8_t *content, int32_t content_len,
                      blosc2_cparams *cparams) {
  int nvlmetalayer = blosc2_vlmeta_exists(schunk, name);
  if (nvlmetalayer >= 0) {
    BLOSC_TRACE_ERROR("Variable-length metalayer \"%s\" already exists.", name);
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  // Add the vlmetalayer
  blosc2_metalayer *vlmetalayer = malloc(sizeof(blosc2_metalayer));
  vlmetalayer->name = strdup(name);
  uint8_t* content_buf = malloc((size_t) content_len + BLOSC2_MAX_OVERHEAD);

  blosc2_context *cctx;
  if (cparams != NULL) {
    cctx = blosc2_create_cctx(*cparams);
  } else {
    cctx = blosc2_create_cctx(BLOSC2_CPARAMS_DEFAULTS);
  }
  if (cctx == NULL) {
    BLOSC_TRACE_ERROR("Error while creating the compression context");
    return BLOSC2_ERROR_NULL_POINTER;
  }

  int csize = blosc2_compress_ctx(cctx, content, content_len, content_buf, content_len + BLOSC2_MAX_OVERHEAD);
  if (csize < 0) {
    BLOSC_TRACE_ERROR("Can not compress the `%s` variable-length metalayer.", name);
    return csize;
  }
  blosc2_free_ctx(cctx);

  vlmetalayer->content = realloc(content_buf, csize);
  vlmetalayer->content_len = csize;
  schunk->vlmetalayers[schunk->nvlmetalayers] = vlmetalayer;
  schunk->nvlmetalayers += 1;

  // Propagate to frames
  int rc = vlmetalayer_flush(schunk);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Can not propagate de `%s` variable-length metalayer to a frame.", name);
    return rc;
  }

  return schunk->nvlmetalayers - 1;
}


int blosc2_vlmeta_get(blosc2_schunk *schunk, const char *name, uint8_t **content,
                      int32_t *content_len) {
  int nvlmetalayer = blosc2_vlmeta_exists(schunk, name);
  if (nvlmetalayer < 0) {
    BLOSC_TRACE_ERROR("User metalayer \"%s\" not found.", name);
    return nvlmetalayer;
  }
  blosc2_metalayer *meta = schunk->vlmetalayers[nvlmetalayer];
  int32_t nbytes, cbytes;
  blosc2_cbuffer_sizes(meta->content, &nbytes, &cbytes, NULL);
  if (cbytes != meta->content_len) {
    BLOSC_TRACE_ERROR("User metalayer \"%s\" is corrupted.", meta->name);
    return BLOSC2_ERROR_DATA;
  }
  *content_len = nbytes;
  *content = malloc((size_t) nbytes);
  blosc2_context *dctx = blosc2_create_dctx(*schunk->storage->dparams);
  if (dctx == NULL) {
    BLOSC_TRACE_ERROR("Error while creating the decompression context");
    return BLOSC2_ERROR_NULL_POINTER;
  }
  int nbytes_ = blosc2_decompress_ctx(dctx, meta->content, meta->content_len, *content, nbytes);
  blosc2_free_ctx(dctx);
  if (nbytes_ != nbytes) {
    BLOSC_TRACE_ERROR("User metalayer \"%s\" is corrupted.", meta->name);
    return BLOSC2_ERROR_READ_BUFFER;
  }
  return nvlmetalayer;
}

int blosc2_vlmeta_update(blosc2_schunk *schunk, const char *name, uint8_t *content, int32_t content_len,
                         blosc2_cparams *cparams) {
  int nvlmetalayer = blosc2_vlmeta_exists(schunk, name);
  if (nvlmetalayer < 0) {
    BLOSC_TRACE_ERROR("User vlmetalayer \"%s\" not found.", name);
    return nvlmetalayer;
  }

  blosc2_metalayer *vlmetalayer = schunk->vlmetalayers[nvlmetalayer];
  free(vlmetalayer->content);
  uint8_t* content_buf = malloc((size_t) content_len + BLOSC2_MAX_OVERHEAD);

  blosc2_context *cctx;
  if (cparams != NULL) {
    cctx = blosc2_create_cctx(*cparams);
  } else {
    cctx = blosc2_create_cctx(BLOSC2_CPARAMS_DEFAULTS);
  }
  if (cctx == NULL) {
    BLOSC_TRACE_ERROR("Error while creating the compression context");
    return BLOSC2_ERROR_NULL_POINTER;
  }

  int csize = blosc2_compress_ctx(cctx, content, content_len, content_buf, content_len + BLOSC2_MAX_OVERHEAD);
  if (csize < 0) {
    BLOSC_TRACE_ERROR("Can not compress the `%s` variable-length metalayer.", name);
    return csize;
  }
  blosc2_free_ctx(cctx);

  vlmetalayer->content = realloc(content_buf, csize);
  vlmetalayer->content_len = csize;

  // Propagate to frames
  int rc = vlmetalayer_flush(schunk);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Can not propagate de `%s` variable-length metalayer to a frame.", name);
    return rc;
  }

  return nvlmetalayer;
}

int blosc2_vlmeta_delete(blosc2_schunk *schunk, const char *name) {
  int nvlmetalayer = blosc2_vlmeta_exists(schunk, name);
  if (nvlmetalayer < 0) {
    BLOSC_TRACE_ERROR("User vlmetalayer \"%s\" not found.", name);
    return nvlmetalayer;
  }

  blosc2_metalayer *vlmetalayer = schunk->vlmetalayers[nvlmetalayer];
  for (int i = nvlmetalayer; i < (schunk->nvlmetalayers - 1); i++) {
    schunk->vlmetalayers[i] = schunk->vlmetalayers[i + 1];
  }
  free(vlmetalayer->content);
  schunk->nvlmetalayers--;

  // Propagate to frames
  int rc = vlmetalayer_flush(schunk);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Can not propagate de `%s` variable-length metalayer to a frame.", name);
    return rc;
  }

  return schunk->nvlmetalayers;
}


int blosc2_vlmeta_get_names(blosc2_schunk *schunk, char **names) {
  int16_t nvlmetalayers = schunk->nvlmetalayers;

  for (int i = 0; i < nvlmetalayers; ++i) {
    names[i] = schunk->vlmetalayers[i]->name;
  }

  return nvlmetalayers;
}

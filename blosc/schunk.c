/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>
  Creation date: 2015-07-30

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
#include "blosc-private.h"
#include "context.h"
#include "frame.h"

#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>

/* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif

#endif  /* _WIN32 */

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
#endif


/* Get the cparams associated with a super-chunk */
int blosc2_schunk_get_cparams(blosc2_schunk *schunk, blosc2_cparams **cparams) {
  *cparams = calloc(sizeof(blosc2_cparams), 1);
  (*cparams)->schunk = schunk;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    (*cparams)->filters[i] = schunk->filters[i];
    (*cparams)->filters_meta[i] = schunk->filters_meta[i];
  }
  (*cparams)->compcode = schunk->compcode;
  (*cparams)->clevel = schunk->clevel;
  (*cparams)->typesize = schunk->typesize;
  (*cparams)->blocksize = schunk->blocksize;
  if (schunk->cctx == NULL) {
    (*cparams)->nthreads = BLOSC2_CPARAMS_DEFAULTS.nthreads;
  }
  else {
    (*cparams)->nthreads = (int16_t)schunk->cctx->nthreads;
  }
  return 0;
}


/* Get the dparams associated with a super-chunk */
int blosc2_schunk_get_dparams(blosc2_schunk *schunk, blosc2_dparams **dparams) {
  *dparams = calloc(sizeof(blosc2_dparams), 1);
  (*dparams)->schunk = schunk;
  if (schunk->dctx == NULL) {
    (*dparams)->nthreads = BLOSC2_DPARAMS_DEFAULTS.nthreads;
  }
  else {
    (*dparams)->nthreads = schunk->dctx->nthreads;
  }
  return 0;
}

blosc2_storage* get_new_storage(const blosc2_storage* storage, const blosc2_cparams* cdefaults,
                                const blosc2_dparams* ddefaults) {
  blosc2_storage* new_storage = (blosc2_storage*)calloc(1, sizeof(blosc2_storage));
  memcpy(new_storage, &storage, sizeof(blosc2_storage));
  if (storage->path != NULL) {
    size_t pathlen = strlen(storage->path);
    new_storage->path = malloc(pathlen + 1);
    strcpy(new_storage->path, storage->path);
  }
  else {
    new_storage->path = NULL;
  }
  // cparams
  blosc2_cparams* cparams = malloc(sizeof(blosc2_cparams));
  if (storage->cparams != NULL) {
    memcpy(cparams, storage->cparams, sizeof(blosc2_cparams));
  } else {
    memcpy(cparams, cdefaults, sizeof(blosc2_cparams));
  }
  new_storage->cparams = cparams;
  // dparams
  blosc2_dparams* dparams = malloc(sizeof(blosc2_dparams));
  if (storage->dparams != NULL) {
    memcpy(dparams, storage->dparams, sizeof(blosc2_dparams));
  }
  else {
    memcpy(dparams, ddefaults, sizeof(blosc2_dparams));
  }
  new_storage->dparams = dparams;
  return new_storage;
}

void update_schunk_properties(struct blosc2_schunk* schunk) {
  blosc2_cparams* cparams = schunk->storage->cparams;
  blosc2_dparams* dparams = schunk->storage->dparams;

  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    schunk->filters[i] = cparams->filters[i];
    schunk->filters_meta[i] = cparams->filters_meta[i];
  }
  schunk->compcode = cparams->compcode;
  schunk->clevel = cparams->clevel;
  schunk->typesize = cparams->typesize;
  schunk->blocksize = cparams->blocksize;
  schunk->chunksize = -1;

  /* The compression context */
  if (schunk->cctx != NULL) {
    blosc2_free_ctx(schunk->cctx);
  }
  cparams->schunk = schunk;
  schunk->cctx = blosc2_create_cctx(*cparams);

  /* The decompression context */
  if (schunk->dctx != NULL) {
    blosc2_free_ctx(schunk->dctx);
  }
  dparams->schunk = schunk;
  schunk->dctx = blosc2_create_dctx(*dparams);
}

/* Create a new super-chunk */
blosc2_schunk* blosc2_schunk_new(const blosc2_storage storage) {
  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));
  schunk->version = 0;     /* pre-first version */

  // Get the storage with proper defaults
  schunk->storage = get_new_storage(&storage, &BLOSC2_CPARAMS_DEFAULTS, &BLOSC2_DPARAMS_DEFAULTS);
  // ...and update internal properties
  update_schunk_properties(schunk);

  if (storage.sequential) {
    // We want a frame as storage
    blosc2_frame* frame = blosc2_frame_new(storage.path);
    // Initialize frame (basically, encode the header)
    int64_t frame_len = blosc2_frame_from_schunk(schunk, frame);
    if (frame_len < 0) {
      BLOSC_TRACE_ERROR("Error during the conversion of schunk to frame.");
    }
    schunk->frame = frame;
  }
  else if (storage.path != NULL) {
    BLOSC_TRACE_ERROR("Creating empty sparse schunks on-disk is not supported yet.");
    return NULL;
  }

  return schunk;
}


/* Create an empty super-chunk */
blosc2_schunk *blosc2_schunk_empty(int nchunks, const blosc2_storage storage) {
  blosc2_schunk* schunk = blosc2_schunk_new(storage);
  if (storage.sequential) {
    BLOSC_TRACE_ERROR("Creating empty frames is not supported yet.");
    return NULL;
  }

  // Init offsets
  schunk->nchunks = nchunks;
  schunk->chunksize = -1;
  schunk->nbytes = 0;
  schunk->cbytes = 0;

  schunk->data_len += sizeof(void *) * nchunks;  // must be a multiple of sizeof(void*)
  schunk->data = calloc(nchunks, sizeof(void *));

  return schunk;
}


/* Open an existing super-chunk that is on-disk (no copy is made). */
blosc2_schunk* blosc2_schunk_open(const blosc2_storage storage) {
  if (!storage.sequential) {
    BLOSC_TRACE_ERROR("Opening sparse super-chunks on-disk is not supported yet.");
    return NULL;
  }
  if (storage.path == NULL) {
    BLOSC_TRACE_ERROR("You need to supply a storage.path.");
    return NULL;
  }

  // We only support frames yet
  blosc2_frame* frame = blosc2_frame_from_file(storage.path);
  blosc2_schunk* schunk = blosc2_frame_to_schunk(frame, false);

  // Get the storage with proper defaults
  blosc2_cparams *store_cparams;
  blosc2_schunk_get_cparams(schunk, &store_cparams);
  blosc2_dparams *store_dparams;
  blosc2_schunk_get_dparams(schunk, &store_dparams);
  schunk->storage = get_new_storage(&storage, store_cparams, store_dparams);
  free(store_cparams);
  free(store_dparams);
  // Update the existing cparams/dparams with the new defaults
  update_schunk_properties(schunk);

  return schunk;
}


/* Free all memory from a super-chunk. */
int blosc2_schunk_free(blosc2_schunk *schunk) {
  if (schunk->data != NULL) {
    for (int i = 0; i < schunk->nchunks; i++) {
      free(schunk->data[i]);
    }
    free(schunk->data);
  }
  blosc2_free_ctx(schunk->cctx);
  blosc2_free_ctx(schunk->dctx);

  if (schunk->nmetalayers > 0) {
    for (int i = 0; i < schunk->nmetalayers; i++) {
      free(schunk->metalayers[i]->name);
      free(schunk->metalayers[i]->content);
      free(schunk->metalayers[i]);
    }
    schunk->nmetalayers = 0;
  }

  if (schunk->storage != NULL) {
    if (schunk->storage->path != NULL) {
      free(schunk->storage->path);
    }
    free(schunk->storage->cparams);
    free(schunk->storage->dparams);
    free(schunk->storage);
  }

  if (schunk->frame != NULL) {
    blosc2_frame_free(schunk->frame);
  }

  if (schunk->usermeta_len > 0) {
    free(schunk->usermeta);
  }

  free(schunk);

  return 0;
}


/* Create a super-chunk out of a serialized frame (no copy is made). */
blosc2_schunk* blosc2_schunk_open_sframe(uint8_t *sframe, int64_t len) {
  blosc2_frame* frame = blosc2_frame_from_sframe(sframe, len, false);
  if (frame == NULL) {
    return NULL;
  }
  blosc2_schunk* schunk = blosc2_frame_to_schunk(frame, false);
  if (schunk == NULL) {
    /* Use free instead of blosc2_frame_free since no copy */
    free(frame);
  }
  return schunk;
}


/* Append an existing chunk into a super-chunk. */
int blosc2_schunk_append_chunk(blosc2_schunk *schunk, uint8_t *chunk, bool copy) {
  int32_t nchunks = schunk->nchunks;
  int32_t nbytes = sw32_(chunk + BLOSC2_CHUNK_NBYTES);
  int32_t cbytes = sw32_(chunk + BLOSC2_CHUNK_CBYTES);

  if (schunk->chunksize == -1) {
    schunk->chunksize = nbytes;  // The super-chunk is initialized now
  }

  if (nbytes > schunk->chunksize) {
    BLOSC_TRACE_ERROR("Appending chunks that have different lengths in the same schunk "
                      "is not supported yet: %d > %d.", nbytes, schunk->chunksize);
    return -1;
  }

  /* Update counters */
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += nbytes;
  schunk->cbytes += cbytes;

  // Update super-chunk or frame
  if (schunk->frame == NULL) {
    if (schunk->storage->path != NULL) {
      BLOSC_TRACE_ERROR("The persistent sparse storage is not supported yet.");
      return -1;
    }
    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (nbytes < schunk->chunksize)) {
      uint8_t* last_chunk = schunk->data[nchunks - 1];
      int32_t last_nbytes = sw32_(last_chunk + BLOSC2_CHUNK_NBYTES);
      if ((last_nbytes < schunk->chunksize) && (nbytes < schunk->chunksize)) {
        BLOSC_TRACE_ERROR(
                "Appending two consecutive chunks with a chunksize smaller than the schunk chunksize "
                "is not allowed yet: %d != %d.", nbytes, schunk->chunksize);
        return -1;
      }
    }

    if (copy) {
        // Make a copy of the chunk
        uint8_t *chunk_copy = malloc(cbytes);
        memcpy(chunk_copy, chunk, cbytes);
        chunk = chunk_copy;
    }
    else if (cbytes < nbytes) {
      // We still want to do a shrink of the chunk
      chunk = realloc(chunk, cbytes);
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
    if (frame_append_chunk(schunk->frame, chunk, schunk) == NULL) {
      BLOSC_TRACE_ERROR("Problems appending a chunk.");
      return -1;
    }
  }

  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n", */
  /*         nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */
  return schunk->nchunks;
}


/* Insert an existing @p chunk in a specified position on a super-chunk */
int blosc2_schunk_insert_chunk(blosc2_schunk *schunk, int nchunk, uint8_t *chunk, bool copy) {
  int32_t nchunks = schunk->nchunks;
  int32_t nbytes = sw32_(chunk + BLOSC2_CHUNK_NBYTES);
  int32_t cbytes = sw32_(chunk + BLOSC2_CHUNK_CBYTES);

  if (schunk->chunksize == -1) {
    schunk->chunksize = nbytes;  // The super-chunk is initialized now
  }

  if (nbytes > schunk->chunksize) {
    BLOSC_TRACE_ERROR("Inserting chunks that have different lengths in the same schunk "
                      "is not supported yet: %d > %d.", nbytes, schunk->chunksize);
    return -1;
  }

  /* Update counters */
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += nbytes;
  schunk->cbytes += cbytes;

  // Update super-chunk or frame
  if (schunk->frame == NULL) {
    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (nbytes < schunk->chunksize)) {
      uint8_t* last_chunk = schunk->data[nchunks - 1];
      int32_t last_nbytes = sw32_(last_chunk + BLOSC2_CHUNK_NBYTES);
      if ((last_nbytes < schunk->chunksize) && (nbytes < schunk->chunksize)) {
        BLOSC_TRACE_ERROR("Appending two consecutive chunks with a chunksize smaller "
                          "than the schunk chunksize is not allowed yet:  %d != %d",
                          nbytes, schunk->chunksize);
        return -1;
      }
    }

    if (copy) {
      // Make a copy of the chunk
      uint8_t *chunk_copy = malloc(cbytes);
      memcpy(chunk_copy, chunk, cbytes);
      chunk = chunk_copy;
    }
    else if (cbytes < nbytes) {
      // We still want to do a shrink of the chunk
      chunk = realloc(chunk, cbytes);
    }

    // Make space for appending the copy of the chunk and do it
    if ((nchunks + 1) * sizeof(void *) > schunk->data_len) {
      // Extend the data pointer by one memory page (4k)
      schunk->data_len += 4096;  // must be a multiple of sizeof(void*)
      schunk->data = realloc(schunk->data, schunk->data_len);
    }

    // Reorder the offsets and insert the new chunk
    for (int i = nchunks; i > nchunk; --i) {
      schunk->data[i] = schunk->data[i-1];
    }
    schunk->data[nchunk] = chunk;
  }

  else {
    BLOSC_TRACE_ERROR("Frames are not allowed yet.");
    return -1;
  }
  return schunk->nchunks;
}


int blosc2_schunk_update_chunk(blosc2_schunk *schunk, int nchunk, uint8_t *chunk, bool copy) {
  int32_t nchunks = schunk->nchunks;
  int32_t nbytes = sw32_(chunk + BLOSC2_CHUNK_NBYTES);
  int32_t cbytes = sw32_(chunk + BLOSC2_CHUNK_CBYTES);

  if (schunk->chunksize == -1) {
    schunk->chunksize = nbytes;  // The super-chunk is initialized now
  }

  if ((schunk->chunksize != 0) && (nbytes > schunk->chunksize)) {
    BLOSC_TRACE_ERROR("Inserting chunks that have different lengths in the same schunk "
                      "is not supported yet: %d > %d.", nbytes, schunk->chunksize);
    return -1;
  }

  // Update super-chunk or frame
  if (schunk->frame == NULL) {
    uint8_t *chunk_old = schunk->data[nchunk];
    int32_t cbytes_old;
    int32_t nbytes_old;

    if (chunk_old == 0) {
      nbytes_old = 0;
      cbytes_old = 0;
    } else {
      nbytes_old = sw32_(chunk_old + BLOSC2_CHUNK_NBYTES);
      cbytes_old = sw32_(chunk_old + BLOSC2_CHUNK_CBYTES);
    }

    /* Update counters */
    schunk->nbytes += nbytes;
    schunk->nbytes -= nbytes_old;
    schunk->cbytes += cbytes;
    schunk->cbytes -= cbytes_old;

    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (nbytes < schunk->chunksize) && (nchunk == nchunks - 1)) {
      uint8_t* last_chunk = schunk->data[nchunks - 1];
      int32_t last_nbytes;
      if (last_chunk == 0) {
        last_nbytes = 0;
      } else {
        last_nbytes = sw32_(last_chunk + BLOSC2_CHUNK_NBYTES);
      }
      if ((last_nbytes < schunk->chunksize) && (nbytes < schunk->chunksize)) {
        BLOSC_TRACE_ERROR("Appending two consecutive chunks with a chunksize smaller "
                          "than the schunk chunksize is not allowed yet: %d != %d.",
                          nbytes, schunk->chunksize);
        return -1;
      }
    }

    if (copy) {
      // Make a copy of the chunk
      uint8_t *chunk_copy = malloc(cbytes);
      memcpy(chunk_copy, chunk, cbytes);
      chunk = chunk_copy;
    }
    else if (cbytes < nbytes) {
      // We still want to do a shrink of the chunk
      chunk = realloc(chunk, cbytes);
    }

    // Free old chunk and add reference to new chunk
    if (schunk->data[nchunk] != 0) {
      free(schunk->data[nchunk]);
    }
    schunk->data[nchunk] = chunk;
  }
  else {
    BLOSC_TRACE_ERROR("Updating chunks in a frame is not allowed yet.");
    return -1;
  }

  return schunk->nchunks;
}


/* Append a data buffer to a super-chunk. */
int blosc2_schunk_append_buffer(blosc2_schunk *schunk, void *src, int32_t nbytes) {
  uint8_t* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);

  /* Compress the src buffer using super-chunk context */
  int cbytes = blosc2_compress_ctx(schunk->cctx, src, nbytes, chunk,
                                   nbytes + BLOSC_MAX_OVERHEAD);
  if (cbytes < 0) {
    free(chunk);
    return cbytes;
  }

  // We don't need a copy of the chunk, as it will be shrinked if necessary
  int nchunks = blosc2_schunk_append_chunk(schunk, chunk, false);

  return nchunks;
}

/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_schunk_decompress_chunk(blosc2_schunk *schunk, int nchunk,
                                   void *dest, int32_t nbytes) {

  uint8_t* src;
  int chunksize;
  if (schunk->frame == NULL) {
    if (nchunk >= schunk->nchunks) {
      BLOSC_TRACE_ERROR("nchunk ('%d') exceeds the number of chunks "
                        "('%d') in super-chunk.", nchunk, schunk->nchunks);
      return -11;
    }

    src = schunk->data[nchunk];
    if (src == 0) {
      return 0;
    }

    int nbytes_ = sw32_(src + BLOSC2_CHUNK_NBYTES);
    if (nbytes < nbytes_) {
      BLOSC_TRACE_ERROR("Buffer size is too small for the decompressed buffer "
                        "('%d' bytes, but '%d' are needed).", nbytes, nbytes_);
      return -11;
    }
    int cbytes = sw32_(src + BLOSC2_CHUNK_CBYTES);
    chunksize = blosc2_decompress_ctx(schunk->dctx, src, cbytes, dest, nbytes);
    if (chunksize < 0 || chunksize != nbytes_) {
      BLOSC_TRACE_ERROR("Error in decompressing chunk.");
      return -11;
    }
  } else {
    chunksize = frame_decompress_chunk(schunk->dctx, schunk->frame, nchunk, dest, nbytes);
    if (chunksize < 0) {
      return -10;
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
int blosc2_schunk_get_chunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk, bool *needs_free) {
  if (schunk->frame != NULL) {
    return frame_get_chunk(schunk->frame, nchunk, chunk, needs_free);
  }

  if (nchunk >= schunk->nchunks) {
    BLOSC_TRACE_ERROR("nchunk ('%d') exceeds the number of chunks "
                      "('%d') in schunk.", nchunk, schunk->nchunks);
    return -2;
  }

  *chunk = schunk->data[nchunk];
  if (*chunk == 0) {
    *needs_free = 0;
    return 0;
  }

  *needs_free = false;
  return sw32_(*chunk + BLOSC2_CHUNK_CBYTES);
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
int blosc2_schunk_get_lazychunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk, bool *needs_free) {
  if (schunk->frame != NULL) {
    return frame_get_lazychunk(schunk->frame, nchunk, chunk, needs_free);
  }

  if (nchunk >= schunk->nchunks) {
    BLOSC_TRACE_ERROR("nchunk ('%d') exceeds the number of chunks "
                      "('%d') in schunk.", nchunk, schunk->nchunks);
    return -2;
  }

  *chunk = schunk->data[nchunk];
  if (*chunk == 0) {
    *needs_free = 0;
    return 0;
  }

  *needs_free = false;
  return sw32_(*chunk + BLOSC2_CHUNK_CBYTES);
}


/* Find whether the schunk has a metalayer or not.
 *
 * If successful, return the index of the metalayer.  Else, return a negative value.
 */
int blosc2_has_metalayer(blosc2_schunk *schunk, const char *name) {
  if (strlen(name) > BLOSC2_METALAYER_NAME_MAXLEN) {
    BLOSC_TRACE_ERROR("Metalayers cannot be larger than %d chars.", BLOSC2_METALAYER_NAME_MAXLEN);
    return -1;
  }

  for (int nmetalayer = 0; nmetalayer < schunk->nmetalayers; nmetalayer++) {
    if (strcmp(name, schunk->metalayers[nmetalayer]->name) == 0) {
      return nmetalayer;  // Found
    }
  }
  return -1;  // Not found
}

/* Reorder the chunk offsets of an existing super-chunk. */
int blosc2_schunk_reorder_offsets(blosc2_schunk *schunk, int *offsets_order) {
  // Check that the offsets order are correct
  bool *index_check = (bool *) calloc(schunk->nchunks, sizeof(bool));
  for (int i = 0; i < schunk->nchunks; ++i) {
    int index = offsets_order[i];
    if (index >= schunk->nchunks) {
      BLOSC_TRACE_ERROR("Index is bigger than the number of chunks.");
      return -1;
    }
    if (index_check[index] == false) {
      index_check[index] = true;
    } else {
      BLOSC_TRACE_ERROR("Index is yet used.");
      return -1;
    }
  }
  free(index_check);

  if (schunk->frame != NULL) {
    return frame_reorder_offsets(schunk->frame, offsets_order, schunk);
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


/**
 * @brief Flush metalayers content into a possible attached frame.
 *
 * @param schunk The super-chunk to which the flush should be applied.
 *
 * @return If successful, a 1 is returned. Else, return a negative value.
 */
// Initially, this was a public function, but as it is really meant to be used only
// in the schunk_add_metalayer(), I decided to convert it into private and call it
// implicitly instead of requiring the user to do so.  The only drawback is that
// each add operation requires a complete frame re-build, but as users should need
// very few metalayers, this overhead should be negligible in practice.
int metalayer_flush(blosc2_schunk* schunk) {
  int rc = 1;
  if (schunk->frame == NULL) {
    return rc;
  }
  rc = frame_update_header(schunk->frame, schunk, true);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Unable to update metalayers into frame.");
    return -1;
  }
  rc = frame_update_trailer(schunk->frame, schunk);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Unable to update trailer into frame.");
    return -2;
  }
  return rc;
}


/* Add content into a new metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_add_metalayer(blosc2_schunk *schunk, const char *name, uint8_t *content, uint32_t content_len) {
  int nmetalayer = blosc2_has_metalayer(schunk, name);
  if (nmetalayer >= 0) {
    BLOSC_TRACE_ERROR("Metalayer \"%s\" already exists.", name);
    return -2;
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
    return -1;
  }

  return schunk->nmetalayers - 1;
}


/* Update the content of an existing metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_update_metalayer(blosc2_schunk *schunk, const char *name, uint8_t *content, uint32_t content_len) {
  int nmetalayer = blosc2_has_metalayer(schunk, name);
  if (nmetalayer < 0) {
    BLOSC_TRACE_ERROR("Metalayer \"%s\" not found.", name);
    return nmetalayer;
  }

  blosc2_metalayer *metalayer = schunk->metalayers[nmetalayer];
  if (content_len > (uint32_t)metalayer->content_len) {
    BLOSC_TRACE_ERROR("`content_len` cannot exceed the existing size of %d bytes.", metalayer->content_len);
    return nmetalayer;
  }

  // Update the contents of the metalayer
  memcpy(metalayer->content, content, content_len);

  // Update the metalayers in frame (as size has not changed, we don't need to update the trailer)
  if (schunk->frame != NULL) {
    int rc = frame_update_header(schunk->frame, schunk, false);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Unable to update meta info from frame.");
      return -1;
    }
  }

  return nmetalayer;
}


/* Get the content out of a metalayer.
 *
 * The `**content` receives a malloc'ed copy of the content.  The user is responsible of freeing it.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_get_metalayer(blosc2_schunk *schunk, const char *name, uint8_t **content,
                         uint32_t *content_len) {
  int nmetalayer = blosc2_has_metalayer(schunk, name);
  if (nmetalayer < 0) {
    BLOSC_TRACE_ERROR("Metalayer \"%s\" not found.", name);
    return nmetalayer;
  }
  *content_len = (uint32_t)schunk->metalayers[nmetalayer]->content_len;
  *content = malloc((size_t)*content_len);
  memcpy(*content, schunk->metalayers[nmetalayer]->content, (size_t)*content_len);
  return nmetalayer;
}


/* Update the content of the usermeta chunk. */
int blosc2_update_usermeta(blosc2_schunk *schunk, uint8_t *content, int32_t content_len,
                           blosc2_cparams cparams) {
  if ((uint32_t) content_len > (1u << 31u)) {
    BLOSC_TRACE_ERROR("content_len cannot exceed 2 GB.");
    return -1;
  }

  // Compress the usermeta chunk
  void* usermeta_chunk = malloc(content_len + BLOSC_MAX_OVERHEAD);
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int usermeta_cbytes = blosc2_compress_ctx(cctx, content, content_len, usermeta_chunk,
                                            content_len + BLOSC_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  if (usermeta_cbytes < 0) {
    free(usermeta_chunk);
    return -1;
  }

  // Update the contents of the usermeta chunk
  if (schunk->usermeta_len > 0) {
    free(schunk->usermeta);
  }
  schunk->usermeta = malloc(usermeta_cbytes);
  memcpy(schunk->usermeta, usermeta_chunk, usermeta_cbytes);
  free(usermeta_chunk);
  schunk->usermeta_len = usermeta_cbytes;

  if (schunk->frame != NULL) {
    int rc = frame_update_trailer(schunk->frame, schunk);
    if (rc < 0) {
      return rc;
    }
  }

  return usermeta_cbytes;
}


/* Retrieve the usermeta chunk */
int32_t blosc2_get_usermeta(blosc2_schunk* schunk, uint8_t** content) {
  size_t nbytes, cbytes, blocksize;
  blosc_cbuffer_sizes(schunk->usermeta, &nbytes, &cbytes, &blocksize);
  *content = malloc(nbytes);
  blosc2_context *dctx = blosc2_create_dctx(BLOSC2_DPARAMS_DEFAULTS);
  int usermeta_nbytes = blosc2_decompress_ctx(dctx, schunk->usermeta, schunk->usermeta_len, *content, (int32_t)nbytes);
  blosc2_free_ctx(dctx);
  if (usermeta_nbytes < 0) {
    return -1;
  }
  return (int32_t)nbytes;
}

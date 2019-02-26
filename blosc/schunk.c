/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-07-30

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blosc.h"
#include "blosc-private.h"
#include "context.h"

#include "zstd.h"
#include "zstd_errors.h"
#include "zdict.h"

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
int blosc2_get_cparams(blosc2_schunk *schunk, blosc2_cparams **cparams) {
  *cparams = calloc(sizeof(blosc2_cparams), 1);
  (*cparams)->schunk = schunk;
  for (int i = 0; i < BLOSC_MAX_FILTERS; i++) {
    (*cparams)->filters[i] = schunk->filters[i];
    (*cparams)->filters_meta[i] = schunk->filters_meta[i];
  }
  (*cparams)->compcode = schunk->compcode;
  (*cparams)->clevel = schunk->clevel;
  (*cparams)->typesize = schunk->typesize;
  (*cparams)->blocksize = schunk->blocksize;
  if (schunk->cctx == NULL) {
    (*cparams)->nthreads = BLOSC_CPARAMS_DEFAULTS.nthreads;
  }
  else {
    (*cparams)->nthreads = (int16_t)schunk->cctx->nthreads;
  }
  return 0;
}


/* Get the dparams associated with a super-chunk */
int blosc2_get_dparams(blosc2_schunk *schunk, blosc2_dparams **dparams) {
  *dparams = calloc(sizeof(blosc2_dparams), 1);
  (*dparams)->schunk = schunk;
  if (schunk->dctx == NULL) {
    (*dparams)->nthreads = BLOSC_DPARAMS_DEFAULTS.nthreads;
  }
  else {
    (*dparams)->nthreads = schunk->dctx->nthreads;
  }
  return 0;
}


/* Create a new super-chunk */
blosc2_schunk *blosc2_new_schunk(blosc2_cparams cparams, blosc2_dparams dparams,
                                 blosc2_frame* frame) {
  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));

  schunk->version = 0;     /* pre-first version */
  for (int i = 0; i < BLOSC_MAX_FILTERS; i++) {
    schunk->filters[i] = cparams.filters[i];
    schunk->filters_meta[i] = cparams.filters_meta[i];
  }
  schunk->compcode = cparams.compcode;
  schunk->clevel = cparams.clevel;
  schunk->typesize = cparams.typesize;
  schunk->blocksize = cparams.blocksize;

  /* The compression context */
  cparams.schunk = schunk;
  schunk->cctx = blosc2_create_cctx(cparams);

  /* The decompression context */
  dparams.schunk = schunk;
  schunk->dctx = blosc2_create_dctx(dparams);

  schunk->frame = frame;
  if (frame != NULL) {
    frame->schunk = schunk;
    if (frame->len == 0) {
      // Initialize frame (basically, encode the header)
      int64_t frame_len = blosc2_schunk_to_frame(schunk, frame);
      if (frame_len < 0) {
        fprintf(stderr, "Error during the conversion of schunk to frame\n");
      }
    }
  }

  return schunk;
}


/* Append an existing chunk into a super-chunk. */
int append_chunk(blosc2_schunk* schunk, uint8_t* chunk) {
  int32_t nchunks = schunk->nchunks;
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  // TODO: update for extended headers
  int32_t nbytes = sw32_(chunk + 4);
  int32_t cbytes = sw32_(chunk + 12);

  if ((schunk->nchunks > 0) && (nbytes > schunk->chunksize)) {
    fprintf(stderr, "appending chunks with a larger chunksize than schunk is not allowed yet: "
                    "%d > %d", nbytes, schunk->chunksize);
    return -1;
  }

  /* Update counters */
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += nbytes;
  schunk->cbytes += cbytes;
  // FIXME: this should be updated when/if super-chunks support chunks with different sizes
  if (nchunks == 0) {
    schunk->chunksize = nbytes;  // Only update chunksize when it is the first chunk
  }

  // Update frame
  if (schunk->frame == NULL) {
    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (nbytes < schunk->chunksize)) {
      uint8_t* last_chunk = schunk->data[nchunks - 1];
      int32_t last_nbytes = sw32_(last_chunk + 4);
      if ((last_nbytes < schunk->chunksize) && (nbytes < schunk->chunksize)) {
        fprintf(stderr,
                "appending two consecutive chunks with a chunksize smaller than the schunk chunksize"
                "is not allowed yet: "
                "%d != %d", nbytes, schunk->chunksize);
        return -1;
      }
    }

    /* Make space for appending a new chunk and do it */
    schunk->data = realloc(schunk->data, (nchunks + 1) * sizeof(void *));
    schunk->data[nchunks] = chunk;
  }
  else {
    blosc2_frame_append_chunk(schunk->frame, chunk);
    free(chunk);  // for a frame, we don't need the chunk anymore
  }

  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n", */
  /*         nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */
  return schunk->nchunks;
}


/* Append a data buffer to a super-chunk. */
int blosc2_schunk_append_buffer(blosc2_schunk *schunk, void *src, size_t nbytes) {
  uint8_t* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);

  /* Compress the src buffer using super-chunk context */
  int cbytes = blosc2_compress_ctx(schunk->cctx, nbytes, src, chunk,
                                   nbytes + BLOSC_MAX_OVERHEAD);
  if (cbytes < 0) {
    free(chunk);
    return cbytes;
  }
  // TODO: use a realloc to get rid of unused space in chunk

  int nchunks = append_chunk(schunk, chunk);
  return nchunks;
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_schunk_decompress_chunk(blosc2_schunk *schunk, int nchunk,
                                   void *dest, size_t nbytes) {

  blosc2_context* cctx = schunk->cctx;
  blosc2_context* dctx = schunk->dctx;
  if (cctx->use_dict && dctx->dict_ddict == NULL) {
    // Create the dictionary for decompression
    // Right now we can only have an schunk if we created it in-memory.
    // TODO: revisit this when schunks can be loaded from disk.
    dctx->dict_ddict = ZSTD_createDDict(cctx->dict_buffer, cctx->dict_size);
    dctx->use_dict = 1;
  }

  uint8_t* src;
  int chunksize;
  if (schunk->frame == NULL) {
    if (nchunk >= schunk->nchunks) {
      fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                      "('%d') in super-chunk\n", nchunk, schunk->nchunks);
      return -11;
    }
    src = schunk->data[nchunk];
    int nbytes_ = sw32_(src + 4);
    if (nbytes < (size_t)nbytes_) {
      fprintf(stderr, "Buffer size is too small for the decompressed buffer "
                      "('%zd' bytes, but '%d' are needed)\n", nbytes, nbytes_);
      return -11;
    }

    chunksize = blosc2_decompress_ctx(schunk->dctx, src, dest, nbytes);
    if (chunksize < 0 || chunksize != nbytes_) {
      fprintf(stderr, "Error in decompressing chunk");
      return -11;
    }
  } else {
    chunksize = blosc2_frame_decompress_chunk(schunk->frame, nchunk, dest, nbytes);
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
    return blosc2_frame_get_chunk(schunk->frame, nchunk, chunk, needs_free);
  }

  if (nchunk >= schunk->nchunks) {
    fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                    "('%d') in schunk\n", nchunk, schunk->nchunks);
    return -2;
  }
  *chunk = schunk->data[nchunk];
  *needs_free = false;
  return sw32_(*chunk + 12);
}


/* Free all memory from a super-chunk. */
int blosc2_free_schunk(blosc2_schunk *schunk) {

  free(schunk->metadata_chunk);
  free(schunk->userdata_chunk);
  if (schunk->data != NULL) {
    for (int i = 0; i < schunk->nchunks; i++) {
      free(schunk->data[i]);
    }
    free(schunk->data);
  }
  blosc2_free_ctx(schunk->cctx);
  blosc2_free_ctx(schunk->dctx);
  free(schunk);

  return 0;
}

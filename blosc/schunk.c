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

  if (frame != NULL) {
    frame->schunk = schunk;
    if (frame->len == 0) {
      // Initialize frame (basically encode the header)
      // TODO: take into account other possibilities than an in-memory frame
      int64_t frame_len = blosc2_schunk_to_frame(schunk, frame);
      if (frame_len < 0) {
        fprintf(stderr, "Error during the conversion of schunk to frame\n");
      }
    }
  }
  schunk->frame = frame;

  return schunk;
}


/* Append an existing chunk into a super-chunk. */
int append_chunk(blosc2_schunk* schunk, void* chunk) {
  int64_t nchunks = schunk->nchunks;
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  // TODO: update for extended headers
  int32_t nbytes = *(int32_t*)((uint8_t*)chunk + 4);
  int32_t cbytes = *(int32_t*)((uint8_t*)chunk + 12);

  if (schunk->frame == NULL) {
    /* Make space for appending a new chunk and do it */
    schunk->data = realloc(schunk->data, (nchunks + 1) * sizeof(void *));
    schunk->data[nchunks] = chunk;
  }
  else {
    blosc2_frame_append_chunk(schunk->frame, chunk);
    free(chunk);  // for a frame, we don't need the chunk anymore
  }
  /* Update counters */
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += nbytes;
  schunk->cbytes += cbytes;
  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n", */
  /*         nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */
  return schunk->nchunks;
}


/* Append a data buffer to a super-chunk. */
int blosc2_schunk_append_buffer(blosc2_schunk *schunk, void *src, size_t nbytes) {
  void* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);

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

  void* src;
  if (schunk->frame == NULL) {
    if (nchunk >= schunk->nchunks) {
      fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                      "('%d') in super-chunk\n", nchunk, schunk->nchunks);
      return -10;
    }
    src = schunk->data[nchunk];
  } else {
    src = blosc2_frame_get_chunk(schunk->frame, nchunk);
    if (src == NULL) {
      return -10;
    }
  }
  int nbytes_ = *(int32_t *) ((uint8_t *) src + 4);
  if (nbytes < nbytes_) {
    fprintf(stderr, "Buffer size is too small for the decompressed buffer "
                    "('%d' bytes, but '%d' are needed)\n", nbytes, nbytes_);
    return -11;
  }

  int chunksize = blosc2_decompress_ctx(schunk->dctx, src, dest, nbytes);
  if (chunksize < 0 || chunksize != nbytes_) {
    fprintf(stderr, "Error in decompressing chunk");
  }

  return chunksize;
}


/* Free all memory from a super-chunk. */
int blosc2_free_schunk(blosc2_schunk *schunk) {

  if (schunk->metadata_chunk != NULL)
    free(schunk->metadata_chunk);
  if (schunk->userdata_chunk != NULL)
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

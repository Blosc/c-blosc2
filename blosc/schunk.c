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
#include "schunk.h"
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
blosc2_schunk* blosc2_new_schunk(blosc2_cparams cparams,
                                 blosc2_dparams dparams) {
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

  return schunk;
}


/* Append an existing chunk into a super-chunk. */
size_t append_chunk(blosc2_schunk* schunk, void* chunk) {
  int64_t nchunks = schunk->nchunks;
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  // TODO: update for extended headers
  int32_t nbytes = *(int32_t*)((uint8_t*)chunk + 4);
  int32_t cbytes = *(int32_t*)((uint8_t*)chunk + 12);

  /* Make space for appending a new chunk and do it */
  schunk->data = realloc(schunk->data, (nchunks + 1) * sizeof(void*));
  schunk->data[nchunks] = chunk;
  /* Update counters */
  schunk->nchunks = nchunks + 1;
  schunk->nbytes += nbytes;
  schunk->cbytes += cbytes;
  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n", */
  /*         nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */

  return (size_t)nchunks + 1;
}


/* Append a data buffer to a super-chunk. */
size_t blosc2_append_buffer(blosc2_schunk* schunk, size_t nbytes, void* src) {
  int cbytes;
  void* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);

  /* Compress the src buffer using super-chunk context */
  cbytes = blosc2_compress_ctx(schunk->cctx, nbytes, src, chunk,
                               nbytes + BLOSC_MAX_OVERHEAD);
  if (cbytes < 0) {
    free(chunk);
    return (size_t)cbytes;
  }
  // TODO: use a realloc to get rid of unused space in chunk

  return append_chunk(schunk, chunk);
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_decompress_chunk(blosc2_schunk* schunk, size_t nchunk,
                            void* dest, size_t nbytes) {
  int64_t nchunks = schunk->nchunks;
  blosc2_context* cctx = schunk->cctx;
  blosc2_context* dctx = schunk->dctx;
  void* src;
  int chunksize;
  int nbytes_;

  if (nchunk >= nchunks) {
    printf("specified nchunk ('%ld') exceeds the number of chunks "
           "('%ld') in super-chunk\n", (long)nchunk, (long)nchunks);
    return -10;
  }

  if (cctx->use_dict && dctx->dict_ddict == NULL) {
    // Create the dictionary for decompression
    // Right now we can only have an schunk if we created it in-memory.
    // TODO: revisit this when schunks can be loaded from disk.
    dctx->dict_ddict = ZSTD_createDDict(cctx->dict_buffer, cctx->dict_size);
    dctx->use_dict = 1;
  }

  src = schunk->data[nchunk];
  nbytes_ = *(int32_t*)((uint8_t*)src + 4);
  if (nbytes < nbytes_) {
    fprintf(stderr, "Buffer size is too small for the decompressed buffer "
                    "('%ld' bytes, but '%d' are needed)\n",
            (long)nbytes, nbytes_);
    return -11;
  }

  chunksize = blosc2_decompress_ctx(schunk->dctx, src, dest, nbytes);

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

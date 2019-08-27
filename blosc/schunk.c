/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
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
#include "zstd.h"

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


/* Create a new super-chunk */
blosc2_schunk *blosc2_new_schunk(blosc2_cparams cparams, blosc2_dparams dparams,
                                 blosc2_frame* frame) {
  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));

  schunk->version = 0;     /* pre-first version */
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
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


/* Free all memory from a super-chunk. */
int blosc2_free_schunk(blosc2_schunk *schunk) {

  // Update the metalayers and trailer in a possible attached frame
//  int rc = blosc2_metalayer_flush(schunk);
//  if (rc < 0) {
//    return -3;
//  }

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

  if (schunk->usermeta_len > 0) {
    free(schunk->usermeta);
  }
  free(schunk);

  return 0;
}

/* Append an existing chunk into a super-chunk. */
int blosc2_schunk_append_chunk(blosc2_schunk *schunk, uint8_t *chunk, bool copy) {
  int32_t nchunks = schunk->nchunks;
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
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

  // Update super-chunk or frame
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
    schunk->data = realloc(schunk->data, (nchunks + 1) * sizeof(void *));
    schunk->data[nchunks] = chunk;
  }
  else {
    frame_append_chunk(schunk->frame, chunk, schunk);
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

  // We don't need a copy of the chunk, as it will be shrinked if necessary
  int nchunks = blosc2_schunk_append_chunk(schunk, chunk, false);

  return nchunks;
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_schunk_decompress_chunk(blosc2_schunk *schunk, int nchunk,
                                   void *dest, size_t nbytes) {

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
    chunksize = frame_decompress_chunk(schunk->frame, nchunk, dest, nbytes);
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
    fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                    "('%d') in schunk\n", nchunk, schunk->nchunks);
    return -2;
  }
  *chunk = schunk->data[nchunk];
  *needs_free = false;
  return sw32_(*chunk + 12);
}


/* Find whether the schunk has a metalayer or not.
 *
 * If successful, return the index of the metalayer.  Else, return a negative value.
 */
int blosc2_has_metalayer(blosc2_schunk *schunk, char *name) {
  if (strlen(name) > BLOSC2_METALAYER_NAME_MAXLEN) {
    fprintf(stderr, "metalayers cannot be larger than %d chars\n", BLOSC2_METALAYER_NAME_MAXLEN);
    return -1;
  }

  for (int nmetalayer = 0; nmetalayer < schunk->nmetalayers; nmetalayer++) {
    if (strcmp(name, schunk->metalayers[nmetalayer]->name) == 0) {
      return nmetalayer;  // Found
    }
  }
  return -1;  // Not found
}


/* Add content into a new metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_add_metalayer(blosc2_schunk *schunk, char *name, uint8_t *content, uint32_t content_len) {
  int nmetalayer = blosc2_has_metalayer(schunk, name);
  if (nmetalayer >= 0) {
    fprintf(stderr, "metalayer \"%s\" already exists", name);
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

  return schunk->nmetalayers - 1;
}


/* Flush metalayers into a possible attached frame */
int blosc2_metalayer_flush(blosc2_schunk* schunk) {
  int rc = 1;
  if (schunk->frame == NULL) {
    return rc;
  }
  rc = frame_update_metalayers(schunk->frame, schunk, true);
  if (rc < 0) {
    fprintf(stderr, "Error: unable to update metalayers into frame\n");
    return -1;
  }
  rc = frame_update_trailer(schunk->frame, schunk);
  if (rc < 0) {
    fprintf(stderr, "Error: unable to update trailer into frame\n");
    return -2;
  }
  return rc;
}


/* Update the content of an existing metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 */
int blosc2_update_metalayer(blosc2_schunk *schunk, char *name, uint8_t *content, uint32_t content_len) {
  int nmetalayer = blosc2_has_metalayer(schunk, name);
  if (nmetalayer < 0) {
    fprintf(stderr, "metalayer \"%s\" not found\n", name);
    return nmetalayer;
  }

  blosc2_metalayer *metalayer = schunk->metalayers[nmetalayer];
  if (content_len > (uint32_t)metalayer->content_len) {
    fprintf(stderr, "`content_len` cannot exceed the existing size of %d bytes", metalayer->content_len);
    return nmetalayer;
  }

  // Update the contents of the metalayer
  memcpy(metalayer->content, content, content_len);

  // Update the metalayers in frame (as size has not changed, we don't need to update the trailer)
  if (schunk->frame != NULL) {
    int rc = frame_update_metalayers(schunk->frame, schunk, false);
    if (rc < 0) {
      fprintf(stderr, "Error: unable to update meta info from frame");
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
int blosc2_get_metalayer(blosc2_schunk *schunk, char *name, uint8_t **content,
                         uint32_t *content_len) {
  int nmetalayer = blosc2_has_metalayer(schunk, name);
  if (nmetalayer < 0) {
    fprintf(stderr, "metalayer \"%s\" not found\n", name);
    return nmetalayer;
  }
  *content_len = (uint32_t)schunk->metalayers[nmetalayer]->content_len;
  *content = malloc((size_t)*content_len);
  memcpy(*content, schunk->metalayers[nmetalayer]->content, (size_t)*content_len);
  return nmetalayer;
}


/* Update the content of the usermeta chunk. */
int blosc2_schunk_update_usermeta(blosc2_schunk *schunk, uint8_t *content, int32_t content_len,
                                  blosc2_cparams cparams) {
  if (content_len > (1u << 31u)) {
    fprintf(stderr, "Error: content_len cannot exceed 2 GB");
    return -1;
  }

  // Compress the usermeta chunk
  void* usermeta_chunk = malloc(content_len + BLOSC_MAX_OVERHEAD);
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int usermeta_cbytes = blosc2_compress_ctx(cctx, content_len, content, usermeta_chunk,
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
int32_t blosc2_schunk_get_usermeta(blosc2_schunk* schunk, uint8_t** content) {
  size_t nbytes, cbytes, blocksize;
  blosc_cbuffer_sizes(schunk->usermeta, &nbytes, &cbytes, &blocksize);
  *content = malloc(nbytes);
  blosc2_context *dctx = blosc2_create_dctx(BLOSC2_DPARAMS_DEFAULTS);
  int usermeta_nbytes = blosc2_decompress_ctx(dctx, schunk->usermeta, *content, nbytes);
  blosc2_free_ctx(dctx);
  if (usermeta_nbytes < 0) {
    return -1;
  }
  return nbytes;
}

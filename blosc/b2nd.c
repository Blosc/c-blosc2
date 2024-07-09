/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "b2nd.h"
#include "context.h"
#include "blosc2/blosc2-common.h"
#include "blosc2.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


int b2nd_serialize_meta(int8_t ndim, const int64_t *shape, const int32_t *chunkshape,
                        const int32_t *blockshape, const char *dtype, int8_t dtype_format,
                        uint8_t **smeta) {
  if (dtype == NULL) {
    dtype = B2ND_DEFAULT_DTYPE;
  }
  // dtype checks
  if (dtype_format < 0) {
    BLOSC_TRACE_ERROR("dtype_format cannot be negative");
    BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
  }
  size_t dtype_len0 = strlen(dtype);
  if (dtype_len0 > INT32_MAX) {
    BLOSC_TRACE_ERROR("dtype is too large (len > %d)", INT32_MAX);
    BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
  }
  const int32_t dtype_len = (int32_t) dtype_len0;
  // Allocate space for b2nd metalayer
  int32_t max_smeta_len = (int32_t) (1 + 1 + 1 + (1 + ndim * (1 + sizeof(int64_t))) +
                                     (1 + ndim * (1 + sizeof(int32_t))) + (1 + ndim * (1 + sizeof(int32_t))) +
                                     1 + 1 + sizeof(int32_t) + dtype_len);
  *smeta = malloc((size_t) max_smeta_len);
  BLOSC_ERROR_NULL(*smeta, BLOSC2_ERROR_MEMORY_ALLOC);
  uint8_t *pmeta = *smeta;

  // Build an array with 7 entries (version, ndim, shape, chunkshape, blockshape, dtype_format, dtype)
  *pmeta++ = 0x90 + 7;

  // version entry
  *pmeta++ = B2ND_METALAYER_VERSION;  // positive fixnum (7-bit positive integer)

  // ndim entry
  *pmeta++ = (uint8_t) ndim;  // positive fixnum (7-bit positive integer)

  // shape entry
  *pmeta++ = (uint8_t) (0x90) + ndim;  // fix array with ndim elements
  for (uint8_t i = 0; i < ndim; i++) {
    *pmeta++ = 0xd3;  // int64
    swap_store(pmeta, shape + i, sizeof(int64_t));
    pmeta += sizeof(int64_t);
  }

  // chunkshape entry
  *pmeta++ = (uint8_t) (0x90) + ndim;  // fix array with ndim elements
  for (uint8_t i = 0; i < ndim; i++) {
    *pmeta++ = 0xd2;  // int32
    swap_store(pmeta, chunkshape + i, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // blockshape entry
  *pmeta++ = (uint8_t) (0x90) + ndim;  // fix array with ndim elements
  for (uint8_t i = 0; i < ndim; i++) {
    *pmeta++ = 0xd2;  // int32
    swap_store(pmeta, blockshape + i, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // dtype entry
  *pmeta++ = dtype_format;  // positive fixint (7-bit positive integer)
  *pmeta++ = (uint8_t) (0xdb);  // str with up to 2^31 elements
  swap_store(pmeta, &dtype_len, sizeof(int32_t));
  pmeta += sizeof(int32_t);
  memcpy(pmeta, dtype, dtype_len);
  pmeta += dtype_len;

  int32_t slen = (int32_t) (pmeta - *smeta);
  if (max_smeta_len != slen) {
    BLOSC_TRACE_ERROR("meta length is inconsistent!");
    return BLOSC2_ERROR_FAILURE;
  }

  return (int)slen;
}


int b2nd_deserialize_meta(const uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                          int32_t *chunkshape, int32_t *blockshape, char **dtype, int8_t *dtype_format) {
  const uint8_t *pmeta = smeta;

  // Check that we have an array with 7 entries (version, ndim, shape, chunkshape, blockshape, dtype_format, dtype)
  pmeta += 1;

  // version entry
  // int8_t version = (int8_t)pmeta[0];  // positive fixnum (7-bit positive integer) commented to avoid warning
  pmeta += 1;

  // ndim entry
  *ndim = (int8_t) pmeta[0];
  int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
  pmeta += 1;

  // shape entry
  // Initialize to ones, as required by b2nd
  for (int i = 0; i < ndim_aux; i++) shape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(shape + i, pmeta, sizeof(int64_t));
    pmeta += sizeof(int64_t);
  }

  // chunkshape entry
  // Initialize to ones, as required by b2nd
  for (int i = 0; i < ndim_aux; i++) chunkshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(chunkshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // blockshape entry
  // Initialize to ones, as required by b2nd
  for (int i = 0; i < ndim_aux; i++) blockshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(blockshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // dtype entry
  if (dtype_format == NULL || dtype == NULL) {
    return (int32_t)(pmeta - smeta);
  }
  if (pmeta - smeta < smeta_len) {
    // dtype info is here
    *dtype_format = (int8_t) *(pmeta++);
    pmeta += 1;
    int dtype_len;
    swap_store(&dtype_len, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
    *dtype = (char*)malloc(dtype_len + 1);
    char* dtype_ = *dtype;
    memcpy(dtype_, (char*)pmeta, dtype_len);
    dtype_[dtype_len] = '\0';
    pmeta += dtype_len;
  }
  else {
    // dtype is mandatory in b2nd metalayer, but this is mainly meant as
    // a fall-back for deprecated caterva headers
    *dtype = NULL;
    *dtype_format = 0;
  }

  int32_t slen = (int32_t) (pmeta - smeta);
  return (int)slen;
}



int update_shape(b2nd_array_t *array, int8_t ndim, const int64_t *shape,
                 const int32_t *chunkshape, const int32_t *blockshape) {
  array->ndim = ndim;
  array->nitems = 1;
  array->extnitems = 1;
  array->extchunknitems = 1;
  array->chunknitems = 1;
  array->blocknitems = 1;
  for (int i = 0; i < B2ND_MAX_DIM; ++i) {
    if (i < ndim) {
      array->shape[i] = shape[i];
      array->chunkshape[i] = chunkshape[i];
      array->blockshape[i] = blockshape[i];
      if (shape[i] != 0) {
        if (shape[i] % array->chunkshape[i] == 0) {
          array->extshape[i] = shape[i];
        } else {
          array->extshape[i] = shape[i] + chunkshape[i] - shape[i] % chunkshape[i];
        }
        if (chunkshape[i] % blockshape[i] == 0) {
          array->extchunkshape[i] = chunkshape[i];
        } else {
          array->extchunkshape[i] =
                  chunkshape[i] + blockshape[i] - chunkshape[i] % blockshape[i];
        }
      } else {
        array->extchunkshape[i] = chunkshape[i];
        array->extshape[i] = 0;
      }
    } else {
      array->blockshape[i] = 1;
      array->chunkshape[i] = 1;
      array->extshape[i] = 1;
      array->extchunkshape[i] = 1;
      array->shape[i] = 1;
    }
    array->nitems *= array->shape[i];
    array->extnitems *= array->extshape[i];
    array->extchunknitems *= array->extchunkshape[i];
    array->chunknitems *= array->chunkshape[i];
    array->blocknitems *= array->blockshape[i];
  }

  // Compute strides
  array->item_array_strides[ndim - 1] = 1;
  array->item_extchunk_strides[ndim - 1] = 1;
  array->item_chunk_strides[ndim - 1] = 1;
  array->item_block_strides[ndim - 1] = 1;
  array->block_chunk_strides[ndim - 1] = 1;
  array->chunk_array_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    if (shape[i + 1] != 0) {
      array->item_array_strides[i] = array->item_array_strides[i + 1] * array->shape[i + 1];
      array->item_extchunk_strides[i] =
              array->item_extchunk_strides[i + 1] * array->extchunkshape[i + 1];
      array->item_chunk_strides[i] =
              array->item_chunk_strides[i + 1] * array->chunkshape[i + 1];
      array->item_block_strides[i] =
              array->item_block_strides[i + 1] * array->blockshape[i + 1];
      array->block_chunk_strides[i] = array->block_chunk_strides[i + 1] *
                                      (array->extchunkshape[i + 1] /
                                       array->blockshape[i + 1]);
      array->chunk_array_strides[i] = array->chunk_array_strides[i + 1] *
                                      (array->extshape[i + 1] * array->chunkshape[i + 1]);
    } else {
      array->item_array_strides[i] = 0;
      array->item_extchunk_strides[i] = 0;
      array->item_chunk_strides[i] = 0;
      array->item_block_strides[i] = 0;
      array->block_chunk_strides[i] = 0;
      array->chunk_array_strides[i] = 0;
    }
  }
  if (array->sc) {
    uint8_t *smeta = NULL;
    // Serialize the dimension info ...
    int32_t smeta_len =
            b2nd_serialize_meta(array->ndim, array->shape, array->chunkshape, array->blockshape,
                                array->dtype, array->dtype_format, &smeta);
    if (smeta_len < 0) {
      BLOSC_TRACE_ERROR("Error during serializing dims info for Blosc2 NDim");
      BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
    }
    // ... and update it in its metalayer
    if (blosc2_meta_exists(array->sc, "b2nd") < 0) {
      if (blosc2_meta_add(array->sc, "b2nd", smeta, smeta_len) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
    } else {
      if (blosc2_meta_update(array->sc, "b2nd", smeta, smeta_len) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
    }
    free(smeta);
  }

  return BLOSC2_ERROR_SUCCESS;
}


int array_without_schunk(b2nd_context_t *ctx, b2nd_array_t **array) {
  /* Create a b2nd_array_t buffer */
  (*array) = (b2nd_array_t *) malloc(sizeof(b2nd_array_t));
  BLOSC_ERROR_NULL(*array, BLOSC2_ERROR_MEMORY_ALLOC);

  (*array)->sc = NULL;

  (*array)->ndim = ctx->ndim;
  int64_t *shape = ctx->shape;
  int32_t *chunkshape = ctx->chunkshape;
  int32_t *blockshape = ctx->blockshape;
  BLOSC_ERROR(update_shape(*array, ctx->ndim, shape, chunkshape, blockshape));

  if (ctx->dtype != NULL) {
    (*array)->dtype = malloc(strlen(ctx->dtype) + 1);
    strcpy((*array)->dtype, ctx->dtype);
  } else {
    (*array)->dtype = NULL;
  }

  (*array)->dtype_format = ctx->dtype_format;

  // The partition cache (empty initially)
  (*array)->chunk_cache.data = NULL;
  (*array)->chunk_cache.nchunk = -1;  // means no valid cache yet

  return BLOSC2_ERROR_SUCCESS;
}


int array_new(b2nd_context_t *ctx, int special_value, b2nd_array_t **array) {
  BLOSC_ERROR(array_without_schunk(ctx, array));

  blosc2_schunk *sc = blosc2_schunk_new(ctx->b2_storage);
  if (sc == NULL) {
    BLOSC_TRACE_ERROR("Pointer is NULL");
    return BLOSC2_ERROR_FAILURE;
  }
  // Set the chunksize for the schunk, as it cannot be derived from storage
  int32_t chunksize = (int32_t) (*array)->extchunknitems * sc->typesize;
  sc->chunksize = chunksize;

  // Serialize the dimension info
  if (sc->nmetalayers >= BLOSC2_MAX_METALAYERS) {
    BLOSC_TRACE_ERROR("the number of metalayers for this schunk has been exceeded");
    return BLOSC2_ERROR_FAILURE;
  }
  uint8_t *smeta = NULL;
  int32_t smeta_len = b2nd_serialize_meta(ctx->ndim,
                                          (*array)->shape,
                                          (*array)->chunkshape,
                                          (*array)->blockshape,
                                          (*array)->dtype,
                                          (*array)->dtype_format,
                                          &smeta);
  if (smeta_len < 0) {
    BLOSC_TRACE_ERROR("error during serializing dims info for Blosc2 NDim");
    return BLOSC2_ERROR_FAILURE;
  }

  // And store it in b2nd metalayer
  if (blosc2_meta_add(sc, "b2nd", smeta, smeta_len) < 0) {
    return BLOSC2_ERROR_FAILURE;
  }

  free(smeta);

  for (int i = 0; i < ctx->nmetalayers; ++i) {
    char *name = ctx->metalayers[i].name;
    uint8_t *data = ctx->metalayers[i].content;
    int32_t size = ctx->metalayers[i].content_len;
    if (blosc2_meta_add(sc, name, data, size) < 0) {
      BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
    }
  }

  if ((*array)->extchunknitems * sc->typesize > BLOSC2_MAX_BUFFERSIZE){
    BLOSC_TRACE_ERROR("Chunksize exceeds maximum of %d", BLOSC2_MAX_BUFFERSIZE);
    return BLOSC2_ERROR_MAX_BUFSIZE_EXCEEDED;
  }
  // Fill schunk with uninit values
  if ((*array)->nitems != 0) {
    int64_t nchunks = (*array)->extnitems / (*array)->chunknitems;
    int64_t nitems = nchunks * (*array)->extchunknitems;
    // blosc2_schunk_fill_special(sc, nitems, BLOSC2_SPECIAL_ZERO, chunksize);
    BLOSC_ERROR(blosc2_schunk_fill_special(sc, nitems, special_value, chunksize));
  }
  (*array)->sc = sc;

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_uninit(b2nd_context_t *ctx, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(array_new(ctx, BLOSC2_SPECIAL_UNINIT, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_empty(b2nd_context_t *ctx, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  // Fill with zeros to avoid variable cratios
  BLOSC_ERROR(array_new(ctx, BLOSC2_SPECIAL_ZERO, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_zeros(b2nd_context_t *ctx, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(array_new(ctx, BLOSC2_SPECIAL_ZERO, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_nans(b2nd_context_t *ctx, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(array_new(ctx, BLOSC2_SPECIAL_NAN, array));

  const int32_t typesize = (*array)->sc->typesize;
  if (typesize != 4 && typesize != 8)
  {
    BLOSC_TRACE_ERROR("Unsupported typesize for NaN");
    return BLOSC2_ERROR_DATA;
  }

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_full(b2nd_context_t *ctx, b2nd_array_t **array, const void *fill_value) {
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(b2nd_empty(ctx, array));

  int32_t chunkbytes = (int32_t) (*array)->extchunknitems * (*array)->sc->typesize;

  blosc2_cparams *cparams;
  if (blosc2_schunk_get_cparams((*array)->sc, &cparams) != 0) {
    BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
  }

  int32_t chunksize = BLOSC_EXTENDED_HEADER_LENGTH + (*array)->sc->typesize;
  uint8_t *chunk = malloc(chunksize);
  BLOSC_ERROR_NULL(chunk, BLOSC2_ERROR_MEMORY_ALLOC);
  if (blosc2_chunk_repeatval(*cparams, chunkbytes, chunk, chunksize, fill_value) < 0) {
    BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
  }
  free(cparams);

  for (int i = 0; i < (*array)->sc->nchunks; ++i) {
    if (blosc2_schunk_update_chunk((*array)->sc, i, chunk, true) < 0) {
      BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
    }
  }
  free(chunk);

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_from_schunk(blosc2_schunk *schunk, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  if (schunk == NULL) {
    BLOSC_TRACE_ERROR("Schunk is null");
    return BLOSC2_ERROR_NULL_POINTER;
  }

  blosc2_cparams *cparams;
  if (blosc2_schunk_get_cparams(schunk, &cparams) < 0) {
    BLOSC_TRACE_ERROR("Blosc error");
    return BLOSC2_ERROR_NULL_POINTER;
  }
  free(cparams);

  b2nd_context_t params = {0};
  params.b2_storage = schunk->storage;

  // Deserialize the b2nd metalayer
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(schunk, "b2nd", &smeta, &smeta_len) < 0) {
    // Try with a caterva metalayer; we are meant to be backward compatible with it
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
      BLOSC_ERROR(BLOSC2_ERROR_METALAYER_NOT_FOUND);
    }
  }
  BLOSC_ERROR(b2nd_deserialize_meta(smeta, smeta_len, &params.ndim, params.shape,
                                    params.chunkshape, params.blockshape, &params.dtype,
                                    &params.dtype_format));
  free(smeta);

  BLOSC_ERROR(array_without_schunk(&params, array));
  free(params.dtype);

  (*array)->sc = schunk;

  if ((*array) == NULL) {
    BLOSC_TRACE_ERROR("Error creating a b2nd container from a frame");
    return BLOSC2_ERROR_NULL_POINTER;
  }

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_to_cframe(const b2nd_array_t *array, uint8_t **cframe, int64_t *cframe_len,
                   bool *needs_free) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(cframe, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(cframe_len, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(needs_free, BLOSC2_ERROR_NULL_POINTER);

  *cframe_len = blosc2_schunk_to_buffer(array->sc, cframe, needs_free);
  if (*cframe_len <= 0) {
    BLOSC_TRACE_ERROR("Error serializing the b2nd array");
    return BLOSC2_ERROR_FAILURE;
  }
  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_from_cframe(uint8_t *cframe, int64_t cframe_len, bool copy, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(cframe, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  blosc2_schunk *sc = blosc2_schunk_from_buffer(cframe, cframe_len, copy);
  if (sc == NULL) {
    BLOSC_TRACE_ERROR("Blosc error");
    return BLOSC2_ERROR_FAILURE;
  }
  // ...and create a b2nd array out of it
  BLOSC_ERROR(b2nd_from_schunk(sc, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_open(const char *urlpath, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(urlpath, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  blosc2_schunk *sc = blosc2_schunk_open(urlpath);

  // ...and create a b2nd array out of it
  BLOSC_ERROR(b2nd_from_schunk(sc, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_open_offset(const char *urlpath, b2nd_array_t **array, int64_t offset) {
  BLOSC_ERROR_NULL(urlpath, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  blosc2_schunk *sc = blosc2_schunk_open_offset(urlpath, offset);

  // ...and create a b2nd array out of it
  BLOSC_ERROR(b2nd_from_schunk(sc, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_free(b2nd_array_t *array) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  if (array) {
    if (array->sc != NULL) {
      blosc2_schunk_free(array->sc);
    }
    free(array->dtype);
    free(array);
  }
  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_from_cbuffer(b2nd_context_t *ctx, b2nd_array_t **array, const void *buffer, int64_t buffersize) {
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(b2nd_empty(ctx, array));

  if (buffersize < (int64_t) (*array)->nitems * (*array)->sc->typesize) {
    BLOSC_TRACE_ERROR("The buffersize (%lld) is smaller than the array size (%lld)",
                        (long long) buffersize, (long long) (*array)->nitems * (*array)->sc->typesize);
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }

  if ((*array)->nitems == 0) {
    return BLOSC2_ERROR_SUCCESS;
  }

  int64_t start[B2ND_MAX_DIM] = {0};
  int64_t *stop = (*array)->shape;
  int64_t *shape = (*array)->shape;
  BLOSC_ERROR(b2nd_set_slice_cbuffer(buffer, shape, buffersize, start, stop, *array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_to_cbuffer(const b2nd_array_t *array, void *buffer,
                    int64_t buffersize) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);

  if (buffersize < (int64_t) array->nitems * array->sc->typesize) {
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }

  if (array->nitems == 0) {
    return BLOSC2_ERROR_SUCCESS;
  }

  int64_t start[B2ND_MAX_DIM] = {0};
  const int64_t *stop = array->shape;
  BLOSC_ERROR(b2nd_get_slice_cbuffer(array, start, stop, buffer, array->shape, buffersize));
  return BLOSC2_ERROR_SUCCESS;
}

int b2nd_get_slice_nchunks(const b2nd_array_t *array, const int64_t *start, const int64_t *stop, int64_t **chunks_idx) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(start, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(stop, BLOSC2_ERROR_NULL_POINTER);

  int8_t ndim = array->ndim;

  // 0-dim case
  if (ndim == 0) {
    *chunks_idx = malloc(1 * sizeof(int64_t));
    *chunks_idx[0] = 0;
    return 1;
  }

  int64_t chunks_in_array[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ndim; ++i) {
    chunks_in_array[i] = array->extshape[i] / array->chunkshape[i];
  }

  int64_t chunks_in_array_strides[B2ND_MAX_DIM];
  chunks_in_array_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    chunks_in_array_strides[i] = chunks_in_array_strides[i + 1] * chunks_in_array[i + 1];
  }

  // Compute the number of chunks to update
  int64_t update_start[B2ND_MAX_DIM];
  int64_t update_shape[B2ND_MAX_DIM];

  int64_t update_nchunks = 1;
  for (int i = 0; i < ndim; ++i) {
    int64_t pos = 0;
    while (pos <= start[i]) {
      pos += array->chunkshape[i];
    }
    update_start[i] = pos / array->chunkshape[i] - 1;
    while (pos < stop[i]) {
      pos += array->chunkshape[i];
    }
    update_shape[i] = pos / array->chunkshape[i] - update_start[i];
    update_nchunks *= update_shape[i];
  }

  int nchunks = 0;
  // Initially we do not know the number of chunks that will be affected
  *chunks_idx = malloc(array->sc->nchunks * sizeof(int64_t));
  int64_t *ptr = *chunks_idx;
  for (int update_nchunk = 0; update_nchunk < update_nchunks; ++update_nchunk) {
    int64_t nchunk_ndim[B2ND_MAX_DIM] = {0};
    blosc2_unidim_to_multidim(ndim, update_shape, update_nchunk, nchunk_ndim);
    for (int i = 0; i < ndim; ++i) {
      nchunk_ndim[i] += update_start[i];
    }
    int64_t nchunk;
    blosc2_multidim_to_unidim(nchunk_ndim, ndim, chunks_in_array_strides, &nchunk);

    // Check if the chunk is inside the slice domain
    int64_t chunk_start[B2ND_MAX_DIM] = {0};
    int64_t chunk_stop[B2ND_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
      chunk_start[i] = nchunk_ndim[i] * array->chunkshape[i];
      chunk_stop[i] = chunk_start[i] + array->chunkshape[i];
      if (chunk_stop[i] > array->shape[i]) {
        chunk_stop[i] = array->shape[i];
      }
    }
    bool chunk_empty = false;
    for (int i = 0; i < ndim; ++i) {
      chunk_empty |= (chunk_stop[i] <= start[i] || chunk_start[i] >= stop[i]);
    }
    if (chunk_empty) {
      continue;
    }

    ptr[nchunks] = nchunk;
    nchunks++;
  }

  if (nchunks < array->sc->nchunks) {
    *chunks_idx = realloc(ptr, nchunks * sizeof(int64_t));
  }

  return nchunks;
}


// Check whether the slice defined by start and stop is a single chunk and contiguous
// in the C order. We also need blocks inside a chunk, and chunks inside an array,
// are C contiguous. This is a fast path for the get_slice and set_slice functions.
int64_t nchunk_fastpath(const b2nd_array_t *array, const int64_t *start,
                        const int64_t *stop, const int64_t slice_size) {
  if (slice_size != array->chunknitems) {
    return -1;
  }

  int ndim = (int) array->ndim;
  int inner_dim = ndim - 1;
  int64_t partial_slice_size = 1;
  int64_t partial_chunk_size = 1;
  for (int i = ndim - 1; i >= 0; --i) {
    // We need to check if the slice is contiguous in the C order
    if (array->extshape[i] != array->shape[i]) {
      return -1;
    }
    if (array->extchunkshape[i] != array->chunkshape[i]) {
      return -1;
    }

    // blocks need to be C contiguous inside the chunk as well
    if (array->chunkshape[i] > array->blockshape[i]) {
      if (i < inner_dim) {
        if (array->chunkshape[i] % array->blockshape[i] != 0) {
          return -1;
        }
      }
      else {
        if (array->chunkshape[i] != array->blockshape[i]) {
          return -1;
        }
      }
      inner_dim = i;
    }

    // We need start and stop to be aligned with the chunkshape
    // Do that by computing the slice size in reverse order and compare with the chunkshape
    partial_slice_size *= stop[i] - start[i];
    partial_chunk_size *= array->chunkshape[i];
    if (partial_slice_size != partial_chunk_size) {
      return -1;
    }
    // Ensure that the slice starts at the beginning of the chunk
    if (start[i] % array->chunkshape[i] != 0) {
      return -1;
    }
  }

  // Compute the chunk number
  int64_t *chunks_idx;
  int nchunks = b2nd_get_slice_nchunks(array, start, stop, &chunks_idx);
  if (nchunks != 1) {
    free(chunks_idx);
    BLOSC_TRACE_ERROR("The number of chunks to read is not 1; go fix the code");
    BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
  }
  int64_t nchunk = chunks_idx[0];
  free(chunks_idx);

  return nchunk;
}


// Setting and getting slices
int get_set_slice(void *buffer, int64_t buffersize, const int64_t *start, const int64_t *stop,
                  const int64_t *shape, b2nd_array_t *array, bool set_slice) {
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(start, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(stop, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  if (buffersize < 0) {
    BLOSC_TRACE_ERROR("buffersize is < 0");
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }

  uint8_t *buffer_b = (uint8_t *) buffer;
  const int64_t *buffer_start = start;
  const int64_t *buffer_stop = stop;
  const int64_t *buffer_shape = shape;

  int8_t ndim = array->ndim;

  // 0-dim case
  if (ndim == 0) {
    if (set_slice) {
      int32_t chunk_size = array->sc->typesize + BLOSC2_MAX_OVERHEAD;
      uint8_t *chunk = malloc(chunk_size);
      BLOSC_ERROR_NULL(chunk, BLOSC2_ERROR_MEMORY_ALLOC);
      if (blosc2_compress_ctx(array->sc->cctx, buffer_b, array->sc->typesize, chunk, chunk_size) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      if (blosc2_schunk_update_chunk(array->sc, 0, chunk, false) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }

    } else {
      if (blosc2_schunk_decompress_chunk(array->sc, 0, buffer_b, array->sc->typesize) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
    }
    return BLOSC2_ERROR_SUCCESS;
  }

  if (array->nitems == 0) {
    return BLOSC2_ERROR_SUCCESS;
  }

  int64_t nelems_slice = 1;
  for (int i = 0; i < array->ndim; ++i) {
    if (stop[i] - start[i] > shape[i]) {
      BLOSC_TRACE_ERROR("The buffer shape can not be smaller than the slice shape");
      return BLOSC2_ERROR_INVALID_PARAM;
    }
    nelems_slice *= stop[i] - start[i];
  }
  int64_t slice_nbytes = nelems_slice * array->sc->typesize;
  int32_t data_nbytes = (int32_t) array->extchunknitems * array->sc->typesize;

  if (buffersize < slice_nbytes) {
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }

  // Check for fast path for aligned slices with chunks and blocks (only 1 chunk is supported)
  int64_t nchunk = nchunk_fastpath(array, start, stop, nelems_slice);
  if (nchunk >= 0) {
    if (set_slice) {
      // Fast path for set. Let's set the chunk buffer straight into the array.
      // Compress the chunk
      int32_t chunk_nbytes = data_nbytes + BLOSC2_MAX_OVERHEAD;
      uint8_t *chunk = malloc(chunk_nbytes);
      BLOSC_ERROR_NULL(chunk, BLOSC2_ERROR_MEMORY_ALLOC);
      int brc;
      // Update current_chunk in case a prefilter is applied
      array->sc->current_nchunk = nchunk;
      brc = blosc2_compress_ctx(array->sc->cctx, buffer, data_nbytes, chunk, chunk_nbytes);
      if (brc < 0) {
        BLOSC_TRACE_ERROR("Blosc can not compress the data");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      int64_t brc_ = blosc2_schunk_update_chunk(array->sc, nchunk, chunk, false);
      if (brc_ < 0) {
        BLOSC_TRACE_ERROR("Blosc can not update the chunk");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      // We are done
      return BLOSC2_ERROR_SUCCESS;
    }
    else {
      // Fast path for get. Let's read the chunk straight into the buffer.
      if (blosc2_schunk_decompress_chunk(array->sc, nchunk, buffer, (int32_t) slice_nbytes) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      return BLOSC2_ERROR_SUCCESS;
    }
  }

  // Slow path for set and get

  uint8_t *data = malloc(data_nbytes);
  BLOSC_ERROR_NULL(data, BLOSC2_ERROR_MEMORY_ALLOC);

  int64_t chunks_in_array[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ndim; ++i) {
    chunks_in_array[i] = array->extshape[i] / array->chunkshape[i];
  }

  int64_t chunks_in_array_strides[B2ND_MAX_DIM];
  chunks_in_array_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    chunks_in_array_strides[i] = chunks_in_array_strides[i + 1] * chunks_in_array[i + 1];
  }

  int64_t blocks_in_chunk[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ndim; ++i) {
    blocks_in_chunk[i] = array->extchunkshape[i] / array->blockshape[i];
  }

  // Compute the number of chunks to update
  int64_t update_start[B2ND_MAX_DIM];
  int64_t update_shape[B2ND_MAX_DIM];

  int64_t update_nchunks = 1;
  for (int i = 0; i < ndim; ++i) {
    int64_t pos = 0;
    while (pos <= buffer_start[i]) {
      pos += array->chunkshape[i];
    }
    update_start[i] = pos / array->chunkshape[i] - 1;
    while (pos < buffer_stop[i]) {
      pos += array->chunkshape[i];
    }
    update_shape[i] = pos / array->chunkshape[i] - update_start[i];
    update_nchunks *= update_shape[i];
  }

  for (int update_nchunk = 0; update_nchunk < update_nchunks; ++update_nchunk) {
    int64_t nchunk_ndim[B2ND_MAX_DIM] = {0};
    blosc2_unidim_to_multidim(ndim, update_shape, update_nchunk, nchunk_ndim);
    for (int i = 0; i < ndim; ++i) {
      nchunk_ndim[i] += update_start[i];
    }
    int64_t nchunk;
    blosc2_multidim_to_unidim(nchunk_ndim, ndim, chunks_in_array_strides, &nchunk);

    // Check if the chunk needs to be updated
    int64_t chunk_start[B2ND_MAX_DIM] = {0};
    int64_t chunk_stop[B2ND_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
      chunk_start[i] = nchunk_ndim[i] * array->chunkshape[i];
      chunk_stop[i] = chunk_start[i] + array->chunkshape[i];
      if (chunk_stop[i] > array->shape[i]) {
        chunk_stop[i] = array->shape[i];
      }
    }
    bool chunk_empty = false;
    for (int i = 0; i < ndim; ++i) {
      chunk_empty |= (chunk_stop[i] <= buffer_start[i] || chunk_start[i] >= buffer_stop[i]);
    }
    if (chunk_empty) {
      continue;
    }

    int32_t nblocks = (int32_t) array->extchunknitems / array->blocknitems;
    if (set_slice) {
      // Check if all the chunk is going to be updated and avoid the decompression
      bool decompress_chunk = false;
      for (int i = 0; i < ndim; ++i) {
        decompress_chunk |= (chunk_start[i] < buffer_start[i] || chunk_stop[i] > buffer_stop[i]);
      }

      if (decompress_chunk) {
        int err = blosc2_schunk_decompress_chunk(array->sc, nchunk, data, data_nbytes);
        if (err < 0) {
          BLOSC_TRACE_ERROR("Error decompressing chunk");
          BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
        }
      } else {
        // Avoid writing non zero padding from previous chunk
        memset(data, 0, data_nbytes);
      }
    } else {
      bool *block_maskout = malloc(nblocks);
      BLOSC_ERROR_NULL(block_maskout, BLOSC2_ERROR_MEMORY_ALLOC);
      for (int nblock = 0; nblock < nblocks; ++nblock) {
        int64_t nblock_ndim[B2ND_MAX_DIM] = {0};
        blosc2_unidim_to_multidim(ndim, blocks_in_chunk, nblock, nblock_ndim);

        // Check if the block needs to be updated
        int64_t block_start[B2ND_MAX_DIM] = {0};
        int64_t block_stop[B2ND_MAX_DIM] = {0};
        for (int i = 0; i < ndim; ++i) {
          block_start[i] = nblock_ndim[i] * array->blockshape[i];
          block_stop[i] = block_start[i] + array->blockshape[i];
          block_start[i] += chunk_start[i];
          block_stop[i] += chunk_start[i];

          if (block_start[i] > chunk_stop[i]) {
            block_start[i] = chunk_stop[i];
          }
          if (block_stop[i] > chunk_stop[i]) {
            block_stop[i] = chunk_stop[i];
          }
        }

        bool block_empty = false;
        for (int i = 0; i < ndim; ++i) {
          block_empty |= (block_stop[i] <= start[i] || block_start[i] >= stop[i]);
        }
        block_maskout[nblock] = block_empty ? true : false;
      }

      if (blosc2_set_maskout(array->sc->dctx, block_maskout, nblocks) != BLOSC2_ERROR_SUCCESS) {
        BLOSC_TRACE_ERROR("Error setting the maskout");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }

      int err = blosc2_schunk_decompress_chunk(array->sc, nchunk, data, data_nbytes);
      if (err < 0) {
        BLOSC_TRACE_ERROR("Error decompressing chunk");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }

      free(block_maskout);
    }

    // Iterate over blocks

    for (int nblock = 0; nblock < nblocks; ++nblock) {
      int64_t nblock_ndim[B2ND_MAX_DIM] = {0};
      blosc2_unidim_to_multidim(ndim, blocks_in_chunk, nblock, nblock_ndim);

      // Check if the block needs to be updated
      int64_t block_start[B2ND_MAX_DIM] = {0};
      int64_t block_stop[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        block_start[i] = nblock_ndim[i] * array->blockshape[i];
        block_stop[i] = block_start[i] + array->blockshape[i];
        block_start[i] += chunk_start[i];
        block_stop[i] += chunk_start[i];

        if (block_start[i] > chunk_stop[i]) {
          block_start[i] = chunk_stop[i];
        }
        if (block_stop[i] > chunk_stop[i]) {
          block_stop[i] = chunk_stop[i];
        }
      }
      int64_t block_shape[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        block_shape[i] = block_stop[i] - block_start[i];
      }
      bool block_empty = false;
      for (int i = 0; i < ndim; ++i) {
        block_empty |= (block_stop[i] <= start[i] || block_start[i] >= stop[i]);
      }
      if (block_empty) {
        continue;
      }

      // compute the start of the slice inside the block
      int64_t slice_start[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        if (block_start[i] < buffer_start[i]) {
          slice_start[i] = buffer_start[i] - block_start[i];
        } else {
          slice_start[i] = 0;
        }
        slice_start[i] += block_start[i];
      }

      int64_t slice_stop[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        if (block_stop[i] > buffer_stop[i]) {
          slice_stop[i] = block_shape[i] - (block_stop[i] - buffer_stop[i]);
        } else {
          slice_stop[i] = block_stop[i] - block_start[i];
        }
        slice_stop[i] += block_start[i];
      }

      int64_t slice_shape[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        slice_shape[i] = slice_stop[i] - slice_start[i];
      }

      uint8_t *src = &buffer_b[0];
      const int64_t *src_pad_shape = buffer_shape;

      int64_t src_start[B2ND_MAX_DIM] = {0};
      int64_t src_stop[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        src_start[i] = slice_start[i] - buffer_start[i];
        src_stop[i] = slice_stop[i] - buffer_start[i];
      }

      uint8_t *dst = &data[nblock * array->blocknitems * array->sc->typesize];
      int64_t dst_pad_shape[B2ND_MAX_DIM];
      for (int i = 0; i < ndim; ++i) {
        dst_pad_shape[i] = array->blockshape[i];
      }

      int64_t dst_start[B2ND_MAX_DIM] = {0};
      int64_t dst_stop[B2ND_MAX_DIM] = {0};
      for (int i = 0; i < ndim; ++i) {
        dst_start[i] = slice_start[i] - block_start[i];
        dst_stop[i] = dst_start[i] + slice_shape[i];
      }

      if (set_slice) {
        b2nd_copy_buffer(ndim, array->sc->typesize,
                         src, src_pad_shape, src_start, src_stop,
                         dst, dst_pad_shape, dst_start);
      } else {
        b2nd_copy_buffer(ndim, array->sc->typesize,
                         dst, dst_pad_shape, dst_start, dst_stop,
                         src, src_pad_shape, src_start);
      }
    }

    if (set_slice) {
      // Recompress the data
      int32_t chunk_nbytes = data_nbytes + BLOSC2_MAX_OVERHEAD;
      uint8_t *chunk = malloc(chunk_nbytes);
      BLOSC_ERROR_NULL(chunk, BLOSC2_ERROR_MEMORY_ALLOC);
      int brc;
      // Update current_chunk in case a prefilter is applied
      array->sc->current_nchunk = nchunk;
      brc = blosc2_compress_ctx(array->sc->cctx, data, data_nbytes, chunk, chunk_nbytes);
      if (brc < 0) {
        BLOSC_TRACE_ERROR("Blosc can not compress the data");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      int64_t brc_ = blosc2_schunk_update_chunk(array->sc, nchunk, chunk, false);
      if (brc_ < 0) {
        BLOSC_TRACE_ERROR("Blosc can not update the chunk");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
    }
  }

  free(data);

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_get_slice_cbuffer(const b2nd_array_t *array, const int64_t *start, const int64_t *stop,
                           void *buffer, const int64_t *buffershape, int64_t buffersize) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(start, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(stop, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(buffershape, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(get_set_slice(buffer, buffersize, start, stop, buffershape, (b2nd_array_t *)array, false));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_set_slice_cbuffer(const void *buffer, const int64_t *buffershape, int64_t buffersize,
                           const int64_t *start, const int64_t *stop,
                           b2nd_array_t *array) {
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(start, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(stop, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  BLOSC_ERROR(get_set_slice((void*)buffer, buffersize, start, stop, (int64_t *)buffershape, array, true));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_get_slice(b2nd_context_t *ctx, b2nd_array_t **array, const b2nd_array_t *src, const int64_t *start,
                   const int64_t *stop) {
  BLOSC_ERROR_NULL(src, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(start, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(stop, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  ctx->ndim = src->ndim;
  for (int i = 0; i < src->ndim; ++i) {
    ctx->shape[i] = stop[i] - start[i];
  }

  // Add data
  BLOSC_ERROR(b2nd_empty(ctx, array));

  if ((*array)->nitems == 0) {
    return BLOSC2_ERROR_SUCCESS;
  }

  int8_t ndim = (*array)->ndim;
  int64_t chunks_in_array[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ndim; ++i) {
    chunks_in_array[i] = (*array)->extshape[i] / (*array)->chunkshape[i];
  }
  int64_t nchunks = (*array)->sc->nchunks;
  for (int nchunk = 0; nchunk < nchunks; ++nchunk) {
    int64_t nchunk_ndim[B2ND_MAX_DIM] = {0};
    blosc2_unidim_to_multidim(ndim, chunks_in_array, nchunk, nchunk_ndim);

    // Check if the chunk needs to be updated
    int64_t chunk_start[B2ND_MAX_DIM] = {0};
    int64_t chunk_stop[B2ND_MAX_DIM] = {0};
    int64_t chunk_shape[B2ND_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
      chunk_start[i] = nchunk_ndim[i] * (*array)->chunkshape[i];
      chunk_stop[i] = chunk_start[i] + (*array)->chunkshape[i];
      if (chunk_stop[i] > (*array)->shape[i]) {
        chunk_stop[i] = (*array)->shape[i];
      }
      chunk_shape[i] = chunk_stop[i] - chunk_start[i];
    }

    int64_t src_start[B2ND_MAX_DIM] = {0};
    int64_t src_stop[B2ND_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
      src_start[i] = chunk_start[i] + start[i];
      src_stop[i] = chunk_stop[i] + start[i];
    }
    int64_t buffersize = ctx->b2_storage->cparams->typesize;
    for (int i = 0; i < ndim; ++i) {
      buffersize *= chunk_shape[i];
    }
    uint8_t *buffer = malloc(buffersize);
    BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_MEMORY_ALLOC);
    BLOSC_ERROR(b2nd_get_slice_cbuffer(src, src_start, src_stop, buffer, chunk_shape,
                                       buffersize));
    BLOSC_ERROR(b2nd_set_slice_cbuffer(buffer, chunk_shape, buffersize, chunk_start,
                                       chunk_stop, *array));
    free(buffer);
  }

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_squeeze(b2nd_array_t *array) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  bool index[B2ND_MAX_DIM];

  for (int i = 0; i < array->ndim; ++i) {
    if (array->shape[i] != 1) {
      index[i] = false;
    } else {
      index[i] = true;
    }
  }
  BLOSC_ERROR(b2nd_squeeze_index(array, index));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_squeeze_index(b2nd_array_t *array, const bool *index) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  uint8_t nones = 0;
  int64_t newshape[B2ND_MAX_DIM];
  int32_t newchunkshape[B2ND_MAX_DIM];
  int32_t newblockshape[B2ND_MAX_DIM];

  for (int i = 0; i < array->ndim; ++i) {
    if (index[i] == true) {
      if (array->shape[i] != 1) {
        BLOSC_ERROR(BLOSC2_ERROR_INVALID_INDEX);
      }
    } else {
      newshape[nones] = array->shape[i];
      newchunkshape[nones] = array->chunkshape[i];
      newblockshape[nones] = array->blockshape[i];
      nones += 1;
    }
  }

  for (int i = 0; i < B2ND_MAX_DIM; ++i) {
    if (i < nones) {
      array->chunkshape[i] = newchunkshape[i];
      array->blockshape[i] = newblockshape[i];
    } else {
      array->chunkshape[i] = 1;
      array->blockshape[i] = 1;
    }
  }

  BLOSC_ERROR(update_shape(array, nones, newshape, newchunkshape, newblockshape));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_copy(b2nd_context_t *ctx, const b2nd_array_t *src, b2nd_array_t **array) {
  BLOSC_ERROR_NULL(src, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  ctx->ndim = src->ndim;

  for (int i = 0; i < src->ndim; ++i) {
    ctx->shape[i] = src->shape[i];
  }

  bool equals = true;
  for (int i = 0; i < src->ndim; ++i) {
    if (src->chunkshape[i] != ctx->chunkshape[i]) {
      equals = false;
      break;
    }
    if (src->blockshape[i] != ctx->blockshape[i]) {
      equals = false;
      break;
    }
  }

  if (equals) {
    BLOSC_ERROR(array_without_schunk(ctx, array));

    blosc2_schunk *new_sc = blosc2_schunk_copy(src->sc, ctx->b2_storage);

    if (new_sc == NULL) {
      return BLOSC2_ERROR_FAILURE;
    }
    (*array)->sc = new_sc;

  } else {
    int64_t start[B2ND_MAX_DIM] = {0};

    int64_t stop[B2ND_MAX_DIM];
    for (int i = 0; i < src->ndim; ++i) {
      stop[i] = src->shape[i];
    }
    // Copy metalayers
    b2nd_context_t params_meta;
    memcpy(&params_meta, ctx, sizeof(params_meta));
    int j = 0;

    for (int i = 0; i < src->sc->nmetalayers; ++i) {
      if (strcmp(src->sc->metalayers[i]->name, "b2nd") == 0) {
        continue;
      }
      blosc2_metalayer *meta = &params_meta.metalayers[j];
      meta->name = src->sc->metalayers[i]->name;
      meta->content = src->sc->metalayers[i]->content;
      meta->content_len = src->sc->metalayers[i]->content_len;
      j++;
    }
    params_meta.nmetalayers = j;

    // Copy data
    BLOSC_ERROR(b2nd_get_slice(&params_meta, array, src, start, stop));

    // Copy vlmetayers
    for (int i = 0; i < src->sc->nvlmetalayers; ++i) {
      uint8_t *content;
      int32_t content_len;
      if (blosc2_vlmeta_get(src->sc, src->sc->vlmetalayers[i]->name, &content,
                            &content_len) < 0) {
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      BLOSC_ERROR(blosc2_vlmeta_add((*array)->sc, src->sc->vlmetalayers[i]->name, content, content_len,
                                      (*array)->sc->storage->cparams));
      free(content);
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_save(const b2nd_array_t *array, char *urlpath) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(urlpath, BLOSC2_ERROR_NULL_POINTER);

  b2nd_array_t *tmp;
  blosc2_storage b2_storage = BLOSC2_STORAGE_DEFAULTS;
  b2nd_context_t params = {.b2_storage=&b2_storage};
  b2_storage.urlpath = urlpath;
  b2_storage.contiguous = array->sc->storage->contiguous;

  for (int i = 0; i < array->ndim; ++i) {
    params.chunkshape[i] = array->chunkshape[i];
    params.blockshape[i] = array->blockshape[i];
  }

  BLOSC_ERROR(b2nd_copy(&params, array, &tmp));
  BLOSC_ERROR(b2nd_free(tmp));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_print_meta(const b2nd_array_t *array) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  int8_t ndim;
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
  char *dtype;
  int8_t dtype_format;
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(array->sc, "b2nd", &smeta, &smeta_len) < 0) {
    // Try with a caterva metalayer; we are meant to be backward compatible with it
    if (blosc2_meta_get(array->sc, "caterva", &smeta, &smeta_len) < 0) {
      BLOSC_ERROR(BLOSC2_ERROR_METALAYER_NOT_FOUND);
    }
  }
  BLOSC_ERROR(b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape,
                                    &dtype, &dtype_format));
  free(smeta);

  printf("b2nd metalayer parameters:\n Ndim:       %d", ndim);
  printf("\n shape:      %" PRId64 "", shape[0]);
  for (int i = 1; i < ndim; ++i) {
    printf(", %" PRId64 "", shape[i]);
  }
  printf("\n chunkshape: %d", chunkshape[0]);
  for (int i = 1; i < ndim; ++i) {
    printf(", %d", chunkshape[i]);
  }
  if (dtype != NULL) {
    printf("\n dtype: %s", dtype);
    free(dtype);
  }

  printf("\n blockshape: %d", blockshape[0]);
  for (int i = 1; i < ndim; ++i) {
    printf(", %d", blockshape[i]);
  }
  printf("\n");

  return BLOSC2_ERROR_SUCCESS;
}


int extend_shape(b2nd_array_t *array, const int64_t *new_shape, const int64_t *start) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(new_shape, BLOSC2_ERROR_NULL_POINTER);

  int8_t ndim = array->ndim;
  int64_t diffs_shape[B2ND_MAX_DIM];
  int64_t diffs_sum = 0;
  for (int i = 0; i < ndim; i++) {
    diffs_shape[i] = new_shape[i] - array->shape[i];
    diffs_sum += diffs_shape[i];
    if (diffs_shape[i] < 0) {
      BLOSC_TRACE_ERROR("The new shape must be greater than the old one");
      BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
    }
    if (array->shape[i] == INT64_MAX) {
      BLOSC_TRACE_ERROR("Cannot extend array with shape[%d] = %" PRId64 "d", i, INT64_MAX);
      BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
    }
  }
  if (diffs_sum == 0) {
    // Shapes are equal. Do nothing.
    return BLOSC2_ERROR_SUCCESS;
  }

  int64_t old_nchunks = array->sc->nchunks;
  // aux array to keep old shapes
  b2nd_array_t *aux = malloc(sizeof(b2nd_array_t));
  BLOSC_ERROR_NULL(aux, BLOSC2_ERROR_MEMORY_ALLOC);
  aux->sc = NULL;
  BLOSC_ERROR(update_shape(aux, ndim, array->shape, array->chunkshape, array->blockshape));

  BLOSC_ERROR(update_shape(array, ndim, new_shape, array->chunkshape, array->blockshape));

  int64_t nchunks = array->extnitems / array->chunknitems;
  int64_t nchunks_;
  int64_t nchunk_ndim[B2ND_MAX_DIM];
  blosc2_cparams *cparams;
  BLOSC_ERROR(blosc2_schunk_get_cparams(array->sc, &cparams));
  void *chunk;
  int64_t csize;
  if (nchunks != old_nchunks) {
    if (start == NULL) {
      start = aux->shape;
    }
    int64_t chunks_in_array[B2ND_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
      chunks_in_array[i] = array->extshape[i] / array->chunkshape[i];
    }
    for (int i = 0; i < nchunks; ++i) {
      blosc2_unidim_to_multidim(ndim, chunks_in_array, i, nchunk_ndim);
      for (int j = 0; j < ndim; ++j) {
        if (start[j] <= (array->chunkshape[j] * nchunk_ndim[j])
            && (array->chunkshape[j] * nchunk_ndim[j]) < (start[j] + new_shape[j] - aux->shape[j])) {
          chunk = malloc(BLOSC_EXTENDED_HEADER_LENGTH);
          BLOSC_ERROR_NULL(chunk, BLOSC2_ERROR_MEMORY_ALLOC);
          csize = blosc2_chunk_zeros(*cparams, array->sc->chunksize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
          if (csize < 0) {
            free(aux);
            free(cparams);
            BLOSC_TRACE_ERROR("Blosc error when creating a chunk");
            return BLOSC2_ERROR_FAILURE;
          }
          nchunks_ = blosc2_schunk_insert_chunk(array->sc, i, chunk, false);
          if (nchunks_ < 0) {
            free(aux);
            free(cparams);
            BLOSC_TRACE_ERROR("Blosc error when inserting a chunk");
            return BLOSC2_ERROR_FAILURE;
          }
          break;
        }
      }
    }
  }
  free(aux);
  free(cparams);

  return BLOSC2_ERROR_SUCCESS;
}


int shrink_shape(b2nd_array_t *array, const int64_t *new_shape, const int64_t *start) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(new_shape, BLOSC2_ERROR_NULL_POINTER);

  int8_t ndim = array->ndim;
  int64_t diffs_shape[B2ND_MAX_DIM];
  int64_t diffs_sum = 0;
  for (int i = 0; i < ndim; i++) {
    diffs_shape[i] = new_shape[i] - array->shape[i];
    diffs_sum += diffs_shape[i];
    if (diffs_shape[i] > 0) {
      BLOSC_TRACE_ERROR("The new shape must be smaller than the old one");
      BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
    }
    if (array->shape[i] == 0) {
      continue;
    }
  }
  if (diffs_sum == 0) {
    // Shapes are equal. Do nothing.
    return BLOSC2_ERROR_SUCCESS;
  }

  int64_t old_nchunks = array->sc->nchunks;
  // aux array to keep old shapes
  b2nd_array_t *aux = malloc(sizeof(b2nd_array_t));
  BLOSC_ERROR_NULL(aux, BLOSC2_ERROR_MEMORY_ALLOC);
  aux->sc = NULL;
  BLOSC_ERROR(update_shape(aux, ndim, array->shape, array->chunkshape, array->blockshape));

  BLOSC_ERROR(update_shape(array, ndim, new_shape, array->chunkshape, array->blockshape));

  // Delete chunks if needed
  int64_t chunks_in_array_old[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ndim; ++i) {
    chunks_in_array_old[i] = aux->extshape[i] / aux->chunkshape[i];
  }
  if (start == NULL) {
    start = new_shape;
  }

  int64_t nchunk_ndim[B2ND_MAX_DIM] = {0};
  int64_t nchunks_;
  for (int i = (int) old_nchunks - 1; i >= 0; --i) {
    blosc2_unidim_to_multidim(ndim, chunks_in_array_old, i, nchunk_ndim);
    for (int j = 0; j < ndim; ++j) {
      if (start[j] <= (array->chunkshape[j] * nchunk_ndim[j])
          && (array->chunkshape[j] * nchunk_ndim[j]) < (start[j] + aux->shape[j] - new_shape[j])) {
        nchunks_ = blosc2_schunk_delete_chunk(array->sc, i);
        if (nchunks_ < 0) {
          free(aux);
          BLOSC_TRACE_ERROR("Blosc error when deleting a chunk");
          return BLOSC2_ERROR_FAILURE;
        }
        break;
      }
    }
  }
  free(aux);

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_resize(b2nd_array_t *array, const int64_t *new_shape,
                const int64_t *start) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(new_shape, BLOSC2_ERROR_NULL_POINTER);

  if (start != NULL) {
    for (int i = 0; i < array->ndim; ++i) {
      if (start[i] > array->shape[i]) {
        BLOSC_TRACE_ERROR("`start` must be lower or equal than old array shape in all dims");
        BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
      }
      if ((new_shape[i] > array->shape[i] && start[i] != array->shape[i])
          || (new_shape[i] < array->shape[i]
              && (start[i] + array->shape[i] - new_shape[i]) != array->shape[i])) {
        // Chunks cannot be cut unless they are in the last position
        if (start[i] % array->chunkshape[i] != 0) {
          BLOSC_TRACE_ERROR("If array end is not being modified "
                              "`start` must be a multiple of chunkshape in all dims");
          BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
        }
        if ((new_shape[i] - array->shape[i]) % array->chunkshape[i] != 0) {
          BLOSC_TRACE_ERROR("If array end is not being modified "
                              "`(new_shape - shape)` must be multiple of chunkshape in all dims");
          BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
        }
      }
    }
  }

  // Get shrunk shape
  int64_t shrunk_shape[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < array->ndim; ++i) {
    if (new_shape[i] <= array->shape[i]) {
      shrunk_shape[i] = new_shape[i];
    } else {
      shrunk_shape[i] = array->shape[i];
    }
  }

  BLOSC_ERROR(shrink_shape(array, shrunk_shape, start));
  BLOSC_ERROR(extend_shape(array, new_shape, start));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_insert(b2nd_array_t *array, const void *buffer, int64_t buffersize,
                int8_t axis, int64_t insert_start) {

  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);

  if (axis >= array->ndim) {
    BLOSC_TRACE_ERROR("`axis` cannot be greater than the number of dimensions");
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }

  int64_t axis_size = array->sc->typesize;
  int64_t buffershape[B2ND_MAX_DIM];
  for (int i = 0; i < array->ndim; ++i) {
    if (i != axis) {
      axis_size *= array->shape[i];
      buffershape[i] = array->shape[i];
    }
  }
  if (buffersize % axis_size != 0) {
    BLOSC_TRACE_ERROR("`buffersize` must be multiple of the array");
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }
  int64_t newshape[B2ND_MAX_DIM];
  memcpy(newshape, array->shape, array->ndim * sizeof(int64_t));
  newshape[axis] += buffersize / axis_size;
  buffershape[axis] = newshape[axis] - array->shape[axis];
  int64_t start[B2ND_MAX_DIM] = {0};
  start[axis] = insert_start;

  if (insert_start == array->shape[axis]) {
    BLOSC_ERROR(b2nd_resize(array, newshape, NULL));
  } else {
    BLOSC_ERROR(b2nd_resize(array, newshape, start));
  }

  int64_t stop[B2ND_MAX_DIM];
  memcpy(stop, array->shape, sizeof(int64_t) * array->ndim);
  stop[axis] = start[axis] + buffershape[axis];
  BLOSC_ERROR(b2nd_set_slice_cbuffer(buffer, buffershape, buffersize, start, stop, array));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_append(b2nd_array_t *array, const void *buffer, int64_t buffersize,
                int8_t axis) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(buffer, BLOSC2_ERROR_NULL_POINTER);

  int32_t chunksize = array->sc->chunksize;
  int64_t nchunks_append = buffersize / chunksize;
  // Check whether chunkshape and blockshape are compatible with accelerated path.
  // Essentially, we are checking whether the buffer is a multiple of the chunksize
  // and that the chunkshape and blockshape are the same, except for the first axis.
  // Also, axis needs to be the first one.
  bool compat_chunks_blocks = true;
  for (int i = 1; i < array->ndim; ++i) {
    if (array->chunkshape[i] != array->blockshape[i]) {
      compat_chunks_blocks = false;
      break;
    }
  }
  if (axis > 0) {
    compat_chunks_blocks = false;
  }
  // General case where a buffer has a different size than the chunksize
  if (!compat_chunks_blocks || buffersize % chunksize != 0 || nchunks_append != 1) {
    BLOSC_ERROR(b2nd_insert(array, buffer, buffersize, axis, array->shape[axis]));
    return BLOSC2_ERROR_SUCCESS;
  }

  // Accelerated path for buffers that are of the same size as the chunksize
  // printf("accelerated path\n");

  // Append the buffer to the underlying schunk. This is very fast, as
  // it doesn't need to do internal partitioning.
  BLOSC_ERROR(blosc2_schunk_append_buffer(array->sc, (void*)buffer, buffersize));

  // Finally, resize the array
  int64_t newshape[B2ND_MAX_DIM];
  memcpy(newshape, array->shape, array->ndim * sizeof(int64_t));
  newshape[axis] += nchunks_append * array->chunkshape[axis];
  BLOSC_ERROR(b2nd_resize(array, newshape, NULL));

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_delete(b2nd_array_t *array, const int8_t axis,
                int64_t delete_start, int64_t delete_len) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);

  if (axis >= array->ndim) {
    BLOSC_TRACE_ERROR("axis cannot be greater than the number of dimensions");
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }


  int64_t newshape[B2ND_MAX_DIM];
  memcpy(newshape, array->shape, array->ndim * sizeof(int64_t));
  newshape[axis] -= delete_len;
  int64_t start[B2ND_MAX_DIM] = {0};
  start[axis] = delete_start;

  if (delete_start == (array->shape[axis] - delete_len)) {
    BLOSC_ERROR(b2nd_resize(array, newshape, NULL));
  } else {
    BLOSC_ERROR(b2nd_resize(array, newshape, start));
  }

  return BLOSC2_ERROR_SUCCESS;
}

// Indexing

typedef struct {
    int64_t value;
    int64_t index;
} b2nd_selection_t;


int compare_selection(const void *a, const void *b) {
  int res = (int) (((b2nd_selection_t *) a)->value - ((b2nd_selection_t *) b)->value);
  // In case values are equal, sort by index
  if (res == 0) {
    res = (int) (((b2nd_selection_t *) a)->index - ((b2nd_selection_t *) b)->index);
  }
  return res;
}


int copy_block_buffer_data(b2nd_array_t *array,
                           int8_t ndim,
                           int64_t *block_selection_size,
                           b2nd_selection_t **chunk_selection,
                           b2nd_selection_t **p_block_selection_0,
                           b2nd_selection_t **p_block_selection_1,
                           uint8_t *block,
                           uint8_t *buffer,
                           int64_t *buffershape,
                           int64_t *bufferstrides,
                           bool get) {
  p_block_selection_0[ndim] = chunk_selection[ndim];
  p_block_selection_1[ndim] = chunk_selection[ndim];
  while (p_block_selection_1[ndim] - p_block_selection_0[ndim] < block_selection_size[ndim]) {
    if (ndim == array->ndim - 1) {

      int64_t index_in_block_n[B2ND_MAX_DIM];
      for (int i = 0; i < array->ndim; ++i) {
        index_in_block_n[i] = p_block_selection_1[i]->value % array->chunkshape[i] % array->blockshape[i];
      }
      int64_t index_in_block = 0;
      for (int i = 0; i < array->ndim; ++i) {
        index_in_block += index_in_block_n[i] * array->item_block_strides[i];
      }

      int64_t index_in_buffer_n[B2ND_MAX_DIM];
      for (int i = 0; i < array->ndim; ++i) {
        index_in_buffer_n[i] = p_block_selection_1[i]->index;
      }
      int64_t index_in_buffer = 0;
      for (int i = 0; i < array->ndim; ++i) {
        index_in_buffer += index_in_buffer_n[i] * bufferstrides[i];
      }
      if (get) {
        memcpy(&buffer[index_in_buffer * array->sc->typesize],
               &block[index_in_block * array->sc->typesize],
               array->sc->typesize);
      } else {
        memcpy(&block[index_in_block * array->sc->typesize],
               &buffer[index_in_buffer * array->sc->typesize],
               array->sc->typesize);
      }
    } else {
      BLOSC_ERROR(copy_block_buffer_data(array, (int8_t) (ndim + 1), block_selection_size,
                                         chunk_selection,
                                         p_block_selection_0, p_block_selection_1, block,
                                         buffer, buffershape, bufferstrides, get)
      );
    }
    p_block_selection_1[ndim]++;
  }
  return BLOSC2_ERROR_SUCCESS;
}


int iter_block_copy(b2nd_array_t *array, int8_t ndim,
                    int64_t *chunk_selection_size,
                    b2nd_selection_t **ordered_selection,
                    b2nd_selection_t **chunk_selection_0,
                    b2nd_selection_t **chunk_selection_1,
                    uint8_t *data,
                    uint8_t *buffer,
                    int64_t *buffershape,
                    int64_t *bufferstrides,
                    bool get) {
  chunk_selection_0[ndim] = ordered_selection[ndim];
  chunk_selection_1[ndim] = ordered_selection[ndim];
  while (chunk_selection_1[ndim] - ordered_selection[ndim] < chunk_selection_size[ndim]) {
    int64_t block_index_ndim = ((*chunk_selection_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim];
    while (chunk_selection_1[ndim] - ordered_selection[ndim] < chunk_selection_size[ndim] &&
           block_index_ndim == ((*chunk_selection_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim]) {
      chunk_selection_1[ndim]++;
    }
    if (ndim == array->ndim - 1) {
      int64_t block_chunk_strides[B2ND_MAX_DIM];
      block_chunk_strides[array->ndim - 1] = 1;
      for (int i = array->ndim - 2; i >= 0; --i) {
        block_chunk_strides[i] = block_chunk_strides[i + 1] * (array->extchunkshape[i + 1] / array->blockshape[i + 1]);
      }
      int64_t block_index[B2ND_MAX_DIM];
      for (int i = 0; i < array->ndim; ++i) {
        block_index[i] = ((*chunk_selection_0[i]).value % array->chunkshape[i]) / array->blockshape[i];
      }
      int64_t nblock = 0;
      for (int i = 0; i < array->ndim; ++i) {
        nblock += block_index[i] * block_chunk_strides[i];
      }
      b2nd_selection_t **p_block_selection_0 = malloc(array->ndim * sizeof(b2nd_selection_t *));
      BLOSC_ERROR_NULL(p_block_selection_0, BLOSC2_ERROR_MEMORY_ALLOC);
      b2nd_selection_t **p_block_selection_1 = malloc(array->ndim * sizeof(b2nd_selection_t *));
      BLOSC_ERROR_NULL(p_block_selection_1, BLOSC2_ERROR_MEMORY_ALLOC);
      int64_t *block_selection_size = malloc(array->ndim * sizeof(int64_t));
      BLOSC_ERROR_NULL(block_selection_size, BLOSC2_ERROR_MEMORY_ALLOC);
      for (int i = 0; i < array->ndim; ++i) {
        block_selection_size[i] = chunk_selection_1[i] - chunk_selection_0[i];
      }

      BLOSC_ERROR(copy_block_buffer_data(array,
                                         (int8_t) 0,
                                         block_selection_size,
                                         chunk_selection_0,
                                         p_block_selection_0,
                                         p_block_selection_1,
                                         &data[nblock * array->blocknitems * array->sc->typesize],
                                         buffer,
                                         buffershape,
                                         bufferstrides,
                                         get)
      );
      free(p_block_selection_0);
      free(p_block_selection_1);
      free(block_selection_size);
    } else {
      BLOSC_ERROR(iter_block_copy(array, (int8_t) (ndim + 1), chunk_selection_size,
                                  ordered_selection, chunk_selection_0, chunk_selection_1,
                                  data, buffer, buffershape, bufferstrides, get)
      );
    }
    chunk_selection_0[ndim] = chunk_selection_1[ndim];

  }

  return BLOSC2_ERROR_SUCCESS;
}


int iter_block_maskout(b2nd_array_t *array, int8_t ndim,
                       int64_t *sel_block_size,
                       b2nd_selection_t **o_selection,
                       b2nd_selection_t **p_o_sel_block_0,
                       b2nd_selection_t **p_o_sel_block_1,
                       bool *maskout) {
  p_o_sel_block_0[ndim] = o_selection[ndim];
  p_o_sel_block_1[ndim] = o_selection[ndim];
  while (p_o_sel_block_1[ndim] - o_selection[ndim] < sel_block_size[ndim]) {
    int64_t block_index_ndim = ((*p_o_sel_block_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim];
    while (p_o_sel_block_1[ndim] - o_selection[ndim] < sel_block_size[ndim] &&
           block_index_ndim == ((*p_o_sel_block_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim]) {
      p_o_sel_block_1[ndim]++;
    }
    if (ndim == array->ndim - 1) {
      int64_t block_chunk_strides[B2ND_MAX_DIM];
      block_chunk_strides[array->ndim - 1] = 1;
      for (int i = array->ndim - 2; i >= 0; --i) {
        block_chunk_strides[i] = block_chunk_strides[i + 1] * (array->extchunkshape[i + 1] / array->blockshape[i + 1]);
      }
      int64_t block_index[B2ND_MAX_DIM];
      for (int i = 0; i < array->ndim; ++i) {
        block_index[i] = ((*p_o_sel_block_0[i]).value % array->chunkshape[i]) / array->blockshape[i];
      }
      int64_t nblock = 0;
      for (int i = 0; i < array->ndim; ++i) {
        nblock += block_index[i] * block_chunk_strides[i];
      }
      maskout[nblock] = false;
    } else {
      BLOSC_ERROR(iter_block_maskout(array, (int8_t) (ndim + 1), sel_block_size,
                                     o_selection, p_o_sel_block_0, p_o_sel_block_1,
                                     maskout)
      );
    }
    p_o_sel_block_0[ndim] = p_o_sel_block_1[ndim];

  }

  return BLOSC2_ERROR_SUCCESS;
}


int iter_chunk(b2nd_array_t *array, int8_t ndim,
               int64_t *selection_size,
               b2nd_selection_t **ordered_selection,
               b2nd_selection_t **p_ordered_selection_0,
               b2nd_selection_t **p_ordered_selection_1,
               uint8_t *buffer,
               int64_t *buffershape,
               int64_t *bufferstrides,
               bool get) {
  p_ordered_selection_0[ndim] = ordered_selection[ndim];
  p_ordered_selection_1[ndim] = ordered_selection[ndim];
  while (p_ordered_selection_1[ndim] - ordered_selection[ndim] < selection_size[ndim]) {
    int64_t chunk_index_ndim = (*p_ordered_selection_1[ndim]).value / array->chunkshape[ndim];
    while (p_ordered_selection_1[ndim] - ordered_selection[ndim] < selection_size[ndim] &&
           chunk_index_ndim == (*p_ordered_selection_1[ndim]).value / array->chunkshape[ndim]) {
      p_ordered_selection_1[ndim]++;
    }
    if (ndim == array->ndim - 1) {
      int64_t chunk_array_strides[B2ND_MAX_DIM];
      chunk_array_strides[array->ndim - 1] = 1;
      for (int i = array->ndim - 2; i >= 0; --i) {
        chunk_array_strides[i] = chunk_array_strides[i + 1] *
                                 (array->extshape[i + 1] / array->chunkshape[i + 1]);
      }
      int64_t chunk_index[B2ND_MAX_DIM];
      for (int i = 0; i < array->ndim; ++i) {
        chunk_index[i] = (*p_ordered_selection_0[i]).value / array->chunkshape[i];
      }
      int64_t nchunk = 0;
      for (int i = 0; i < array->ndim; ++i) {
        nchunk += chunk_index[i] * chunk_array_strides[i];
      }

      int64_t nblocks = array->extchunknitems / array->blocknitems;
      b2nd_selection_t **p_chunk_selection_0 = malloc(array->ndim * sizeof(b2nd_selection_t *));
      BLOSC_ERROR_NULL(p_chunk_selection_0, BLOSC2_ERROR_MEMORY_ALLOC);
      b2nd_selection_t **p_chunk_selection_1 = malloc(array->ndim * sizeof(b2nd_selection_t *));
      BLOSC_ERROR_NULL(p_chunk_selection_1, BLOSC2_ERROR_MEMORY_ALLOC);
      int64_t *chunk_selection_size = malloc(array->ndim * sizeof(int64_t));
      BLOSC_ERROR_NULL(chunk_selection_size, BLOSC2_ERROR_MEMORY_ALLOC);
      for (int i = 0; i < array->ndim; ++i) {
        chunk_selection_size[i] = p_ordered_selection_1[i] - p_ordered_selection_0[i];
      }

      if (get) {
        bool *maskout = calloc(nblocks, sizeof(bool));
        for (int i = 0; i < nblocks; ++i) {
          maskout[i] = true;
        }

        BLOSC_ERROR(iter_block_maskout(array, (int8_t) 0,
                                       chunk_selection_size,
                                       p_ordered_selection_0,
                                       p_chunk_selection_0,
                                       p_chunk_selection_1,
                                       maskout));

        if (blosc2_set_maskout(array->sc->dctx, maskout, (int) nblocks) !=
            BLOSC2_ERROR_SUCCESS) {
          BLOSC_TRACE_ERROR("Error setting the maskout");
          BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
        }
        free(maskout);
      }
      int data_nitems = (int) array->extchunknitems;
      int data_nbytes = data_nitems * array->sc->typesize;
      uint8_t *data = malloc(data_nitems * array->sc->typesize);
      BLOSC_ERROR_NULL(data, BLOSC2_ERROR_MEMORY_ALLOC);
      int err = blosc2_schunk_decompress_chunk(array->sc, nchunk, data, data_nbytes);
      if (err < 0) {
        BLOSC_TRACE_ERROR("Error decompressing chunk");
        BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
      }
      BLOSC_ERROR(iter_block_copy(array, 0, chunk_selection_size,
                                  p_ordered_selection_0, p_chunk_selection_0, p_chunk_selection_1,
                                  data, buffer, buffershape, bufferstrides, get));

      if (!get) {
        int32_t chunk_size = data_nbytes + BLOSC_EXTENDED_HEADER_LENGTH;
        uint8_t *chunk = malloc(chunk_size);
        BLOSC_ERROR_NULL(chunk, BLOSC2_ERROR_MEMORY_ALLOC);
        err = blosc2_compress_ctx(array->sc->cctx, data, data_nbytes, chunk, chunk_size);
        if (err < 0) {
          BLOSC_TRACE_ERROR("Error compressing data");
          BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
        }
        err = (int) blosc2_schunk_update_chunk(array->sc, nchunk, chunk, false);
        if (err < 0) {
          BLOSC_TRACE_ERROR("Error updating chunk");
          BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
        }
      }
      free(data);
      free(chunk_selection_size);
      free(p_chunk_selection_0);
      free(p_chunk_selection_1);
    } else {
      BLOSC_ERROR(iter_chunk(array, (int8_t) (ndim + 1), selection_size,
                             ordered_selection, p_ordered_selection_0, p_ordered_selection_1,
                             buffer, buffershape, bufferstrides, get));
    }

    p_ordered_selection_0[ndim] = p_ordered_selection_1[ndim];
  }
  return BLOSC2_ERROR_SUCCESS;
}


int orthogonal_selection(b2nd_array_t *array, int64_t **selection, int64_t *selection_size, void *buffer,
                         int64_t *buffershape, int64_t buffersize, bool get) {
  BLOSC_ERROR_NULL(array, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(selection, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(selection_size, BLOSC2_ERROR_NULL_POINTER);

  int8_t ndim = array->ndim;

  for (int i = 0; i < ndim; ++i) {
    BLOSC_ERROR_NULL(selection[i], BLOSC2_ERROR_NULL_POINTER);
    // Check that indexes are not larger than array shape
    for (int j = 0; j < selection_size[i]; ++j) {
      if (selection[i][j] > array->shape[i]) {
        BLOSC_ERROR(BLOSC2_ERROR_INVALID_INDEX);
      }
    }
  }

  // Check buffer size
  int64_t sel_size = array->sc->typesize;
  for (int i = 0; i < ndim; ++i) {
    sel_size *= selection_size[i];
  }

  if (sel_size < buffersize) {
    BLOSC_ERROR(BLOSC2_ERROR_INVALID_PARAM);
  }

  // Sort selections
  b2nd_selection_t **ordered_selection = malloc(ndim * sizeof(b2nd_selection_t *));
  BLOSC_ERROR_NULL(ordered_selection, BLOSC2_ERROR_MEMORY_ALLOC);
  for (int i = 0; i < ndim; ++i) {
    ordered_selection[i] = malloc(selection_size[i] * sizeof(b2nd_selection_t));
    for (int j = 0; j < selection_size[i]; ++j) {
      ordered_selection[i][j].index = j;
      ordered_selection[i][j].value = selection[i][j];
    }
    qsort(ordered_selection[i], selection_size[i], sizeof(b2nd_selection_t), compare_selection);
  }

  // Define pointers to iterate over ordered_selection data
  b2nd_selection_t **p_ordered_selection_0 = malloc(ndim * sizeof(b2nd_selection_t *));
  BLOSC_ERROR_NULL(p_ordered_selection_0, BLOSC2_ERROR_MEMORY_ALLOC);
  b2nd_selection_t **p_ordered_selection_1 = malloc(ndim * sizeof(b2nd_selection_t *));
  BLOSC_ERROR_NULL(p_ordered_selection_1, BLOSC2_ERROR_MEMORY_ALLOC);

  int64_t bufferstrides[B2ND_MAX_DIM];
  bufferstrides[array->ndim - 1] = 1;
  for (int i = array->ndim - 2; i >= 0; --i) {
    bufferstrides[i] = bufferstrides[i + 1] * buffershape[i + 1];
  }

  BLOSC_ERROR(iter_chunk(array, 0,
                         selection_size, ordered_selection,
                         p_ordered_selection_0,
                         p_ordered_selection_1,
                         buffer, buffershape, bufferstrides, get));

  // Free allocated memory
  free(p_ordered_selection_0);
  free(p_ordered_selection_1);
  for (int i = 0; i < ndim; ++i) {
    free(ordered_selection[i]);
  }
  free(ordered_selection);

  return BLOSC2_ERROR_SUCCESS;
}


int b2nd_get_orthogonal_selection(const b2nd_array_t *array, int64_t **selection, int64_t *selection_size, void *buffer,
                                  int64_t *buffershape, int64_t buffersize) {
  return orthogonal_selection((b2nd_array_t *)array, selection, selection_size, buffer, buffershape, buffersize, true);
}


int b2nd_set_orthogonal_selection(b2nd_array_t *array, int64_t **selection, int64_t *selection_size, const void *buffer,
                                  int64_t *buffershape, int64_t buffersize) {
  return orthogonal_selection(array, selection, selection_size, (void*)buffer, buffershape, buffersize, false);
}


b2nd_context_t *
b2nd_create_ctx(const blosc2_storage *b2_storage, int8_t ndim, const int64_t *shape, const int32_t *chunkshape,
                const int32_t *blockshape, const char *dtype, int8_t dtype_format, const blosc2_metalayer *metalayers,
                int32_t nmetalayers) {
  b2nd_context_t *ctx = malloc(sizeof(b2nd_context_t));
  BLOSC_ERROR_NULL(ctx, NULL);
  blosc2_storage *params_b2_storage = malloc(sizeof(blosc2_storage));
  BLOSC_ERROR_NULL(params_b2_storage, NULL);
  if (b2_storage == NULL) {
    memcpy(params_b2_storage, &BLOSC2_STORAGE_DEFAULTS, sizeof(blosc2_storage));
  }
  else {
    memcpy(params_b2_storage, b2_storage, sizeof(blosc2_storage));
  }
  blosc2_cparams *cparams = malloc(sizeof(blosc2_cparams));
  BLOSC_ERROR_NULL(cparams, NULL);
  // We need a copy of cparams mainly to be able to modify blocksize
  if (b2_storage->cparams == NULL) {
    memcpy(cparams, &BLOSC2_CPARAMS_DEFAULTS, sizeof(blosc2_cparams));
  }
  else {
    memcpy(cparams, b2_storage->cparams, sizeof(blosc2_cparams));
  }

  if (dtype == NULL) {
    ctx->dtype = strdup(B2ND_DEFAULT_DTYPE);
    ctx->dtype_format = 0;  // The default is NumPy format
  }
  else {
    ctx->dtype = strdup(dtype);
    ctx->dtype_format = dtype_format;
  }

  params_b2_storage->cparams = cparams;
  ctx->b2_storage = params_b2_storage;
  ctx->ndim = ndim;
  int32_t blocknitems = 1;
  for (int i = 0; i < ndim; i++) {
    ctx->shape[i] = shape[i];
    ctx->chunkshape[i] = chunkshape[i];
    ctx->blockshape[i] = blockshape[i];
    blocknitems *= ctx->blockshape[i];
  }
  cparams->blocksize = blocknitems * cparams->typesize;

  ctx->nmetalayers = nmetalayers;
  for (int i = 0; i < nmetalayers; ++i) {
    ctx->metalayers[i] = metalayers[i];
  }

#if defined(HAVE_PLUGINS)
  #include "blosc2/codecs-registry.h"
  if ((ctx->b2_storage->cparams->compcode >= BLOSC_CODEC_ZFP_FIXED_ACCURACY) &&
      (ctx->b2_storage->cparams->compcode <= BLOSC_CODEC_ZFP_FIXED_RATE)) {
    for (int i = 0; i < BLOSC2_MAX_FILTERS; ++i) {
      if ((ctx->b2_storage->cparams->filters[i] == BLOSC_SHUFFLE) ||
          (ctx->b2_storage->cparams->filters[i] == BLOSC_BITSHUFFLE)) {
        BLOSC_TRACE_ERROR("ZFP cannot be run in presence of SHUFFLE / BITSHUFFLE");
        return NULL;
      }
    }
  }
#endif /* HAVE_PLUGINS */

  return ctx;
}


int b2nd_free_ctx(b2nd_context_t *ctx) {
  ctx->b2_storage->cparams->schunk = NULL;
  free(ctx->b2_storage->cparams);
  free(ctx->b2_storage);
  free(ctx->dtype);
  free(ctx);

  return BLOSC2_ERROR_SUCCESS;
}

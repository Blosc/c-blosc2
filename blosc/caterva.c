/*
 * Copyright (C) 2018 Francesc Alted, Aleix Alcacer.
 * Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <caterva.h>

#include "caterva_utils.h"
#include "blosc2.h"
#include <inttypes.h>


int caterva_ctx_new(caterva_config_t *cfg, caterva_ctx_t **ctx) {
    CATERVA_ERROR_NULL(cfg);
    CATERVA_ERROR_NULL(ctx);

    (*ctx) = (caterva_ctx_t *) cfg->alloc(sizeof(caterva_ctx_t));
    CATERVA_ERROR_NULL(ctx);
    if (!(*ctx)) {
        CATERVA_TRACE_ERROR("Allocation fails");
        return CATERVA_ERR_NULL_POINTER;
    }

    (*ctx)->cfg = (caterva_config_t *) cfg->alloc(sizeof(caterva_config_t));
    CATERVA_ERROR_NULL((*ctx)->cfg);
    if (!(*ctx)->cfg) {
        CATERVA_TRACE_ERROR("Allocation fails");
        return CATERVA_ERR_NULL_POINTER;
    }
    memcpy((*ctx)->cfg, cfg, sizeof(caterva_config_t));

    return CATERVA_SUCCEED;
}

int caterva_ctx_free(caterva_ctx_t **ctx) {
    CATERVA_ERROR_NULL(ctx);

    void (*auxfree)(void *) = (*ctx)->cfg->free;
    auxfree((*ctx)->cfg);
    auxfree(*ctx);

    return CATERVA_SUCCEED;
}

// Only for internal use
int caterva_update_shape(caterva_array_t *array, int8_t ndim, const int64_t *shape,
                               const int32_t *chunkshape, const int32_t *blockshape) {
    array->ndim = ndim;
    array->nitems = 1;
    array->extnitems = 1;
    array->extchunknitems = 1;
    array->chunknitems = 1;
    array->blocknitems = 1;
    for (int i = 0; i < CATERVA_MAX_DIM; ++i) {
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
                array->extchunkshape[i] = 0;
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
                caterva_serialize_meta(array->ndim, array->shape, array->chunkshape, array->blockshape,
                                       &smeta);
        if (smeta_len < 0) {
            fprintf(stderr, "error during serializing dims info for Caterva");
            return -1;
        }
        // ... and update it in its metalayer
        if (blosc2_meta_exists(array->sc, "caterva") < 0) {
            if (blosc2_meta_add(array->sc, "caterva", smeta, smeta_len) < 0) {
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
        } else {
            if (blosc2_meta_update(array->sc, "caterva", smeta, smeta_len) < 0) {
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
        }
        free(smeta);
    }

    return CATERVA_SUCCEED;
}

// Only for internal use
int caterva_array_without_schunk(caterva_ctx_t *ctx, caterva_params_t *params,
                                       caterva_storage_t *storage, caterva_array_t **array) {
    /* Create a caterva_array_t buffer */
    (*array) = (caterva_array_t *) ctx->cfg->alloc(sizeof(caterva_array_t));
    CATERVA_ERROR_NULL(*array);

    (*array)->cfg = (caterva_config_t *) ctx->cfg->alloc(sizeof(caterva_config_t));
    memcpy((*array)->cfg, ctx->cfg, sizeof(caterva_config_t));

    (*array)->sc = NULL;

    (*array)->ndim = params->ndim;
    (*array)->itemsize = params->itemsize;

    int64_t *shape = params->shape;
    int32_t *chunkshape = storage->chunkshape;
    int32_t *blockshape = storage->blockshape;

    caterva_update_shape(*array, params->ndim, shape, chunkshape, blockshape);

    // The partition cache (empty initially)
    (*array)->chunk_cache.data = NULL;
    (*array)->chunk_cache.nchunk = -1;  // means no valid cache yet

    if ((*array)->nitems != 0) {
        (*array)->nchunks = (*array)->extnitems / (*array)->chunknitems;
    } else {
        (*array)->nchunks = 0;
    }

    return CATERVA_SUCCEED;
}

// Only for internal use
int caterva_blosc_array_new(caterva_ctx_t *ctx, caterva_params_t *params,
                            caterva_storage_t *storage,
                            int special_value, caterva_array_t **array) {
    CATERVA_ERROR(caterva_array_without_schunk(ctx, params, storage, array));
    blosc2_storage b_storage;
    blosc2_cparams b_cparams;
    blosc2_dparams b_dparams;
    CATERVA_ERROR(create_blosc_params(ctx, params, storage, &b_cparams, &b_dparams, &b_storage));

    blosc2_schunk *sc = blosc2_schunk_new(&b_storage);
    if (sc == NULL) {
        CATERVA_TRACE_ERROR("Pointer is NULL");
        return CATERVA_ERR_BLOSC_FAILED;
    }

    // Serialize the dimension info
    if (sc->nmetalayers >= BLOSC2_MAX_METALAYERS) {
        CATERVA_TRACE_ERROR("the number of metalayers for this schunk has been exceeded");
        return CATERVA_ERR_BLOSC_FAILED;
    }
    uint8_t *smeta = NULL;
    int32_t smeta_len = caterva_serialize_meta(params->ndim,
                                               (*array)->shape,
                                               (*array)->chunkshape,
                                               (*array)->blockshape, &smeta);
    if (smeta_len < 0) {
        CATERVA_TRACE_ERROR("error during serializing dims info for Caterva");
        return CATERVA_ERR_BLOSC_FAILED;
    }

    // And store it in caterva metalayer
    if (blosc2_meta_add(sc, "caterva", smeta, smeta_len) < 0) {
        return CATERVA_ERR_BLOSC_FAILED;
    }

    free(smeta);

    for (int i = 0; i < storage->nmetalayers; ++i) {
        char *name = storage->metalayers[i].name;
        uint8_t *data = storage->metalayers[i].sdata;
        int32_t size = storage->metalayers[i].size;
        if (blosc2_meta_add(sc, name, data, size) < 0) {
            CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
        }
    }

    // Fill schunk with uninit values
    if ((*array)->nitems != 0) {
        int32_t chunksize = (int32_t) (*array)->extchunknitems * (*array)->itemsize;
        int64_t nchunks = (*array)->extnitems / (*array)->chunknitems;
        int64_t nitems = nchunks * (*array)->extchunknitems;
        // blosc2_schunk_fill_special(sc, nitems, BLOSC2_SPECIAL_ZERO, chunksize);
        blosc2_schunk_fill_special(sc, nitems, special_value, chunksize);
    }
    (*array)->sc = sc;
    (*array)->nchunks = sc->nchunks;

    return CATERVA_SUCCEED;
}

int caterva_uninit(caterva_ctx_t *ctx, caterva_params_t *params,
                  caterva_storage_t *storage, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(params);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(array);

    CATERVA_ERROR(caterva_blosc_array_new(ctx, params, storage, BLOSC2_SPECIAL_UNINIT, array));

    return CATERVA_SUCCEED;
}

int caterva_empty(caterva_ctx_t *ctx, caterva_params_t *params,
                  caterva_storage_t *storage, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(params);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(array);

    // CATERVA_ERROR(caterva_blosc_array_new(ctx, params, storage, BLOSC2_SPECIAL_UNINIT, array));
    // Avoid variable cratios
    CATERVA_ERROR(caterva_blosc_array_new(ctx, params, storage, BLOSC2_SPECIAL_ZERO, array));

    return CATERVA_SUCCEED;
}

int caterva_zeros(caterva_ctx_t *ctx, caterva_params_t *params,
                  caterva_storage_t *storage, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(params);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(array);

    CATERVA_ERROR(caterva_blosc_array_new(ctx, params, storage, BLOSC2_SPECIAL_ZERO, array));

    return CATERVA_SUCCEED;
}

int caterva_full(caterva_ctx_t *ctx, caterva_params_t *params,
                 caterva_storage_t *storage, void *fill_value, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(params);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(array);

    CATERVA_ERROR(caterva_empty(ctx, params, storage, array));

    int32_t chunkbytes = (int32_t) (*array)->extchunknitems * (*array)->itemsize;

    blosc2_cparams *cparams;
    if (blosc2_schunk_get_cparams((*array)->sc, &cparams) != 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }

    int32_t chunksize = BLOSC_EXTENDED_HEADER_LENGTH + (*array)->itemsize;
    uint8_t *chunk = malloc(chunksize);
    if (blosc2_chunk_repeatval(*cparams, chunkbytes, chunk, chunksize, fill_value) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }
    free(cparams);

    for (int i = 0; i < (*array)->sc->nchunks; ++i) {
        if (blosc2_schunk_update_chunk((*array)->sc, i, chunk, true) < 0) {
            CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
        }
    }
    free(chunk);

    return CATERVA_SUCCEED;
}

int caterva_from_schunk(caterva_ctx_t *ctx, blosc2_schunk *schunk, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(schunk);
    CATERVA_ERROR_NULL(array);

    if (ctx == NULL) {
        CATERVA_TRACE_ERROR("Context is null");
        return CATERVA_ERR_NULL_POINTER;
    }
    if (schunk == NULL) {
        CATERVA_TRACE_ERROR("Schunk is null");
        return CATERVA_ERR_NULL_POINTER;
    }

    blosc2_cparams *cparams;
    if (blosc2_schunk_get_cparams(schunk, &cparams) < 0) {
        CATERVA_TRACE_ERROR("Blosc error");
        return CATERVA_ERR_NULL_POINTER;
    }
    uint8_t itemsize = (int8_t) cparams->typesize;
    free(cparams);

    caterva_params_t params = {0};
    params.itemsize = itemsize;
    caterva_storage_t storage = {0};
    storage.urlpath = schunk->storage->urlpath;
    storage.contiguous = schunk->storage->contiguous;

    // Deserialize the caterva metalayer
    uint8_t *smeta;
    int32_t smeta_len;
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
        CATERVA_TRACE_ERROR("Blosc error");
        return CATERVA_ERR_BLOSC_FAILED;
    }
    caterva_deserialize_meta(smeta, smeta_len, &params.ndim,
                             params.shape,
                             storage.chunkshape,
                             storage.blockshape);
    free(smeta);

    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    caterva_config_from_schunk(ctx, schunk, &cfg);

    caterva_ctx_t *ctx_sc;
    caterva_ctx_new(&cfg, &ctx_sc);

    caterva_array_without_schunk(ctx_sc, &params, &storage, array);

    caterva_ctx_free(&ctx_sc);

    (*array)->sc = schunk;

    if ((*array) == NULL) {
        CATERVA_TRACE_ERROR("Error creating a caterva container from a frame");
        return CATERVA_ERR_NULL_POINTER;
    }

    return CATERVA_SUCCEED;
}

int
caterva_to_cframe(caterva_ctx_t *ctx, caterva_array_t *array, uint8_t **cframe, int64_t *cframe_len,
                  bool *needs_free) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(cframe);
    CATERVA_ERROR_NULL(cframe_len);
    CATERVA_ERROR_NULL(needs_free);

    *cframe_len = blosc2_schunk_to_buffer(array->sc, cframe, needs_free);
    if (*cframe_len <= 0) {
        CATERVA_TRACE_ERROR("Error serializing the caterva array");
        return CATERVA_ERR_BLOSC_FAILED;
    }
    return CATERVA_SUCCEED;
}

int caterva_from_cframe(caterva_ctx_t *ctx, uint8_t *cframe, int64_t cframe_len, bool copy,
                        caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(cframe);
    CATERVA_ERROR_NULL(array);

    blosc2_schunk *sc = blosc2_schunk_from_buffer(cframe, cframe_len, copy);
    if (sc == NULL) {
        CATERVA_TRACE_ERROR("Blosc error");
        return CATERVA_ERR_BLOSC_FAILED;
    }
    // ...and create a caterva array out of it
    CATERVA_ERROR(caterva_from_schunk(ctx, sc, array));

    return CATERVA_SUCCEED;
}

int caterva_open(caterva_ctx_t *ctx, const char *urlpath, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(urlpath);
    CATERVA_ERROR_NULL(array);

    blosc2_schunk *sc = blosc2_schunk_open(urlpath);

    // ...and create a caterva array out of it
    CATERVA_ERROR(caterva_from_schunk(ctx, sc, array));

    return CATERVA_SUCCEED;
}

int caterva_free(caterva_ctx_t *ctx, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    void (*free)(void *) = (*array)->cfg->free;

    free((*array)->cfg);
    if (*array) {
        if ((*array)->sc != NULL) {
            blosc2_schunk_free((*array)->sc);
        }
        free(*array);
    }
    return CATERVA_SUCCEED;
}

int caterva_from_buffer(caterva_ctx_t *ctx, void *buffer, int64_t buffersize,
                        caterva_params_t *params, caterva_storage_t *storage,
                        caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(params);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(buffer);
    CATERVA_ERROR_NULL(array);

    CATERVA_ERROR(caterva_empty(ctx, params, storage, array));

    if (buffersize < (int64_t)(*array)->nitems * (*array)->itemsize) {
        CATERVA_TRACE_ERROR("The buffersize (%lld) is smaller than the array size (%lld)",
                            (long long)buffersize, (long long)(*array)->nitems * (*array)->itemsize);
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    if ((*array)->nitems == 0) {
        return CATERVA_SUCCEED;
    }

    int64_t start[CATERVA_MAX_DIM] = {0};
    int64_t *stop = (*array)->shape;
    int64_t *shape = (*array)->shape;
    CATERVA_ERROR(caterva_set_slice_buffer(ctx, buffer, shape, buffersize, start, stop, *array));

    return CATERVA_SUCCEED;
}

int caterva_to_buffer(caterva_ctx_t *ctx, caterva_array_t *array, void *buffer,
                      int64_t buffersize) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(buffer);

    if (buffersize < (int64_t) array->nitems * array->itemsize) {
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    if (array->nitems == 0) {
        return CATERVA_SUCCEED;
    }

    int64_t start[CATERVA_MAX_DIM] = {0};
    int64_t *stop = array->shape;
    CATERVA_ERROR(caterva_get_slice_buffer(ctx, array, start, stop,
                                           buffer, array->shape, buffersize));
    return CATERVA_SUCCEED;
}


// Only for internal use: It is used for setting slices and for getting slices.
int caterva_blosc_slice(caterva_ctx_t *ctx, void *buffer,
                        int64_t buffersize, int64_t *start, int64_t *stop, int64_t *shape,
                        caterva_array_t *array, bool set_slice) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(buffer);
    CATERVA_ERROR_NULL(start);
    CATERVA_ERROR_NULL(stop);
    CATERVA_ERROR_NULL(array);
    if (buffersize < 0) {
        CATERVA_TRACE_ERROR("buffersize is < 0");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    uint8_t *buffer_b = (uint8_t *) buffer;
    int64_t *buffer_start = start;
    int64_t *buffer_stop = stop;
    int64_t *buffer_shape = shape;

    int8_t ndim = array->ndim;

    // 0-dim case
    if (ndim == 0) {
        if (set_slice) {
            int32_t chunk_size = array->itemsize + BLOSC2_MAX_OVERHEAD;
            uint8_t *chunk = malloc(chunk_size);
            if (blosc2_compress_ctx(array->sc->cctx, buffer_b, array->itemsize, chunk, chunk_size) < 0) {
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
            if (blosc2_schunk_update_chunk(array->sc, 0, chunk, false) < 0) {
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }

        } else {
            if (blosc2_schunk_decompress_chunk(array->sc, 0, buffer_b, array->itemsize) < 0) {
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
        }
        return CATERVA_SUCCEED;
    }

    int32_t data_nbytes = (int32_t) array->extchunknitems * array->itemsize;
    uint8_t *data = malloc(data_nbytes);

    int64_t chunks_in_array[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
        chunks_in_array[i] = array->extshape[i] / array->chunkshape[i];
    }

    int64_t chunks_in_array_strides[CATERVA_MAX_DIM];
    chunks_in_array_strides[ndim - 1] = 1;
    for (int i = ndim - 2; i >= 0; --i) {
        chunks_in_array_strides[i] = chunks_in_array_strides[i + 1] * chunks_in_array[i + 1];
    }

    int64_t blocks_in_chunk[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
        blocks_in_chunk[i] = array->extchunkshape[i] / array->blockshape[i];
    }

    // Compute the number of chunks to update
    int64_t update_start[CATERVA_MAX_DIM];
    int64_t update_shape[CATERVA_MAX_DIM];

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
        int64_t nchunk_ndim[CATERVA_MAX_DIM] = {0};
        blosc2_unidim_to_multidim(ndim, update_shape, update_nchunk, nchunk_ndim);
        for (int i = 0; i < ndim; ++i) {
            nchunk_ndim[i] += update_start[i];
        }
        int64_t nchunk;
        blosc2_multidim_to_unidim(nchunk_ndim, ndim, chunks_in_array_strides, &nchunk);

        // check if the chunk needs to be updated
        int64_t chunk_start[CATERVA_MAX_DIM] = {0};
        int64_t chunk_stop[CATERVA_MAX_DIM] = {0};
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

        int32_t nblocks = (int32_t)array->extchunknitems / array->blocknitems;



        if (set_slice) {
            // Check if all the chunk is going to be updated and avoid the decompression
            bool decompress_chunk = false;
            for (int i = 0; i < ndim; ++i) {
                decompress_chunk |= (chunk_start[i] < buffer_start[i] || chunk_stop[i] > buffer_stop[i]);
            }

            if (decompress_chunk) {
                int err = blosc2_schunk_decompress_chunk(array->sc, nchunk, data, data_nbytes);
                if (err < 0) {
                    CATERVA_TRACE_ERROR("Error decompressing chunk");
                    CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
                }
            } else {
                // Avoid writing non zero padding from previous chunk
                memset(data, 0, data_nbytes);
            }
        } else {
            bool *block_maskout = ctx->cfg->alloc(nblocks);
            CATERVA_ERROR_NULL(block_maskout);
            for (int nblock = 0; nblock < nblocks; ++nblock) {
                int64_t nblock_ndim[CATERVA_MAX_DIM] = {0};
                blosc2_unidim_to_multidim(ndim, blocks_in_chunk, nblock, nblock_ndim);

                // check if the block needs to be updated
                int64_t block_start[CATERVA_MAX_DIM] = {0};
                int64_t block_stop[CATERVA_MAX_DIM] = {0};
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
                CATERVA_TRACE_ERROR("Error setting the maskout");
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }

            int err = blosc2_schunk_decompress_chunk(array->sc, nchunk, data, data_nbytes);
            if (err < 0) {
                CATERVA_TRACE_ERROR("Error decompressing chunk");
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }

            ctx->cfg->free(block_maskout);
        }

        // Iterate over blocks

        for (int nblock = 0; nblock < nblocks; ++nblock) {
            int64_t nblock_ndim[CATERVA_MAX_DIM] = {0};
            blosc2_unidim_to_multidim(ndim, blocks_in_chunk, nblock, nblock_ndim);

            // check if the block needs to be updated
            int64_t block_start[CATERVA_MAX_DIM] = {0};
            int64_t block_stop[CATERVA_MAX_DIM] = {0};
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
            int64_t block_shape[CATERVA_MAX_DIM] = {0};
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
            int64_t slice_start[CATERVA_MAX_DIM] = {0};
            for (int i = 0; i < ndim; ++i) {
                if (block_start[i] < buffer_start[i]) {
                    slice_start[i] = buffer_start[i] - block_start[i];
                } else {
                    slice_start[i] = 0;
                }
                slice_start[i] += block_start[i];
            }

            int64_t slice_stop[CATERVA_MAX_DIM] = {0};
            for (int i = 0; i < ndim; ++i) {
                if (block_stop[i] > buffer_stop[i]) {
                    slice_stop[i] = block_shape[i] - (block_stop[i] - buffer_stop[i]);
                } else {
                    slice_stop[i] = block_stop[i] - block_start[i];
                }
                slice_stop[i] += block_start[i];
            }

            int64_t slice_shape[CATERVA_MAX_DIM] = {0};
            for (int i = 0; i < ndim; ++i) {
                slice_shape[i] = slice_stop[i] - slice_start[i];
            }


            uint8_t *src = &buffer_b[0];
            int64_t *src_pad_shape = buffer_shape;

            int64_t src_start[CATERVA_MAX_DIM] = {0};
            int64_t src_stop[CATERVA_MAX_DIM] = {0};
            for (int i = 0; i < ndim; ++i) {
                src_start[i] = slice_start[i] - buffer_start[i];
                src_stop[i] = slice_stop[i] - buffer_start[i];
            }

            uint8_t *dst = &data[nblock * array->blocknitems * array->itemsize];
            int64_t dst_pad_shape[CATERVA_MAX_DIM];
            for (int i = 0; i < ndim; ++i) {
                dst_pad_shape[i] = array->blockshape[i];
            }

            int64_t dst_start[CATERVA_MAX_DIM] = {0};
            int64_t dst_stop[CATERVA_MAX_DIM] = {0};
            for (int i = 0; i < ndim; ++i) {
                dst_start[i] = slice_start[i] - block_start[i];
                dst_stop[i] = dst_start[i] + slice_shape[i];
            }

            if (set_slice) {
                caterva_copy_buffer(ndim, array->itemsize,
                                    src, src_pad_shape, src_start, src_stop,
                                    dst, dst_pad_shape, dst_start);
            } else {
                caterva_copy_buffer(ndim, array->itemsize,
                                    dst, dst_pad_shape, dst_start, dst_stop,
                                    src, src_pad_shape, src_start);
            }
        }

        if (set_slice) {
            // Recompress the data
            int32_t chunk_nbytes = data_nbytes + BLOSC2_MAX_OVERHEAD;
            uint8_t *chunk = malloc(chunk_nbytes);
            int brc;
            brc = blosc2_compress_ctx(array->sc->cctx, data, data_nbytes, chunk, chunk_nbytes);
            if (brc < 0) {
                CATERVA_TRACE_ERROR("Blosc can not compress the data");
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
            int64_t brc_ = blosc2_schunk_update_chunk(array->sc, nchunk, chunk, false);
            if (brc_ < 0) {
                CATERVA_TRACE_ERROR("Blosc can not update the chunk");
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
        }
    }

    free(data);


    return CATERVA_SUCCEED;
}

int caterva_get_slice_buffer(caterva_ctx_t *ctx,
                             caterva_array_t *array,
                             int64_t *start, int64_t *stop,
                             void *buffer, int64_t *buffershape, int64_t buffersize) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(start);
    CATERVA_ERROR_NULL(stop);
    CATERVA_ERROR_NULL(buffershape);
    CATERVA_ERROR_NULL(buffer);

    int64_t size = array->itemsize;
    for (int i = 0; i < array->ndim; ++i) {
        if (stop[i] - start[i] > buffershape[i]) {
            CATERVA_TRACE_ERROR("The buffer shape can not be smaller than the slice shape");
            return CATERVA_ERR_INVALID_ARGUMENT;
        }
        size *= buffershape[i];
    }

    if (array->nitems == 0) {
        return CATERVA_SUCCEED;
    }

    if (buffersize < size) {
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }
    CATERVA_ERROR(caterva_blosc_slice(ctx, buffer, buffersize, start, stop, buffershape, array, false));

    return CATERVA_SUCCEED;
}

int caterva_set_slice_buffer(caterva_ctx_t *ctx,
                             void *buffer, int64_t *buffershape, int64_t buffersize,
                             int64_t *start, int64_t *stop,
                             caterva_array_t *array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(buffer);
    CATERVA_ERROR_NULL(start);
    CATERVA_ERROR_NULL(stop);
    CATERVA_ERROR_NULL(array);

    int64_t size = array->itemsize;
    for (int i = 0; i < array->ndim; ++i) {
        size *= stop[i] - start[i];
    }

    if (buffersize < size) {
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    if (array->nitems == 0) {
        return CATERVA_SUCCEED;
    }

    CATERVA_ERROR(caterva_blosc_slice(ctx, buffer, buffersize, start, stop, buffershape, array, true));

    return CATERVA_SUCCEED;
}

int caterva_get_slice(caterva_ctx_t *ctx, caterva_array_t *src, const int64_t *start,
                      const int64_t *stop, caterva_storage_t *storage, caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(src);
    CATERVA_ERROR_NULL(start);
    CATERVA_ERROR_NULL(stop);
    CATERVA_ERROR_NULL(array);

    caterva_params_t params;
    params.ndim = src->ndim;
    params.itemsize = src->itemsize;
    for (int i = 0; i < src->ndim; ++i) {
        params.shape[i] = stop[i] - start[i];
    }

    // Add data
    CATERVA_ERROR(caterva_empty(ctx, &params, storage, array));

    if ((*array)->nitems == 0) {
        return CATERVA_SUCCEED;
    }

    int8_t ndim = (*array)->ndim;
    int64_t chunks_in_array[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
        chunks_in_array[i] = (*array)->extshape[i] / (*array)->chunkshape[i];
    }
    int64_t nchunks = (*array)->sc->nchunks;
    for (int nchunk = 0; nchunk < nchunks; ++nchunk) {
        int64_t nchunk_ndim[CATERVA_MAX_DIM] = {0};
        blosc2_unidim_to_multidim(ndim, chunks_in_array, nchunk, nchunk_ndim);

        // check if the chunk needs to be updated
        int64_t chunk_start[CATERVA_MAX_DIM] = {0};
        int64_t chunk_stop[CATERVA_MAX_DIM] = {0};
        int64_t chunk_shape[CATERVA_MAX_DIM] = {0};
        for (int i = 0; i < ndim; ++i) {
            chunk_start[i] = nchunk_ndim[i] * (*array)->chunkshape[i];
            chunk_stop[i] = chunk_start[i] + (*array)->chunkshape[i];
            if (chunk_stop[i] > (*array)->shape[i]) {
                chunk_stop[i] = (*array)->shape[i];
            }
            chunk_shape[i] = chunk_stop[i] - chunk_start[i];
        }

        int64_t src_start[CATERVA_MAX_DIM] = {0};
        int64_t src_stop[CATERVA_MAX_DIM] = {0};
        for (int i = 0; i < ndim; ++i) {
            src_start[i] = chunk_start[i] + start[i];
            src_stop[i] = chunk_stop[i] + start[i];
        }
        int64_t buffersize = params.itemsize;
        for (int i = 0; i < ndim; ++i) {
            buffersize *= chunk_shape[i];
        }
        uint8_t *buffer = ctx->cfg->alloc(buffersize);
        CATERVA_ERROR(caterva_get_slice_buffer(ctx, src, src_start, src_stop, buffer, chunk_shape,
                                               buffersize));
        CATERVA_ERROR(caterva_set_slice_buffer(ctx, buffer, chunk_shape, buffersize, chunk_start,
                                               chunk_stop, *array));
        ctx->cfg->free(buffer);
    }

    return CATERVA_SUCCEED;
}

int caterva_squeeze(caterva_ctx_t *ctx, caterva_array_t *array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);

    bool index[CATERVA_MAX_DIM];

    for (int i = 0; i < array->ndim; ++i) {
        if (array->shape[i] != 1) {
            index[i] = false;
        } else {
            index[i] = true;
        }
    }
    CATERVA_ERROR(caterva_squeeze_index(ctx, array, index));

    return CATERVA_SUCCEED;
}

int caterva_squeeze_index(caterva_ctx_t *ctx, caterva_array_t *array, const bool *index) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);

    uint8_t nones = 0;
    int64_t newshape[CATERVA_MAX_DIM];
    int32_t newchunkshape[CATERVA_MAX_DIM];
    int32_t newblockshape[CATERVA_MAX_DIM];

    for (int i = 0; i < array->ndim; ++i) {
        if (index[i] == true) {
            if (array->shape[i] != 1) {
                CATERVA_ERROR(CATERVA_ERR_INVALID_INDEX);
            }
        } else {
            newshape[nones] = array->shape[i];
            newchunkshape[nones] = array->chunkshape[i];
            newblockshape[nones] = array->blockshape[i];
            nones += 1;
        }
    }

    for (int i = 0; i < CATERVA_MAX_DIM; ++i) {
        if (i < nones) {
            array->chunkshape[i] = newchunkshape[i];
            array->blockshape[i] = newblockshape[i];
        } else {
            array->chunkshape[i] = 1;
            array->blockshape[i] = 1;
        }
    }

    CATERVA_ERROR(caterva_update_shape(array, nones, newshape, newchunkshape, newblockshape));

    return CATERVA_SUCCEED;
}

int caterva_copy(caterva_ctx_t *ctx, caterva_array_t *src, caterva_storage_t *storage,
                 caterva_array_t **array) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(src);
    CATERVA_ERROR_NULL(storage);
    CATERVA_ERROR_NULL(array);


    caterva_params_t params;
    params.itemsize = src->itemsize;
    params.ndim = src->ndim;
    for (int i = 0; i < src->ndim; ++i) {
        params.shape[i] = src->shape[i];
    }

    bool equals = true;
    for (int i = 0; i < src->ndim; ++i) {
        if (src->chunkshape[i] != storage->chunkshape[i]) {
            equals = false;
            break;
        }
        if (src->blockshape[i] != storage->blockshape[i]) {
            equals = false;
            break;
        }
    }

    if (equals) {
        CATERVA_ERROR(caterva_array_without_schunk(ctx, &params, storage, array));
        blosc2_storage b_storage;
        blosc2_cparams cparams;
        blosc2_dparams dparams;
        CATERVA_ERROR(
                create_blosc_params(ctx, &params, storage, &cparams, &dparams, &b_storage));
        blosc2_schunk *new_sc = blosc2_schunk_copy(src->sc, &b_storage);

        if (new_sc == NULL) {
            return CATERVA_ERR_BLOSC_FAILED;
        }
        (*array)->sc = new_sc;

    } else {
        int64_t start[CATERVA_MAX_DIM] = {0, 0, 0, 0, 0, 0, 0, 0};

        int64_t stop[CATERVA_MAX_DIM];
        for (int i = 0; i < src->ndim; ++i) {
            stop[i] = src->shape[i];
        }
        // Copy metalayers
        caterva_storage_t storage_meta;
        memcpy(&storage_meta, storage, sizeof(storage_meta));
        int j = 0;

        for (int i = 0; i < src->sc->nmetalayers; ++i) {
            if (strcmp(src->sc->metalayers[i]->name, "caterva") == 0) {
                continue;
            }
            caterva_metalayer_t *meta = &storage_meta.metalayers[j];
            meta->name = src->sc->metalayers[i]->name;
            meta->sdata = src->sc->metalayers[i]->content;
            meta->size = src->sc->metalayers[i]->content_len;
            j++;
        }
        storage_meta.nmetalayers = j;

        // Copy data
        CATERVA_ERROR(caterva_get_slice(ctx, src, start, stop, &storage_meta, array));

        // Copy vlmetayers
        for (int i = 0; i < src->sc->nvlmetalayers; ++i) {
            uint8_t *content;
            int32_t content_len;
            if (blosc2_vlmeta_get(src->sc, src->sc->vlmetalayers[i]->name, &content,
                                  &content_len) < 0) {
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
            caterva_metalayer_t vlmeta;
            vlmeta.name = src->sc->vlmetalayers[i]->name;
            vlmeta.sdata = content;
            vlmeta.size = content_len;
            CATERVA_ERROR(caterva_vlmeta_add(ctx, *array, &vlmeta));
            free(content);
        }

    }
    return CATERVA_SUCCEED;
}

int caterva_save(caterva_ctx_t *ctx, caterva_array_t *array, char *urlpath) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(urlpath);

    caterva_array_t *tmp;
    caterva_storage_t storage;
    storage.urlpath = urlpath;
    storage.contiguous = array->sc->storage->contiguous;

    for (int i = 0; i < array->ndim; ++i) {
        storage.chunkshape[i] = array->chunkshape[i];
        storage.blockshape[i] = array->blockshape[i];
    }

    caterva_copy(ctx, array, &storage, &tmp);
    caterva_free(ctx, &tmp);

    return CATERVA_SUCCEED;
}

int caterva_remove(caterva_ctx_t *ctx, char *urlpath) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(urlpath);

    int rc = blosc2_remove_urlpath(urlpath);
    if (rc != BLOSC2_ERROR_SUCCESS) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }
    return CATERVA_SUCCEED;
}


int caterva_vlmeta_add(caterva_ctx_t *ctx, caterva_array_t *array, caterva_metalayer_t *vlmeta) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(vlmeta);
    CATERVA_ERROR_NULL(vlmeta->name);
    CATERVA_ERROR_NULL(vlmeta->sdata);
    if (vlmeta->size < 0) {
        CATERVA_TRACE_ERROR("metalayer size must be hgreater than 0");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    if (blosc2_vlmeta_add(array->sc, vlmeta->name, vlmeta->sdata, vlmeta->size, &cparams) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }

    return CATERVA_SUCCEED;
}


int caterva_vlmeta_get(caterva_ctx_t *ctx, caterva_array_t *array,
                             const char *name, caterva_metalayer_t *vlmeta) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(name);
    CATERVA_ERROR_NULL(vlmeta);

    if (blosc2_vlmeta_get(array->sc, name, &vlmeta->sdata, &vlmeta->size) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }
    vlmeta->name = strdup(name);

    return CATERVA_SUCCEED;
}

int caterva_vlmeta_exists(caterva_ctx_t *ctx, caterva_array_t *array,
                                const char *name, bool *exists) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(name);
    CATERVA_ERROR_NULL(exists);

    if (blosc2_vlmeta_exists(array->sc, name) < 0) {
        *exists = false;
    } else {
        *exists = true;
    }

    return CATERVA_SUCCEED;
}


int caterva_vlmeta_update(caterva_ctx_t *ctx, caterva_array_t *array,
                          caterva_metalayer_t *vlmeta) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(vlmeta);
    CATERVA_ERROR_NULL(vlmeta->name);
    CATERVA_ERROR_NULL(vlmeta->sdata);
    if (vlmeta->size < 0) {
        CATERVA_TRACE_ERROR("metalayer size must be hgreater than 0");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    if (blosc2_vlmeta_update(array->sc, vlmeta->name, vlmeta->sdata, vlmeta->size, &cparams) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }

    return CATERVA_SUCCEED;
}

int caterva_meta_get(caterva_ctx_t *ctx, caterva_array_t *array,
                       const char *name, caterva_metalayer_t *meta) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(name);
    CATERVA_ERROR_NULL(meta);

    if (blosc2_meta_get(array->sc, name, &meta->sdata, &meta->size) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }
    meta->name = strdup(name);
    return CATERVA_SUCCEED;
}

int caterva_meta_exists(caterva_ctx_t *ctx, caterva_array_t *array,
                          const char *name, bool *exists) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(name);
    CATERVA_ERROR_NULL(exists);

    if (blosc2_meta_exists(array->sc, name) < 0) {
        *exists = false;
    } else {
        *exists = true;
    }
    return CATERVA_SUCCEED;
}

int caterva_print_meta(caterva_array_t *array){
    CATERVA_ERROR_NULL(array);
    int8_t ndim;
    int64_t shape[8];
    int32_t chunkshape[8];
    int32_t blockshape[8];
    uint8_t *smeta;
    int32_t smeta_len;
    if (blosc2_meta_get(array->sc, "caterva", &smeta, &smeta_len) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }
    caterva_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    printf("Caterva metalayer parameters: \n Ndim:       %d", ndim);
    printf("\n Shape:      %" PRId64 "", shape[0]);
    for (int i = 1; i < ndim; ++i) {
        printf(", %" PRId64 "", shape[i]);
    }
    printf("\n Chunkshape: %d", chunkshape[0]);
    for (int i = 1; i < ndim; ++i) {
        printf(", %d", chunkshape[i]);
    }
    printf("\n Blockshape: %d", blockshape[0]);
    for (int i = 1; i < ndim; ++i) {
        printf(", %d", blockshape[i]);
    }
    printf("\n");
    return CATERVA_SUCCEED;
}

int caterva_meta_update(caterva_ctx_t *ctx, caterva_array_t *array,
                          caterva_metalayer_t *meta) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(meta);
    CATERVA_ERROR_NULL(meta->name);
    CATERVA_ERROR_NULL(meta->sdata);
    if (meta->size < 0) {
        CATERVA_TRACE_ERROR("metalayer size must be greater than 0");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    if (blosc2_meta_update(array->sc, meta->name, meta->sdata, meta->size) < 0) {
        CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
    }
    return CATERVA_SUCCEED;
}

int extend_shape(caterva_array_t *array, const int64_t *new_shape, const int64_t *start) {
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(new_shape);

    int8_t ndim = array->ndim;
    int64_t diffs_shape[CATERVA_MAX_DIM];
    int64_t diffs_sum = 0;
    for (int i = 0; i < ndim; i++) {
        diffs_shape[i] = new_shape[i] - array->shape[i];
        diffs_sum += diffs_shape[i];
        if (diffs_shape[i] < 0) {
            CATERVA_TRACE_ERROR("The new shape must be greater than the old one");
            CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
        }
        if (array->shape[i] == 0) {
            CATERVA_TRACE_ERROR("Cannot extend array with shape[%d] = 0", i);
            CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }
    if (diffs_sum == 0) {
        // Shapes are equal. Do nothing.
        return CATERVA_SUCCEED;
    }

    int64_t old_nchunks = array->nchunks;
    // aux array to keep old shapes
    caterva_array_t *aux = malloc(sizeof (caterva_array_t));
    aux->sc = NULL;
    CATERVA_ERROR(caterva_update_shape(aux, ndim, array->shape, array->chunkshape, array->blockshape));

    CATERVA_ERROR(caterva_update_shape(array, ndim, new_shape, array->chunkshape, array->blockshape));

    int64_t nchunks = array->extnitems / array->chunknitems;
    int64_t nchunks_;
    int64_t nchunk_ndim[CATERVA_MAX_DIM];
    blosc2_cparams *cparams;
    blosc2_schunk_get_cparams(array->sc, &cparams);
    void* chunk;
    int64_t csize;
    if (nchunks != old_nchunks) {
        if (start == NULL) {
            start = aux->shape;
        }
        int64_t chunks_in_array[CATERVA_MAX_DIM] = {0};
        for (int i = 0; i < ndim; ++i) {
            chunks_in_array[i] = array->extshape[i] / array->chunkshape[i];
        }
        for (int i = 0; i < nchunks; ++i) {
            blosc2_unidim_to_multidim(ndim, chunks_in_array, i, nchunk_ndim);
            for (int j = 0; j < ndim; ++j) {
                if (start[j] <= (array->chunkshape[j] * nchunk_ndim[j])
                    && (array->chunkshape[j] * nchunk_ndim[j]) < (start[j] + new_shape[j] - aux->shape[j])) {
                    chunk = malloc(BLOSC_EXTENDED_HEADER_LENGTH);
                    csize = blosc2_chunk_zeros(*cparams, array->sc->chunksize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
                    if (csize < 0) {
                        free(aux);
                        free(cparams);
                        CATERVA_TRACE_ERROR("Blosc error when creating a chunk");
                        return CATERVA_ERR_BLOSC_FAILED;
                    }
                    nchunks_ = blosc2_schunk_insert_chunk(array->sc, i, chunk, false);
                    if (nchunks_ < 0) {
                        free(aux);
                        free(cparams);
                        CATERVA_TRACE_ERROR("Blosc error when inserting a chunk");
                        return CATERVA_ERR_BLOSC_FAILED;
                    }
                    break;
                }
            }
        }
    }
    array->nchunks = array->sc->nchunks;
    free(aux);
    free(cparams);

    return CATERVA_SUCCEED;
}

int shrink_shape(caterva_array_t *array, const int64_t *new_shape, const int64_t *start) {
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(new_shape);

    int8_t ndim = array->ndim;
    int64_t diffs_shape[CATERVA_MAX_DIM];
    int64_t diffs_sum = 0;
    for (int i = 0; i < ndim; i++) {
        diffs_shape[i] = new_shape[i] - array->shape[i];
        diffs_sum += diffs_shape[i];
        if (diffs_shape[i] > 0) {
            CATERVA_TRACE_ERROR("The new shape must be smaller than the old one");
            CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
        }
        if (array->shape[i] == 0) {
            CATERVA_TRACE_ERROR("Cannot shrink array with shape[%d] = 0", i);
            CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }
    if (diffs_sum == 0) {
        // Shapes are equal. Do nothing.
        return CATERVA_SUCCEED;
    }

    int64_t old_nchunks = array->nchunks;
    // aux array to keep old shapes
    caterva_array_t *aux = malloc(sizeof (caterva_array_t));
    aux->sc = NULL;
    CATERVA_ERROR(caterva_update_shape(aux, ndim, array->shape, array->chunkshape, array->blockshape));

    CATERVA_ERROR(caterva_update_shape(array, ndim, new_shape, array->chunkshape, array->blockshape));

    // Delete chunks if needed
    int64_t chunks_in_array_old[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < ndim; ++i) {
        chunks_in_array_old[i] = aux->extshape[i] / aux->chunkshape[i];
    }
    if (start == NULL) {
        start = new_shape;
    }

    int64_t nchunk_ndim[CATERVA_MAX_DIM] = {0};
    int64_t nchunks_;
    for (int i = (int)old_nchunks - 1; i >= 0; --i) {
        blosc2_unidim_to_multidim(ndim, chunks_in_array_old, i, nchunk_ndim);
        for (int j = 0; j < ndim; ++j) {
            if (start[j] <= (array->chunkshape[j] * nchunk_ndim[j])
                && (array->chunkshape[j] * nchunk_ndim[j]) < (start[j] + aux->shape[j] - new_shape[j])) {
                nchunks_ = blosc2_schunk_delete_chunk(array->sc, i);
                if (nchunks_ < 0) {
                    free(aux);
                    CATERVA_TRACE_ERROR("Blosc error when deleting a chunk");
                    return CATERVA_ERR_BLOSC_FAILED;
                }
                break;
            }
        }
    }
    array->nchunks = array->sc->nchunks;
    free(aux);

    return CATERVA_SUCCEED;
}


int caterva_resize(caterva_ctx_t *ctx, caterva_array_t *array, const int64_t *new_shape,
                   const int64_t *start) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(new_shape);

    if (start != NULL) {
        for (int i = 0; i < array->ndim; ++i) {
            if (start[i] > array->shape[i]) {
                CATERVA_TRACE_ERROR("`start` must be lower or equal than old array shape in all dims");
                CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
            }
            if ((new_shape[i] > array->shape[i] && start[i] != array->shape[i])
                || (new_shape[i] < array->shape[i]
                    && (start[i] + array->shape[i] - new_shape[i]) != array->shape[i])) {
                // Chunks cannot be cut unless they are in the last position
                if (start[i] % array->chunkshape[i] != 0) {
                    CATERVA_TRACE_ERROR("If array end is not being modified "
                        "`start` must be a multiple of chunkshape in all dims");
                    CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
                }
                if ((new_shape[i] - array->shape[i]) % array->chunkshape[i] != 0) {
                    CATERVA_TRACE_ERROR("If array end is not being modified "
                        "`(new_shape - shape)` must be multiple of chunkshape in all dims");
                    CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
                }
            }
        }
    }

    // Get shrinked shape
    int64_t shrinked_shape[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < array->ndim; ++i) {
        if (new_shape[i] <= array->shape[i]) {
            shrinked_shape[i] = new_shape[i];
        } else {
            shrinked_shape[i] = array->shape[i];
        }
    }

    CATERVA_ERROR(shrink_shape(array, shrinked_shape, start));
    CATERVA_ERROR(extend_shape(array, new_shape, start));

    return CATERVA_SUCCEED;
}

int caterva_insert(caterva_ctx_t *ctx, caterva_array_t *array, void *buffer, int64_t buffersize,
                   const int8_t axis, int64_t insert_start) {

    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(buffer);

    if (axis >= array->ndim) {
        CATERVA_TRACE_ERROR("`axis` cannot be greater than the number of dimensions");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    int64_t axis_size = array->itemsize;
    int64_t buffershape[CATERVA_MAX_DIM];
    for (int i = 0; i < array->ndim; ++i) {
        if (i != axis) {
            axis_size *= array->shape[i];
            buffershape[i] = array->shape[i];
        }
    }
    if (buffersize % axis_size != 0) {
        CATERVA_TRACE_ERROR("`buffersize` must be multiple of the array");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }
    int64_t newshape[CATERVA_MAX_DIM];
    memcpy(newshape, array->shape, array->ndim * sizeof(int64_t));
    newshape[axis] += buffersize / axis_size;
    buffershape[axis] = newshape[axis] - array->shape[axis];
    int64_t start[CATERVA_MAX_DIM] = {0};
    start[axis] = insert_start;

    if (insert_start == array->shape[axis]) {
        CATERVA_ERROR(caterva_resize(ctx, array, newshape, NULL));
    }
    else {
        CATERVA_ERROR(caterva_resize(ctx, array, newshape, start));
    }

    int64_t stop[CATERVA_MAX_DIM];
    memcpy(stop, array->shape, sizeof(int64_t) * array->ndim);
    stop[axis] = start[axis] + buffershape[axis];
    CATERVA_ERROR(caterva_set_slice_buffer(ctx, buffer, buffershape, buffersize, start, stop, array));

    return CATERVA_SUCCEED;
}


int caterva_append(caterva_ctx_t *ctx, caterva_array_t *array, void *buffer, int64_t buffersize,
                   const int8_t axis) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(buffer);

    CATERVA_ERROR(caterva_insert(ctx, array, buffer, buffersize, axis, array->shape[axis]));

    return CATERVA_SUCCEED;
}


int caterva_delete(caterva_ctx_t *ctx, caterva_array_t *array, const int8_t axis,
                   int64_t delete_start, int64_t delete_len) {

    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);

    if (axis >= array->ndim) {
        CATERVA_TRACE_ERROR("axis cannot be greater than the number of dimensions");
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }


    int64_t newshape[CATERVA_MAX_DIM];
    memcpy(newshape, array->shape, array->ndim * sizeof(int64_t));
    newshape[axis] -= delete_len;
    int64_t start[CATERVA_MAX_DIM] = {0};
    start[axis] = delete_start;

    if (delete_start == (array->shape[axis] - delete_len)) {
        CATERVA_ERROR(caterva_resize(ctx, array, newshape, NULL));
    }
    else {
        CATERVA_ERROR(caterva_resize(ctx, array, newshape, start));
    }

    return CATERVA_SUCCEED;
}

// Indexing

typedef struct {
    int64_t value;
    int64_t index;
} caterva_selection_t;

int caterva_compare_selection(const void * a, const void * b) {
    int res = (int) (((caterva_selection_t *) a)->value - ((caterva_selection_t *) b)->value);
    // In case values are equal, sort by index
    if (res == 0) {
        res = (int) (((caterva_selection_t *) a)->index - ((caterva_selection_t *) b)->index);
    }
    return res;
}

int caterva_copy_block_buffer_data(caterva_array_t *array,
                                   int8_t ndim,
                                   int64_t *block_selection_size,
                                   caterva_selection_t **chunk_selection,
                                   caterva_selection_t **p_block_selection_0,
                                   caterva_selection_t **p_block_selection_1,
                                   uint8_t *block,
                                   uint8_t *buffer,
                                   int64_t *buffershape,
                                   int64_t *bufferstrides,
                                   bool get) {
    p_block_selection_0[ndim] = chunk_selection[ndim];
    p_block_selection_1[ndim] = chunk_selection[ndim];
    while (p_block_selection_1[ndim] - p_block_selection_0[ndim] < block_selection_size[ndim]) {
        if (ndim == array->ndim - 1) {

            int64_t index_in_block_n[CATERVA_MAX_DIM];
            for (int i = 0; i < array->ndim; ++i) {
                index_in_block_n[i] = p_block_selection_1[i]->value % array->chunkshape[i] % array->blockshape[i];
            }
            int64_t index_in_block = 0;
            for (int i = 0; i < array->ndim; ++i) {
                index_in_block += index_in_block_n[i] * array->item_block_strides[i];
            }

            int64_t index_in_buffer_n[CATERVA_MAX_DIM];
            for (int i = 0; i < array->ndim; ++i) {
                index_in_buffer_n[i] = p_block_selection_1[i]->index;
            }
            int64_t index_in_buffer = 0;
            for (int i = 0; i < array->ndim; ++i) {
                index_in_buffer += index_in_buffer_n[i] * bufferstrides[i];
            }
            if (get) {
                memcpy(&buffer[index_in_buffer * array->itemsize],
                       &block[index_in_block * array->itemsize],
                       array->itemsize);
            } else {
                memcpy(&block[index_in_block * array->itemsize],
                       &buffer[index_in_buffer * array->itemsize],
                       array->itemsize);
            }
        } else {
            caterva_copy_block_buffer_data(array, (int8_t) (ndim + 1), block_selection_size,
                                           chunk_selection,
                                           p_block_selection_0, p_block_selection_1, block,
                                           buffer, buffershape, bufferstrides, get);
        }
        p_block_selection_1[ndim]++;
    }
    return CATERVA_SUCCEED;
}

int caterva_iterate_over_block_copy(caterva_array_t *array, int8_t ndim,
                                    int64_t *chunk_selection_size,
                                    caterva_selection_t **ordered_selection,
                                    caterva_selection_t **chunk_selection_0,
                                    caterva_selection_t **chunk_selection_1,
                                    uint8_t *data,
                                    uint8_t *buffer,
                                    int64_t *buffershape,
                                    int64_t *bufferstrides,
                                    bool get) {
    chunk_selection_0[ndim] = ordered_selection[ndim];
    chunk_selection_1[ndim] = ordered_selection[ndim];
    while(chunk_selection_1[ndim] - ordered_selection[ndim] < chunk_selection_size[ndim]) {
        int64_t block_index_ndim = ((*chunk_selection_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim];
        while (chunk_selection_1[ndim] - ordered_selection[ndim] < chunk_selection_size[ndim] &&
               block_index_ndim == ((*chunk_selection_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim]) {
            chunk_selection_1[ndim]++;
        }
        if (ndim == array->ndim - 1) {
            int64_t block_chunk_strides[CATERVA_MAX_DIM];
            block_chunk_strides[array->ndim - 1] = 1;
            for (int i = array->ndim - 2; i >= 0; --i) {
                block_chunk_strides[i] = block_chunk_strides[i + 1] * (array->extchunkshape[i + 1] / array->blockshape[i + 1]);
            }
            int64_t block_index[CATERVA_MAX_DIM];
            for (int i = 0; i < array->ndim; ++i) {
                block_index[i] = ((*chunk_selection_0[i]).value % array->chunkshape[i]) / array->blockshape[i];
            }
            int64_t nblock = 0;
            for (int i = 0; i < array->ndim; ++i) {
                nblock += block_index[i] * block_chunk_strides[i];
            }
            caterva_selection_t **p_block_selection_0 = malloc(array->ndim * sizeof(caterva_selection_t *));
            caterva_selection_t **p_block_selection_1 = malloc(array->ndim * sizeof(caterva_selection_t *));
            int64_t *block_selection_size = malloc(array->ndim * sizeof(int64_t));
            for (int i = 0; i < array->ndim; ++i) {
                block_selection_size[i] = chunk_selection_1[i] - chunk_selection_0[i];
            }

            caterva_copy_block_buffer_data(array,
                                           (int8_t) 0,
                                           block_selection_size,
                                           chunk_selection_0,
                                           p_block_selection_0,
                                           p_block_selection_1,
                                           &data[nblock * array->blocknitems * array->itemsize],
                                           buffer,
                                           buffershape,
                                           bufferstrides,
                                           get);
            free(p_block_selection_0);
            free(p_block_selection_1);
            free(block_selection_size);
        } else {
            caterva_iterate_over_block_copy(array, (int8_t) (ndim + 1), chunk_selection_size,
                                            ordered_selection, chunk_selection_0, chunk_selection_1,
                                            data, buffer, buffershape, bufferstrides, get);
        }
        chunk_selection_0[ndim] = chunk_selection_1[ndim];

    }

    return CATERVA_SUCCEED;
}

int caterva_iterate_over_block_maskout(caterva_array_t *array, int8_t ndim,
                                       int64_t *sel_block_size,
                                       caterva_selection_t **o_selection,
                                       caterva_selection_t **p_o_sel_block_0,
                                       caterva_selection_t **p_o_sel_block_1,
                                       bool *maskout) {
    p_o_sel_block_0[ndim] = o_selection[ndim];
    p_o_sel_block_1[ndim] = o_selection[ndim];
    while(p_o_sel_block_1[ndim] - o_selection[ndim] < sel_block_size[ndim]) {
        int64_t block_index_ndim = ((*p_o_sel_block_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim];
        while (p_o_sel_block_1[ndim] - o_selection[ndim] < sel_block_size[ndim] &&
               block_index_ndim == ((*p_o_sel_block_1[ndim]).value % array->chunkshape[ndim]) / array->blockshape[ndim]) {
            p_o_sel_block_1[ndim]++;
        }
        if (ndim == array->ndim - 1) {
            int64_t block_chunk_strides[CATERVA_MAX_DIM];
            block_chunk_strides[array->ndim - 1] = 1;
            for (int i = array->ndim - 2; i >= 0; --i) {
                block_chunk_strides[i] = block_chunk_strides[i + 1] * (array->extchunkshape[i + 1] / array->blockshape[i + 1]);
            }
            int64_t block_index[CATERVA_MAX_DIM];
            for (int i = 0; i < array->ndim; ++i) {
                block_index[i] = ((*p_o_sel_block_0[i]).value % array->chunkshape[i]) / array->blockshape[i];
            }
            int64_t nblock = 0;
            for (int i = 0; i < array->ndim; ++i) {
                nblock += block_index[i] * block_chunk_strides[i];
            }
            maskout[nblock] = false;
        } else {
            caterva_iterate_over_block_maskout(array, (int8_t) (ndim + 1), sel_block_size,
                                               o_selection, p_o_sel_block_0, p_o_sel_block_1,
                                               maskout);
        }
        p_o_sel_block_0[ndim] = p_o_sel_block_1[ndim];

    }

    return CATERVA_SUCCEED;
}

int caterva_iterate_over_chunk(caterva_array_t *array, int8_t ndim,
                               int64_t *selection_size,
                               caterva_selection_t **ordered_selection,
                               caterva_selection_t **p_ordered_selection_0,
                               caterva_selection_t **p_ordered_selection_1,
                               uint8_t *buffer,
                               int64_t *buffershape,
                               int64_t *bufferstrides,
                               bool get) {
    p_ordered_selection_0[ndim] = ordered_selection[ndim];
    p_ordered_selection_1[ndim] = ordered_selection[ndim];
    while(p_ordered_selection_1[ndim] - ordered_selection[ndim] < selection_size[ndim]) {
        int64_t chunk_index_ndim = (*p_ordered_selection_1[ndim]).value / array->chunkshape[ndim];
        while (p_ordered_selection_1[ndim] - ordered_selection[ndim] < selection_size[ndim] &&
               chunk_index_ndim == (*p_ordered_selection_1[ndim]).value / array->chunkshape[ndim]) {
            p_ordered_selection_1[ndim]++;
        }
        if (ndim == array->ndim - 1) {
            int64_t chunk_array_strides[CATERVA_MAX_DIM];
            chunk_array_strides[array->ndim - 1] = 1;
            for (int i = array->ndim - 2; i >= 0; --i) {
                chunk_array_strides[i] = chunk_array_strides[i + 1] *
                                         (array->extshape[i + 1] / array->chunkshape[i + 1]);
            }
            int64_t chunk_index[CATERVA_MAX_DIM];
            for (int i = 0; i < array->ndim; ++i) {
                chunk_index[i] = (*p_ordered_selection_0[i]).value / array->chunkshape[i];
            }
            int64_t nchunk = 0;
            for (int i = 0; i < array->ndim; ++i) {
                nchunk += chunk_index[i] * chunk_array_strides[i];
            }

            int64_t nblocks = array->extchunknitems / array->blocknitems;
            caterva_selection_t **p_chunk_selection_0 = malloc(
                    array->ndim * sizeof(caterva_selection_t *));
            caterva_selection_t **p_chunk_selection_1 = malloc(
                    array->ndim * sizeof(caterva_selection_t *));
            int64_t *chunk_selection_size = malloc(array->ndim * sizeof(int64_t));
            for (int i = 0; i < array->ndim; ++i) {
                chunk_selection_size[i] = p_ordered_selection_1[i] - p_ordered_selection_0[i];
            }

            if (get) {
                bool *maskout = calloc(nblocks, sizeof(bool));
                for (int i = 0; i < nblocks; ++i) {
                    maskout[i] = true;
                }

                CATERVA_ERROR(caterva_iterate_over_block_maskout(array, (int8_t) 0,
                                                                 chunk_selection_size,
                                                                 p_ordered_selection_0,
                                                                 p_chunk_selection_0,
                                                                 p_chunk_selection_1,
                                                                 maskout));

                if (blosc2_set_maskout(array->sc->dctx, maskout, (int) nblocks) !=
                    BLOSC2_ERROR_SUCCESS) {
                    CATERVA_TRACE_ERROR("Error setting the maskout");
                    CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
                }
                free(maskout);
            }
            int data_nitems = (int) array->extchunknitems;
            int data_nbytes = data_nitems * array->itemsize;
            uint8_t *data = malloc(data_nitems * array->itemsize);
            int err = blosc2_schunk_decompress_chunk(array->sc, nchunk, data, data_nbytes);
            if (err < 0) {
                CATERVA_TRACE_ERROR("Error decompressing chunk");
                CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
            }
            caterva_iterate_over_block_copy(array,
                                            0,
                                            chunk_selection_size,
                                            p_ordered_selection_0,
                                            p_chunk_selection_0,
                                            p_chunk_selection_1,
                                            data,
                                            buffer,
                                            buffershape,
                                            bufferstrides,
                                            get);

            if (!get) {
                int32_t chunk_size = data_nbytes + BLOSC_EXTENDED_HEADER_LENGTH;
                uint8_t *chunk = malloc(chunk_size);
                err = blosc2_compress_ctx(array->sc->cctx, data, data_nbytes, chunk, chunk_size);
                if (err < 0) {
                    CATERVA_TRACE_ERROR("Error compressing data");
                    CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
                }
                err = (int) blosc2_schunk_update_chunk(array->sc, nchunk, chunk, false);
                if (err < 0) {
                    CATERVA_TRACE_ERROR("Error updating chunk");
                    CATERVA_ERROR(CATERVA_ERR_BLOSC_FAILED);
                }
            }
            free(data);
            free(chunk_selection_size);
            free(p_chunk_selection_0);
            free(p_chunk_selection_1);
        } else {
            CATERVA_ERROR(caterva_iterate_over_chunk(array, (int8_t) (ndim + 1), selection_size,
                                                     ordered_selection, p_ordered_selection_0, p_ordered_selection_1,
                                                     buffer, buffershape, bufferstrides, get));
        }

        p_ordered_selection_0[ndim] = p_ordered_selection_1[ndim];
    }
    return CATERVA_SUCCEED;
}

int caterva_orthogonal_selection(caterva_ctx_t *ctx, caterva_array_t *array,
                                     int64_t **selection, int64_t *selection_size,
                                     void *buffer, int64_t *buffershape, int64_t buffersize,
                                     bool get) {
    CATERVA_ERROR_NULL(ctx);
    CATERVA_ERROR_NULL(array);
    CATERVA_ERROR_NULL(selection);
    CATERVA_ERROR_NULL(selection_size);

    int8_t ndim = array->ndim;

    for (int i = 0; i < ndim; ++i) {
        CATERVA_ERROR_NULL(selection[i]);
        // Check that indexes are not larger than array shape
        for (int j = 0; j < selection_size[i]; ++j) {
            if (selection[i][j] > array->shape[i]) {
                CATERVA_ERROR(CATERVA_ERR_INVALID_INDEX);
            }
        }
    }

    // Check buffer size
    int64_t sel_size = array->itemsize;
    for (int i = 0; i < ndim; ++i) {
        sel_size *= selection_size[i];
    }

    if (sel_size < buffersize) {
        CATERVA_ERROR(CATERVA_ERR_INVALID_ARGUMENT);
    }

    // Sort selections
    caterva_selection_t **ordered_selection = malloc(ndim * sizeof(caterva_selection_t *));
    for (int i = 0; i < ndim; ++i) {
        ordered_selection[i] = malloc(selection_size[i] * sizeof(caterva_selection_t));
        for (int j = 0; j < selection_size[i]; ++j) {
            ordered_selection[i][j].index = j;
            ordered_selection[i][j].value = selection[i][j];
        }
        qsort(ordered_selection[i], selection_size[i], sizeof(caterva_selection_t), caterva_compare_selection);
    }

    // Define pointers to iterate over ordered_selection data
    caterva_selection_t **p_ordered_selection_0 = malloc(ndim * sizeof(caterva_selection_t *));
    caterva_selection_t **p_ordered_selection_1 = malloc(ndim * sizeof(caterva_selection_t *));


    int64_t bufferstrides[CATERVA_MAX_DIM];
    bufferstrides[array->ndim - 1] = 1;
    for (int i = array->ndim - 2; i >= 0; --i) {
        bufferstrides[i] = bufferstrides[i + 1] * buffershape[i + 1];
    }

    CATERVA_ERROR(caterva_iterate_over_chunk(array, 0,
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

    return CATERVA_SUCCEED;
}

int caterva_get_orthogonal_selection(caterva_ctx_t *ctx, caterva_array_t *array,
                                     int64_t **selection, int64_t *selection_size,
                                     void *buffer, int64_t *buffershape, int64_t buffersize) {

    return caterva_orthogonal_selection(ctx, array, selection, selection_size,
                                        buffer, buffershape, buffersize, true);
}

int caterva_set_orthogonal_selection(caterva_ctx_t *ctx, caterva_array_t *array,
                                     int64_t **selection, int64_t *selection_size,
                                     void *buffer, int64_t *buffershape, int64_t buffersize) {

    return caterva_orthogonal_selection(ctx, array, selection, selection_size,
                                        buffer, buffershape, buffersize, false);
}


int32_t caterva_serialize_meta(int8_t ndim, int64_t *shape, const int32_t *chunkshape,
                               const int32_t *blockshape, uint8_t **smeta) {
    // Allocate space for Caterva metalayer
    int32_t max_smeta_len = (int32_t) (1 + 1 + 1 + (1 + ndim * (1 + sizeof(int64_t))) +
                                       (1 + ndim * (1 + sizeof(int32_t))) + (1 + ndim * (1 + sizeof(int32_t))));
    *smeta = malloc((size_t) max_smeta_len);
    CATERVA_ERROR_NULL(smeta);
    uint8_t *pmeta = *smeta;

    // Build an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
    *pmeta++ = 0x90 + 5;

    // version entry
    *pmeta++ = CATERVA_METALAYER_VERSION;  // positive fixnum (7-bit positive integer)

    // ndim entry
    *pmeta++ = (uint8_t) ndim;  // positive fixnum (7-bit positive integer)

    // shape entry
    *pmeta++ = (uint8_t)(0x90) + ndim;  // fix array with ndim elements
    for (uint8_t i = 0; i < ndim; i++) {
        *pmeta++ = 0xd3;  // int64
        swap_store(pmeta, shape + i, sizeof(int64_t));
        pmeta += sizeof(int64_t);
    }

    // chunkshape entry
    *pmeta++ = (uint8_t)(0x90) + ndim;  // fix array with ndim elements
    for (uint8_t i = 0; i < ndim; i++) {
        *pmeta++ = 0xd2;  // int32
        swap_store(pmeta, chunkshape + i, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }

    // blockshape entry
    *pmeta++ = (uint8_t)(0x90) + ndim;  // fix array with ndim elements
    for (uint8_t i = 0; i < ndim; i++) {
        *pmeta++ = 0xd2;  // int32
        swap_store(pmeta, blockshape + i, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }
    int32_t slen = (int32_t)(pmeta - *smeta);

    return slen;
}

int32_t caterva_deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                                 int32_t *chunkshape, int32_t *blockshape) {
    BLOSC_UNUSED_PARAM(smeta_len);
    uint8_t *pmeta = smeta;

    // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
    pmeta += 1;

    // version entry
    // int8_t version = (int8_t)pmeta[0];  // positive fixnum (7-bit positive integer) commented to avoid warning
    pmeta += 1;

    // ndim entry
    *ndim = (int8_t)pmeta[0];
    int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // shape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < ndim_aux; i++) shape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(shape + i, pmeta, sizeof(int64_t));
        pmeta += sizeof(int64_t);
    }

    // chunkshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < ndim_aux; i++) chunkshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(chunkshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }

    // blockshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < ndim_aux; i++) blockshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(blockshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }
    int32_t slen = (int32_t)(pmeta - smeta);
    return slen;
}

/* Native storage is a binding adapter, not another numerical scheduler. */
#include "b2nd-exec-storage.h"
#include "b2nd-exec-private.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const b2nd_array_t *array;
    blosc2_context *dctx;
    unsigned char *decoded;
    size_t bytes, items, block_items;
    size_t chunk_grid[B2ND_EXEC_MAX_RANK], block_grid[B2ND_EXEC_MAX_RANK];
    int64_t cached_chunk;
    b2nd_exec_storage_report *report;
} storage_input;

static me_status storage_error(me_error *error, me_status status, const char *message) {
    if (error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

static bool storage_product(size_t *product, size_t factor) {
    if (factor && *product > (size_t)INT64_MAX / factor) return false;
    *product *= factor;
    return true;
}

static bool storage_f64(const char *dtype) {
    if (!dtype) return false;
    uint16_t endian = 1;
    return !strcmp(dtype, "=f8") ||
        !strcmp(dtype, *(unsigned char *)&endian ? "<f8" : ">f8");
}

static me_status storage_validate(storage_input *state, const b2nd_exec_input_meta *meta,
                                   me_error *error) {
    const b2nd_array_t *a = state->array;
    if (!a || !a->sc || a->ndim < 1 || a->ndim > B2ND_EXEC_MAX_RANK ||
        (size_t)a->ndim != meta->ndim || meta->dtype != ME_FLOAT64 || meta->itemsize != 8 ||
        a->dtype_format != DTYPE_NUMPY_FORMAT || !storage_f64(a->dtype) ||
        a->sc->typesize != 8 || a->sc->frame || (a->sc->nchunks && !a->sc->data) ||
        a->sc->compcode > BLOSC_ZSTD) {
        return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported native storage binding");
    }
    blosc2_dparams params;
    if (!a->sc->dctx || blosc2_ctx_get_dparams(a->sc->dctx, &params) < 0 || params.postfilter) {
        return storage_error(error, ME_ERR_UNSUPPORTED, "Storage postfilters are not admitted");
    }
    for (size_t i = 0; i < BLOSC2_MAX_FILTERS; i++) {
        if (a->sc->filters[i] > BLOSC_DELTA) {
            return storage_error(error, ME_ERR_UNSUPPORTED, "Storage plugin filters are not admitted");
        }
    }
    size_t items = 1, chunks = 1, physical = 1, block_items = 1, chunk_items = 1;
    for (int i = 0; i < a->ndim; i++) {
        if (a->shape[i] < 0 || (uint64_t)a->shape[i] > SIZE_MAX || a->shape[i] != meta->shape[i] ||
            a->chunkshape[i] <= 0 || a->blockshape[i] <= 0) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Storage shape does not match the schedule");
        }
        size_t shape = (size_t)a->shape[i], chunk = (size_t)a->chunkshape[i];
        size_t block = (size_t)a->blockshape[i];
        state->chunk_grid[i] = shape / chunk + (shape % chunk != 0);
        state->block_grid[i] = chunk / block + (chunk % block != 0);
        if (!storage_product(&items, shape) ||
            !storage_product(&chunks, state->chunk_grid[i]) ||
            !storage_product(&physical, state->block_grid[i]) ||
            !storage_product(&physical, block) ||
            !storage_product(&block_items, block) ||
            !storage_product(&chunk_items, chunk)) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Storage geometry overflow");
        }
    }
    if (physical > INT32_MAX / 8 || a->extchunknitems != (int64_t)physical ||
        a->blocknitems != (int64_t)block_items || a->chunknitems != (int64_t)chunk_items ||
        a->nitems != (int64_t)items || a->sc->nchunks != (int64_t)chunks ||
        (chunks && a->sc->chunksize != (int32_t)(physical * 8))) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Inconsistent storage geometry");
    }
    state->items = items;
    state->block_items = block_items;
    state->bytes = items ? physical * 8 : 0;
    state->cached_chunk = -1;
    return ME_SUCCESS;
}

static me_status storage_read(void *context, size_t start, size_t count,
                               me_mut_buffer output, me_error *error) {
    storage_input *state = context;
    const b2nd_array_t *a = state->array;
    if (start > state->items || count > state->items - start || output.capacity < count * 8) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid logical storage span");
    }
    state->report->read_spans++;
    for (size_t lane = 0; lane < count; lane++) {
        size_t remaining = start + lane, coords[B2ND_EXEC_MAX_RANK];
        for (int i = a->ndim; i-- > 0;) {
            coords[i] = remaining % (size_t)a->shape[i];
            remaining /= (size_t)a->shape[i];
        }
        size_t chunk = 0, block = 0, item = 0;
        for (int i = 0; i < a->ndim; i++) {
            size_t local = coords[i] % (size_t)a->chunkshape[i];
            chunk = chunk * state->chunk_grid[i] + coords[i] / (size_t)a->chunkshape[i];
            block = block * state->block_grid[i] + local / (size_t)a->blockshape[i];
            item = item * (size_t)a->blockshape[i] + local % (size_t)a->blockshape[i];
        }
        if (state->cached_chunk != (int64_t)chunk) {
            /* The admitted in-memory representation owns these bytes. Avoid
             * schunk_get_chunk: it mutates shared current_nchunk even here. */
            const uint8_t *encoded = a->sc->data[chunk];
            int32_t nbytes = 0, cbytes = 0, blocksize = 0;
            int rc = -1;
            if (encoded && blosc2_cbuffer_sizes(encoded, &nbytes, &cbytes, &blocksize) == 0 &&
                cbytes >= BLOSC_MIN_HEADER_LENGTH && nbytes == (int32_t)state->bytes &&
                blocksize == (int32_t)(state->block_items * 8)) {
                rc = blosc2_decompress_ctx(state->dctx, encoded, cbytes,
                                           state->decoded, (int32_t)state->bytes);
            }
            if (rc != (int32_t)state->bytes) {
                return storage_error(error, ME_ERR_EVAL, "Storage chunk decode failed");
            }
            state->cached_chunk = (int64_t)chunk;
            state->report->decodes++;
            state->report->decoded_bytes += state->bytes;
        } else {
            state->report->cache_hits++;
        }
        size_t offset = (block * state->block_items + item) * 8;
        memcpy((unsigned char *)output.data + lane * 8, state->decoded + offset, 8);
    }
    return ME_SUCCESS;
}

me_status b2nd_exec_execute_arrays(const b2nd_exec_schedule *schedule,
                                  const b2nd_array_t *const *inputs, size_t ninputs,
                                  me_mut_buffer output, unsigned raise_mask,
                                  size_t max_decode_bytes, size_t max_gather_bytes,
                                  b2nd_exec_storage_report *report, me_error *error) {
    if (report) *report = (b2nd_exec_storage_report){0};
    if (error) *error = (me_error){0};
    if (!schedule || !inputs || !report || !ninputs || ninputs > B2ND_EXEC_MAX_INPUTS) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage execution bindings");
    }
    b2nd_exec_input_meta meta;
    if (b2nd_exec_schedule_input(schedule, ninputs, &meta) == ME_SUCCESS) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Storage input count mismatch");
    }
    storage_input *states = calloc(ninputs, sizeof(*states));
    if (!states) return storage_error(error, ME_ERR_OOM, "Cannot allocate storage binding metadata");
    report->storage_metadata_bytes = ninputs * sizeof(*states);
    me_status status = ME_SUCCESS;
    size_t bytes = 0;
    for (size_t i = 0; i < ninputs; i++) {
        states[i].array = inputs[i];
        states[i].report = report;
        if (b2nd_exec_schedule_input(schedule, i, &meta) != ME_SUCCESS) {
            status = storage_error(error, ME_ERR_INVALID_ARG, "Storage input count mismatch");
            break;
        }
        status = storage_validate(&states[i], &meta, error);
        if (status != ME_SUCCESS) break;
        if (states[i].bytes > max_decode_bytes - bytes) {
            status = storage_error(error, ME_ERR_INVALID_ARG, "Storage decode budget exceeded");
            break;
        }
        bytes += states[i].bytes;
    }
    b2nd_exec_reader readers[B2ND_EXEC_MAX_INPUTS] = {0};
    b2nd_exec_view views[B2ND_EXEC_MAX_INPUTS] = {0};
    /* In-memory chunk headers are storage metadata. Do not permit a resident
     * destination to overwrite borrowed compressed bytes during traversal. */
    if (status == ME_SUCCESS && output.capacity > UINTPTR_MAX - (uintptr_t)output.data) {
        status = storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage output extent");
    }
    for (size_t i = 0; status == ME_SUCCESS && i < ninputs; i++) {
        for (int64_t c = 0; c < inputs[i]->sc->nchunks; c++) {
            const uint8_t *chunk = inputs[i]->sc->data[c];
            int32_t nbytes, cbytes, blocksize;
            if (!chunk || blosc2_cbuffer_sizes(chunk, &nbytes, &cbytes, &blocksize) < 0 ||
                cbytes < BLOSC_EXTENDED_HEADER_LENGTH ||
                (size_t)cbytes > states[i].bytes + BLOSC2_MAX_OVERHEAD ||
                nbytes != (int32_t)states[i].bytes ||
                blocksize != (int32_t)(states[i].block_items * 8) ||
                (size_t)cbytes > UINTPTR_MAX - (uintptr_t)chunk) {
                status = storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage chunk metadata");
                break;
            }
            if (chunk[BLOSC2_CHUNK_TYPESIZE] != 8 ||
                (chunk[BLOSC2_CHUNK_FLAGS] >> 5) > BLOSC_ZSTD_LIB) {
                status = storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported storage chunk codec");
                break;
            }
            for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
                if (chunk[BLOSC2_CHUNK_FILTER_CODES + f] > BLOSC_DELTA) {
                    status = storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported storage chunk filter");
                    break;
                }
            }
            if (status != ME_SUCCESS) break;
            if (output.capacity && (uintptr_t)chunk < (uintptr_t)output.data + output.capacity &&
                (uintptr_t)output.data < (uintptr_t)chunk + (size_t)cbytes) {
                status = storage_error(error, ME_ERR_UNSUPPORTED, "Aliased compressed storage output");
                break;
            }
        }
    }
    for (size_t i = 0; status == ME_SUCCESS && i < ninputs; i++) {
        states[i].decoded = states[i].bytes ? malloc(states[i].bytes) : NULL;
        if (states[i].decoded) report->decode_bytes += states[i].bytes;
        blosc2_dparams params = BLOSC2_DPARAMS_DEFAULTS;
        params.nthreads = 1;
        states[i].dctx = blosc2_create_dctx(params);
        if ((states[i].bytes && !states[i].decoded) || !states[i].dctx) {
            status = storage_error(error, ME_ERR_OOM, "Cannot allocate storage decode workspace");
            break;
        }
        readers[i] = (b2nd_exec_reader){&states[i], storage_read};
    }
    if (status == ME_SUCCESS) {
        status = b2nd_exec_execute_readers(schedule, views, readers, ninputs, output,
                                          raise_mask, max_gather_bytes, &report->execution, error);
    }
    for (size_t i = 0; i < ninputs; i++) {
        if (states[i].dctx) blosc2_free_ctx(states[i].dctx);
        free(states[i].decoded);
    }
    free(states);
    return status;
}

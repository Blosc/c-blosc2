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
    size_t itemsize, bytes, items, block_items;
    size_t chunk_grid[B2ND_EXEC_MAX_RANK], block_grid[B2ND_EXEC_MAX_RANK];
    int64_t cached_chunk;
    size_t decodes, decoded_bytes, cache_hits, read_spans;
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

static bool storage_dtype_matches(const char *dtype, me_dtype expected, size_t width) {
    if (!dtype || !dtype[0] || !dtype[1] || !dtype[2] || dtype[3] != '\0') return false;
    uint16_t endian = 1;
    char host = *(const unsigned char *)&endian ? '<' : '>';
    if (width == 1) {
        if (dtype[0] != '=' && dtype[0] != '|' && dtype[0] != host) return false;
    } else if (dtype[0] != '=' && dtype[0] != host) {
        return false;
    }
    const char *suffix = NULL;
    switch (expected) {
        case ME_BOOL: suffix = "b1"; break;
        case ME_INT8: suffix = "i1"; break;
        case ME_UINT8: suffix = "u1"; break;
        case ME_INT16: suffix = "i2"; break;
        case ME_UINT16: suffix = "u2"; break;
        case ME_INT32: suffix = "i4"; break;
        case ME_UINT32: suffix = "u4"; break;
        case ME_INT64: suffix = "i8"; break;
        case ME_UINT64: suffix = "u8"; break;
        case ME_FLOAT32: suffix = "f4"; break;
        case ME_FLOAT64: suffix = "f8"; break;
        default: return false;
    }
    return strcmp(dtype + 1, suffix) == 0;
}

static me_status storage_validate(storage_input *state, const b2nd_exec_input_meta *meta,
                                   me_error *error) {
    const b2nd_array_t *a = state->array;
    size_t width = meta->itemsize;
    if (!a || !a->sc || a->ndim < 1 || a->ndim > B2ND_EXEC_MAX_RANK ||
        (size_t)a->ndim != meta->ndim || !width || width > 8 ||
        a->dtype_format != DTYPE_NUMPY_FORMAT ||
        !storage_dtype_matches(a->dtype, meta->dtype, width) ||
        a->sc->typesize != (int32_t)width || a->sc->frame ||
        (a->sc->nchunks && !a->sc->data) || a->sc->compcode > BLOSC_ZSTD) {
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
    if (physical > (size_t)INT32_MAX / width || a->extchunknitems != (int64_t)physical ||
        a->blocknitems != (int64_t)block_items || a->chunknitems != (int64_t)chunk_items ||
        a->nitems != (int64_t)items || a->sc->nchunks != (int64_t)chunks ||
        (chunks && a->sc->chunksize != (int32_t)(physical * width))) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Inconsistent storage geometry");
    }
    state->itemsize = width;
    state->items = items;
    state->block_items = block_items;
    state->bytes = items ? physical * width : 0;
    state->cached_chunk = -1;
    return ME_SUCCESS;
}

static me_status storage_read(void *context, size_t start, size_t count,
                               me_mut_buffer output, me_error *error) {
    storage_input *state = context;
    const b2nd_array_t *a = state->array;
    size_t width = state->itemsize;
    if (start > state->items || count > state->items - start || output.capacity < count * width) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid logical storage span");
    }
    state->read_spans++;
    int last = a->ndim - 1;
    size_t lane = 0;
    while (lane < count) {
        size_t remaining = start + lane, coords[B2ND_EXEC_MAX_RANK];
        for (int i = a->ndim; i-- > 0;) {
            coords[i] = remaining % (size_t)a->shape[i];
            remaining /= (size_t)a->shape[i];
        }
        size_t chunk = 0, block = 0, item = 0;
        size_t local_last = 0, item_last = 0;
        for (int i = 0; i < a->ndim; i++) {
            size_t local = coords[i] % (size_t)a->chunkshape[i];
            size_t bitem = local % (size_t)a->blockshape[i];
            chunk = chunk * state->chunk_grid[i] + coords[i] / (size_t)a->chunkshape[i];
            block = block * state->block_grid[i] + local / (size_t)a->blockshape[i];
            item = item * (size_t)a->blockshape[i] + bitem;
            if (i == last) {
                local_last = local;
                item_last = bitem;
            }
        }
        size_t rem_logical = (size_t)a->shape[last] - coords[last];
        size_t rem_chunk = (size_t)a->chunkshape[last] - local_last;
        size_t rem_block = (size_t)a->blockshape[last] - item_last;
        size_t run = count - lane;
        if (rem_logical < run) run = rem_logical;
        if (rem_chunk < run) run = rem_chunk;
        if (rem_block < run) run = rem_block;
        if (state->cached_chunk != (int64_t)chunk) {
            /* The admitted in-memory representation owns these bytes. Avoid
             * schunk_get_chunk: it mutates shared current_nchunk even here. */
            const uint8_t *encoded = a->sc->data[chunk];
            int32_t nbytes = 0, cbytes = 0, blocksize = 0;
            int rc = -1;
            if (encoded && blosc2_cbuffer_sizes(encoded, &nbytes, &cbytes, &blocksize) == 0 &&
                cbytes >= BLOSC_MIN_HEADER_LENGTH && nbytes == (int32_t)state->bytes &&
                blocksize == (int32_t)(state->block_items * width)) {
                rc = blosc2_decompress_ctx(state->dctx, encoded, cbytes,
                                           state->decoded, (int32_t)state->bytes);
            }
            if (rc != (int32_t)state->bytes) {
                return storage_error(error, ME_ERR_EVAL, "Storage chunk decode failed");
            }
            state->cached_chunk = (int64_t)chunk;
            state->decodes++;
            state->decoded_bytes += state->bytes;
            if (run > 1) state->cache_hits += run - 1;
        } else {
            state->cache_hits += run;
        }
        size_t offset = (block * state->block_items + item) * width;
        memcpy((unsigned char *)output.data + lane * width, state->decoded + offset, run * width);
        lane += run;
    }
    return ME_SUCCESS;
}

me_status b2nd_exec_execute_arrays_ex(const b2nd_exec_schedule *schedule,
                                     const b2nd_array_t *const *arrays,
                                     const b2nd_exec_view *views, size_t ninputs,
                                     me_mut_buffer output, unsigned raise_mask,
                                     size_t max_decode_bytes, size_t max_gather_bytes,
                                     const b2nd_exec_jobs *jobs,
                                     b2nd_exec_storage_report *report, me_error *error) {
    if (report) *report = (b2nd_exec_storage_report){0};
    if (error) *error = (me_error){0};
    if (!schedule || (!arrays && !views) || !report || !ninputs || ninputs > B2ND_EXEC_MAX_INPUTS ||
        (jobs && (jobs->struct_size != sizeof(*jobs) || !jobs->workers))) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage execution bindings");
    }
    b2nd_exec_input_meta meta;
    if (b2nd_exec_schedule_input(schedule, ninputs, &meta) == ME_SUCCESS) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Storage input count mismatch");
    }
    storage_input base_states[B2ND_EXEC_MAX_INPUTS] = {0};
    bool is_reader[B2ND_EXEC_MAX_INPUTS] = {0};
    b2nd_exec_view bound_views[B2ND_EXEC_MAX_INPUTS] = {0};
    me_status status = ME_SUCCESS;
    size_t per_job_decode = 0;
    for (size_t i = 0; i < ninputs; i++) {
        if (b2nd_exec_schedule_input(schedule, i, &meta) != ME_SUCCESS) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Storage input count mismatch");
        }
        const b2nd_array_t *arr = arrays ? arrays[i] : NULL;
        if (arr) {
            is_reader[i] = true;
            base_states[i].array = arr;
            status = storage_validate(&base_states[i], &meta, error);
            if (status != ME_SUCCESS) return status;
            if (base_states[i].bytes > max_decode_bytes - per_job_decode) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Storage decode budget exceeded");
            }
            per_job_decode += base_states[i].bytes;
        } else if (views) {
            bound_views[i] = views[i];
        } else {
            return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage input binding");
        }
    }
    /* In-memory chunk headers are storage metadata. Do not permit a resident
     * destination to overwrite borrowed compressed bytes during traversal. */
    if (output.capacity > UINTPTR_MAX - (uintptr_t)output.data) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage output extent");
    }
    for (size_t i = 0; i < ninputs; i++) {
        if (!is_reader[i]) continue;
        size_t width = base_states[i].itemsize;
        for (int64_t c = 0; c < arrays[i]->sc->nchunks; c++) {
            const uint8_t *chunk = arrays[i]->sc->data[c];
            int32_t nbytes, cbytes, blocksize;
            if (!chunk || blosc2_cbuffer_sizes(chunk, &nbytes, &cbytes, &blocksize) < 0 ||
                cbytes < BLOSC_EXTENDED_HEADER_LENGTH ||
                (size_t)cbytes > base_states[i].bytes + BLOSC2_MAX_OVERHEAD ||
                nbytes != (int32_t)base_states[i].bytes ||
                blocksize != (int32_t)(base_states[i].block_items * width) ||
                (size_t)cbytes > UINTPTR_MAX - (uintptr_t)chunk) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage chunk metadata");
            }
            if (chunk[BLOSC2_CHUNK_TYPESIZE] != (uint8_t)width ||
                (chunk[BLOSC2_CHUNK_FLAGS] >> 5) > BLOSC_ZSTD_LIB) {
                return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported storage chunk codec");
            }
            for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
                if (chunk[BLOSC2_CHUNK_FILTER_CODES + f] > BLOSC_DELTA) {
                    return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported storage chunk filter");
                }
            }
            if (output.capacity && (uintptr_t)chunk < (uintptr_t)output.data + output.capacity &&
                (uintptr_t)output.data < (uintptr_t)chunk + (size_t)cbytes) {
                return storage_error(error, ME_ERR_UNSUPPORTED, "Aliased compressed storage output");
            }
        }
    }
    b2nd_exec_jobs capped_jobs;
    const b2nd_exec_jobs *eff_jobs = jobs;
    size_t ntasks = 1;
    status = b2nd_exec_plan_readers(schedule, bound_views, is_reader, ninputs, output,
                                    max_gather_bytes, jobs, &ntasks, error);
    if (status != ME_SUCCESS) return status;
    if (jobs && ntasks > 1 && per_job_decode) {
        size_t dec_fit = max_decode_bytes / per_job_decode;
        if (ntasks > dec_fit) {
            ntasks = dec_fit ? dec_fit : 1;
            capped_jobs = *jobs;
            capped_jobs.workers = ntasks;
            eff_jobs = &capped_jobs;
        }
    }
    if (ntasks > B2ND_EXEC_MAX_TASKS) return storage_error(error, ME_ERR_INVALID_ARG, "Task limit exceeded");
    storage_input *states = calloc(ntasks * ninputs, sizeof(*states));
    if (!states) return storage_error(error, ME_ERR_OOM, "Cannot allocate storage binding metadata");
    report->storage_metadata_bytes = ntasks * ninputs * sizeof(*states);
    b2nd_exec_reader stack_readers[B2ND_EXEC_MAX_INPUTS] = {0};
    b2nd_exec_reader *readers = stack_readers;
    if (ntasks > 1) {
        readers = calloc(ntasks * ninputs, sizeof(*readers));
        if (!readers) {
            free(states);
            return storage_error(error, ME_ERR_OOM, "Cannot allocate storage reader workspace");
        }
    }
    for (size_t t = 0; status == ME_SUCCESS && t < ntasks; t++) {
        for (size_t i = 0; i < ninputs; i++) {
            size_t idx = t * ninputs + i;
            if (!is_reader[i]) continue;
            states[idx] = base_states[i];
            states[idx].decoded = states[idx].bytes ? malloc(states[idx].bytes) : NULL;
            if (states[idx].decoded) report->decode_bytes += states[idx].bytes;
            blosc2_dparams params = BLOSC2_DPARAMS_DEFAULTS;
            params.nthreads = 1;
            states[idx].dctx = blosc2_create_dctx(params);
            if ((states[idx].bytes && !states[idx].decoded) || !states[idx].dctx) {
                status = storage_error(error, ME_ERR_OOM, "Cannot allocate storage decode workspace");
                break;
            }
            readers[idx] = (b2nd_exec_reader){&states[idx], storage_read};
        }
    }
    if (status == ME_SUCCESS) {
        status = b2nd_exec_execute_readers_ex(schedule, bound_views, readers,
                                             ntasks > 1 ? ninputs : 0, ntasks, ninputs,
                                             output, raise_mask, max_gather_bytes,
                                             eff_jobs, &report->execution, error);
    }
    for (size_t idx = 0; idx < ntasks * ninputs; idx++) {
        report->decodes += states[idx].decodes;
        report->decoded_bytes += states[idx].decoded_bytes;
        report->cache_hits += states[idx].cache_hits;
        report->read_spans += states[idx].read_spans;
        if (states[idx].dctx) blosc2_free_ctx(states[idx].dctx);
        free(states[idx].decoded);
    }
    if (readers != stack_readers) free(readers);
    free(states);
    return status;
}

me_status b2nd_exec_execute_arrays(const b2nd_exec_schedule *schedule,
                                  const b2nd_array_t *const *inputs, size_t ninputs,
                                  me_mut_buffer output, unsigned raise_mask,
                                  size_t max_decode_bytes, size_t max_gather_bytes,
                                  b2nd_exec_storage_report *report, me_error *error) {
    return b2nd_exec_execute_arrays_ex(schedule, inputs, NULL, ninputs, output, raise_mask,
                                       max_decode_bytes, max_gather_bytes, NULL, report, error);
}

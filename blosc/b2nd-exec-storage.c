/* Native storage is a binding adapter, not another numerical scheduler. */
#include "b2nd-exec-storage.h"
#include "b2nd-exec-private.h"
#include "frame.h"
#include "threading.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORAGE_BLOCK_CACHE_SLOTS 4
#define STORAGE_CHUNK_CACHE_SLOTS 8

typedef struct {
    const b2nd_array_t *array;
    blosc2_context *dctx;
    blosc2_pthread_mutex_t *frame_mutex;
    unsigned char *decoded;
    unsigned char *chunk_cache;
    size_t nslots, clock;
    int64_t cached_chunks[STORAGE_CHUNK_CACHE_SLOTS];
    size_t ages[STORAGE_CHUNK_CACHE_SLOTS];
    size_t itemsize, bytes, items, block_items;
    size_t chunk_grid[B2ND_EXEC_MAX_RANK], block_grid[B2ND_EXEC_MAX_RANK];
    size_t dom_ndim, reduction_lanes;
    int64_t dom_shape[B2ND_EXEC_MAX_RANK];
    bool is_reduction, reduced_axis[B2ND_EXEC_MAX_RANK], fast_linear;
    size_t decodes, decoded_bytes, cache_hits, read_spans;
} storage_input;

static int storage_fetch_chunk(const b2nd_array_t *a, blosc2_pthread_mutex_t *lock,
                               int64_t chunk, const uint8_t **out_chunk, bool *needs_free) {
    *out_chunk = NULL;
    *needs_free = false;
    if (!a || !a->sc || chunk < 0 || chunk >= a->sc->nchunks) return BLOSC2_ERROR_INVALID_PARAM;
    if (!a->sc->frame) {
        *out_chunk = a->sc->data[chunk];
        return 0;
    }
    blosc2_frame_s *frame = (blosc2_frame_s *)a->sc->frame;
    if (lock) blosc2_pthread_mutex_lock(lock);
    int rc = frame_lock(frame, false);
    if (rc >= 0) {
        uint8_t *ptr = NULL;
        rc = frame_get_chunk(frame, chunk, &ptr, needs_free);
        frame_unlock(frame);
        *out_chunk = ptr;
    }
    if (lock) blosc2_pthread_mutex_unlock(lock);
    return rc;
}

static me_status storage_error(me_error *error, me_status status, const char *message) {
    if (error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

static void storage_mutexes_destroy(blosc2_pthread_mutex_t *mutexes, bool *live, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (live[i]) {
            blosc2_pthread_mutex_destroy(&mutexes[i]);
            live[i] = false;
        }
    }
}

static me_status storage_frame_mutex_init(blosc2_pthread_mutex_t *mutexes, bool *live,
                                    size_t slot, me_error *error) {
#ifdef _WIN32
    (void)error;
    blosc2_pthread_mutex_init(&mutexes[slot], NULL);
#else
    if (blosc2_pthread_mutex_init(&mutexes[slot], NULL) != 0) {
        storage_mutexes_destroy(mutexes, live, slot);
        return storage_error(error, ME_ERR_OOM, "Cannot initialize storage frame mutex");
    }
#endif
    live[slot] = true;
    return ME_SUCCESS;
}

static bool storage_product(size_t *product, size_t factor) {
    if (factor && *product > (size_t)INT64_MAX / factor) return false;
    *product *= factor;
    return true;
}

static bool storage_dtype_matches(const char *dtype, me_dtype expected, size_t width) {
    if (!dtype || !dtype[0] || !dtype[1]) return false;
    uint16_t endian = 1;
    char host = *(const unsigned char *)&endian ? '<' : '>';
    if (expected == ME_STRING) {
        if (dtype[1] == 'S') {
            if (dtype[0] != '=' && dtype[0] != '|' && dtype[0] != host) return false;
        } else if (dtype[1] == 'U') {
            if (dtype[0] != '=' && dtype[0] != host) return false;
        } else {
            return false;
        }
        if (!dtype[2]) return false;
        char *endp = NULL;
        unsigned long long count = strtoull(dtype + 2, &endp, 10);
        if (!endp || *endp != '\0') return false;
        size_t unit = dtype[1] == 'U' ? 4 : 1;
        return count <= SIZE_MAX / unit && (size_t)count * unit == width;
    }
    if (!dtype[2]) return false;
    if (expected == ME_COMPLEX128) {
        return (dtype[0] == '=' || dtype[0] == host) &&
            strcmp(dtype + 1, "c16") == 0 && width == 16;
    }
    if (dtype[3] != '\0') return false;
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
        case ME_COMPLEX64: suffix = width == 8 ? "c8" : NULL; break;
        default: return false;
    }
    return suffix && strcmp(dtype + 1, suffix) == 0;
}

static size_t storage_required_align(me_dtype dtype, size_t width) {
    if (dtype == ME_STRING) return 1;
    if (dtype == ME_COMPLEX64) return sizeof(float);
    if (dtype == ME_COMPLEX128) return sizeof(double);
    return width ? width : 1;
}

static me_status storage_validate_chunks(const storage_input *state,
                                         const me_mut_buffer *alias_output,
                                         me_error *error) {
    const b2nd_array_t *a = state->array;
    size_t width = state->itemsize;
    if (a->sc->frame && alias_output && alias_output->capacity) {
        const blosc2_frame_s *frame = (const blosc2_frame_s *)a->sc->frame;
        if (frame->cframe && frame->len > 0 &&
            (size_t)frame->len <= UINTPTR_MAX - (uintptr_t)frame->cframe &&
            (uintptr_t)frame->cframe < (uintptr_t)alias_output->data + alias_output->capacity &&
            (uintptr_t)alias_output->data < (uintptr_t)frame->cframe + (size_t)frame->len) {
            return storage_error(error, ME_ERR_UNSUPPORTED, "Aliased compressed storage output");
        }
    }
    for (int64_t c = 0; c < a->sc->nchunks; c++) {
        const uint8_t *chunk = NULL;
        bool needs_free = false;
        int rc = storage_fetch_chunk(a, NULL, c, &chunk, &needs_free);
        int32_t nbytes, cbytes, blocksize;
        if (rc < 0 || !chunk || blosc2_cbuffer_sizes(chunk, &nbytes, &cbytes, &blocksize) < 0 ||
            cbytes < BLOSC_EXTENDED_HEADER_LENGTH ||
            (size_t)cbytes > state->bytes + BLOSC2_MAX_OVERHEAD ||
            nbytes != (int32_t)state->bytes ||
            blocksize != (int32_t)(state->block_items * width) ||
            (size_t)cbytes > UINTPTR_MAX - (uintptr_t)chunk) {
            if (needs_free) free((void *)chunk);
            return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage chunk metadata");
        }
        if ((width <= 255 && chunk[BLOSC2_CHUNK_TYPESIZE] != (uint8_t)width) ||
            (chunk[BLOSC2_CHUNK_FLAGS] >> 5) > BLOSC_ZSTD_LIB) {
            if (needs_free) free((void *)chunk);
            return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported storage chunk codec");
        }
        for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
            if (chunk[BLOSC2_CHUNK_FILTER_CODES + f] > BLOSC_DELTA) {
                if (needs_free) free((void *)chunk);
                return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported storage chunk filter");
            }
        }
        if (alias_output && alias_output->capacity &&
            (uintptr_t)chunk < (uintptr_t)alias_output->data + alias_output->capacity &&
            (uintptr_t)alias_output->data < (uintptr_t)chunk + (size_t)cbytes) {
            if (needs_free) free((void *)chunk);
            return storage_error(error, ME_ERR_UNSUPPORTED, "Aliased compressed storage output");
        }
        if (needs_free) free((void *)chunk);
    }
    return ME_SUCCESS;
}

static me_status storage_validate(storage_input *state, const b2nd_exec_input_meta *meta,
                                  me_error *error) {
    const b2nd_array_t *a = state->array;
    size_t width = meta->itemsize;
    if (!a || !a->sc || a->ndim < 0 || a->ndim > B2ND_EXEC_MAX_RANK ||
        (size_t)a->ndim != meta->ndim || !width ||
        (width > 8 && meta->dtype != ME_COMPLEX128 && meta->dtype != ME_STRING) ||
        a->dtype_format != DTYPE_NUMPY_FORMAT ||
        !storage_dtype_matches(a->dtype, meta->dtype, width) ||
        a->sc->typesize != (int32_t)width ||
        (!a->sc->frame && a->sc->nchunks && !a->sc->data) ||
        (a->sc->frame && ((const blosc2_frame_s *)a->sc->frame)->schunk != a->sc) ||
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
    return ME_SUCCESS;
}

static me_status storage_decode_chunk(storage_input *state, size_t chunk,
                                       me_error *error, bool *was_miss) {
    size_t victim = 0;
    for (size_t s = 0; s < state->nslots; s++) {
        if (state->cached_chunks[s] == (int64_t)chunk) {
            state->ages[s] = ++state->clock;
            state->decoded = state->chunk_cache + s * state->bytes;
            *was_miss = false;
            return ME_SUCCESS;
        }
        if (state->ages[s] < state->ages[victim]) victim = s;
    }
    state->decoded = state->chunk_cache + victim * state->bytes;
    /* A failed decode must not leave an old cache identity on overwritten bytes. */
    state->cached_chunks[victim] = -1;
    const b2nd_array_t *a = state->array;
    size_t width = state->itemsize;
    /* Fetch without mutating schunk->current_nchunk; serialize frame reads via frame_mutex. */
    const uint8_t *encoded = NULL;
    bool needs_free = false;
    int fetch_rc = storage_fetch_chunk(a, state->frame_mutex, (int64_t)chunk, &encoded, &needs_free);
    int32_t nbytes = 0, cbytes = 0, blocksize = 0;
    int rc = -1;
    if (fetch_rc >= 0 && encoded &&
        blosc2_cbuffer_sizes(encoded, &nbytes, &cbytes, &blocksize) == 0 &&
        cbytes >= BLOSC_MIN_HEADER_LENGTH && nbytes == (int32_t)state->bytes &&
        blocksize == (int32_t)(state->block_items * width)) {
        rc = blosc2_decompress_ctx(state->dctx, encoded, cbytes,
                                   state->decoded, (int32_t)state->bytes);
    }
    if (needs_free) free((void *)encoded);
    if (rc != (int32_t)state->bytes) {
        return storage_error(error, ME_ERR_EVAL, "Storage chunk decode failed");
    }
    state->cached_chunks[victim] = (int64_t)chunk;
    state->ages[victim] = ++state->clock;
    state->decodes++;
    state->decoded_bytes += state->bytes;
    *was_miss = true;
    return ME_SUCCESS;
}

static me_status storage_read(void *context, size_t group, size_t pos, size_t count,
                              me_mut_buffer output, me_error *error) {
    storage_input *state = context;
    const b2nd_array_t *a = state->array;
    size_t width = state->itemsize;
    if (output.capacity < count * width) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid logical storage span");
    }
    if (state->fast_linear) {
        size_t start = state->is_reduction ? group * state->reduction_lanes + pos : pos;
        if (start > state->items || count > state->items - start) {
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
            bool miss = false;
            me_status st = storage_decode_chunk(state, chunk, error, &miss);
            if (st != ME_SUCCESS) return st;
            if (miss) {
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

    state->read_spans++;
    if (!count) return ME_SUCCESS;
    size_t coords[B2ND_EXEC_MAX_RANK] = {0};
    size_t g_rem = group, l_rem = pos;
    size_t fast_axis = state->dom_ndim - 1;
    for (size_t d = state->dom_ndim; d-- > 0;) {
        size_t *idx = (state->is_reduction && !state->reduced_axis[d]) ? &g_rem : &l_rem;
        coords[d] = *idx % (size_t)state->dom_shape[d];
        *idx /= (size_t)state->dom_shape[d];
    }
    if (state->is_reduction) {
        for (size_t d = state->dom_ndim; d-- > 0;) {
            if (state->reduced_axis[d]) {
                fast_axis = d;
                break;
            }
        }
    }
    size_t shift = state->dom_ndim - (size_t)a->ndim;
    size_t a_last = (size_t)a->ndim - 1;
    size_t lane = 0;
    while (lane < count) {
        size_t chunk = 0, block = 0, item = 0;
        size_t c_last = 0, b_last = 0;
        for (int d = 0; d < a->ndim; d++) {
            size_t coord = a->shape[d] == 1 ? 0 : coords[shift + (size_t)d];
            size_t cdim = (size_t)a->chunkshape[d];
            size_t bdim = (size_t)a->blockshape[d];
            size_t local = coord % cdim;
            size_t bitem = local % bdim;
            chunk = chunk * state->chunk_grid[d] + coord / cdim;
            block = block * state->block_grid[d] + local / bdim;
            item = item * bdim + bitem;
            if ((size_t)d == a_last) {
                c_last = local;
                b_last = bitem;
            }
        }
        bool fast_bcast = fast_axis < shift || a->shape[fast_axis - shift] == 1;
        bool fast_inner = !fast_bcast && (fast_axis - shift == a_last);
        size_t rem_dom = (size_t)state->dom_shape[fast_axis] - coords[fast_axis];
        size_t run = count - lane;
        if (rem_dom < run) run = rem_dom;
        if (fast_inner) {
            size_t rem_chunk = (size_t)a->chunkshape[a_last] - c_last;
            size_t rem_block = (size_t)a->blockshape[a_last] - b_last;
            if (rem_chunk < run) run = rem_chunk;
            if (rem_block < run) run = rem_block;
        } else if (!fast_bcast) {
            run = 1;
        }
        bool miss = false;
        me_status st = storage_decode_chunk(state, chunk, error, &miss);
        if (st != ME_SUCCESS) return st;
        if (miss) {
            if (run > 1) state->cache_hits += run - 1;
        } else {
            state->cache_hits += run;
        }
        size_t offset = (block * state->block_items + item) * width;
        unsigned char *dst = (unsigned char *)output.data + lane * width;
        const unsigned char *src = state->decoded + offset;
        if (fast_bcast) {
            for (size_t k = 0; k < run; k++) {
                memcpy(dst + k * width, src, width);
            }
        } else {
            memcpy(dst, src, run * width);
        }
        lane += run;
        if (lane < count) {
            coords[fast_axis] += run;
            if (coords[fast_axis] >= (size_t)state->dom_shape[fast_axis]) {
                coords[fast_axis] = 0;
                for (size_t d = fast_axis; d-- > 0;) {
                    if (state->is_reduction && !state->reduced_axis[d]) continue;
                    if (++coords[d] < (size_t)state->dom_shape[d]) break;
                    coords[d] = 0;
                }
            }
        }
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
    b2nd_exec_plan_info plan_info = {0};
    b2nd_exec_schedule_info sched_info = {0};
    b2nd_exec_schedule_kernel(schedule, &plan_info, &sched_info, NULL);
    bool reduced_axis[B2ND_EXEC_MAX_RANK] = {0};
    b2nd_exec_schedule_layout(schedule, 0, NULL, NULL, reduced_axis);
    blosc2_pthread_mutex_t frame_mutexes[B2ND_EXEC_MAX_INPUTS];
    bool has_frame_mutex[B2ND_EXEC_MAX_INPUTS] = {0};
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
            bool matches = false, suffix = false;
            b2nd_exec_schedule_layout(schedule, i, &matches, &suffix, NULL);
            is_reader[i] = true;
            base_states[i].array = arr;
            base_states[i].dom_ndim = sched_info.ndim;
            memcpy(base_states[i].dom_shape, sched_info.shape, sizeof(base_states[i].dom_shape));
            base_states[i].reduction_lanes = sched_info.reduction_lanes;
            base_states[i].is_reduction = plan_info.reduction;
            memcpy(base_states[i].reduced_axis, reduced_axis, sizeof(reduced_axis));
            base_states[i].fast_linear = matches && (!plan_info.reduction || suffix);
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
        status = storage_validate_chunks(&base_states[i], &output, error);
        if (status != ME_SUCCESS) return status;
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
    /* Keep the admitted task geometry; spend only the remaining aggregate
     * decode budget on per-worker LRU slots. Logical traversal is unchanged. */
    size_t cache_slots[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t extra = max_decode_bytes - ntasks * per_job_decode;
    for (size_t i = 0; i < ninputs; i++) {
        if (is_reader[i] && base_states[i].bytes) cache_slots[i] = 1;
    }
    for (size_t pass = 1; pass < STORAGE_CHUNK_CACHE_SLOTS; pass++) {
        bool grew = false;
        for (size_t i = 0; i < ninputs; i++) {
            if (!cache_slots[i] || cache_slots[i] >= (size_t)arrays[i]->sc->nchunks) continue;
            size_t cost = ntasks * base_states[i].bytes;
            if (cost <= extra) {
                cache_slots[i]++;
                extra -= cost;
                grew = true;
            }
        }
        if (!grew) break;
    }
    for (size_t i = 0; i < ninputs; i++) {
        if (ntasks > 1 && is_reader[i] && base_states[i].array->sc->frame) {
            status = storage_frame_mutex_init(frame_mutexes, has_frame_mutex, i, error);
            if (status != ME_SUCCESS) return status;
            base_states[i].frame_mutex = &frame_mutexes[i];
        }
    }
    storage_input *states = calloc(ntasks * ninputs, sizeof(*states));
    if (!states) {
        storage_mutexes_destroy(frame_mutexes, has_frame_mutex, ninputs);
        return storage_error(error, ME_ERR_OOM, "Cannot allocate storage binding metadata");
    }
    report->storage_metadata_bytes = ntasks * ninputs * sizeof(*states);
    b2nd_exec_reader stack_readers[B2ND_EXEC_MAX_INPUTS] = {0};
    b2nd_exec_reader *readers = stack_readers;
    if (ntasks > 1) {
        readers = calloc(ntasks * ninputs, sizeof(*readers));
        if (!readers) {
            storage_mutexes_destroy(frame_mutexes, has_frame_mutex, ninputs);
            free(states);
            return storage_error(error, ME_ERR_OOM, "Cannot allocate storage reader workspace");
        }
    }
    for (size_t t = 0; status == ME_SUCCESS && t < ntasks; t++) {
        for (size_t i = 0; i < ninputs; i++) {
            size_t idx = t * ninputs + i;
            if (!is_reader[i]) continue;
            states[idx] = base_states[i];
            states[idx].nslots = cache_slots[i];
            size_t cache_bytes = states[idx].bytes * states[idx].nslots;
            states[idx].chunk_cache = cache_bytes ? malloc(cache_bytes) : NULL;
            for (size_t s = 0; s < states[idx].nslots; s++) states[idx].cached_chunks[s] = -1;
            if (states[idx].chunk_cache) report->decode_bytes += cache_bytes;
            blosc2_dparams params = BLOSC2_DPARAMS_DEFAULTS;
            params.nthreads = 1;
            states[idx].dctx = blosc2_create_dctx(params);
            if ((cache_bytes && !states[idx].chunk_cache) || !states[idx].dctx) {
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
        free(states[idx].chunk_cache);
    }
    storage_mutexes_destroy(frame_mutexes, has_frame_mutex, ninputs);
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

typedef struct {
    const b2nd_array_t *array;
    blosc2_context *dctx;
    blosc2_pthread_mutex_t *frame_mutex;
    unsigned char *blocks[STORAGE_BLOCK_CACHE_SLOTS];
    int64_t cached_chunk[STORAGE_BLOCK_CACHE_SLOTS];
    int64_t cached_block[STORAGE_BLOCK_CACHE_SLOTS];
    uint64_t lru_tick[STORAGE_BLOCK_CACHE_SLOTS];
    uint64_t tick;
    size_t nslots;
    size_t itemsize, block_items, block_bytes;
    size_t chunk_grid[B2ND_EXEC_MAX_RANK], block_grid[B2ND_EXEC_MAX_RANK];
    size_t decodes, decoded_bytes, cache_hits, read_spans;
} storage_block_cache;

static me_status storage_read_box_cached(storage_block_cache *cache, size_t dom_ndim,
                                         const size_t *origin, const size_t *extent,
                                         unsigned char *dst, me_error *error) {
    const b2nd_array_t *a = cache->array;
    size_t ndim = (size_t)a->ndim;
    size_t width = cache->itemsize;
    size_t total = 1;
    for (size_t d = 0; d < dom_ndim; d++) {
        total *= extent[d];
    }
    cache->read_spans++;
    if (!total) return ME_SUCCESS;
    if (!ndim || !dom_ndim) {
        if (cache->cached_chunk[0] != 0 || cache->cached_block[0] != 0) {
            const uint8_t *encoded = NULL;
            bool needs_free = false;
            int fetch_rc = storage_fetch_chunk(a, cache->frame_mutex, 0, &encoded, &needs_free);
            int32_t nbytes = 0, cbytes = 0, blocksize = 0;
            int got = -1;
            if (fetch_rc >= 0 && encoded &&
                blosc2_cbuffer_sizes(encoded, &nbytes, &cbytes, &blocksize) == 0) {
                got = blosc2_getitem_ctx(cache->dctx, encoded, cbytes, 0,
                                         (int)cache->block_items, cache->blocks[0],
                                         (int32_t)cache->block_bytes);
            }
            if (needs_free) free((void *)encoded);
            if (got != (int)cache->block_bytes) {
                return storage_error(error, ME_ERR_EVAL, "Storage block decode failed");
            }
            cache->cached_chunk[0] = 0;
            cache->cached_block[0] = 0;
            cache->decodes++;
            cache->decoded_bytes += cache->block_bytes;
            if (total > 1) cache->cache_hits += total - 1;
        } else {
            cache->cache_hits += total;
        }
        for (size_t k = 0; k < total; k++) {
            memcpy(dst + k * width, cache->blocks[0], width);
        }
        return ME_SUCCESS;
    }
    size_t shift = dom_ndim - ndim;
    size_t d_last = dom_ndim - 1;
    size_t a_last = ndim - 1;
    bool last_bcast = (d_last < shift) || (a->shape[a_last] == 1);
    size_t local_c[B2ND_EXEC_MAX_RANK] = {0};
    size_t lane = 0;
    while (lane < total) {
        size_t chunk = 0, block = 0, item = 0;
        size_t c_last = 0, b_last = 0;
        for (size_t d = 0; d < ndim; d++) {
            size_t coord = a->shape[d] == 1 ? 0 : (origin[shift + d] + local_c[shift + d]);
            size_t cdim = (size_t)a->chunkshape[d];
            size_t bdim = (size_t)a->blockshape[d];
            size_t local = coord % cdim;
            size_t bitem = local % bdim;
            chunk = chunk * cache->chunk_grid[d] + coord / cdim;
            block = block * cache->block_grid[d] + local / bdim;
            item = item * bdim + bitem;
            if (d == a_last) {
                c_last = local;
                b_last = bitem;
            }
        }
        size_t rem_box = extent[d_last] - local_c[d_last];
        size_t run = rem_box;
        if (!last_bcast) {
            size_t rem_chunk = (size_t)a->chunkshape[a_last] - c_last;
            size_t rem_block = (size_t)a->blockshape[a_last] - b_last;
            if (rem_chunk < run) run = rem_chunk;
            if (rem_block < run) run = rem_block;
        }
        size_t slot = cache->nslots;
        bool hit = false;
        for (size_t s = 0; s < cache->nslots; s++) {
            if (cache->cached_chunk[s] == (int64_t)chunk &&
                cache->cached_block[s] == (int64_t)block) {
                slot = s;
                hit = true;
                break;
            }
        }
        if (!hit) {
            slot = 0;
            for (size_t s = 0; s < cache->nslots; s++) {
                if (cache->cached_chunk[s] < 0) {
                    slot = s;
                    break;
                }
                if (cache->lru_tick[s] < cache->lru_tick[slot]) {
                    slot = s;
                }
            }
            const uint8_t *encoded = NULL;
            bool needs_free = false;
            int fetch_rc = storage_fetch_chunk(a, cache->frame_mutex, (int64_t)chunk,
                                               &encoded, &needs_free);
            int32_t nbytes = 0, cbytes = 0, blocksize = 0;
            int got = -1;
            if (fetch_rc >= 0 && encoded &&
                blosc2_cbuffer_sizes(encoded, &nbytes, &cbytes, &blocksize) == 0) {
                got = blosc2_getitem_ctx(cache->dctx, encoded, cbytes,
                                         (int)(block * cache->block_items),
                                         (int)cache->block_items, cache->blocks[slot],
                                         (int32_t)cache->block_bytes);
            }
            if (needs_free) free((void *)encoded);
            if (got != (int)cache->block_bytes) {
                return storage_error(error, ME_ERR_EVAL, "Storage block decode failed");
            }
            cache->cached_chunk[slot] = (int64_t)chunk;
            cache->cached_block[slot] = (int64_t)block;
            cache->decodes++;
            cache->decoded_bytes += cache->block_bytes;
            if (run > 1) cache->cache_hits += run - 1;
        } else {
            cache->cache_hits += run;
        }
        cache->lru_tick[slot] = ++cache->tick;
        const unsigned char *src = cache->blocks[slot] + item * width;
        unsigned char *outp = dst + lane * width;
        if (last_bcast) {
            for (size_t k = 0; k < run; k++) {
                memcpy(outp + k * width, src, width);
            }
        } else {
            memcpy(outp, src, run * width);
        }
        lane += run;
        if (lane < total) {
            local_c[d_last] += run;
            if (local_c[d_last] == extent[d_last]) {
                local_c[d_last] = 0;
                for (size_t d = d_last; d-- > 0;) {
                    if (++local_c[d] < extent[d]) break;
                    local_c[d] = 0;
                }
            }
        }
    }
    return ME_SUCCESS;
}

static void block_compact_or_scatter(size_t ndim, const int32_t *blockshape,
                                     const size_t *valid, size_t width,
                                     void *phys, void *compact, bool to_compact) {
    size_t row_bytes = valid[ndim - 1] * width;
    size_t phys_row_stride = (size_t)blockshape[ndim - 1] * width;
    if (ndim == 1) {
        if (!to_compact) memcpy(phys, compact, row_bytes);
        return;
    }
    size_t nrows = 1;
    for (size_t d = 0; d < ndim - 1; d++) nrows *= valid[d];
    size_t coords[B2ND_EXEC_MAX_RANK] = {0};
    size_t bstrides[B2ND_EXEC_MAX_RANK] = {0};
    size_t stride = phys_row_stride;
    for (size_t d = ndim - 1; d-- > 0;) {
        bstrides[d] = stride;
        stride *= (size_t)blockshape[d];
    }
    for (size_t r = 0; r < nrows; r++) {
        size_t phys_off = 0;
        for (size_t d = 0; d < ndim - 1; d++) phys_off += coords[d] * bstrides[d];
        size_t comp_off = r * row_bytes;
        if (to_compact) {
            if (comp_off != phys_off) {
                memmove((unsigned char *)compact + comp_off,
                        (const unsigned char *)phys + phys_off, row_bytes);
            }
        } else {
            memcpy((unsigned char *)phys + phys_off,
                   (const unsigned char *)compact + comp_off, row_bytes);
        }
        if (r + 1 < nrows) {
            for (size_t d = ndim - 1; d-- > 0;) {
                if (++coords[d] < valid[d]) break;
                coords[d] = 0;
            }
        }
    }
}

typedef struct {
    storage_block_cache bcaches[B2ND_EXEC_MAX_INPUTS];
    blosc2_context *dctx[B2ND_EXEC_MAX_INPUTS];
    unsigned char *in_buf[B2ND_EXEC_MAX_INPUTS];
    unsigned char *map_buf;
    size_t tiles, map_calls, direct_tiles, gathered_bytes;
    size_t decodes, decoded_bytes, cache_hits, read_spans;
    size_t interpreter_calls, jit_calls;
    me_fp_status fp;
    bool fp_seen;
    bool entered;
    me_status status;
    me_error error;
} sink_worker;

typedef struct {
    const b2nd_exec_schedule *schedule;
    const me_kernel *kernel;
    b2nd_exec_plan_info plan_info;
    b2nd_exec_schedule_info sched_info;
    uint32_t direct_slot;
    size_t ninputs;
    size_t input_widths[B2ND_EXEC_MAX_INPUTS];
    const b2nd_array_t *const *arrays;
    b2nd_exec_view bound_views[B2ND_EXEC_MAX_INPUTS];
    bool is_reader[B2ND_EXEC_MAX_INPUTS];
    bool matched_layout[B2ND_EXEC_MAX_INPUTS];
    bool needed[B2ND_EXEC_MAX_INPUTS];
    blosc2_pthread_mutex_t *frame_mutex[B2ND_EXEC_MAX_INPUTS];
    b2nd_array_t *output;
    size_t out_block_items;
    size_t out_chunk_grid[B2ND_EXEC_MAX_RANK];
    size_t out_block_grid[B2ND_EXEC_MAX_RANK];
    int64_t current_chunk;
    sink_worker *workers;
    size_t nworkers;
} sink_context;

static int sink_prefilter(blosc2_prefilter_params *p) {
    sink_context *sink = p ? p->user_data : NULL;
    if (!sink || p->tid < 0 || (size_t)p->tid >= sink->nworkers) return -1;
    sink_worker *w = &sink->workers[p->tid];
    w->entered = true;
    if (w->status != ME_SUCCESS) return -1;

    const b2nd_array_t *out = sink->output;
    size_t ndim = (size_t)out->ndim;
    size_t out_width = sink->plan_info.itemsize;
    size_t origin[B2ND_EXEC_MAX_RANK] = {0};
    size_t valid[B2ND_EXEC_MAX_RANK] = {0};
    size_t c_rem = (size_t)sink->current_chunk;
    size_t b_rem = (size_t)p->nblock;
    size_t valid_nitems = 1;
    for (size_t d = ndim; d-- > 0;) {
        size_t cg = sink->out_chunk_grid[d];
        size_t bg = sink->out_block_grid[d];
        size_t cdim = c_rem % cg;
        size_t bdim = b_rem % bg;
        c_rem /= cg;
        b_rem /= bg;
        size_t chunk_off = cdim * (size_t)out->chunkshape[d];
        size_t local_off = bdim * (size_t)out->blockshape[d];
        origin[d] = chunk_off + local_off;
        size_t rem_shape = origin[d] < (size_t)out->shape[d] ? (size_t)out->shape[d] - origin[d] : 0;
        size_t rem_chunk = local_off < (size_t)out->chunkshape[d] ?
            (size_t)out->chunkshape[d] - local_off : 0;
        size_t rem_block = (size_t)out->blockshape[d];
        size_t v = rem_shape < rem_chunk ? rem_shape : rem_chunk;
        if (rem_block < v) v = rem_block;
        valid[d] = v;
        valid_nitems *= v;
    }
    if (!valid_nitems) {
        memset(p->output, 0, (size_t)p->output_size);
        return 0;
    }
    bool padded = valid_nitems != sink->out_block_items;
    if (padded && !w->map_buf) {
        w->status = storage_error(&w->error, ME_ERR_INVALID_ARG, "Missing padded sink map buffer");
        return -1;
    }

    me_buffer spans[B2ND_EXEC_MAX_INPUTS] = {0};
    for (size_t i = 0; i < sink->ninputs; i++) {
        if (!sink->needed[i]) continue;
        size_t in_width = sink->input_widths[i];
        size_t live_bytes = valid_nitems * in_width;
        if (sink->is_reader[i] && sink->matched_layout[i]) {
            const uint8_t *encoded = NULL;
            bool needs_free = false;
            int fetch_rc = storage_fetch_chunk(sink->arrays[i], sink->frame_mutex[i],
                                               sink->current_chunk, &encoded, &needs_free);
            int32_t nbytes = 0, cbytes = 0, blocksize = 0;
            int got = -1;
            if (fetch_rc >= 0 && encoded &&
                blosc2_cbuffer_sizes(encoded, &nbytes, &cbytes, &blocksize) == 0) {
                got = blosc2_getitem_ctx(w->dctx[i], encoded, cbytes,
                                         (int)(p->nblock * (int32_t)sink->out_block_items),
                                         (int)sink->out_block_items, w->in_buf[i],
                                         (int32_t)(sink->out_block_items * in_width));
            }
            if (needs_free) free((void *)encoded);
            w->read_spans++;
            if (got != (int)(sink->out_block_items * in_width)) {
                w->status = storage_error(&w->error, ME_ERR_EVAL, "Storage block decode failed");
                return -1;
            }
            w->decodes++;
            w->decoded_bytes += sink->out_block_items * in_width;
            if (padded) {
                block_compact_or_scatter(ndim, out->blockshape, valid, in_width,
                                         w->in_buf[i], w->in_buf[i], true);
            }
        } else if (sink->is_reader[i]) {
            me_status st = storage_read_box_cached(&w->bcaches[i], ndim, origin, valid,
                                                   w->in_buf[i], &w->error);
            if (st != ME_SUCCESS) {
                w->status = st;
                return -1;
            }
            w->gathered_bytes += live_bytes;
        } else {
            b2nd_exec_gather_box(sink->schedule, i, &sink->bound_views[i], origin, valid,
                                 w->in_buf[i]);
            w->gathered_bytes += live_bytes;
        }
        spans[i] = (me_buffer){w->in_buf[i], live_bytes};
    }

    void *eval_dst = padded ? w->map_buf : p->output;
    if (sink->plan_info.identity) {
        memcpy(eval_dst, spans[sink->direct_slot].data, valid_nitems * out_width);
        w->direct_tiles++;
    } else {
        me_block block = ME_BLOCK_INITIALIZER;
        block.nitems = valid_nitems;
        me_fp_status fp = {0};
        me_run_options options = ME_RUN_OPTIONS_INITIALIZER;
        options.fp_status = &fp;
        options.raise_mask = 0;
        me_execution_backend backend = ME_BACKEND_NONE;
        me_status st = me_eval_block_backend(
            sink->kernel, spans, sink->ninputs,
            (me_mut_buffer){eval_dst, valid_nitems * out_width}, &block, &options,
            &backend, &w->error);
        w->interpreter_calls += backend == ME_BACKEND_INTERPRETER;
        w->jit_calls += backend == ME_BACKEND_JIT;
        w->fp.flags |= fp.flags;
        w->fp.supported = w->fp_seen ? (w->fp.supported && fp.supported) : fp.supported;
        w->fp_seen = true;
        if (st != ME_SUCCESS) {
            w->status = st;
            return -1;
        }
        w->map_calls++;
    }
    if (padded) {
        memset(p->output, 0, (size_t)p->output_size);
        block_compact_or_scatter(ndim, out->blockshape, valid, out_width,
                                 p->output, eval_dst, false);
    }
    w->tiles++;
    return 0;
}

me_status b2nd_exec_execute_to_array(const b2nd_exec_schedule *schedule,
                                     const b2nd_array_t *const *arrays,
                                     const b2nd_exec_view *views, size_t ninputs,
                                     b2nd_array_t *output, unsigned raise_mask,
                                     size_t max_decode_bytes, size_t max_gather_bytes,
                                     const b2nd_exec_jobs *jobs,
                                     b2nd_exec_storage_report *report, me_error *error) {
    if (report) *report = (b2nd_exec_storage_report){0};
    if (error) *error = (me_error){0};
    b2nd_exec_plan_info plan_info = {0};
    b2nd_exec_schedule_info sched_info = {0};
    uint32_t direct_slot = 0;
    const me_kernel *kernel = b2nd_exec_schedule_kernel(
        schedule, &plan_info, &sched_info, &direct_slot);
    if (!schedule || !plan_info.ninputs || (!arrays && !views) || !output || !report ||
        ninputs != plan_info.ninputs || ninputs > B2ND_EXEC_MAX_INPUTS ||
        (raise_mask & ~(ME_FP_INVALID | ME_FP_DIVIDE | ME_FP_OVERFLOW | ME_FP_UNDERFLOW)) ||
        (jobs && (jobs->struct_size != sizeof(*jobs) || !jobs->workers))) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid compressed sink bindings");
    }
    if (plan_info.reduction) {
        return storage_error(error, ME_ERR_UNSUPPORTED, "Compressed sinks require an elementwise map");
    }
    if (jobs && jobs->workers > 1 && !plan_info.partitionable) {
        return storage_error(error, ME_ERR_UNSUPPORTED, "Unproven kernels require serial execution");
    }
    size_t out_width = plan_info.itemsize;
    if (!output->sc || output->ndim < 1 || output->ndim > B2ND_EXEC_MAX_RANK ||
        (size_t)output->ndim != sched_info.output_ndim || !out_width || out_width > 8 ||
        output->dtype_format != DTYPE_NUMPY_FORMAT ||
        !storage_dtype_matches(output->dtype, plan_info.dtype, out_width) ||
        output->sc->typesize != (int32_t)out_width ||
        (output->sc->frame && ((const blosc2_frame_s *)output->sc->frame)->schunk != output->sc) ||
        !output->sc->cctx) {
        return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported compressed sink destination");
    }
    blosc2_cparams cparams;
    if (blosc2_ctx_get_cparams(output->sc->cctx, &cparams) < 0 || cparams.prefilter != NULL ||
        cparams.use_dict != 0 || cparams.compcode > BLOSC_ZSTD) {
        return storage_error(error, ME_ERR_UNSUPPORTED, "Unsupported compressed sink compression parameters");
    }
    for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
        if (cparams.filters[f] > BLOSC_DELTA) {
            return storage_error(error, ME_ERR_UNSUPPORTED, "Sink plugin filters are not admitted");
        }
    }

    size_t out_items = 1, out_chunks = 1, out_physical = 1, out_block_items = 1, out_chunk_items = 1;
    size_t out_chunk_grid[B2ND_EXEC_MAX_RANK] = {0};
    size_t out_block_grid[B2ND_EXEC_MAX_RANK] = {0};
    bool has_padding = false;
    for (int d = 0; d < output->ndim; d++) {
        if (output->shape[d] < 0 || output->shape[d] != sched_info.output_shape[d] ||
            output->chunkshape[d] <= 0 || output->blockshape[d] <= 0) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Sink shape does not match schedule");
        }
        size_t s = (size_t)output->shape[d];
        size_t c = (size_t)output->chunkshape[d];
        size_t b = (size_t)output->blockshape[d];
        if ((s % c) != 0 || (c % b) != 0) has_padding = true;
        out_chunk_grid[d] = s / c + (s % c != 0);
        out_block_grid[d] = c / b + (c % b != 0);
        if (!storage_product(&out_items, s) ||
            !storage_product(&out_chunks, out_chunk_grid[d]) ||
            !storage_product(&out_physical, out_block_grid[d]) ||
            !storage_product(&out_physical, b) ||
            !storage_product(&out_block_items, b) ||
            !storage_product(&out_chunk_items, c)) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Sink geometry overflow");
        }
    }
    if (out_physical > (size_t)INT32_MAX / out_width ||
        output->extchunknitems != (int64_t)out_physical ||
        output->blocknitems != (int64_t)out_block_items ||
        output->chunknitems != (int64_t)out_chunk_items ||
        output->nitems != (int64_t)out_items ||
        output->sc->nchunks != (int64_t)out_chunks ||
        (out_chunks && output->sc->chunksize != (int32_t)(out_physical * out_width))) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Inconsistent sink storage geometry");
    }

    sink_context sink = {
        .schedule = schedule, .kernel = kernel, .plan_info = plan_info, .sched_info = sched_info,
        .direct_slot = direct_slot, .ninputs = ninputs, .arrays = arrays, .output = output,
        .out_block_items = out_block_items};
    memcpy(sink.out_chunk_grid, out_chunk_grid, sizeof(out_chunk_grid));
    memcpy(sink.out_block_grid, out_block_grid, sizeof(out_block_grid));

    storage_input base_states[B2ND_EXEC_MAX_INPUTS] = {0};
    me_status status = ME_SUCCESS;
    size_t per_worker_decode = 0, per_worker_gather = 0;
    for (size_t i = 0; i < ninputs; i++) {
        b2nd_exec_input_meta meta;
        if (b2nd_exec_schedule_input(schedule, i, &meta) != ME_SUCCESS) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Sink input count mismatch");
        }
        sink.needed[i] = !plan_info.identity || i == direct_slot;
        sink.input_widths[i] = meta.itemsize;
        const b2nd_array_t *arr = arrays ? arrays[i] : NULL;
        if (arr) {
            if (arr == output || arr->sc == output->sc ||
                (arr->sc->frame && output->sc->frame &&
                 (arr->sc->frame == output->sc->frame ||
                  (((const blosc2_frame_s *)arr->sc->frame)->urlpath &&
                   ((const blosc2_frame_s *)output->sc->frame)->urlpath &&
                   strcmp(((const blosc2_frame_s *)arr->sc->frame)->urlpath,
                          ((const blosc2_frame_s *)output->sc->frame)->urlpath) == 0)))) {
                return storage_error(error, ME_ERR_UNSUPPORTED, "Aliased compressed storage output");
            }
            sink.is_reader[i] = true;
            base_states[i].array = arr;
            status = storage_validate(&base_states[i], &meta, error);
            if (status != ME_SUCCESS) return status;
            status = storage_validate_chunks(&base_states[i], NULL, error);
            if (status != ME_SUCCESS) return status;
            bool match = (size_t)arr->ndim == (size_t)output->ndim;
            for (int d = 0; match && d < arr->ndim; d++) {
                if (arr->shape[d] != output->shape[d] ||
                    arr->chunkshape[d] != output->chunkshape[d] ||
                    arr->blockshape[d] != output->blockshape[d]) {
                    match = false;
                }
            }
            sink.matched_layout[i] = match;
            if (sink.needed[i] && out_chunks) {
                size_t d_need = match ? out_block_items * meta.itemsize :
                    (base_states[i].bytes ? base_states[i].block_items * meta.itemsize : 0);
                if (d_need > max_decode_bytes - per_worker_decode) {
                    return storage_error(error, ME_ERR_INVALID_ARG, "Storage decode budget exceeded");
                }
                per_worker_decode += d_need;
                if (!match) {
                    size_t g_need = out_block_items * meta.itemsize;
                    if (g_need > max_gather_bytes - per_worker_gather) {
                        return storage_error(error, ME_ERR_INVALID_ARG, "Execution gather scratch budget exceeded");
                    }
                    per_worker_gather += g_need;
                }
            }
        } else if (views) {
            status = b2nd_exec_validate_view(schedule, i, &views[i], error);
            if (status != ME_SUCCESS) return status;
            if (views[i].capacity && output->sc->frame) {
                const blosc2_frame_s *out_frame = (const blosc2_frame_s *)output->sc->frame;
                if (out_frame->cframe && out_frame->len > 0 &&
                    (size_t)out_frame->len <= UINTPTR_MAX - (uintptr_t)out_frame->cframe &&
                    (uintptr_t)out_frame->cframe < (uintptr_t)views[i].base + views[i].capacity &&
                    (uintptr_t)views[i].base < (uintptr_t)out_frame->cframe + (size_t)out_frame->len) {
                    return storage_error(error, ME_ERR_UNSUPPORTED,
                                         "Aliased compressed storage output");
                }
            } else if (views[i].capacity && output->sc->data) {
                for (int64_t c = 0; c < output->sc->nchunks; c++) {
                    const uint8_t *chunk = output->sc->data[c];
                    int32_t nb = 0, cb = 0, bs = 0;
                    if (chunk && blosc2_cbuffer_sizes(chunk, &nb, &cb, &bs) == 0 && cb > 0 &&
                        (size_t)cb <= UINTPTR_MAX - (uintptr_t)chunk &&
                        (uintptr_t)chunk < (uintptr_t)views[i].base + views[i].capacity &&
                        (uintptr_t)views[i].base < (uintptr_t)chunk + (size_t)cb) {
                        return storage_error(error, ME_ERR_UNSUPPORTED,
                                             "Aliased compressed storage output");
                    }
                }
            }
            sink.bound_views[i] = views[i];
            if (sink.needed[i] && out_chunks) {
                size_t g_need = out_block_items * meta.itemsize;
                if (g_need > max_gather_bytes - per_worker_gather) {
                    return storage_error(error, ME_ERR_INVALID_ARG, "Execution gather scratch budget exceeded");
                }
                per_worker_gather += g_need;
            }
        } else {
            return storage_error(error, ME_ERR_INVALID_ARG, "Invalid storage input binding");
        }
    }

    size_t per_worker_mapped = (out_chunks && has_padding) ? out_block_items * out_width : 0;
    size_t per_worker_scratch = per_worker_mapped + per_worker_gather;
    if (per_worker_decode > max_decode_bytes) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Storage decode budget exceeded");
    }
    if (per_worker_gather > max_gather_bytes) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Execution gather scratch budget exceeded");
    }
    if (jobs && jobs->max_worker_scratch && per_worker_scratch > jobs->max_worker_scratch) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Execution worker scratch budget exceeded");
    }
    me_fp_status probe_fp = {0};
    if (raise_mask != 0) {
        me_fp_scope probe_scope;
        status = me_fp_scope_begin(&probe_scope, &probe_fp, error);
        if (status != ME_SUCCESS) return status;
        me_fp_scope_end(&probe_scope, NULL);
        if (!probe_fp.supported) {
            return storage_error(error, ME_ERR_UNSUPPORTED,
                                 "Floating exception reporting is unavailable");
        }
    }
    if (!out_items || !out_chunks) {
        if (raise_mask != 0) {
            report->execution.fp_observed = true;
            report->execution.fp.supported = probe_fp.supported;
        }
        return ME_SUCCESS;
    }

    size_t nworkers = jobs ? jobs->workers : 1;
#ifdef _WIN32
    nworkers = 1;
#endif
    size_t out_nblocks = out_physical / out_block_items;
    if (nworkers > B2ND_EXEC_MAX_TASKS) nworkers = B2ND_EXEC_MAX_TASKS;
    if (out_nblocks && nworkers > out_nblocks) nworkers = out_nblocks;
    if (per_worker_decode) {
        size_t fit = max_decode_bytes / per_worker_decode;
        if (nworkers > fit) nworkers = fit;
    }
    if (jobs && jobs->max_worker_scratch && per_worker_scratch) {
        size_t fit = jobs->max_worker_scratch / per_worker_scratch;
        if (nworkers > fit) nworkers = fit;
    }
    if (!nworkers) nworkers = 1;

    size_t cache_slots[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t worker_dec_budget = max_decode_bytes / nworkers;
    size_t extra_budget = worker_dec_budget > per_worker_decode ?
        worker_dec_budget - per_worker_decode : 0;
    for (size_t i = 0; i < ninputs; i++) {
        if (!sink.needed[i] || !sink.is_reader[i] || sink.matched_layout[i]) continue;
        cache_slots[i] = 1;
    }
    for (size_t pass = 1; pass < STORAGE_BLOCK_CACHE_SLOTS; pass++) {
        bool grew = false;
        for (size_t i = 0; i < ninputs; i++) {
            if (!cache_slots[i]) continue;
            size_t bbytes = base_states[i].block_items * sink.input_widths[i];
            size_t blocks_per_chunk = bbytes && base_states[i].bytes ?
                base_states[i].bytes / bbytes : 1;
            size_t total_blocks = (size_t)arrays[i]->sc->nchunks * blocks_per_chunk;
            if (cache_slots[i] >= total_blocks) continue;
            if (bbytes <= extra_budget) {
                cache_slots[i]++;
                extra_budget -= bbytes;
                grew = true;
            }
        }
        if (!grew) break;
    }

    blosc2_pthread_mutex_t frame_mutexes[B2ND_EXEC_MAX_INPUTS];
    bool has_frame_mutex[B2ND_EXEC_MAX_INPUTS];
    memset(has_frame_mutex, 0, sizeof(has_frame_mutex));
    for (size_t i = 0; i < ninputs; i++) {
        if (nworkers > 1 && sink.needed[i] && sink.is_reader[i] && arrays[i]->sc->frame) {
            status = storage_frame_mutex_init(frame_mutexes, has_frame_mutex, i, error);
            if (status != ME_SUCCESS) return status;
            sink.frame_mutex[i] = &frame_mutexes[i];
        }
    }
    sink.nworkers = nworkers;
    sink.workers = calloc(nworkers, sizeof(*sink.workers));
    if (!sink.workers) {
        storage_mutexes_destroy(frame_mutexes, has_frame_mutex, ninputs);
        return storage_error(error, ME_ERR_OOM, "Cannot allocate sink worker metadata");
    }
    report->storage_metadata_bytes = nworkers * sizeof(*sink.workers);
    report->execution.mapped_bytes = per_worker_mapped;
    report->execution.gather_bytes = per_worker_gather;
    report->execution.worker_bytes = nworkers * per_worker_scratch;

    for (size_t w = 0; status == ME_SUCCESS && w < nworkers; w++) {
        sink_worker *sw = &sink.workers[w];
        if (has_padding) {
            sw->map_buf = malloc(per_worker_mapped);
            if (!sw->map_buf) {
                status = storage_error(error, ME_ERR_OOM, "Cannot allocate sink map workspace");
                break;
            }
        }
        for (size_t i = 0; i < ninputs; i++) {
            if (!sink.needed[i]) continue;
            size_t in_width = sink.input_widths[i];
            size_t in_buf_bytes = out_block_items * in_width;
            sw->in_buf[i] = in_buf_bytes ? malloc(in_buf_bytes) : NULL;
            if (in_buf_bytes && !sw->in_buf[i]) {
                status = storage_error(error, ME_ERR_OOM, "Cannot allocate sink input buffer");
                break;
            }
            if (sink.is_reader[i] && sink.matched_layout[i]) {
                blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
                dp.nthreads = 1;
                sw->dctx[i] = blosc2_create_dctx(dp);
                if (!sw->dctx[i]) {
                    status = storage_error(error, ME_ERR_OOM, "Cannot allocate sink decode context");
                    break;
                }
                report->decode_bytes += in_buf_bytes;
            } else if (sink.is_reader[i]) {
                storage_block_cache *bc = &sw->bcaches[i];
                bc->array = base_states[i].array;
                bc->frame_mutex = sink.frame_mutex[i];
                bc->itemsize = in_width;
                bc->block_items = base_states[i].block_items;
                bc->block_bytes = base_states[i].block_items * in_width;
                bc->nslots = cache_slots[i];
                memcpy(bc->chunk_grid, base_states[i].chunk_grid, sizeof(bc->chunk_grid));
                memcpy(bc->block_grid, base_states[i].block_grid, sizeof(bc->block_grid));
                blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
                dp.nthreads = 1;
                bc->dctx = blosc2_create_dctx(dp);
                if (!bc->dctx) {
                    status = storage_error(error, ME_ERR_OOM, "Cannot allocate sink decode context");
                    break;
                }
                for (size_t s = 0; s < bc->nslots; s++) {
                    bc->cached_chunk[s] = -1;
                    bc->cached_block[s] = -1;
                    bc->blocks[s] = bc->block_bytes ? malloc(bc->block_bytes) : NULL;
                    if (bc->block_bytes && !bc->blocks[s]) {
                        status = storage_error(error, ME_ERR_OOM,
                                               "Cannot allocate sink block cache slot");
                        break;
                    }
                    report->decode_bytes += bc->block_bytes;
                }
                if (status != ME_SUCCESS) break;
            }
        }
    }

    size_t chunk_bytes = (size_t)output->sc->chunksize;
    uint8_t *dummy = NULL, *chunk_buf = NULL;
    blosc2_context *compressor = NULL;
    if (status == ME_SUCCESS) {
        dummy = calloc(1, chunk_bytes);
        chunk_buf = malloc(chunk_bytes + BLOSC2_MAX_OVERHEAD);
        if (!dummy || !chunk_buf) {
            status = storage_error(error, ME_ERR_OOM, "Cannot allocate sink chunk buffer");
        }
    }
    if (status == ME_SUCCESS) {
        blosc2_cparams cp = cparams;
        blosc2_prefilter_params pre = {.user_data = &sink};
        cp.prefilter = sink_prefilter;
        cp.preparams = &pre;
        cp.schunk = NULL;
        cp.nthreads = (int16_t)nworkers;
        compressor = blosc2_create_cctx(cp);
        if (!compressor || blosc2_set_prefilter_full_write(compressor, true) < 0) {
            status = storage_error(error, ME_ERR_OOM, "Cannot create sink compression context");
        }
    }
    for (int64_t c = 0; status == ME_SUCCESS && c < output->sc->nchunks; c++) {
        sink.current_chunk = c;
        int cbytes = blosc2_compress_ctx(compressor, dummy, (int32_t)chunk_bytes,
                                         chunk_buf, (int32_t)(chunk_bytes + BLOSC2_MAX_OVERHEAD));
        if (cbytes <= 0) {
            for (size_t w = 0; w < nworkers; w++) {
                if (sink.workers[w].status != ME_SUCCESS) {
                    status = sink.workers[w].status;
                    if (error) *error = sink.workers[w].error;
                    break;
                }
            }
            if (status == ME_SUCCESS) {
                status = storage_error(error, ME_ERR_EVAL, "Compressed sink chunk write failed");
            }
            break;
        }
        if (blosc2_schunk_update_chunk(output->sc, c, chunk_buf, true) < 0) {
            status = storage_error(error, ME_ERR_EVAL, "Compressed sink chunk update failed");
            break;
        }
    }
    if (compressor) blosc2_free_ctx(compressor);
    free(dummy);
    free(chunk_buf);

    bool any_fp = false;
    bool all_supported = true;
    for (size_t w = 0; w < nworkers; w++) {
        sink_worker *sw = &sink.workers[w];
        report->execution.tiles += sw->tiles;
        report->execution.map_calls += sw->map_calls;
        report->execution.direct_tiles += sw->direct_tiles;
        report->execution.gathered_bytes += sw->gathered_bytes;
        report->execution.jit_calls += sw->jit_calls;
        report->execution.interpreter_calls += sw->interpreter_calls;
        report->execution.tasks += sw->entered;
        report->decodes += sw->decodes;
        report->decoded_bytes += sw->decoded_bytes;
        report->cache_hits += sw->cache_hits;
        report->read_spans += sw->read_spans;
        if (sw->fp_seen) {
            report->execution.fp.flags |= sw->fp.flags;
            all_supported = all_supported && sw->fp.supported;
            any_fp = true;
        }
        for (size_t i = 0; i < ninputs; i++) {
            storage_block_cache *bc = &sw->bcaches[i];
            report->decodes += bc->decodes;
            report->decoded_bytes += bc->decoded_bytes;
            report->cache_hits += bc->cache_hits;
            report->read_spans += bc->read_spans;
            if (sw->dctx[i]) blosc2_free_ctx(sw->dctx[i]);
            if (bc->dctx) blosc2_free_ctx(bc->dctx);
            for (size_t s = 0; s < STORAGE_BLOCK_CACHE_SLOTS; s++) {
                free(bc->blocks[s]);
            }
            free(sw->in_buf[i]);
        }
        free(sw->map_buf);
    }
    free(sink.workers);
    storage_mutexes_destroy(frame_mutexes, has_frame_mutex, ninputs);
    if (any_fp) {
        report->execution.fp_observed = true;
        report->execution.fp.supported = all_supported;
    } else if (raise_mask != 0) {
        report->execution.fp_observed = true;
        report->execution.fp.supported = probe_fp.supported;
    }
    if (status == ME_SUCCESS && (report->execution.fp.flags & raise_mask)) {
        return storage_error(error, ME_ERR_EVAL, "Execution raised floating-point status");
    }
    return status;
}

typedef struct {
    b2nd_exec_source source;
    size_t *shared_idx_buf;
    size_t itemsize, dom_ndim, reduction_lanes;
    int64_t dom_shape[B2ND_EXEC_MAX_RANK];
    size_t eff_stride[B2ND_EXEC_MAX_RANK];
    bool is_reduction, fast_linear, reduced_axis[B2ND_EXEC_MAX_RANK];
    size_t read_calls, read_items, max_read_items, read_bytes;
} source_callback_state;

static void source_state_init(source_callback_state *state, const b2nd_exec_source *src,
                              const b2nd_exec_schedule *schedule, size_t slot,
                              const b2nd_exec_plan_info *plan_info,
                              const b2nd_exec_schedule_info *sched_info,
                              size_t *idx_buf) {
    memset(state, 0, sizeof(*state));
    state->source = *src;
    state->shared_idx_buf = idx_buf;
    state->itemsize = src->itemsize;
    state->dom_ndim = sched_info->ndim;
    state->reduction_lanes = sched_info->reduction_lanes;
    state->is_reduction = plan_info->reduction;
    memcpy(state->dom_shape, sched_info->shape, sizeof(state->dom_shape));
    bool matches = false, suffix = false;
    b2nd_exec_schedule_layout(schedule, slot, &matches, &suffix, state->reduced_axis);
    state->fast_linear = matches && suffix;
    if (!state->fast_linear && src->ndim > 0) {
        size_t in_strides[B2ND_EXEC_MAX_RANK] = {0};
        size_t stride = 1;
        for (size_t a = src->ndim; a-- > 0;) {
            in_strides[a] = src->shape[a] == 1 ? 0 : stride;
            stride *= (size_t)src->shape[a];
        }
        size_t shift = state->dom_ndim - src->ndim;
        for (size_t d = shift; d < state->dom_ndim; d++) {
            state->eff_stride[d] = in_strides[d - shift];
        }
    }
}

static me_status source_read_trampoline(void *context, size_t group, size_t pos, size_t count,
                                        me_mut_buffer output, me_error *error) {
    source_callback_state *state = context;
    if (output.capacity < count * state->itemsize) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid source callback buffer");
    }
    if (!count) return ME_SUCCESS;
    size_t *idx = state->shared_idx_buf;
    if (state->fast_linear) {
        size_t start = state->is_reduction ? group * state->reduction_lanes + pos : pos;
        for (size_t lane = 0; lane < count; lane++) {
            idx[lane] = start + lane;
        }
    } else {
        size_t coords[B2ND_EXEC_MAX_RANK] = {0};
        size_t g_rem = group, l_rem = pos;
        for (size_t d = state->dom_ndim; d-- > 0;) {
            size_t *dim_idx = (state->is_reduction && !state->reduced_axis[d]) ? &g_rem : &l_rem;
            coords[d] = *dim_idx % (size_t)state->dom_shape[d];
            *dim_idx /= (size_t)state->dom_shape[d];
        }
        for (size_t lane = 0; lane < count; lane++) {
            size_t flat = 0;
            for (size_t d = 0; d < state->dom_ndim; d++) {
                flat += coords[d] * state->eff_stride[d];
            }
            idx[lane] = flat;
            if (lane + 1 < count) {
                for (size_t d = state->dom_ndim; d-- > 0;) {
                    if (state->is_reduction && !state->reduced_axis[d]) continue;
                    if (++coords[d] < (size_t)state->dom_shape[d]) break;
                    coords[d] = 0;
                }
            }
        }
    }
    state->read_calls++;
    me_status st = state->source.read(state->source.context, idx, count, output, error);
    if (st != ME_SUCCESS) return st;
    state->read_items += count;
    if (count > state->max_read_items) state->max_read_items = count;
    state->read_bytes += count * state->itemsize;
    return ME_SUCCESS;
}

static me_status source_stream_map(const b2nd_exec_schedule *schedule,
                                   const me_kernel *kernel,
                                   const b2nd_exec_plan_info *plan_info,
                                   const b2nd_exec_schedule_info *sched_info,
                                   uint32_t direct_slot,
                                   const b2nd_exec_view *bound_views,
                                   source_callback_state *src_states,
                                   const bool *is_cb, size_t ninputs,
                                   const b2nd_exec_sink *sink,
                                   unsigned raise_mask, size_t max_gather_bytes,
                                   size_t max_output_bytes, size_t *idx_buf,
                                   size_t index_buffer_bytes,
                                   b2nd_exec_callback_report *report, me_error *error) {
    size_t nitems = sched_info->nitems;
    size_t tile_items = sched_info->tile_items;
    size_t out_width = plan_info->itemsize;
    report->index_buffer_bytes = index_buffer_bytes;
    if (!nitems) {
        if (raise_mask) {
            me_fp_scope scope;
            me_fp_status fp = {0};
            me_status st = me_fp_scope_begin(&scope, &fp, error);
            if (st != ME_SUCCESS) return st;
            me_status end_st = me_fp_scope_end(&scope, NULL);
            report->execution.fp_observed = true;
            report->execution.fp.supported = fp.supported;
            if (end_st != ME_SUCCESS) return end_st;
            if (!fp.supported) {
                return storage_error(error, ME_ERR_UNSUPPORTED,
                                     "Floating exception reporting is unavailable");
            }
        }
        return ME_SUCCESS;
    }

    bool needed[B2ND_EXEC_MAX_INPUTS] = {0};
    bool direct[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t gather_offsets[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t gather_bytes = 0;
    for (size_t i = 0; i < ninputs; i++) {
        needed[i] = !plan_info->identity || i == direct_slot;
        if (!needed[i]) continue;
        if (!is_cb[i] && b2nd_exec_view_is_direct(schedule, i, &bound_views[i])) {
            direct[i] = true;
            continue;
        }
        size_t slot_bytes = tile_items * src_states[i].source.itemsize;
        if (gather_bytes > SIZE_MAX - slot_bytes) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Source gather buffer overflow");
        }
        gather_offsets[i] = gather_bytes;
        gather_bytes += slot_bytes;
    }
    if (gather_bytes > max_gather_bytes) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Execution gather buffer budget exceeded");
    }

    size_t out_tile_items = tile_items < nitems ? tile_items : nitems;
    size_t out_buf_bytes = plan_info->identity ? 0 : out_tile_items * out_width;
    if (out_buf_bytes > max_output_bytes) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Callback output buffer budget exceeded");
    }

    unsigned char *gather_buf = gather_bytes ? malloc(gather_bytes) : NULL;
    unsigned char *out_buf = out_buf_bytes ? malloc(out_buf_bytes) : NULL;
    if ((gather_bytes && !gather_buf) || (out_buf_bytes && !out_buf)) {
        free(gather_buf);
        free(out_buf);
        return storage_error(error, ME_ERR_OOM, "Cannot allocate streaming source buffers");
    }
    report->execution.gather_bytes = gather_bytes;
    report->execution.mapped_bytes = sched_info->mapped_bytes;
    report->execution.worker_bytes = gather_bytes + sched_info->mapped_bytes;
    report->execution.tasks = 1;
    report->output_buffer_bytes = out_buf_bytes;

    bool scoped = !plan_info->identity || raise_mask != 0;
    me_fp_scope scope;
    me_fp_status fp = {0};
    me_status status = ME_SUCCESS;
    if (scoped) {
        report->execution.fp_observed = true;
        status = me_fp_scope_begin(&scope, &fp, error);
        if (status == ME_SUCCESS && raise_mask && !fp.supported) {
            me_fp_scope_end(&scope, NULL);
            free(gather_buf);
            free(out_buf);
            return storage_error(error, ME_ERR_UNSUPPORTED,
                                 "Floating exception reporting is unavailable");
        }
    }

    me_buffer spans[B2ND_EXEC_MAX_INPUTS] = {0};
    unsigned char *in_ptrs[B2ND_EXEC_MAX_INPUTS] = {0};
    for (size_t i = 0; i < ninputs; i++) {
        if (needed[i] && !direct[i]) {
            in_ptrs[i] = gather_buf + gather_offsets[i];
        }
    }
    for (size_t pos = 0; status == ME_SUCCESS && pos < nitems;) {
        size_t count = nitems - pos < tile_items ? nitems - pos : tile_items;
        for (size_t i = 0; status == ME_SUCCESS && i < ninputs; i++) {
            if (!needed[i]) {
                spans[i] = (me_buffer){NULL, 0};
                continue;
            }
            size_t in_w = src_states[i].source.itemsize;
            if (direct[i]) {
                const unsigned char *ptr =
                    (const unsigned char *)bound_views[i].base + bound_views[i].offset + pos * in_w;
                spans[i] = (me_buffer){ptr, count * in_w};
            } else if (is_cb[i]) {
                me_mut_buffer dst = {in_ptrs[i], count * in_w};
                status = source_read_trampoline(&src_states[i], 0, pos, count, dst, error);
                if (status != ME_SUCCESS) break;
                report->execution.gathered_bytes += dst.capacity;
                spans[i] = (me_buffer){in_ptrs[i], dst.capacity};
            } else {
                b2nd_exec_gather_span(schedule, i, &bound_views[i], 0, pos, count, in_ptrs[i]);
                report->execution.gathered_bytes += count * in_w;
                spans[i] = (me_buffer){in_ptrs[i], count * in_w};
            }
        }
        if (status != ME_SUCCESS) break;
        const void *write_ptr = NULL;
        if (plan_info->identity) {
            write_ptr = spans[direct_slot].data;
            report->execution.direct_tiles++;
        } else {
            me_block block = ME_BLOCK_INITIALIZER;
            block.nitems = count;
            me_fp_status tile_fp = {0};
            me_run_options run_opts = ME_RUN_OPTIONS_INITIALIZER;
            run_opts.fp_status = &tile_fp;
            me_execution_backend backend = ME_BACKEND_NONE;
            status = me_eval_block_backend(kernel, spans, ninputs,
                                           (me_mut_buffer){out_buf, count * out_width},
                                           &block, &run_opts, &backend, error);
            report->execution.interpreter_calls += backend == ME_BACKEND_INTERPRETER;
            report->execution.jit_calls += backend == ME_BACKEND_JIT;
            fp.flags |= tile_fp.flags;
            if (status != ME_SUCCESS) break;
            report->execution.map_calls++;
            write_ptr = out_buf;
        }
        report->execution.tiles++;
        report->write_calls++;
        status = sink->write(sink->context, pos, count,
                             (me_buffer){write_ptr, count * out_width}, error);
        if (status != ME_SUCCESS) break;
        report->write_items += count;
        if (count > report->max_write_items) report->max_write_items = count;
        report->written_bytes += count * out_width;
        pos += count;
    }

    if (scoped && status != ME_ERR_UNSUPPORTED) {
        me_status end_st = me_fp_scope_end(&scope, status == ME_SUCCESS ? error : NULL);
        report->execution.fp.flags |= fp.flags;
        report->execution.fp.supported = fp.supported;
        if (status == ME_SUCCESS) status = end_st;
    }
    if (status == ME_SUCCESS && (report->execution.fp.flags & raise_mask)) {
        status = storage_error(error, ME_ERR_EVAL, "Execution raised floating-point status");
    }
    free(gather_buf);
    free(out_buf);
    (void)idx_buf;
    return status;
}

me_status b2nd_exec_execute_sources(const b2nd_exec_schedule *schedule,
                                    const b2nd_exec_source *sources,
                                    const b2nd_exec_view *views, size_t ninputs,
                                    me_mut_buffer output,
                                    const b2nd_exec_sink *sink,
                                    unsigned raise_mask,
                                    size_t max_gather_bytes,
                                    size_t max_output_bytes,
                                    b2nd_exec_callback_report *report,
                                    me_error *error) {
    if (error) *error = (me_error){0};
    b2nd_exec_callback_report local_rep = {0};
    if (report) *report = local_rep;
    if (!schedule || !sources || !ninputs || ninputs > B2ND_EXEC_MAX_INPUTS ||
        (raise_mask & ~15u)) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid source callback execution bindings");
    }
    if (sink && output.data) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Specify either resident output or sink, not both");
    }
    b2nd_exec_plan_info plan_info = {0};
    b2nd_exec_schedule_info sched_info = {0};
    uint32_t direct_slot = 0;
    const me_kernel *kernel = b2nd_exec_schedule_kernel(schedule, &plan_info, &sched_info, &direct_slot);
    if (!kernel || ninputs != plan_info.ninputs) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Mismatched source callback input count");
    }
    if (sink) {
        if (sink->struct_size != sizeof(*sink) || !sink->write ||
            sink->dtype != plan_info.dtype || sink->itemsize != plan_info.itemsize ||
            sink->ndim != sched_info.output_ndim) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Invalid execution sink binding");
        }
        for (size_t d = 0; d < sink->ndim; d++) {
            if (sink->shape[d] != sched_info.output_shape[d]) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Execution sink shape mismatch");
            }
        }
    }

    bool is_cb[B2ND_EXEC_MAX_INPUTS] = {0};
    bool has_cb = false;
    b2nd_exec_view bound_views[B2ND_EXEC_MAX_INPUTS] = {0};
    source_callback_state src_states[B2ND_EXEC_MAX_INPUTS];
    for (size_t i = 0; i < ninputs; i++) {
        if (sources[i].struct_size != sizeof(sources[i])) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Invalid execution source descriptor");
        }
        b2nd_exec_input_meta in_meta = {0};
        me_status st = b2nd_exec_schedule_input(schedule, i, &in_meta);
        if (st != ME_SUCCESS) {
            return storage_error(error, st, "Invalid execution source slot");
        }
        if (sources[i].dtype != in_meta.dtype || sources[i].itemsize != in_meta.itemsize ||
            sources[i].ndim != in_meta.ndim) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Execution source dtype or rank mismatch");
        }
        for (size_t d = 0; d < in_meta.ndim; d++) {
            if (sources[i].shape[d] != in_meta.shape[d]) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Execution source shape mismatch");
            }
        }
        if (sources[i].read) {
            is_cb[i] = true;
            has_cb = true;
        } else {
            if (!views) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Missing resident view for non-callback source");
            }
            st = b2nd_exec_validate_view(schedule, i, &views[i], error);
            if (st != ME_SUCCESS) return st;
            bound_views[i] = views[i];
        }
    }

    size_t idx_bytes = (has_cb && sched_info.nitems && sched_info.tile_items)
        ? sched_info.tile_items * sizeof(size_t) : 0;
    size_t *idx_buf = idx_bytes ? malloc(idx_bytes) : NULL;
    if (idx_bytes && !idx_buf) {
        return storage_error(error, ME_ERR_OOM, "Cannot allocate callback index buffer");
    }
    for (size_t i = 0; i < ninputs; i++) {
        source_state_init(&src_states[i], &sources[i], schedule, i, &plan_info, &sched_info, idx_buf);
    }

    me_status status = ME_SUCCESS;
    if (sink && !plan_info.reduction) {
        status = source_stream_map(schedule, kernel, &plan_info, &sched_info, direct_slot,
                                   bound_views, src_states, is_cb, ninputs, sink, raise_mask,
                                   max_gather_bytes, max_output_bytes, idx_buf, idx_bytes,
                                   &local_rep, error);
    } else {
        b2nd_exec_reader readers[B2ND_EXEC_MAX_INPUTS] = {0};
        for (size_t i = 0; i < ninputs; i++) {
            if (is_cb[i]) {
                readers[i] = (b2nd_exec_reader){&src_states[i], source_read_trampoline};
            }
        }
        local_rep.index_buffer_bytes = idx_bytes;
        if (!sink) {
            size_t dummy_tasks = 1;
            status = b2nd_exec_plan_readers(schedule, bound_views, is_cb, ninputs, output,
                                            max_gather_bytes, NULL, &dummy_tasks, error);
            if (status == ME_SUCCESS) {
                status = b2nd_exec_execute_readers(schedule, bound_views, readers, ninputs,
                                                   output, raise_mask, max_gather_bytes,
                                                   &local_rep.execution, error);
            }
        } else {
            size_t out_buf_bytes = sched_info.output_items * plan_info.itemsize;
            if (out_buf_bytes > max_output_bytes) {
                free(idx_buf);
                return storage_error(error, ME_ERR_INVALID_ARG,
                                     "Callback reduction output buffer budget exceeded");
            }
            unsigned char *red_buf = out_buf_bytes ? malloc(out_buf_bytes) : NULL;
            if (out_buf_bytes && !red_buf) {
                free(idx_buf);
                return storage_error(error, ME_ERR_OOM, "Cannot allocate callback reduction output");
            }
            local_rep.output_buffer_bytes = out_buf_bytes;
            me_mut_buffer red_out = {red_buf, out_buf_bytes};
            size_t dummy_tasks = 1;
            status = b2nd_exec_plan_readers(schedule, bound_views, is_cb, ninputs, red_out,
                                            max_gather_bytes, NULL, &dummy_tasks, error);
            if (status == ME_SUCCESS) {
                status = b2nd_exec_execute_readers(schedule, bound_views, readers, ninputs,
                                                   red_out, raise_mask, max_gather_bytes,
                                                   &local_rep.execution, error);
            }
            size_t chunk_items = sched_info.tile_items ? sched_info.tile_items : 1;
            for (size_t pos = 0; status == ME_SUCCESS && pos < sched_info.output_items;) {
                size_t count = sched_info.output_items - pos < chunk_items
                    ? sched_info.output_items - pos : chunk_items;
                local_rep.write_calls++;
                status = sink->write(sink->context, pos, count,
                                     (me_buffer){red_buf + pos * plan_info.itemsize,
                                                 count * plan_info.itemsize},
                                     error);
                if (status != ME_SUCCESS) break;
                local_rep.write_items += count;
                if (count > local_rep.max_write_items) local_rep.max_write_items = count;
                local_rep.written_bytes += count * plan_info.itemsize;
                pos += count;
            }
            free(red_buf);
        }
    }

    for (size_t i = 0; i < ninputs; i++) {
        if (!is_cb[i]) continue;
        local_rep.read_calls += src_states[i].read_calls;
        local_rep.read_items += src_states[i].read_items;
        if (src_states[i].max_read_items > local_rep.max_read_items) {
            local_rep.max_read_items = src_states[i].max_read_items;
        }
        local_rep.read_bytes += src_states[i].read_bytes;
    }
    free(idx_buf);
    if (report) *report = local_rep;
    return status;
}

static me_status partition_read_source_box(const b2nd_exec_source *src, size_t dom_ndim,
                                           const size_t *origin, const size_t *extent,
                                           size_t total, size_t *idx_buf,
                                           unsigned char *dst, me_error *error) {
    if (!total) return ME_SUCCESS;
    size_t eff_stride[B2ND_EXEC_MAX_RANK] = {0};
    if (src->ndim > 0) {
        size_t in_strides[B2ND_EXEC_MAX_RANK] = {0};
        size_t stride = 1;
        for (size_t a = src->ndim; a-- > 0;) {
            in_strides[a] = src->shape[a] == 1 ? 0 : stride;
            stride *= (size_t)src->shape[a];
        }
        size_t shift = dom_ndim - src->ndim;
        for (size_t d = shift; d < dom_ndim; d++) {
            eff_stride[d] = in_strides[d - shift];
        }
    }
    size_t local_c[B2ND_EXEC_MAX_RANK] = {0};
    for (size_t lane = 0; lane < total; lane++) {
        size_t flat = 0;
        for (size_t d = 0; d < dom_ndim; d++) {
            flat += (origin[d] + local_c[d]) * eff_stride[d];
        }
        idx_buf[lane] = flat;
        if (lane + 1 < total) {
            for (size_t d = dom_ndim; d-- > 0;) {
                if (++local_c[d] < extent[d]) break;
                local_c[d] = 0;
            }
        }
    }
    return src->read(src->context, idx_buf, total,
                     (me_mut_buffer){dst, total * src->itemsize}, error);
}

static void partition_commit_elementwise(const b2nd_exec_partition_layout *part,
                                         const size_t *origin, const size_t *extent,
                                         const unsigned char *group_buf,
                                         unsigned char *output_base, size_t itemsize) {
    if (part->ndim == 0) {
        memcpy(output_base, group_buf, itemsize);
        return;
    }
    size_t ndim = part->ndim;
    size_t out_strides[B2ND_EXEC_MAX_RANK] = {0};
    size_t grp_strides[B2ND_EXEC_MAX_RANK] = {0};
    size_t o_stride = 1, g_stride = 1;
    for (size_t d = ndim; d-- > 0;) {
        out_strides[d] = o_stride;
        grp_strides[d] = g_stride;
        o_stride *= (size_t)(part->sel_stop[d] - part->sel_start[d]);
        g_stride *= extent[d];
    }
    size_t i_start[B2ND_EXEC_MAX_RANK] = {0};
    size_t i_len[B2ND_EXEC_MAX_RANK] = {0};
    size_t outer_rows = 1;
    for (size_t d = 0; d < ndim; d++) {
        size_t s0 = (size_t)part->sel_start[d] > origin[d] ? (size_t)part->sel_start[d] : origin[d];
        size_t o1 = origin[d] + extent[d];
        size_t s1 = (size_t)part->sel_stop[d] < o1 ? (size_t)part->sel_stop[d] : o1;
        i_start[d] = s0;
        i_len[d] = s1 - s0;
        if (d + 1 < ndim) outer_rows *= i_len[d];
    }
    size_t d_last = ndim - 1;
    size_t run_bytes = i_len[d_last] * itemsize;
    size_t coords[B2ND_EXEC_MAX_RANK] = {0};
    for (size_t row = 0; row < outer_rows; row++) {
        size_t o_off = (i_start[d_last] - (size_t)part->sel_start[d_last]) * out_strides[d_last];
        size_t g_off = (i_start[d_last] - origin[d_last]) * grp_strides[d_last];
        for (size_t d = 0; d < d_last; d++) {
            size_t c = i_start[d] + coords[d];
            o_off += (c - (size_t)part->sel_start[d]) * out_strides[d];
            g_off += (c - origin[d]) * grp_strides[d];
        }
        memcpy(output_base + o_off * itemsize, group_buf + g_off * itemsize, run_bytes);
        if (row + 1 < outer_rows) {
            for (size_t d = d_last; d-- > 0;) {
                if (++coords[d] < i_len[d]) break;
                coords[d] = 0;
            }
        }
    }
}

me_status b2nd_exec_admit_partitions(const b2nd_exec_schedule *schedule,
                                     const b2nd_array_t *const *arrays,
                                     const b2nd_exec_view *views,
                                     const b2nd_exec_source *sources, size_t ninputs,
                                     bool *admitted, me_error *error) {
    if (error) *error = (me_error){0};
    if (admitted) *admitted = false;
    b2nd_exec_partition_layout part = {0};
    b2nd_exec_plan_info info = {0};
    if (!schedule || !admitted || ninputs > B2ND_EXEC_MAX_INPUTS ||
        b2nd_exec_schedule_partitions(schedule, &part) != ME_SUCCESS || !part.is_partition_schedule) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid partition admission arguments");
    }
    b2nd_exec_schedule_kernel(schedule, &info, NULL, NULL);
    if (ninputs != info.ninputs) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Partition admission input count mismatch");
    }
    for (size_t i = 0; i < ninputs; i++) {
        b2nd_exec_input_meta meta;
        b2nd_exec_schedule_input(schedule, i, &meta);
        me_status st;
        if (arrays && arrays[i] && sources && sources[i].read) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Ambiguous partition admission binding");
        }
        if (arrays && arrays[i]) {
            storage_input state = {0};
            state.array = arrays[i];
            st = storage_validate(&state, &meta, error);
            if (st == ME_SUCCESS) st = storage_validate_chunks(&state, NULL, error);
        } else if (sources && sources[i].read) {
            const b2nd_exec_source *src = &sources[i];
            if (src->struct_size != sizeof(*src) || src->dtype != meta.dtype ||
                src->itemsize != meta.itemsize || src->ndim != meta.ndim ||
                memcmp(src->shape, meta.shape, meta.ndim * sizeof(meta.shape[0]))) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Partition admission source metadata mismatch");
            }
            st = ME_SUCCESS;
        } else {
            st = b2nd_exec_validate_view(schedule, i, views ? &views[i] : NULL, error);
        }
        if (st == ME_ERR_UNSUPPORTED) {
            if (error) *error = (me_error){0};
            return ME_SUCCESS; /* Capability rejection, never an execution failure. */
        }
        if (st != ME_SUCCESS) return st;
    }
    *admitted = true;
    return ME_SUCCESS;
}

me_status b2nd_exec_execute_partitions(const b2nd_exec_schedule *schedule,
                                       const b2nd_array_t *const *arrays,
                                       const b2nd_exec_view *views,
                                       const b2nd_exec_source *sources,
                                       size_t ninputs,
                                       me_mut_buffer output,
                                       unsigned raise_mask,
                                       size_t max_decode_bytes,
                                       size_t max_gather_bytes,
                                       b2nd_exec_storage_report *report,
                                       me_error *error) {
    if (error) *error = (me_error){0};
    if (report) *report = (b2nd_exec_storage_report){0};
    if (!schedule || ninputs > B2ND_EXEC_MAX_INPUTS || (raise_mask & ~15u)) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid partition execution arguments");
    }
    b2nd_exec_partition_layout part = {0};
    if (b2nd_exec_schedule_partitions(schedule, &part) != ME_SUCCESS ||
        !part.is_partition_schedule) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Schedule is not a partition schedule");
    }
    b2nd_exec_plan_info plan_info = {0};
    b2nd_exec_schedule_info sched_info = {0};
    uint32_t direct_slot = 0;
    const me_kernel *kernel = b2nd_exec_schedule_kernel(schedule, &plan_info, &sched_info, &direct_slot);
    (void)direct_slot;
    if (!kernel || ninputs != plan_info.ninputs) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Mismatched partition input count");
    }
    size_t out_width = plan_info.itemsize;
    size_t output_items = sched_info.output_items;
    size_t output_bytes = output_items * out_width;
    size_t out_align = storage_required_align(plan_info.dtype, out_width);
    if (output.capacity > UINTPTR_MAX - (uintptr_t)output.data ||
        output.capacity < output_bytes ||
        (output_bytes && (!output.data || (uintptr_t)output.data % out_align != 0))) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Invalid partition output extent or alignment");
    }

    typedef enum { KIND_VIEW = 0, KIND_ARRAY = 1, KIND_SOURCE = 2 } part_in_kind;
    part_in_kind kinds[B2ND_EXEC_MAX_INPUTS] = {0};
    storage_input base_arrays[B2ND_EXEC_MAX_INPUTS];
    b2nd_exec_view bound_views[B2ND_EXEC_MAX_INPUTS] = {0};
    b2nd_exec_source bound_srcs[B2ND_EXEC_MAX_INPUTS] = {0};
    b2nd_exec_input_meta in_metas[B2ND_EXEC_MAX_INPUTS] = {0};
    bool has_source = false;
    size_t per_group_gather = 0;
    size_t base_decode = 0;
    size_t max_group_items = sched_info.tile_items;
    for (size_t i = 0; i < ninputs; i++) {
        me_status st = b2nd_exec_schedule_input(schedule, i, &in_metas[i]);
        if (st != ME_SUCCESS) return storage_error(error, st, "Invalid partition input slot");
        bool is_arr = arrays && arrays[i] != NULL;
        bool is_src = sources && sources[i].read != NULL;
        if (is_arr && is_src) {
            return storage_error(error, ME_ERR_INVALID_ARG, "Ambiguous partition input binding");
        }
        if (is_arr) {
            kinds[i] = KIND_ARRAY;
            memset(&base_arrays[i], 0, sizeof(base_arrays[i]));
            base_arrays[i].array = arrays[i];
            st = storage_validate(&base_arrays[i], &in_metas[i], error);
            if (st != ME_SUCCESS) return st;
            st = storage_validate_chunks(&base_arrays[i], &output, error);
            if (st != ME_SUCCESS) return st;
            size_t bbytes = base_arrays[i].block_items * in_metas[i].itemsize;
            if (base_arrays[i].items && bbytes > max_decode_bytes - base_decode) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Storage decode buffer budget exceeded");
            }
            if (base_arrays[i].items) base_decode += bbytes;
        } else if (is_src) {
            kinds[i] = KIND_SOURCE;
            has_source = true;
            if (sources[i].struct_size != sizeof(sources[i]) ||
                sources[i].dtype != in_metas[i].dtype ||
                sources[i].itemsize != in_metas[i].itemsize ||
                sources[i].ndim != in_metas[i].ndim) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Partition source metadata mismatch");
            }
            for (size_t d = 0; d < in_metas[i].ndim; d++) {
                if (sources[i].shape[d] != in_metas[i].shape[d]) {
                    return storage_error(error, ME_ERR_INVALID_ARG, "Partition source shape mismatch");
                }
            }
            bound_srcs[i] = sources[i];
        } else if (views) {
            kinds[i] = KIND_VIEW;
            st = b2nd_exec_validate_view(schedule, i, &views[i], error);
            if (st != ME_SUCCESS) return st;
            if (views[i].capacity && output_bytes && output.capacity &&
                (uintptr_t)views[i].base < (uintptr_t)output.data + output.capacity &&
                (uintptr_t)output.data < (uintptr_t)views[i].base + views[i].capacity) {
                return storage_error(error, ME_ERR_UNSUPPORTED, "Aliased partition output is unsupported");
            }
            bound_views[i] = views[i];
        } else {
            return storage_error(error, ME_ERR_INVALID_ARG, "Missing partition input binding");
        }
        if (output_items) {
            size_t in_bytes = max_group_items * in_metas[i].itemsize;
            if (per_group_gather > SIZE_MAX - in_bytes) {
                return storage_error(error, ME_ERR_INVALID_ARG, "Partition gather size overflow");
            }
            per_group_gather += in_bytes;
        }
    }
    if (per_group_gather > max_gather_bytes) {
        return storage_error(error, ME_ERR_INVALID_ARG, "Partition gather buffer budget exceeded");
    }

    if (!output_items) {
        if (raise_mask) {
            me_fp_scope scope;
            me_fp_status fp = {0};
            me_status st = me_fp_scope_begin(&scope, &fp, error);
            if (st != ME_SUCCESS) return st;
            me_status end_st = me_fp_scope_end(&scope, NULL);
            if (report) {
                report->execution.fp_observed = true;
                report->execution.fp.supported = fp.supported;
            }
            if (end_st != ME_SUCCESS) return end_st;
            if (!fp.supported) {
                return storage_error(error, ME_ERR_UNSUPPORTED,
                                     "Floating exception reporting is unavailable");
            }
        }
        if (report) report->execution.tasks = 1;
        return ME_SUCCESS;
    }

    size_t cache_slots[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t extra_budget = max_decode_bytes - base_decode;
    for (size_t i = 0; i < ninputs; i++) {
        if (kinds[i] == KIND_ARRAY && base_arrays[i].items) cache_slots[i] = 1;
    }
    for (size_t pass = 1; pass < STORAGE_BLOCK_CACHE_SLOTS; pass++) {
        bool grew = false;
        for (size_t i = 0; i < ninputs; i++) {
            if (!cache_slots[i]) continue;
            size_t bbytes = base_arrays[i].block_items * in_metas[i].itemsize;
            size_t blocks_per_chunk = bbytes && base_arrays[i].bytes ? base_arrays[i].bytes / bbytes : 1;
            size_t total_blocks = (size_t)arrays[i]->sc->nchunks * blocks_per_chunk;
            if (cache_slots[i] >= total_blocks) continue;
            if (bbytes <= extra_budget) {
                cache_slots[i]++;
                extra_budget -= bbytes;
                grew = true;
            }
        }
        if (!grew) break;
    }

    storage_block_cache bcaches[B2ND_EXEC_MAX_INPUTS];
    memset(bcaches, 0, sizeof(bcaches));
    unsigned char *in_bufs[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t *idx_buf = NULL;
    unsigned char *out_buf = NULL;
    b2nd_exec_storage_report local_rep = {0};
    local_rep.execution.tasks = 1;
    local_rep.execution.gather_bytes = per_group_gather;
    local_rep.execution.mapped_bytes = sched_info.mapped_bytes;
    local_rep.execution.worker_bytes = per_group_gather + sched_info.mapped_bytes;
    me_status status = ME_SUCCESS;

    if (has_source && max_group_items) {
        idx_buf = malloc(max_group_items * sizeof(size_t));
        if (!idx_buf) status = storage_error(error, ME_ERR_OOM, "Cannot allocate partition index buffer");
    }
    if (status == ME_SUCCESS && sched_info.mapped_bytes) {
        out_buf = malloc(sched_info.mapped_bytes);
        if (!out_buf) status = storage_error(error, ME_ERR_OOM, "Cannot allocate partition output buffer");
    }
    for (size_t i = 0; status == ME_SUCCESS && i < ninputs; i++) {
        size_t in_bytes = max_group_items * in_metas[i].itemsize;
        in_bufs[i] = in_bytes ? malloc(in_bytes) : NULL;
        if (in_bytes && !in_bufs[i]) {
            status = storage_error(error, ME_ERR_OOM, "Cannot allocate partition gather buffer");
            break;
        }
        if (kinds[i] == KIND_ARRAY && cache_slots[i]) {
            storage_block_cache *bc = &bcaches[i];
            bc->array = base_arrays[i].array;
            bc->itemsize = in_metas[i].itemsize;
            bc->block_items = base_arrays[i].block_items;
            bc->block_bytes = base_arrays[i].block_items * in_metas[i].itemsize;
            bc->nslots = cache_slots[i];
            memcpy(bc->chunk_grid, base_arrays[i].chunk_grid, sizeof(bc->chunk_grid));
            memcpy(bc->block_grid, base_arrays[i].block_grid, sizeof(bc->block_grid));
            blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
            dp.nthreads = 1;
            bc->dctx = blosc2_create_dctx(dp);
            if (!bc->dctx) {
                status = storage_error(error, ME_ERR_OOM, "Cannot allocate partition decode context");
                break;
            }
            for (size_t s = 0; s < bc->nslots; s++) {
                bc->cached_chunk[s] = -1;
                bc->cached_block[s] = -1;
                bc->blocks[s] = bc->block_bytes ? malloc(bc->block_bytes) : NULL;
                if (bc->block_bytes && !bc->blocks[s]) {
                    status = storage_error(error, ME_ERR_OOM, "Cannot allocate partition block slot");
                    break;
                }
                local_rep.decode_bytes += bc->block_bytes;
            }
        }
    }

    size_t g_start[B2ND_EXEC_MAX_RANK] = {0};
    size_t g_stop[B2ND_EXEC_MAX_RANK] = {0};
    size_t ngroups = 1;
    bool scalar_blocks = plan_info.cardinality == ME_BLOCK_SCALAR;
    for (size_t d = 0; d < part.ndim; d++) {
        if (scalar_blocks) {
            g_start[d] = (size_t)part.sel_start[d];
            g_stop[d] = (size_t)part.sel_stop[d];
        } else {
            size_t p = (size_t)part.partitions[d];
            g_start[d] = (size_t)part.sel_start[d] / p;
            g_stop[d] = ((size_t)part.sel_stop[d] + p - 1) / p;
        }
        ngroups *= g_stop[d] - g_start[d];
    }

    bool scoped = (plan_info.dtype != ME_STRING) || raise_mask != 0;
    me_fp_scope scope;
    me_fp_status fp = {0};
    if (status == ME_SUCCESS && scoped) {
        local_rep.execution.fp_observed = true;
        status = me_fp_scope_begin(&scope, &fp, error);
        if (status == ME_SUCCESS && raise_mask && !fp.supported) {
            me_fp_scope_end(&scope, NULL);
            status = storage_error(error, ME_ERR_UNSUPPORTED,
                                   "Floating exception reporting is unavailable");
            scoped = false;
        }
    }

    size_t g_coord[B2ND_EXEC_MAX_RANK];
    memcpy(g_coord, g_start, sizeof(g_coord));
    me_buffer spans[B2ND_EXEC_MAX_INPUTS] = {0};
    for (size_t g_idx = 0; status == ME_SUCCESS && g_idx < ngroups; g_idx++) {
        size_t origin[B2ND_EXEC_MAX_RANK] = {0}, extent[B2ND_EXEC_MAX_RANK] = {0};
        int64_t origin_i64[B2ND_EXEC_MAX_RANK] = {0}, extent_i64[B2ND_EXEC_MAX_RANK] = {0};
        size_t group_items = 1;
        for (size_t d = 0; d < part.ndim; d++) {
            size_t o = g_coord[d] * (size_t)part.partitions[d];
            size_t rem = (size_t)part.domain[d] - o;
            size_t ext = (size_t)part.partitions[d] < rem ? (size_t)part.partitions[d] : rem;
            origin[d] = o;
            extent[d] = ext;
            origin_i64[d] = (int64_t)o;
            extent_i64[d] = (int64_t)ext;
            group_items *= ext;
        }
        for (size_t i = 0; status == ME_SUCCESS && i < ninputs; i++) {
            size_t bytes = group_items * in_metas[i].itemsize;
            if (kinds[i] == KIND_ARRAY) {
                status = storage_read_box_cached(&bcaches[i], part.ndim, origin, extent,
                                                 in_bufs[i], error);
            } else if (kinds[i] == KIND_SOURCE) {
                status = partition_read_source_box(&bound_srcs[i], part.ndim, origin, extent,
                                                   group_items, idx_buf, in_bufs[i], error);
            } else {
                b2nd_exec_gather_box(schedule, i, &bound_views[i], origin, extent, in_bufs[i]);
            }
            if (status != ME_SUCCESS) break;
            local_rep.execution.gathered_bytes += bytes;
            spans[i] = (me_buffer){in_bufs[i], bytes};
        }
        if (status != ME_SUCCESS) break;
        me_block block = ME_BLOCK_INITIALIZER;
        block.nitems = group_items;
        if (plan_info.context_ndim > 0) {
            block.ndim = (int)part.ndim;
            block.logical_shape = part.domain;
            block.origin = origin_i64;
            block.extent = extent_i64;
        }
        size_t group_out_items = scalar_blocks ? 1 : group_items;
        size_t group_out_bytes = group_out_items * out_width;
        me_fp_status tile_fp = {0};
        me_run_options run_opts = ME_RUN_OPTIONS_INITIALIZER;
        run_opts.fp_status = &tile_fp;
        me_execution_backend backend = ME_BACKEND_NONE;
        status = me_eval_block_backend(kernel, spans, ninputs,
                                       (me_mut_buffer){out_buf, group_out_bytes},
                                       &block, &run_opts, &backend, error);
        local_rep.execution.interpreter_calls += backend == ME_BACKEND_INTERPRETER;
        local_rep.execution.jit_calls += backend == ME_BACKEND_JIT;
        fp.flags |= tile_fp.flags;
        if (status != ME_SUCCESS) break;
        local_rep.execution.tiles++;
        local_rep.execution.map_calls++;
        local_rep.evaluated_groups++;
        if (group_items > local_rep.max_group_items) local_rep.max_group_items = group_items;
        if (scalar_blocks) {
            memcpy((unsigned char *)output.data + g_idx * out_width, out_buf, out_width);
        } else {
            partition_commit_elementwise(&part, origin, extent, out_buf, output.data, out_width);
        }
        if (g_idx + 1 < ngroups) {
            for (size_t d = part.ndim; d-- > 0;) {
                if (++g_coord[d] < g_stop[d]) break;
                g_coord[d] = g_start[d];
            }
        }
    }

    if (scoped && status != ME_ERR_UNSUPPORTED) {
        me_status end_st = me_fp_scope_end(&scope, status == ME_SUCCESS ? error : NULL);
        local_rep.execution.fp.flags |= fp.flags;
        local_rep.execution.fp.supported = fp.supported;
        if (status == ME_SUCCESS) status = end_st;
    }
    if (status == ME_SUCCESS && (local_rep.execution.fp.flags & raise_mask)) {
        status = storage_error(error, ME_ERR_EVAL, "Execution raised floating-point status");
    }
    for (size_t i = 0; i < ninputs; i++) {
        local_rep.decodes += bcaches[i].decodes;
        local_rep.decoded_bytes += bcaches[i].decoded_bytes;
        local_rep.cache_hits += bcaches[i].cache_hits;
        local_rep.read_spans += bcaches[i].read_spans;
        if (bcaches[i].dctx) blosc2_free_ctx(bcaches[i].dctx);
        for (size_t s = 0; s < STORAGE_BLOCK_CACHE_SLOTS; s++) free(bcaches[i].blocks[s]);
        free(in_bufs[i]);
    }
    free(out_buf);
    free(idx_buf);
    if (report) *report = local_rep;
    return status;
}

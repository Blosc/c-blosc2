/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#ifdef BLOSC2_HAVE_MINIEXPR
#include "b2nd-kernel.h"
#include "blosc-checked.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef B2ND_RESIDENT_MIN_I64_API
int b2nd_min_i64_prepare(int64_t items, int64_t tile, b2nd_min_i64_plan *plan) {
    if (!plan || items <= 0 || (uint64_t)items > SIZE_MAX / sizeof(int64_t) || tile <= 0) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *plan = (b2nd_min_i64_plan){items, tile};
    return BLOSC2_ERROR_SUCCESS;
}

int b2nd_min_i64_execute(const b2nd_min_i64_plan *plan, const int64_t *input,
                         int64_t items, int64_t *output, int64_t *tiles, size_t *scratch) {
    if (!plan || !input || !output || !tiles || !scratch || items != plan->items ||
        items <= 0 || (uint64_t)items > SIZE_MAX / sizeof(*input) || plan->tile <= 0) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    uint64_t best = 0;
    int64_t count = 0;
    for (int64_t pos = 0; pos < items; count++) {
        int64_t remaining = items - pos;
        int64_t span = remaining < plan->tile ? remaining : plan->tile;
        if (!me_extrema_span(ME_INT64, input + pos, (size_t)span, best, pos != 0, true, &best)) {
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        pos += span;
    }
    memcpy(output, &best, sizeof(*output));
    *tiles = count;
    *scratch = sizeof(best);
    return BLOSC2_ERROR_SUCCESS;
}
#endif

#ifdef B2ND_MAP_MIN_I64_API
struct b2nd_map_min_i64_plan {
    me_kernel *kernel;
    int64_t items, tile;
    size_t bytes;
};

static me_status map_min_error(me_error *error, me_status status, const char *message) {
    if (error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

me_status b2nd_map_min_i64_prepare(const char *source, int64_t items, int64_t tile,
                                  size_t max_scratch, b2nd_map_min_i64_plan **out, me_error *error) {
    if (out) {
        *out = NULL;
    }
    if (!out || !source || items <= 0 || tile <= 0 ||
        (uint64_t)items > SIZE_MAX / sizeof(int64_t)) {
        return map_min_error(error, ME_ERR_INVALID_ARG, "Invalid resident map/min geometry");
    }
    size_t lanes = (size_t)(items < tile ? items : tile);
    size_t bytes = lanes * sizeof(int64_t);
    if (bytes > max_scratch) {
        return map_min_error(error, ME_ERR_INVALID_ARG, "Resident map/min scratch budget exceeded");
    }
    me_input_spec input = {"x", ME_INT64, sizeof(int64_t)};
    me_signature signature = ME_SIGNATURE_INITIALIZER;
    signature.inputs = &input;
    signature.ninputs = 1;
    signature.cardinality = ME_ELEMENTWISE;
    me_prepare_options options = ME_PREPARE_OPTIONS_INITIALIZER;
    options.menudet_forbidden_features = ME_MENUDET_FEATURE_BLOCK_REDUCTIONS;
    me_kernel *kernel = NULL;
    me_status status = me_prepare_menudet("0.2", source, &signature, &options, &kernel, error);
    if (status != ME_SUCCESS) {
        return status;
    }
    me_kernel_info info = ME_KERNEL_INFO_INITIALIZER;
    status = me_describe(kernel, &info);
    if (status != ME_SUCCESS || info.context_ndim || info.cardinality != ME_ELEMENTWISE ||
        info.output_dtype != ME_INT64 || info.output_itemsize != sizeof(int64_t)) {
        me_kernel_free(kernel);
        return map_min_error(error, ME_ERR_SIGNATURE, "Resident map/min requires an int64 elementwise map");
    }
    b2nd_map_min_i64_plan *plan = malloc(sizeof(*plan));
    if (!plan) {
        me_kernel_free(kernel);
        return map_min_error(error, ME_ERR_OOM, "Cannot allocate resident map/min plan");
    }
    *plan = (b2nd_map_min_i64_plan){kernel, items, (int64_t)lanes, bytes};
    *out = plan;
    return ME_SUCCESS;
}

void b2nd_map_min_i64_free(b2nd_map_min_i64_plan *plan) {
    if (plan) {
        me_kernel_free(plan->kernel);
        free(plan);
    }
}

me_status b2nd_map_min_i64_execute(const b2nd_map_min_i64_plan *plan, const int64_t *input,
                                  int64_t items, int64_t *output,
                                  b2nd_map_min_i64_report *report, me_error *error) {
    if (report) {
        *report = (b2nd_map_min_i64_report){0};
    }
    if (!plan || !input || !output || !report || items != plan->items) {
        return map_min_error(error, ME_ERR_INVALID_ARG, "Input does not match resident map/min plan");
    }
    int64_t *mapped = malloc(plan->bytes);
    if (!mapped) {
        return map_min_error(error, ME_ERR_OOM, "Cannot allocate resident map/min scratch");
    }
    report->scratch = plan->bytes;
    uint64_t best = 0;
    me_status status = ME_SUCCESS;
    for (int64_t pos = 0; pos < items;) {
        size_t count = (size_t)(items - pos < plan->tile ? items - pos : plan->tile);
        me_buffer buffer = {input + pos, count * sizeof(*input)};
        me_mut_buffer output_tile = {mapped, plan->bytes};
        me_block block = ME_BLOCK_INITIALIZER;
        block.nitems = count;
        me_fp_status fp = {0};
        me_run_options options = ME_RUN_OPTIONS_INITIALIZER;
        options.fp_status = &fp;
        status = me_eval_block(plan->kernel, &buffer, 1, output_tile, &block, &options, error);
        if (status != ME_SUCCESS) {
            break;
        }
        report->fp.flags |= fp.flags;
        report->fp.supported = report->tiles ? report->fp.supported && fp.supported : fp.supported;
        report->tiles++;
        if (!me_extrema_span(ME_INT64, mapped, count, best, pos != 0, true, &best)) {
            status = map_min_error(error, ME_ERR_EVAL, "Resident map/min reduction failed");
            break;
        }
        pos += (int64_t)count;
    }
    free(mapped);
    if (status == ME_SUCCESS) {
        memcpy(output, &best, sizeof(*output));
    }
    return status;
}
#endif

static int eval_kernel_grid(const me_grid_spec *spec, int64_t nchunks, int64_t nblocks,
                            size_t lanes, const me_kernel *kernel, size_t ninputs,
                            const size_t *input_widths, size_t max_scratch, b2nd_kernel_read read,
                            void *read_context, b2nd_kernel_write write, void *write_context,
                            me_fp_status *fp, me_error *error) {
    enum { max_inputs = 128 };
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
    }
    if (fp != NULL) {
        *fp = (me_fp_status){0};
    }
    me_kernel_info kernel_info = ME_KERNEL_INFO_INITIALIZER;
    if (write == NULL ||
        ninputs > max_inputs || (ninputs && (read == NULL || input_widths == NULL)) ||
        me_describe(kernel, &kernel_info) != ME_SUCCESS ||
        (kernel_info.context_ndim && kernel_info.context_ndim != spec->ndim)) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (nchunks == 0) {
        return BLOSC2_ERROR_SUCCESS;
    }
    if (nchunks < 0 || nblocks <= 0 || !lanes) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    size_t sizes[max_inputs];
    size_t output_bytes;
    if (!blosc2_checked_mul_size(kernel_info.cardinality == ME_BLOCK_SCALAR ? 1 : lanes,
                                  kernel_info.output_itemsize, &output_bytes) ||
        !output_bytes || output_bytes > max_scratch) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    size_t remaining = max_scratch - output_bytes;
    for (size_t i = 0; i < ninputs; i++) {
        if (!input_widths[i] || !blosc2_checked_mul_size(lanes, input_widths[i], &sizes[i]) ||
            sizes[i] > remaining) {
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        remaining -= sizes[i];
    }

    me_grid *grid = NULL;
    me_status status = me_grid_create(spec, &grid, error);
    if (status != ME_SUCCESS) {
        return status == ME_ERR_OOM ? BLOSC2_ERROR_MEMORY_ALLOC : BLOSC2_ERROR_INVALID_PARAM;
    }
    void *storage[max_inputs] = {0};
    me_buffer inputs[max_inputs];
    void *output = malloc(output_bytes);
    int rc = BLOSC2_ERROR_MEMORY_ALLOC;
    if (output == NULL) {
        goto cleanup;
    }
    for (size_t i = 0; i < ninputs; i++) {
        storage[i] = malloc(sizes[i]);
        if (storage[i] == NULL) {
            goto cleanup;
        }
        inputs[i] = (me_buffer){storage[i], sizes[i]};
    }

    for (int64_t chunk = 0; chunk < nchunks; chunk++) {
        for (int64_t block = 0; block < nblocks; block++) {
            me_grid_block_info info = ME_GRID_BLOCK_INFO_INITIALIZER;
            if (me_grid_describe_block(grid, chunk, block, &info, error) != ME_SUCCESS ||
                info.padded_nitems != lanes) {
                rc = BLOSC2_ERROR_INVALID_PARAM;
                goto cleanup;
            }
            if (info.valid_nitems) {
                for (size_t i = 0; i < ninputs; i++) {
                    rc = read(read_context, i, chunk, block, &info,
                              (me_mut_buffer){storage[i], sizes[i]});
                    if (rc < 0) {
                        goto cleanup;
                    }
                }
            }
            me_fp_status group_fp;
            me_run_options options = ME_RUN_OPTIONS_INITIALIZER;
            options.fp_status = &group_fp;
            status = me_grid_eval_block(grid, kernel, chunk, block, inputs, ninputs,
                                        (me_mut_buffer){output, output_bytes}, fp ? &options : NULL, error);
            if (status != ME_SUCCESS) {
                rc = status == ME_ERR_OOM ? BLOSC2_ERROR_MEMORY_ALLOC : BLOSC2_ERROR_FAILURE;
                goto cleanup;
            }
            if (fp != NULL) {
                fp->flags |= group_fp.flags;
                fp->supported = group_fp.supported;
            }
            rc = write(write_context, chunk, block, &info, (me_buffer){output, output_bytes});
            if (rc < 0) {
                goto cleanup;
            }
        }
    }
    rc = BLOSC2_ERROR_SUCCESS;

cleanup:
    free(output);
    for (size_t i = 0; i < ninputs; i++) {
        free(storage[i]);
    }
    me_grid_free(grid);
    return rc;
}

int b2nd_eval_kernel_grid(const b2nd_array_t *geometry, const me_kernel *kernel,
                         size_t ninputs, const size_t *input_widths, size_t max_scratch,
                         b2nd_kernel_read read, void *read_context,
                         b2nd_kernel_write write, void *write_context, me_error *error) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
    }
    if (geometry == NULL || geometry->sc == NULL) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (geometry->nitems && (geometry->blocknitems <= 0 || geometry->extchunknitems <= 0 ||
        geometry->extchunknitems % geometry->blocknitems || geometry->sc->nchunks <= 0)) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    me_grid_spec spec = {sizeof(spec), geometry->ndim, geometry->shape,
                        geometry->chunkshape, geometry->blockshape};
    int64_t nchunks = geometry->nitems ? geometry->sc->nchunks : 0;
    int64_t nblocks = nchunks ? geometry->extchunknitems / geometry->blocknitems : 0;
    return eval_kernel_grid(&spec, nchunks, nblocks, (size_t)geometry->blocknitems, kernel,
                            ninputs, input_widths, max_scratch, read, read_context, write, write_context, NULL, error);
}

int b2nd_eval_kernel_partitions_1d(int64_t size, int32_t partition, const me_kernel *kernel,
                                 size_t ninputs, const size_t *input_widths, size_t max_scratch,
                                 b2nd_kernel_read read, void *read_context,
                                 b2nd_kernel_write write, void *write_context, me_fp_status *fp, me_error *error) {
    if (size < 0 || partition <= 0) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
        }
        if (fp != NULL) {
            *fp = (me_fp_status){0};
        }
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    /* One virtual chunk and one block per logical partition. Storage
     * geometry is known only to the callbacks and cannot change grouping. */
    me_grid_spec spec = {sizeof(spec), 1, &size, &partition, &partition};
    int64_t groups = size / partition + (size % partition != 0);
    return eval_kernel_grid(&spec, groups, 1, (size_t)partition, kernel, ninputs, input_widths,
                            max_scratch, read, read_context, write, write_context, fp, error);
}
#endif

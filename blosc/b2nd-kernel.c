/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#ifdef BLOSC2_HAVE_MINIEXPR
#include "b2nd-kernel.h"
#include "blosc-checked.h"

#include <stdlib.h>
#include <string.h>

static int eval_kernel_grid(const me_grid_spec *spec, int64_t nchunks, int64_t nblocks,
                            size_t lanes, const me_kernel *kernel, size_t ninputs,
                            const size_t *input_widths, size_t max_scratch, b2nd_kernel_read read,
                            void *read_context, b2nd_kernel_write write, void *write_context,
                            me_error *error) {
    enum { max_inputs = 128 };
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
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
            status = me_grid_eval_block(grid, kernel, chunk, block, inputs, ninputs,
                                        (me_mut_buffer){output, output_bytes}, NULL, error);
            if (status != ME_SUCCESS) {
                rc = status == ME_ERR_OOM ? BLOSC2_ERROR_MEMORY_ALLOC : BLOSC2_ERROR_FAILURE;
                goto cleanup;
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
                            ninputs, input_widths, max_scratch, read, read_context, write, write_context, error);
}

int b2nd_eval_kernel_partitions_1d(int64_t size, int32_t partition, const me_kernel *kernel,
                                 size_t ninputs, const size_t *input_widths, size_t max_scratch,
                                 b2nd_kernel_read read, void *read_context,
                                 b2nd_kernel_write write, void *write_context, me_error *error) {
    if (size < 0 || partition <= 0) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
        }
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    /* One virtual chunk and one block per logical partition. Storage
     * geometry is known only to the callbacks and cannot change grouping. */
    me_grid_spec spec = {sizeof(spec), 1, &size, &partition, &partition};
    int64_t groups = size / partition + (size % partition != 0);
    return eval_kernel_grid(&spec, groups, 1, (size_t)partition, kernel, ninputs, input_widths,
                            max_scratch, read, read_context, write, write_context, error);
}
#endif

/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#ifdef BLOSC2_HAVE_MINIEXPR
#include "b2nd-kernel.h"
#include "blosc-checked.h"

#include <stdlib.h>
#include <string.h>

int b2nd_eval_kernel_grid(const b2nd_array_t *geometry, const me_kernel *kernel,
                         size_t ninputs, const size_t *input_widths, size_t max_scratch,
                         b2nd_kernel_read read, void *read_context,
                         b2nd_kernel_write write, void *write_context, me_error *error) {
    enum { max_inputs = 128 };
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
    }
    me_kernel_info kernel_info = ME_KERNEL_INFO_INITIALIZER;
    if (geometry == NULL || geometry->sc == NULL || write == NULL ||
        ninputs > max_inputs || (ninputs && (read == NULL || input_widths == NULL)) ||
        me_describe(kernel, &kernel_info) != ME_SUCCESS ||
        (kernel_info.context_ndim && kernel_info.context_ndim != geometry->ndim)) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (geometry->nitems == 0) {
        return BLOSC2_ERROR_SUCCESS;
    }
    if (geometry->blocknitems <= 0 || geometry->extchunknitems <= 0 ||
        geometry->extchunknitems % geometry->blocknitems || geometry->sc->nchunks <= 0) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    const int64_t nblocks = geometry->extchunknitems / geometry->blocknitems;
    const size_t lanes = (size_t)geometry->blocknitems;
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
    me_grid_spec spec = {sizeof(spec), geometry->ndim, geometry->shape,
                        geometry->chunkshape, geometry->blockshape};
    me_status status = me_grid_create(&spec, &grid, error);
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

    for (int64_t chunk = 0; chunk < geometry->sc->nchunks; chunk++) {
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
#endif

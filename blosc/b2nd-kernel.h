/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#ifndef BLOSC_B2ND_KERNEL_H
#define BLOSC_B2ND_KERNEL_H

#include "b2nd.h"
#include "miniexpr_grid.h"
#include "miniexpr_reduce.h"

/* Internal specialized plan for input(int64, rank=1) -> whole-axis min.
 * The plan contains only immutable geometry. Execute borrows a contiguous
 * resident view for this call and publishes the scalar only on success. */
#ifdef ME_EXTREMA_SPAN_API
#define B2ND_RESIDENT_MIN_I64_API 1
typedef struct {
    int64_t items;
    int64_t tile;
} b2nd_min_i64_plan;

int b2nd_min_i64_prepare(int64_t items, int64_t tile, b2nd_min_i64_plan *plan);
int b2nd_min_i64_execute(const b2nd_min_i64_plan *plan, const int64_t *input,
                         int64_t items, int64_t *output, int64_t *tiles, size_t *scratch);
#endif

typedef int (*b2nd_kernel_read)(void *context, size_t input, int64_t chunk, int64_t block,
                               const me_grid_block_info *info, me_mut_buffer out);
typedef int (*b2nd_kernel_write)(void *context, int64_t chunk, int64_t block,
                                const me_grid_block_info *info, me_buffer result);

/* Internal serial pilot: walk physical b2nd blocks in storage order. The
 * caller owns the prepared kernel and supplies its input slot widths. Read
 * produces a natively aligned, padded block in scheduler-owned scratch.
 * Write consumes the result synchronously (one item for a block scalar).
 * max_scratch bounds the scheduler's reusable buffers, not core evaluation
 * scratch. On a failure, any preceding writes remain committed. */
int b2nd_eval_kernel_grid(const b2nd_array_t *geometry, const me_kernel *kernel,
                         size_t ninputs, const size_t *input_widths, size_t max_scratch,
                         b2nd_kernel_read read, void *read_context,
                         b2nd_kernel_write write, void *write_context, me_error *error);

/* Internal logical-grid pilot. Each nonempty group is one callback chunk
 * with block index zero, including a possibly short final group. The optional
 * report combines flags from completed evaluations; an empty domain reports
 * zero flags and no observed floating-point status support. */
int b2nd_eval_kernel_partitions_1d(int64_t size, int32_t partition, const me_kernel *kernel,
                                 size_t ninputs, const size_t *input_widths, size_t max_scratch,
                                 b2nd_kernel_read read, void *read_context,
                                 b2nd_kernel_write write, void *write_context, me_fp_status *fp, me_error *error);

#endif

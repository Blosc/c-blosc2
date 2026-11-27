/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#ifndef BLOSC_B2ND_EXEC_H
#define BLOSC_B2ND_EXEC_H

#include "miniexpr_kernel.h"
#include "miniexpr_reduce.h"
#ifndef ME_FP_SCOPE_API
#error "The typed executor requires the miniexpr shared FP scope API"
#endif
#ifndef ME_KERNEL_INPUT_INFO_API
#error "The typed executor requires miniexpr runtime-slot introspection"
#endif
#ifndef ME_SERIAL_REDUCE_API
#error "The typed executor requires miniexpr serial reduction primitives"
#endif

/* Experimental internal boundary, not an installed ABI or interchange format.
 * IDs are array indices. Slots, unlike diagnostic names, bind runtime inputs.
 * Unsupported operations/options fail before compilation; no adapter fallback. */
#define B2ND_EXEC_MAX_NODES 1024
#define B2ND_EXEC_MAX_INPUTS 128
#define B2ND_EXEC_MAX_CAPTURES 128
#define B2ND_EXEC_MAX_DEPTH 128
#define B2ND_EXEC_MAX_RANK 16

typedef enum {
    B2ND_EXEC_INPUT,
    B2ND_EXEC_CAPTURE,
    B2ND_EXEC_ADD,
    B2ND_EXEC_SUBTRACT,
    B2ND_EXEC_MULTIPLY,
    B2ND_EXEC_NEGATE,
    B2ND_EXEC_MIN,
    B2ND_EXEC_SUM,
    B2ND_EXEC_PROD,
    B2ND_EXEC_MAX
} b2nd_exec_op;

typedef struct {
    size_t struct_size;
    uint32_t id;
    b2nd_exec_op op;
    uint32_t args[2];
    size_t nargs;
    uint32_t slot;
    me_dtype dtype;
    size_t itemsize;
    uint8_t scalar[16]; /* Exact native bytes, never normalized through double. */
    size_t scalar_size;
    me_capture_category category;
} b2nd_exec_node;

typedef struct {
    size_t struct_size;
    const b2nd_exec_node *nodes;
    size_t nnodes;
    size_t ninputs;
    uint32_t root;
} b2nd_exec_descriptor;

typedef struct {
    size_t struct_size;
    me_execution_policy execution;
    bool inherit_environment;
    bool integer_powers;
    me_casting casting;
} b2nd_exec_options;

#define B2ND_EXEC_OPTIONS_INITIALIZER \
    {sizeof(b2nd_exec_options), ME_EXEC_INTERPRET, false, false, ME_CAST_UNSAFE}

typedef struct b2nd_exec_plan b2nd_exec_plan;
typedef struct b2nd_exec_schedule b2nd_exec_schedule;

typedef struct {
    me_dtype dtype;
    size_t itemsize;
    size_t ndim;
    int64_t shape[B2ND_EXEC_MAX_RANK];
} b2nd_exec_input_meta;

typedef struct {
    me_dtype dtype;
    size_t itemsize;
    size_t ninputs;
    bool reduction;
    bool identity;
    me_cardinality cardinality;
    int context_ndim;
    me_dtype map_dtype;
    size_t map_itemsize;
    me_reduction_kind reducer;
    size_t plan_metadata_bytes; /* Excludes kernel/compiler allocations. */
} b2nd_exec_plan_info;

typedef struct {
    size_t nitems;
    size_t tile_items;
    size_t mapped_bytes; /* Invocation mapped tile only; excludes core scratch. */
    size_t reducer_bytes;
    size_t output_items;
    size_t schedule_metadata_bytes;
} b2nd_exec_schedule_info;

typedef struct {
    size_t tiles;
    size_t map_calls;
    size_t direct_tiles;
    size_t mapped_bytes;
    me_fp_status fp;
} b2nd_exec_report;

/* Prepare copies all retained descriptor data and owns its kernel. Metadata
 * specialization reads no input values. Schedules BORROW plans: destroy every
 * schedule before its plan. Independent calls share no mutable scratch. */
me_status b2nd_exec_prepare(const b2nd_exec_descriptor *descriptor,
                           const b2nd_exec_options *options,
                           b2nd_exec_plan **out, me_error *error);
/* Authored kernels enter the SAME scheduler without a graph/source rewrite.
 * Success transfers *kernel to the plan and clears the caller's pointer.
 * Failure leaves the caller's kernel intact. Captures/policy stay in the
 * prepared kernel. Context/block-scalar plans can be inspected but require a
 * future logical-group schedule; resident specialization rejects them. */
me_status b2nd_exec_prepare_kernel(me_kernel **kernel, me_reduction_kind reducer,
                                  b2nd_exec_plan **out, me_error *error);
me_status b2nd_exec_describe(const b2nd_exec_plan *plan, b2nd_exec_plan_info *out);
me_status b2nd_exec_specialize(const b2nd_exec_plan *plan,
                              const b2nd_exec_input_meta *inputs, size_t ninputs,
                              size_t tile_items, size_t max_mapped_bytes,
                              b2nd_exec_schedule **out, me_error *error);
me_status b2nd_exec_describe_schedule(const b2nd_exec_schedule *schedule,
                                     b2nd_exec_schedule_info *out);
/* Native, aligned, contiguous, disjoint buffers in slot order. Reduction
 * output is published only on success; map output may contain earlier tiles
 * and a partially written failing tile on failure. FP raise masks apply after
 * successful traversal, as in the graph adapter. Buffers/report/options are
 * invocation-local and never retained.
 * mapped_bytes excludes core evaluation/compiler allocation. */
me_status b2nd_exec_execute(const b2nd_exec_schedule *schedule,
                            const me_buffer *inputs, size_t ninputs,
                            me_mut_buffer output, unsigned raise_mask,
                            b2nd_exec_report *report, me_error *error);
void b2nd_exec_schedule_free(b2nd_exec_schedule *schedule);
void b2nd_exec_plan_free(b2nd_exec_plan *plan);

#endif

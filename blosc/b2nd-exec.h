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
#ifndef ME_TRUTH_REDUCE_API
#error "The typed executor requires miniexpr truth reduction primitives"
#endif
#ifndef ME_REDUCE_STORE_SCALAR_API
#error "The typed executor requires miniexpr scalar store primitives"
#endif
#ifndef ME_PAIRWISE_SUM_API
#error "The typed executor requires miniexpr pairwise sum primitives"
#endif
#ifndef ME_KERNEL_BACKEND_INFO_API
#error "The typed executor requires miniexpr invocation backend reporting"
#endif
#ifndef ME_KERNEL_PREPARE_POLICY_API
#error "The typed executor requires miniexpr compiler/math preparation policy"
#endif
#ifndef ME_KERNEL_PARTITION_INFO_API
#error "The typed executor requires miniexpr conservative partitionability facts"
#endif

/* Experimental internal boundary, not an installed ABI or interchange format.
 * IDs are array indices. Slots, unlike diagnostic names, bind runtime inputs.
 * Unsupported operations/options fail before compilation; no adapter fallback. */
#define B2ND_EXEC_MAX_NODES 1024
#define B2ND_EXEC_MAX_INPUTS 128
#define B2ND_EXEC_MAX_CAPTURES 128
#define B2ND_EXEC_MAX_DEPTH 128
#define B2ND_EXEC_MAX_RANK 16
#define B2ND_EXEC_MAX_TASKS 64

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
    B2ND_EXEC_MAX,
    B2ND_EXEC_ANY,
    B2ND_EXEC_ALL,
    B2ND_EXEC_ARGMIN,
    B2ND_EXEC_ARGMAX,
    B2ND_EXEC_VAR,
    B2ND_EXEC_STD,
    B2ND_EXEC_DIVIDE, B2ND_EXEC_CAST, B2ND_EXEC_FUNCTION,
    B2ND_EXEC_LT, B2ND_EXEC_GT, B2ND_EXEC_EQ, B2ND_EXEC_NE, B2ND_EXEC_LE, B2ND_EXEC_GE,
    B2ND_EXEC_BITAND, B2ND_EXEC_BITOR, B2ND_EXEC_BITXOR,
    B2ND_EXEC_FLOORDIV, B2ND_EXEC_MOD, B2ND_EXEC_POWER,
    B2ND_EXEC_LSHIFT, B2ND_EXEC_RSHIFT,
    B2ND_EXEC_INVERT, B2ND_EXEC_NOT, B2ND_EXEC_AND, B2ND_EXEC_OR, B2ND_EXEC_SELECT,
    B2ND_EXEC_POS
} b2nd_exec_op;

typedef struct {
    size_t struct_size;
    uint32_t id;
    b2nd_exec_op op;
    uint32_t args[3];
    size_t nargs;
    uint32_t slot;
    me_dtype dtype;
    size_t itemsize;
    uint8_t scalar[16]; /* Exact native bytes, never normalized through double. */
    size_t scalar_size;
    me_capture_category category;
    bool has_axes; /* False: all axes (whole-domain). True: axes[0..naxes-1]. */
    size_t naxes;
    int axes[B2ND_EXEC_MAX_RANK];
    bool keepdims;
    bool has_initial;
    me_dtype initial_dtype;
    me_capture_category initial_category;
    uint8_t initial_scalar[16];
    size_t initial_size;
    double ddof;
    char function[32]; /* Identifier only; numerical arity/inference are core-owned. */
    bool has_where;
    uint32_t where; /* Earlier Boolean input node; never an interchange reference. */
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
    me_math_backend math_backend;
    me_jit_compiler jit_compiler;
} b2nd_exec_options;

#define B2ND_EXEC_OPTIONS_INITIALIZER \
    {sizeof(b2nd_exec_options), ME_EXEC_INTERPRET, false, false, ME_CAST_UNSAFE, \
     ME_MATH_DEFAULT, ME_COMPILER_DEFAULT}

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
    bool partitionable;
    me_cardinality cardinality;
    int context_ndim;
    me_dtype map_dtype;
    size_t map_itemsize;
    me_reduction_kind reducer;
    size_t plan_metadata_bytes; /* Excludes kernel/compiler allocations. */
    bool has_axes;
    size_t naxes;
    int axes[B2ND_EXEC_MAX_RANK];
    bool keepdims;
    bool has_initial;
    bool has_jit; /* Prepared availability, not proof that an invocation used JIT. */
    bool has_where;
} b2nd_exec_plan_info;

typedef struct {
    size_t nitems;
    size_t tile_items;
    size_t mapped_bytes; /* Invocation mapped tile only; excludes core scratch. */
    size_t reducer_bytes;
    size_t output_items;
    size_t schedule_metadata_bytes;
    size_t ndim;
    int64_t shape[B2ND_EXEC_MAX_RANK];
    size_t broadcast_gather_bytes;
    size_t output_ndim;
    int64_t output_shape[B2ND_EXEC_MAX_RANK];
    size_t reduction_groups;
    size_t reduction_lanes;
    bool pairwise_sum;
} b2nd_exec_schedule_info;

typedef struct {
    size_t tiles;
    size_t map_calls;
    size_t direct_tiles;
    size_t mapped_bytes;
    me_fp_status fp;
    size_t tasks;        /* Callbacks actually entered, including serial execution. */
    size_t worker_bytes; /* Aggregate mapped + gather scratch actually held this call. */
    size_t gather_bytes; /* Per-job gather scratch allocation. */
    size_t gathered_bytes;
    size_t staging_bytes; /* Separate transactional alias staging buffer, if used. */
    size_t interpreter_calls; /* Map routes entered, including failed calls. */
    size_t jit_calls;
    bool fp_observed; /* Distinguishes absent observation from unsupported FP. */
} b2nd_exec_report;

/* Writable, native-endian, provably injective destination view. */
typedef struct {
    size_t struct_size;
    void *base;
    size_t capacity;
    size_t offset;
    int64_t strides[B2ND_EXEC_MAX_RANK];
} b2nd_exec_output_view;

/* Borrowed resident array view. `base` and `capacity` describe the entire
 * owning allocation; `offset` is the byte offset of element [0, ..., 0]
 * within [base, base + capacity). `strides` are signed byte strides.
 * `byte_order`: 0 = native, 1 = little-endian, 2 = big-endian. */
typedef struct {
    size_t struct_size;
    const void *base;
    size_t capacity;
    size_t offset;
    int64_t strides[B2ND_EXEC_MAX_RANK];
    unsigned byte_order;
} b2nd_exec_view;

/* Invocation-local job request. Hosts request at most `workers` jobs; the
 * executor decides how many are safe. Aggregate mapped scratch across those
 * jobs must stay within `max_worker_scratch`; the job count is capped to fit.
 * A request the contract cannot honour (ordered reducer, budget below one
 * job) is rejected before any task starts, never silently serialized or
 * replayed. */
typedef struct {
    size_t struct_size;
    size_t workers;
    size_t max_worker_scratch;
} b2nd_exec_jobs;

typedef struct {
    size_t struct_size;
    bool has_axes;
    size_t naxes;
    int axes[B2ND_EXEC_MAX_RANK];
    bool keepdims;
    bool has_initial;
    me_dtype initial_dtype;
    me_capture_category initial_category;
    uint8_t initial_scalar[16];
    size_t initial_size;
} b2nd_exec_reduce_opts;

typedef struct {
    size_t struct_size;
    size_t ndim;
    int64_t domain[B2ND_EXEC_MAX_RANK];
    int64_t partitions[B2ND_EXEC_MAX_RANK];
    bool has_selection;
    int64_t sel_start[B2ND_EXEC_MAX_RANK];
    int64_t sel_stop[B2ND_EXEC_MAX_RANK];
} b2nd_exec_partition_spec;

/* Prepare copies all retained descriptor data and owns its kernel. Metadata
 * specialization reads no input values. Schedules BORROW plans: destroy every
 * schedule before its plan. Independent calls share no mutable scratch. */
me_status b2nd_exec_prepare(const b2nd_exec_descriptor *descriptor,
                           const b2nd_exec_options *options,
                           b2nd_exec_plan **out, me_error *error);
/* Authored kernels enter the SAME scheduler without a graph/source rewrite.
 * Success transfers *kernel to the plan and clears the caller's pointer.
 * Failure leaves the caller's kernel intact. Captures/policy stay in the
 * prepared kernel. */
me_status b2nd_exec_prepare_kernel(me_kernel **kernel, me_reduction_kind reducer,
                                  b2nd_exec_plan **out, me_error *error);
/* Borrow an already-prepared caller-owned core kernel (the caller must keep
 * `kernel` alive until `b2nd_exec_plan_free`). Supports optional reduction opts
 * as well as context/block-scalar/string/complex kernels for partition specs. */
me_status b2nd_exec_prepare_borrowed_kernel(const me_kernel *kernel,
                                           me_reduction_kind reducer,
                                           const b2nd_exec_reduce_opts *opts,
                                           b2nd_exec_plan **out, me_error *error);
me_status b2nd_exec_describe(const b2nd_exec_plan *plan, b2nd_exec_plan_info *out);
me_status b2nd_exec_specialize(const b2nd_exec_plan *plan,
                              const b2nd_exec_input_meta *inputs, size_t ninputs,
                              size_t tile_items, size_t max_mapped_bytes,
                              b2nd_exec_schedule **out, me_error *error);
me_status b2nd_exec_specialize_ex(const b2nd_exec_plan *plan,
                                 const b2nd_exec_input_meta *inputs, size_t ninputs,
                                 size_t tile_items, size_t max_mapped_bytes,
                                 bool pairwise_sum,
                                 b2nd_exec_schedule **out, me_error *error);
/* Specialize an elementwise/block-scalar/context plan over explicit immutable
 * logical partitions, independent of physical storage chunks/blocks. */
me_status b2nd_exec_specialize_partitions(const b2nd_exec_plan *plan,
                                          const b2nd_exec_input_meta *inputs,
                                          size_t ninputs,
                                          const b2nd_exec_partition_spec *spec,
                                          size_t max_group_bytes,
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
                            const b2nd_exec_jobs *jobs,
                            b2nd_exec_report *report, me_error *error);
/* Resident views supporting N-D broadcasting, arbitrary byte strides,
 * unaligned offsets and endian conversion via bounded per-job gathering.
 * Output remains native, aligned, contiguous and disjoint. */
me_status b2nd_exec_execute_views(const b2nd_exec_schedule *schedule,
                                  const b2nd_exec_view *inputs, size_t ninputs,
                                  me_mut_buffer output, unsigned raise_mask,
                                  size_t max_gather_bytes,
                                  const b2nd_exec_jobs *jobs,
                                  b2nd_exec_report *report, me_error *error);
/* Descriptor-output variant supporting disjoint strided/reversed/transposed
 * destinations without changing logical evaluation or reduction order. */
me_status b2nd_exec_execute_into(const b2nd_exec_schedule *schedule,
                                 const b2nd_exec_view *inputs, size_t ninputs,
                                 const b2nd_exec_output_view *output,
                                 unsigned raise_mask, size_t max_gather_bytes,
                                 const b2nd_exec_jobs *jobs,
                                 b2nd_exec_report *report, me_error *error);
/* Transactional alias adapter: evaluates into a separate staging buffer
 * bounded by `staging_budget`, then commits to `output` only on success.
 * Failure leaves the final destination unchanged. */
me_status b2nd_exec_execute_alias(const b2nd_exec_schedule *schedule,
                                  const b2nd_exec_view *inputs, size_t ninputs,
                                  const b2nd_exec_output_view *output,
                                  size_t staging_budget,
                                  unsigned raise_mask, size_t max_gather_bytes,
                                  const b2nd_exec_jobs *jobs,
                                  b2nd_exec_report *report, me_error *error);
void b2nd_exec_schedule_free(b2nd_exec_schedule *schedule);
void b2nd_exec_plan_free(b2nd_exec_plan *plan);

#endif

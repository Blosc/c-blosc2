/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC2_HAVE_MINIEXPR

#include "b2nd_exec.h"

#include <stdio.h>
#include <string.h>

static b2nd_exec_status_t stub_unsupported(b2nd_exec_error_t *error, const char *message) {
  if (error && error->struct_size >= B2ND_EXEC_ERROR_V1_SIZE) {
    size_t sz = error->struct_size;
    b2nd_exec_error_t local = B2ND_EXEC_ERROR_INITIALIZER;
    local.struct_size = sz;
    local.status = B2ND_EXEC_ERR_UNSUPPORTED;
    snprintf(local.message, sizeof(local.message), "%s", message);
    memcpy(error, &local, sz < sizeof(local) ? sz : sizeof(local));
  }
  return B2ND_EXEC_ERR_UNSUPPORTED;
}

bool b2nd_exec_available(void) {
  return false;
}

b2nd_exec_status_t b2nd_plan_from_expr(const char *source, const b2nd_exec_input_spec_t *inputs,
                                       size_t ninputs, const b2nd_exec_options_t *exec_opts,
                                       const b2nd_expr_options_t *expr_opts,
                                       b2nd_plan_t **out, b2nd_exec_error_t *error) {
  (void)source;
  (void)inputs;
  (void)ninputs;
  (void)exec_opts;
  (void)expr_opts;
  if (out) {
    *out = NULL;
  }
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

b2nd_exec_status_t b2nd_plan_from_graph(const b2nd_exec_graph_t *graph,
                                        const b2nd_exec_options_t *exec_opts,
                                        b2nd_plan_t **out, b2nd_exec_error_t *error) {
  (void)graph;
  (void)exec_opts;
  if (out) {
    *out = NULL;
  }
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

b2nd_exec_status_t b2nd_plan_compose(const b2nd_stage_binding_t *stages, size_t nstages,
                                     size_t ninputs, b2nd_plan_t **out,
                                     b2nd_exec_error_t *error) {
  (void)stages;
  (void)nstages;
  (void)ninputs;
  if (out) {
    *out = NULL;
  }
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

b2nd_exec_status_t b2nd_plan_describe(const b2nd_plan_t *plan, b2nd_plan_info_t *out) {
  (void)plan;
  (void)out;
  return B2ND_EXEC_ERR_UNSUPPORTED;
}

b2nd_exec_status_t b2nd_plan_describe_stage(const b2nd_plan_t *plan, size_t stage,
                                            b2nd_plan_info_t *out, int *sources,
                                            size_t sources_cap, size_t *ninputs_out,
                                            uint32_t *node_out) {
  (void)plan;
  (void)stage;
  (void)out;
  (void)sources;
  (void)sources_cap;
  if (ninputs_out) {
    *ninputs_out = 0;
  }
  if (node_out) {
    *node_out = UINT32_MAX;
  }
  return B2ND_EXEC_ERR_UNSUPPORTED;
}

void b2nd_plan_free(b2nd_plan_t *plan) {
  (void)plan;
}

b2nd_exec_status_t b2nd_schedule_create(const b2nd_plan_t *plan,
                                        const b2nd_exec_input_meta_t *inputs, size_t ninputs,
                                        const b2nd_schedule_options_t *options,
                                        b2nd_schedule_t **out, b2nd_exec_error_t *error) {
  (void)plan;
  (void)inputs;
  (void)ninputs;
  (void)options;
  if (out) {
    *out = NULL;
  }
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

b2nd_exec_status_t b2nd_schedule_describe(const b2nd_schedule_t *schedule,
                                          b2nd_schedule_info_t *out,
                                          b2nd_stage_info_t *stages, size_t stages_cap) {
  (void)schedule;
  (void)out;
  (void)stages;
  (void)stages_cap;
  return B2ND_EXEC_ERR_UNSUPPORTED;
}

b2nd_exec_status_t b2nd_schedule_admit(const b2nd_schedule_t *schedule,
                                       const b2nd_operand_t *inputs, size_t ninputs,
                                       const b2nd_output_t *output, bool *admitted,
                                       b2nd_exec_error_t *error) {
  (void)schedule;
  (void)inputs;
  (void)ninputs;
  (void)output;
  if (admitted) {
    *admitted = false;
  }
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

void b2nd_schedule_free(b2nd_schedule_t *schedule) {
  (void)schedule;
}

b2nd_exec_status_t b2nd_execute(const b2nd_schedule_t *schedule,
                                const b2nd_operand_t *inputs, size_t ninputs,
                                const b2nd_output_t *output,
                                const b2nd_run_options_t *options,
                                b2nd_exec_report_t *report, b2nd_exec_error_t *error) {
  (void)schedule;
  (void)inputs;
  (void)ninputs;
  (void)output;
  (void)options;
  (void)report;
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

b2nd_exec_status_t b2nd_eval(const char *source, const char *const *names,
                             const b2nd_array_t *const *inputs, size_t ninputs,
                             const b2nd_exec_options_t *exec_opts,
                             const b2nd_expr_options_t *expr_opts,
                             const b2nd_schedule_options_t *sched_opts,
                             const b2nd_run_options_t *run_opts,
                             b2nd_context_t *out_ctx, b2nd_array_t **out,
                             b2nd_exec_report_t *report, b2nd_exec_error_t *error) {
  (void)source;
  (void)names;
  (void)inputs;
  (void)ninputs;
  (void)exec_opts;
  (void)expr_opts;
  (void)sched_opts;
  (void)run_opts;
  (void)out_ctx;
  (void)report;
  if (out) {
    *out = NULL;
  }
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

b2nd_exec_status_t b2nd_eval_into(const char *source, const char *const *names,
                                  const b2nd_operand_t *inputs, size_t ninputs,
                                  const b2nd_exec_options_t *exec_opts,
                                  const b2nd_expr_options_t *expr_opts,
                                  const b2nd_schedule_options_t *sched_opts,
                                  const b2nd_run_options_t *run_opts,
                                  const b2nd_output_t *output,
                                  b2nd_exec_report_t *report, b2nd_exec_error_t *error) {
  (void)source;
  (void)names;
  (void)inputs;
  (void)ninputs;
  (void)exec_opts;
  (void)expr_opts;
  (void)sched_opts;
  (void)run_opts;
  (void)output;
  (void)report;
  return stub_unsupported(error, "C-Blosc2 was built without MiniExpr execution support");
}

#else /* BLOSC2_HAVE_MINIEXPR */

#include "b2nd-exec-stages.h"
#include "b2nd-exec-private.h"
#include "context.h"
#include "frame.h"
#include "blosc-checked.h"
#include "miniexpr_reduce.h"

#undef B2ND_EXEC_OPTIONS_INITIALIZER
#include "b2nd_exec.h"
#include "b2nd_exec_kernel.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

struct b2nd_plan_s {
  int32_t refcount;
  bool is_staged;
  b2nd_exec_plan *single;
  b2nd_exec_staged_plan *staged;
  b2nd_plan_t **retained;
  size_t nretained;
};

struct b2nd_schedule_s {
  const b2nd_plan_t *owner;
  bool is_staged;
  bool is_partition;
  b2nd_exec_schedule *single;
  b2nd_exec_staged_schedule *staged;
  size_t ninputs;
  b2nd_exec_input_meta inputs[B2ND_EXEC_MAX_INPUTS];
};

static void api_plan_retain(const b2nd_plan_t *plan) {
  if (!plan) {
    return;
  }
  b2nd_plan_t *m = (b2nd_plan_t *)plan;
#if defined(_MSC_VER) && !defined(__clang__)
  _InterlockedIncrement((long volatile *)&m->refcount);
#else
  __atomic_add_fetch(&m->refcount, 1, __ATOMIC_RELAXED);
#endif
}

static b2nd_exec_status_t api_set_error(b2nd_exec_error_t *err,
                                        b2nd_exec_status_t status,
                                        int line, int column,
                                        uint32_t node, size_t stage,
                                        const char *message) {
  if (err && err->struct_size >= B2ND_EXEC_ERROR_V1_SIZE) {
    size_t sz = err->struct_size;
    b2nd_exec_error_t local = B2ND_EXEC_ERROR_INITIALIZER;
    local.struct_size = sz;
    local.status = status;
    local.line = line;
    local.column = column;
    local.node = node;
    local.stage = stage;
    if (message) {
      snprintf(local.message, sizeof(local.message), "%s", message);
    }
    memcpy(err, &local, sz < sizeof(local) ? sz : sizeof(local));
  }
  return status;
}

static void api_clear_error(b2nd_exec_error_t *err) {
  if (err && err->struct_size >= B2ND_EXEC_ERROR_V1_SIZE) {
    api_set_error(err, B2ND_EXEC_SUCCESS, 0, 0, UINT32_MAX, SIZE_MAX, NULL);
  }
}

static b2nd_exec_status_t me_to_api_status(me_status st) {
  switch (st) {
    case ME_SUCCESS: return B2ND_EXEC_SUCCESS;
    case ME_ERR_INVALID_ARG: return B2ND_EXEC_ERR_INVALID_ARG;
    case ME_ERR_UNSUPPORTED: return B2ND_EXEC_ERR_UNSUPPORTED;
    case ME_ERR_SOURCE: return B2ND_EXEC_ERR_SOURCE;
    case ME_ERR_SIGNATURE: return B2ND_EXEC_ERR_SIGNATURE;
    case ME_ERR_EVAL: return B2ND_EXEC_ERR_EVAL;
    case ME_ERR_OOM: return B2ND_EXEC_ERR_OOM;
    default: return B2ND_EXEC_ERR_EVAL;
  }
}

static me_status api_to_me_status(b2nd_exec_status_t st) {
  switch (st) {
    case B2ND_EXEC_SUCCESS: return ME_SUCCESS;
    case B2ND_EXEC_ERR_INVALID_ARG: return ME_ERR_INVALID_ARG;
    case B2ND_EXEC_ERR_UNSUPPORTED: return ME_ERR_UNSUPPORTED;
    case B2ND_EXEC_ERR_SOURCE: return ME_ERR_SOURCE;
    case B2ND_EXEC_ERR_SIGNATURE: return ME_ERR_SIGNATURE;
    case B2ND_EXEC_ERR_EVAL: return ME_ERR_EVAL;
    case B2ND_EXEC_ERR_OOM: return ME_ERR_OOM;
    default: return ME_ERR_EVAL;
  }
}

static b2nd_exec_status_t api_from_me_error(b2nd_exec_error_t *err, me_status st,
                                            const me_error *me_err,
                                            uint32_t node, size_t stage) {
  b2nd_exec_status_t pub = me_to_api_status(st);
  if (pub == B2ND_EXEC_SUCCESS) {
    api_clear_error(err);
    return B2ND_EXEC_SUCCESS;
  }
  const char *msg = (me_err && me_err->message[0]) ? me_err->message : "Execution error";
  return api_set_error(err, pub, me_err ? me_err->line : 0, me_err ? me_err->column : 0,
                       node, stage, msg);
}

static bool api_to_me_dtype(b2nd_exec_dtype_t dtype, me_dtype *out) {
  if ((int)dtype < (int)B2ND_DTYPE_AUTO || (int)dtype > (int)B2ND_DTYPE_BYTES) {
    return false;
  }
  *out = (me_dtype)dtype;
  return true;
}

static b2nd_exec_dtype_t me_to_api_dtype(me_dtype dtype) {
  if ((int)dtype < (int)ME_AUTO || (int)dtype > (int)ME_BYTES) {
    return B2ND_DTYPE_AUTO;
  }
  return (b2nd_exec_dtype_t)dtype;
}

static size_t api_dtype_width(b2nd_exec_dtype_t dtype) {
  switch (dtype) {
    case B2ND_DTYPE_BOOL:
    case B2ND_DTYPE_INT8:
    case B2ND_DTYPE_UINT8:
      return 1;
    case B2ND_DTYPE_INT16:
    case B2ND_DTYPE_UINT16:
      return 2;
    case B2ND_DTYPE_INT32:
    case B2ND_DTYPE_UINT32:
    case B2ND_DTYPE_FLOAT32:
      return 4;
    case B2ND_DTYPE_INT64:
    case B2ND_DTYPE_UINT64:
    case B2ND_DTYPE_FLOAT64:
    case B2ND_DTYPE_COMPLEX64:
      return 8;
    case B2ND_DTYPE_COMPLEX128:
      return 16;
    default:
      return 0;
  }
}

static b2nd_exec_status_t api_resolve_itemsize(b2nd_exec_dtype_t dtype, size_t supplied,
                                               size_t *out, b2nd_exec_error_t *error) {
  size_t natural = api_dtype_width(dtype);
  if (natural) {
    if (supplied && supplied != natural) {
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Input itemsize does not match dtype");
    }
    *out = natural;
    return B2ND_EXEC_SUCCESS;
  }
  if ((dtype == B2ND_DTYPE_STRING || dtype == B2ND_DTYPE_BYTES) && supplied > 0) {
    *out = supplied;
    return B2ND_EXEC_SUCCESS;
  }
  return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                       "Invalid or incomplete dtype specification");
}

static b2nd_reduction_kind_t exec_op_to_public_reducer(b2nd_exec_op op, me_reduction_kind kind) {
  switch (op) {
    case B2ND_EXEC_ARGMIN: return B2ND_REDUCTION_ARGMIN;
    case B2ND_EXEC_ARGMAX: return B2ND_REDUCTION_ARGMAX;
    case B2ND_EXEC_VAR: return B2ND_REDUCTION_VAR;
    case B2ND_EXEC_STD: return B2ND_REDUCTION_STD;
    default: break;
  }
  switch (kind) {
    case ME_REDUCTION_SUM: return B2ND_REDUCTION_SUM;
    case ME_REDUCTION_PROD: return B2ND_REDUCTION_PROD;
    case ME_REDUCTION_MIN: return B2ND_REDUCTION_MIN;
    case ME_REDUCTION_MAX: return B2ND_REDUCTION_MAX;
    case ME_REDUCTION_ANY: return B2ND_REDUCTION_ANY;
    case ME_REDUCTION_ALL: return B2ND_REDUCTION_ALL;
    default: return B2ND_REDUCTION_NONE;
  }
}

static bool public_reducer_to_exec_op(b2nd_reduction_kind_t kind, b2nd_exec_op *out) {
  switch (kind) {
    case B2ND_REDUCTION_NONE: *out = B2ND_EXEC_INPUT; return true;
    case B2ND_REDUCTION_SUM: *out = B2ND_EXEC_SUM; return true;
    case B2ND_REDUCTION_PROD: *out = B2ND_EXEC_PROD; return true;
    case B2ND_REDUCTION_MIN: *out = B2ND_EXEC_MIN; return true;
    case B2ND_REDUCTION_MAX: *out = B2ND_EXEC_MAX; return true;
    case B2ND_REDUCTION_ANY: *out = B2ND_EXEC_ANY; return true;
    case B2ND_REDUCTION_ALL: *out = B2ND_EXEC_ALL; return true;
    case B2ND_REDUCTION_ARGMIN: *out = B2ND_EXEC_ARGMIN; return true;
    case B2ND_REDUCTION_ARGMAX: *out = B2ND_EXEC_ARGMAX; return true;
    case B2ND_REDUCTION_VAR: *out = B2ND_EXEC_VAR; return true;
    case B2ND_REDUCTION_STD: *out = B2ND_EXEC_STD; return true;
    default: return false;
  }
}

static bool api_copy_in(const void *src, size_t src_size, size_t min_size,
                        void *dst, size_t dst_size) {
  if (src_size < min_size) {
    return false;
  }
  memcpy(dst, src, src_size < dst_size ? src_size : dst_size);
  memcpy(dst, &dst_size, sizeof(size_t));
  return true;
}

static bool api_copy_out(const void *src, size_t src_size, size_t min_size,
                         void *dst, size_t dst_size) {
  if (!dst || dst_size < min_size) {
    return false;
  }
  size_t copy_len = dst_size < src_size ? dst_size : src_size;
  memcpy(dst, src, copy_len);
  memcpy(dst, &dst_size, sizeof(size_t));
  return true;
}

static b2nd_exec_status_t api_read_exec_options(const b2nd_exec_options_t *in,
                                                b2nd_exec_options_t *out,
                                                b2nd_exec_error_t *error) {
  b2nd_exec_options_t local = B2ND_EXEC_OPTIONS_INITIALIZER;
  if (in) {
    if (!api_copy_in(in, in->struct_size, B2ND_EXEC_OPTIONS_V1_SIZE, &local, sizeof(local))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid execution options struct_size");
    }
    if (local.execution < B2ND_EXEC_POLICY_INTERPRET || local.execution > B2ND_EXEC_POLICY_AUTO ||
        local.compiler < B2ND_COMPILER_DEFAULT || local.compiler > B2ND_COMPILER_CC ||
        local.math_backend < B2ND_MATH_DEFAULT || local.math_backend > B2ND_MATH_SCALAR ||
        local.fp_mode < B2ND_FP_MODE_DEFAULT || local.fp_mode > B2ND_FP_MODE_FAST ||
        local.math_accuracy < B2ND_ACCURACY_DEFAULT || local.math_accuracy > B2ND_ACCURACY_U35 ||
        local.casting < B2ND_CAST_UNSAFE || local.casting > B2ND_CAST_SAFE) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid execution options value");
    }
  }
  *out = local;
  return B2ND_EXEC_SUCCESS;
}

static b2nd_exec_status_t api_read_reduce_opts(const b2nd_exec_reduce_opts_t *in,
                                               b2nd_exec_reduce_opts_t *out,
                                               b2nd_exec_error_t *error) {
  b2nd_exec_reduce_opts_t local = B2ND_EXEC_REDUCE_OPTS_INITIALIZER;
  if (in) {
    if (!api_copy_in(in, in->struct_size, B2ND_EXEC_REDUCE_OPTS_V1_SIZE, &local, sizeof(local))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid reduction options struct_size");
    }
  }
  *out = local;
  return B2ND_EXEC_SUCCESS;
}

static void api_fill_plan_info(const b2nd_exec_plan_info *native, size_t nstages,
                               b2nd_exec_op root_op, b2nd_plan_info_t *out) {
  *out = (b2nd_plan_info_t)B2ND_PLAN_INFO_INITIALIZER;
  out->dtype = me_to_api_dtype(native->dtype);
  out->itemsize = native->itemsize;
  out->ninputs = native->ninputs;
  out->nstages = nstages;
  out->reduction = native->reduction;
  out->identity = native->identity;
  out->partitionable = native->partitionable;
  out->has_jit = native->has_jit;
  out->cardinality = (b2nd_cardinality_t)native->cardinality;
  out->context_ndim = native->context_ndim;
  out->map_dtype = me_to_api_dtype(native->map_dtype);
  out->map_itemsize = native->map_itemsize;
  out->reducer = exec_op_to_public_reducer(root_op, native->reducer);
  out->plan_metadata_bytes = native->plan_metadata_bytes;
  out->has_axes = native->has_axes;
  out->keepdims = native->keepdims;
  out->has_initial = native->has_initial;
  out->has_where = native->has_where;
  out->naxes = native->naxes;
  if (native->naxes && native->naxes <= B2ND_EXEC_MAX_RANK) {
    memcpy(out->axes, native->axes, native->naxes * sizeof(int));
  }
  out->where_slot = native->where_slot;
  out->fp_mode = native->fp_mode;
  out->math_backend = native->math_backend;
  out->sleef_accuracy = native->sleef_accuracy;
}

static void api_fill_sched_info(const b2nd_exec_schedule_info *native, size_t nstages,
                                size_t intermediate_bytes, b2nd_exec_dtype_t out_dtype,
                                size_t out_itemsize, bool is_partition,
                                b2nd_schedule_info_t *out) {
  *out = (b2nd_schedule_info_t)B2ND_SCHEDULE_INFO_INITIALIZER;
  out->nstages = nstages;
  out->nitems = native->nitems;
  out->tile_items = native->tile_items;
  out->mapped_bytes = native->mapped_bytes;
  out->reducer_bytes = native->reducer_bytes;
  out->output_items = native->output_items;
  out->schedule_metadata_bytes = native->schedule_metadata_bytes;
  out->ndim = native->ndim;
  if (native->ndim <= B2ND_EXEC_MAX_RANK) {
    memcpy(out->shape, native->shape, native->ndim * sizeof(int64_t));
  }
  out->broadcast_gather_bytes = native->broadcast_gather_bytes;
  out->output_ndim = native->output_ndim;
  if (native->output_ndim <= B2ND_EXEC_MAX_RANK) {
    memcpy(out->output_shape, native->output_shape, native->output_ndim * sizeof(int64_t));
  }
  out->reduction_groups = native->reduction_groups;
  out->reduction_lanes = native->reduction_lanes;
  out->intermediate_bytes = intermediate_bytes;
  out->output_dtype = out_dtype;
  out->output_itemsize = out_itemsize;
  out->pairwise_sum = native->pairwise_sum;
  out->is_partition_schedule = is_partition;
}

static b2nd_exec_status_t api_wrap_plan(bool is_staged, b2nd_exec_plan *single,
                                        b2nd_exec_staged_plan *staged,
                                        b2nd_plan_t **retained, size_t nretained,
                                        b2nd_plan_t **out, b2nd_exec_error_t *error) {
  b2nd_plan_t *plan = calloc(1, sizeof(*plan));
  if (!plan) {
    if (is_staged) {
      b2nd_exec_staged_plan_free(staged);
    } else {
      b2nd_exec_plan_free(single);
    }
    free(retained);
    return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Cannot allocate public execution plan");
  }
  plan->refcount = 1;
  plan->is_staged = is_staged;
  plan->single = single;
  plan->staged = staged;
  plan->retained = retained;
  plan->nretained = nretained;
  for (size_t i = 0; i < nretained; i++) {
    api_plan_retain(retained[i]);
  }
  *out = plan;
  return B2ND_EXEC_SUCCESS;
}

static b2nd_exec_status_t api_convert_reduce_opts(const b2nd_exec_reduce_opts_t *pub,
                                                  b2nd_exec_op *root_op_out,
                                                  b2nd_exec_reduce_opts *native_out,
                                                  me_dtype *acc_out,
                                                  b2nd_exec_error_t *error) {
  b2nd_exec_op root_op = B2ND_EXEC_INPUT;
  if (!public_reducer_to_exec_op(pub->reduction, &root_op)) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid reduction kind");
  }
  if ((!pub->has_axes && pub->naxes) || pub->naxes > B2ND_EXEC_MAX_RANK) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid reduction axes");
  }
  me_dtype init_dtype = ME_AUTO;
  me_dtype acc = ME_AUTO;
  if (!api_to_me_dtype(pub->initial_dtype, &init_dtype) ||
      !api_to_me_dtype(pub->accumulator, &acc)) {
    return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid reduction dtype");
  }
  if (pub->has_initial) {
    size_t iw = api_dtype_width(pub->initial_dtype);
    if (!iw || iw != pub->initial_size ||
        (pub->initial_category != B2ND_CAPTURE_TYPED &&
         pub->initial_category != B2ND_CAPTURE_WEAK)) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Malformed reduction initial scalar");
    }
  } else if (pub->initial_size || pub->initial_dtype != B2ND_DTYPE_AUTO) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Unexpected reduction initial scalar payload");
  }
  *native_out = (b2nd_exec_reduce_opts){0};
  native_out->struct_size = sizeof(*native_out);
  native_out->has_axes = pub->has_axes;
  native_out->naxes = pub->naxes;
  if (pub->naxes) {
    memcpy(native_out->axes, pub->axes, pub->naxes * sizeof(int));
  }
  native_out->keepdims = pub->keepdims;
  native_out->has_initial = pub->has_initial;
  native_out->initial_dtype = init_dtype;
  native_out->initial_category = pub->initial_category == B2ND_CAPTURE_WEAK
      ? ME_CAPTURE_WEAK : ME_CAPTURE_TYPED;
  memcpy(native_out->initial_scalar, pub->initial_scalar, sizeof(native_out->initial_scalar));
  native_out->initial_size = pub->initial_size;
  *root_op_out = root_op;
  *acc_out = acc;
  return B2ND_EXEC_SUCCESS;
}

static bool api_looks_like_menudet(const char *source) {
  const char *p = source;
  for (;;) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
      p++;
    }
    if (*p == '#') {
      while (*p && *p != '\n') {
        p++;
      }
      continue;
    }
    break;
  }
  return strncmp(p, "def", 3) == 0 && (p[3] == ' ' || p[3] == '\t');
}

bool b2nd_exec_available(void) {
  return true;
}

b2nd_exec_status_t b2nd_plan_from_expr(const char *source, const b2nd_exec_input_spec_t *inputs,
                                       size_t ninputs, const b2nd_exec_options_t *exec_opts,
                                       const b2nd_expr_options_t *expr_opts,
                                       b2nd_plan_t **out, b2nd_exec_error_t *error) {
  if (out) {
    *out = NULL;
  }
  api_clear_error(error);
  if (!out || !source || (ninputs && !inputs) || ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid expression plan arguments");
  }
  b2nd_exec_options_t eopts;
  b2nd_exec_status_t st = api_read_exec_options(exec_opts, &eopts, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }
  b2nd_expr_options_t xopts = B2ND_EXPR_OPTIONS_INITIALIZER;
  if (expr_opts) {
    if (!api_copy_in(expr_opts, expr_opts->struct_size, B2ND_EXPR_OPTIONS_V1_SIZE,
                     &xopts, sizeof(xopts))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid expression options struct_size");
    }
  }
  b2nd_exec_reduce_opts_t ropts = B2ND_EXEC_REDUCE_OPTS_INITIALIZER;
  if (xopts.reduce.struct_size != 0) {
    st = api_read_reduce_opts(&xopts.reduce, &ropts, error);
    if (st != B2ND_EXEC_SUCCESS) {
      return st;
    }
  }
  if (xopts.lang < B2ND_EXEC_LANG_AUTO || xopts.lang > B2ND_EXEC_LANG_MENUDET_0_2 ||
      (xopts.ncaptures && !xopts.captures) || xopts.ncaptures > B2ND_EXEC_MAX_CAPTURES ||
      xopts.cardinality < B2ND_CARDINALITY_INFER || xopts.cardinality > B2ND_CARDINALITY_BLOCK_SCALAR) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid expression language or capture bounds");
  }
  me_input_spec me_inputs[B2ND_EXEC_MAX_INPUTS] = {{0}};
  for (size_t i = 0; i < ninputs; i++) {
    if (!inputs[i].name || !inputs[i].name[0]) {
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Expression input requires a non-empty name");
    }
    me_dtype dt = ME_AUTO;
    if (!api_to_me_dtype(inputs[i].dtype, &dt) || dt == ME_AUTO) {
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Expression inputs require concrete dtypes");
    }
    size_t width = 0;
    st = api_resolve_itemsize(inputs[i].dtype, inputs[i].itemsize, &width, error);
    if (st != B2ND_EXEC_SUCCESS) {
      return st;
    }
    me_inputs[i].name = inputs[i].name;
    me_inputs[i].dtype = dt;
    me_inputs[i].itemsize = width;
  }
  me_capture me_captures[B2ND_EXEC_MAX_CAPTURES] = {{0}};
  for (size_t i = 0; i < xopts.ncaptures; i++) {
    const b2nd_exec_capture_t *cap = &xopts.captures[i];
    if (!cap->spec.name || !cap->spec.name[0] || !cap->value) {
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Expression capture requires name and value");
    }
    me_dtype dt = ME_AUTO;
    if (!api_to_me_dtype(cap->spec.dtype, &dt) || dt == ME_AUTO) {
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Expression capture requires a concrete dtype");
    }
    size_t width = 0;
    st = api_resolve_itemsize(cap->spec.dtype, cap->spec.itemsize, &width, error);
    if (st != B2ND_EXEC_SUCCESS) {
      return st;
    }
    me_captures[i].spec.name = cap->spec.name;
    me_captures[i].spec.dtype = dt;
    me_captures[i].spec.itemsize = width;
    me_captures[i].value = cap->value;
    me_captures[i].category = cap->category == B2ND_CAPTURE_WEAK
        ? ME_CAPTURE_WEAK : ME_CAPTURE_TYPED;
  }
  me_dtype sig_out_dt = ME_AUTO;
  if (!api_to_me_dtype(xopts.output_dtype, &sig_out_dt)) {
    return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid output dtype constraint");
  }
  me_signature sig = ME_SIGNATURE_INITIALIZER;
  sig.inputs = ninputs ? me_inputs : NULL;
  sig.ninputs = ninputs;
  sig.captures = xopts.ncaptures ? me_captures : NULL;
  sig.ncaptures = xopts.ncaptures;
  sig.output_dtype = sig_out_dt;
  sig.output_itemsize = xopts.output_itemsize;
  sig.cardinality = (me_cardinality)xopts.cardinality;
  sig.context_ndim = xopts.context_ndim;
  sig.casting = (me_casting)eopts.casting;
  sig.integer_powers = eopts.integer_powers;

  b2nd_exec_op root_op = B2ND_EXEC_INPUT;
  b2nd_exec_reduce_opts native_ropts = {0};
  me_dtype red_acc = ME_AUTO;
  st = api_convert_reduce_opts(&ropts, &root_op, &native_ropts, &red_acc, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }

  bool is_menudet = false;
  const char *version = NULL;
  if (xopts.lang == B2ND_EXEC_LANG_MENUDET_0_1) {
    is_menudet = true;
    version = "0.1";
  } else if (xopts.lang == B2ND_EXEC_LANG_MENUDET_0_2) {
    is_menudet = true;
    version = "0.2";
  } else if (xopts.lang == B2ND_EXEC_LANG_AUTO && api_looks_like_menudet(source)) {
    is_menudet = true;
    version = "0.2";
  }

  me_prepare_options popts = ME_PREPARE_OPTIONS_INITIALIZER;
  popts.execution = (me_execution_policy)eopts.execution;
  popts.inherit_environment = eopts.inherit_environment;
  popts.menudet_forbidden_features = is_menudet ? xopts.forbidden_features : 0;
  popts.math_backend = (me_math_backend)eopts.math_backend;
  popts.jit_compiler = (me_jit_compiler)eopts.compiler;
  popts.fp_mode = (me_fp_mode)eopts.fp_mode;
  popts.math_accuracy = (me_math_accuracy)eopts.math_accuracy;
  if (!is_menudet && xopts.forbidden_features) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Forbidden Menudet feature flags are invalid for formula expressions");
  }

  me_kernel *kernel = NULL;
  me_error me_err = {0};
  me_status mst = is_menudet
      ? me_prepare_menudet(version, source, &sig, &popts, &kernel, &me_err)
      : me_prepare_formula(source, &sig, &popts, &kernel, &me_err);
  if (mst != ME_SUCCESS) {
    return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
  }

  b2nd_exec_plan *single = NULL;
  mst = b2nd_exec_prepare_kernel_ex(&kernel, true, root_op,
                                    root_op != B2ND_EXEC_INPUT ? &native_ropts : NULL,
                                    red_acc, ropts.has_where_slot, ropts.ddof,
                                    &single, &me_err);
  if (mst != ME_SUCCESS) {
    me_kernel_free(kernel);
    return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
  }
  return api_wrap_plan(false, single, NULL, NULL, 0, out, error);
}

b2nd_exec_status_t b2nd_plan_from_kernel(me_kernel **kernel, bool take_ownership,
                                         const b2nd_exec_reduce_opts_t *reduce_opts,
                                         b2nd_plan_t **out, b2nd_exec_error_t *error) {
  if (out) {
    *out = NULL;
  }
  api_clear_error(error);
  if (!out || !kernel || !*kernel) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid kernel plan arguments");
  }
  b2nd_exec_reduce_opts_t ropts = B2ND_EXEC_REDUCE_OPTS_INITIALIZER;
  b2nd_exec_status_t st = api_read_reduce_opts(reduce_opts, &ropts, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }
  b2nd_exec_op root_op = B2ND_EXEC_INPUT;
  b2nd_exec_reduce_opts native_ropts = {0};
  me_dtype red_acc = ME_AUTO;
  st = api_convert_reduce_opts(&ropts, &root_op, &native_ropts, &red_acc, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }
  b2nd_exec_plan *single = NULL;
  me_error me_err = {0};
  me_status mst = b2nd_exec_prepare_kernel_ex(
      kernel, take_ownership, root_op,
      (root_op != B2ND_EXEC_INPUT || reduce_opts) ? &native_ropts : NULL,
      red_acc, ropts.has_where_slot, ropts.ddof, &single, &me_err);
  if (mst != ME_SUCCESS) {
    return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
  }
  return api_wrap_plan(false, single, NULL, NULL, 0, out, error);
}

b2nd_exec_status_t b2nd_plan_from_graph(const b2nd_exec_graph_t *graph,
                                        const b2nd_exec_options_t *exec_opts,
                                        b2nd_plan_t **out, b2nd_exec_error_t *error) {
  if (out) {
    *out = NULL;
  }
  api_clear_error(error);
  if (!out || !graph) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid graph descriptor");
  }
  b2nd_exec_graph_t g = B2ND_EXEC_GRAPH_INITIALIZER;
  if (!api_copy_in(graph, graph->struct_size, B2ND_EXEC_GRAPH_V1_SIZE, &g, sizeof(g))) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid graph struct_size");
  }
  if (!g.nodes || !g.nnodes || g.nnodes > B2ND_EXEC_MAX_NODES || g.ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid graph bounds");
  }
  b2nd_exec_options_t eopts;
  b2nd_exec_status_t st = api_read_exec_options(exec_opts, &eopts, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }
  b2nd_exec_node *native = calloc(g.nnodes, sizeof(*native));
  if (!native) {
    return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Cannot allocate graph descriptor");
  }
  for (size_t i = 0; i < g.nnodes; i++) {
    b2nd_exec_node_t n = B2ND_EXEC_NODE_INITIALIZER;
    if (!api_copy_in(&g.nodes[i], g.nodes[i].struct_size, B2ND_EXEC_NODE_V1_SIZE, &n, sizeof(n))) {
      free(native);
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, (uint32_t)i, SIZE_MAX,
                           "Invalid graph node struct_size");
    }
    if ((int)n.op < (int)B2ND_OP_INPUT || (int)n.op > (int)B2ND_OP_POS) {
      free(native);
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, (uint32_t)i, SIZE_MAX,
                           "Invalid graph node op");
    }
    me_dtype dt = ME_AUTO, init_dt = ME_AUTO;
    if (!api_to_me_dtype(n.dtype, &dt) || !api_to_me_dtype(n.initial_dtype, &init_dt)) {
      free(native);
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, (uint32_t)i, SIZE_MAX,
                           "Invalid graph node dtype");
    }
    native[i].struct_size = sizeof(native[i]);
    native[i].id = n.id;
    native[i].op = (b2nd_exec_op)n.op;
    memcpy(native[i].args, n.args, sizeof(native[i].args));
    native[i].nargs = n.nargs;
    native[i].slot = n.slot;
    native[i].dtype = dt;
    native[i].itemsize = n.itemsize;
    memcpy(native[i].scalar, n.scalar, sizeof(native[i].scalar));
    native[i].scalar_size = n.scalar_size;
    native[i].category = n.category == B2ND_CAPTURE_WEAK ? ME_CAPTURE_WEAK : ME_CAPTURE_TYPED;
    native[i].has_axes = n.has_axes;
    native[i].naxes = n.naxes;
    memcpy(native[i].axes, n.axes, sizeof(native[i].axes));
    native[i].keepdims = n.keepdims;
    native[i].has_initial = n.has_initial;
    native[i].initial_dtype = init_dt;
    native[i].initial_category = n.initial_category == B2ND_CAPTURE_WEAK
        ? ME_CAPTURE_WEAK : ME_CAPTURE_TYPED;
    memcpy(native[i].initial_scalar, n.initial_scalar, sizeof(native[i].initial_scalar));
    native[i].initial_size = n.initial_size;
    native[i].ddof = n.ddof;
    memcpy(native[i].function, n.function, sizeof(native[i].function));
    native[i].has_where = n.has_where;
    native[i].where = n.where;
  }

  b2nd_exec_descriptor desc = {sizeof(desc), native, g.nnodes, g.ninputs, g.root};
  b2nd_exec_options opts = {
      sizeof(opts),
      (me_execution_policy)eopts.execution,
      eopts.inherit_environment,
      eopts.integer_powers,
      (me_casting)eopts.casting,
      (me_math_backend)eopts.math_backend,
      (me_jit_compiler)eopts.compiler,
      (me_fp_mode)eopts.fp_mode,
      (me_math_accuracy)eopts.math_accuracy};

  me_error me_err = {0};
  bool try_single = g.disable_auto_stages;
  if (!try_single && b2nd_exec_validate_descriptor(&desc, false, &me_err) == ME_SUCCESS) {
    unsigned uses[B2ND_EXEC_MAX_NODES] = {0};
    bool has_shared_computed = false;
    for (size_t i = 0; i < desc.nnodes && !has_shared_computed; i++) {
      const b2nd_exec_node *n = &desc.nodes[i];
      for (size_t a = 0; a < n->nargs; a++) {
        uint32_t arg = n->args[a];
        if (arg < desc.nnodes && ++uses[arg] > 1 && desc.nodes[arg].nargs > 0) {
          has_shared_computed = true;
          break;
        }
      }
      if (!has_shared_computed && n->has_where && n->where < desc.nnodes) {
        if (++uses[n->where] > 1 && desc.nodes[n->where].nargs > 0) {
          has_shared_computed = true;
        }
      }
    }
    try_single = !has_shared_computed;
  }
  if (try_single) {
    b2nd_exec_plan *single = NULL;
    me_status mst = b2nd_exec_prepare(&desc, &opts, &single, &me_err);
    free(native);
    if (mst != ME_SUCCESS) {
      return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
    }
    return api_wrap_plan(false, single, NULL, NULL, 0, out, error);
  }

  b2nd_exec_staged_plan *staged = NULL;
  b2nd_exec_stage_diagnostic diag = {SIZE_MAX, UINT32_MAX};
  me_status mst = b2nd_exec_staged_prepare_ex(&desc, &opts, &staged, &diag, &me_err);
  free(native);
  if (mst != ME_SUCCESS) {
    return api_from_me_error(error, mst, &me_err, diag.node, diag.stage);
  }
  return api_wrap_plan(true, NULL, staged, NULL, 0, out, error);
}

b2nd_exec_status_t b2nd_plan_compose(const b2nd_stage_binding_t *stages, size_t nstages,
                                     size_t ninputs, b2nd_plan_t **out,
                                     b2nd_exec_error_t *error) {
  if (out) {
    *out = NULL;
  }
  api_clear_error(error);
  if (!out || !stages || !nstages || nstages > B2ND_EXEC_MAX_STAGES ||
      ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid stage composition bounds");
  }
  b2nd_exec_stage_spec flat[B2ND_EXEC_MAX_STAGES] = {{0}};
  int *storage = calloc(B2ND_EXEC_MAX_STAGES * B2ND_EXEC_MAX_INPUTS, sizeof(int));
  b2nd_plan_t **retained = calloc(nstages, sizeof(*retained));
  if (!storage || !retained) {
    free(storage);
    free(retained);
    return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Cannot allocate stage composition tables");
  }
  size_t flat_count = 0;
  size_t stage_final[B2ND_EXEC_MAX_STAGES] = {0};
  for (size_t s = 0; s < nstages; s++) {
    b2nd_stage_binding_t b = B2ND_STAGE_BINDING_INITIALIZER;
    if (!api_copy_in(&stages[s], stages[s].struct_size, B2ND_STAGE_BINDING_V1_SIZE,
                     &b, sizeof(b)) || !b.plan) {
      free(storage);
      free(retained);
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, s,
                           "Invalid stage binding entry");
    }
    retained[s] = (b2nd_plan_t *)b.plan;
    b2nd_plan_info_t cinfo = B2ND_PLAN_INFO_INITIALIZER;
    if (b2nd_plan_describe(b.plan, &cinfo) != B2ND_EXEC_SUCCESS ||
        (cinfo.ninputs && !b.sources)) {
      free(storage);
      free(retained);
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                           "Invalid stage plan or missing sources");
    }
    for (size_t i = 0; i < cinfo.ninputs; i++) {
      int src = b.sources[i];
      if (src >= 0 && (size_t)src >= s) {
        free(storage);
        free(retained);
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                             "Forward or cyclic stage binding");
      }
      if (src < 0) {
        size_t slot = (size_t)(-(int64_t)src - 1);
        if (slot >= ninputs) {
          free(storage);
          free(retained);
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                               "Invalid external stage binding");
        }
      }
    }
    if (!b.plan->is_staged) {
      if (flat_count == B2ND_EXEC_MAX_STAGES) {
        free(storage);
        free(retained);
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                             "Flattened stage count exceeds limit");
      }
      int *dst = storage + flat_count * B2ND_EXEC_MAX_INPUTS;
      for (size_t i = 0; i < cinfo.ninputs; i++) {
        int src = b.sources[i];
        dst[i] = src >= 0 ? (int)stage_final[src] : src;
      }
      flat[flat_count].plan = b.plan->single;
      flat[flat_count].sources = cinfo.ninputs ? dst : NULL;
      flat[flat_count].node = b.node;
      stage_final[s] = flat_count++;
    } else {
      size_t base_flat = flat_count;
      size_t child_cnt = b2nd_exec_staged_count(b.plan->staged);
      if (!child_cnt || flat_count + child_cnt > B2ND_EXEC_MAX_STAGES) {
        free(storage);
        free(retained);
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                             "Flattened stage count exceeds limit");
      }
      for (size_t k = 0; k < child_cnt; k++) {
        const b2nd_exec_plan *sub_plan = NULL;
        const int *sub_src = NULL;
        size_t sub_ninputs = 0;
        uint32_t sub_node = 0;
        if (b2nd_exec_staged_child(b.plan->staged, k, &sub_plan, &sub_src,
                                   &sub_ninputs, &sub_node) != ME_SUCCESS) {
          free(storage);
          free(retained);
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                               "Cannot inspect nested stage child");
        }
        int *dst = storage + flat_count * B2ND_EXEC_MAX_INPUTS;
        for (size_t i = 0; i < sub_ninputs; i++) {
          int ss = sub_src[i];
          if (ss >= 0) {
            dst[i] = (int)(base_flat + (size_t)ss);
          } else {
            size_t ext = (size_t)(-(int64_t)ss - 1);
            if (ext >= cinfo.ninputs) {
              free(storage);
              free(retained);
              return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, b.node, s,
                                   "Invalid nested stage external binding");
            }
            int mapped = b.sources[ext];
            dst[i] = mapped >= 0 ? (int)stage_final[mapped] : mapped;
          }
        }
        flat[flat_count].plan = sub_plan;
        flat[flat_count].sources = sub_ninputs ? dst : NULL;
        flat[flat_count].node = sub_node;
        flat_count++;
      }
      stage_final[s] = flat_count - 1;
    }
  }
  b2nd_exec_staged_plan *staged = NULL;
  b2nd_exec_stage_diagnostic diag = {SIZE_MAX, UINT32_MAX};
  me_error me_err = {0};
  me_status mst = b2nd_exec_staged_assemble_ex(flat, flat_count, ninputs, &staged,
                                                &diag, &me_err);
  free(storage);
  if (mst != ME_SUCCESS) {
    free(retained);
    return api_from_me_error(error, mst, &me_err, diag.node, diag.stage);
  }
  return api_wrap_plan(true, NULL, staged, retained, nstages, out, error);
}

b2nd_exec_status_t b2nd_plan_describe(const b2nd_plan_t *plan, b2nd_plan_info_t *out) {
  if (!plan || !out || out->struct_size < B2ND_PLAN_INFO_V1_SIZE) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  b2nd_exec_plan_info native = {0};
  me_status mst;
  size_t nstages = 1;
  b2nd_exec_op root_op = B2ND_EXEC_INPUT;
  if (plan->is_staged) {
    nstages = b2nd_exec_staged_count(plan->staged);
    mst = b2nd_exec_staged_plan_describe(plan->staged, &native);
    if (mst == ME_SUCCESS && nstages > 0) {
      const b2nd_exec_plan *final_child = NULL;
      const int *src = NULL;
      size_t cnt = 0;
      uint32_t node = 0;
      if (b2nd_exec_staged_child(plan->staged, nstages - 1, &final_child,
                                 &src, &cnt, &node) == ME_SUCCESS) {
        root_op = b2nd_exec_plan_root_op(final_child);
      }
    }
  } else {
    mst = b2nd_exec_describe(plan->single, &native);
    root_op = b2nd_exec_plan_root_op(plan->single);
  }
  if (mst != ME_SUCCESS) {
    return me_to_api_status(mst);
  }
  b2nd_plan_info_t local;
  api_fill_plan_info(&native, nstages, root_op, &local);
  if (plan->is_staged) {
    local.plan_metadata_bytes += sizeof(*plan) + plan->nretained * sizeof(*plan->retained);
  } else {
    local.plan_metadata_bytes += sizeof(*plan);
  }
  api_copy_out(&local, sizeof(local), B2ND_PLAN_INFO_V1_SIZE, out, out->struct_size);
  return B2ND_EXEC_SUCCESS;
}

b2nd_exec_status_t b2nd_plan_describe_stage(const b2nd_plan_t *plan, size_t stage,
                                            b2nd_plan_info_t *out, int *sources,
                                            size_t sources_cap, size_t *ninputs_out,
                                            uint32_t *node_out) {
  if (!plan) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  if (out && out->struct_size < B2ND_PLAN_INFO_V1_SIZE) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  const b2nd_exec_plan *child = NULL;
  const int *child_sources = NULL;
  size_t child_ninputs = 0;
  uint32_t child_node = UINT32_MAX;
  int default_sources[B2ND_EXEC_MAX_INPUTS];
  if (!plan->is_staged) {
    if (stage != 0) {
      return B2ND_EXEC_ERR_INVALID_ARG;
    }
    b2nd_exec_plan_info info = {0};
    if (b2nd_exec_describe(plan->single, &info) != ME_SUCCESS) {
      return B2ND_EXEC_ERR_INVALID_ARG;
    }
    child = plan->single;
    child_ninputs = info.ninputs;
    for (size_t i = 0; i < child_ninputs; i++) {
      default_sources[i] = -1 - (int)i;
    }
    child_sources = default_sources;
  } else {
    if (b2nd_exec_staged_child(plan->staged, stage, &child, &child_sources,
                               &child_ninputs, &child_node) != ME_SUCCESS) {
      return B2ND_EXEC_ERR_INVALID_ARG;
    }
  }
  if ((sources_cap && !sources) || (sources && sources_cap < child_ninputs)) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  if (sources && child_ninputs) {
    memcpy(sources, child_sources, child_ninputs * sizeof(int));
  }
  if (ninputs_out) {
    *ninputs_out = child_ninputs;
  }
  if (node_out) {
    *node_out = child_node;
  }
  if (out) {
    b2nd_exec_plan_info info = {0};
    me_status mst = b2nd_exec_describe(child, &info);
    if (mst != ME_SUCCESS) {
      return me_to_api_status(mst);
    }
    b2nd_plan_info_t local;
    api_fill_plan_info(&info, 1, b2nd_exec_plan_root_op(child), &local);
    api_copy_out(&local, sizeof(local), B2ND_PLAN_INFO_V1_SIZE, out, out->struct_size);
  }
  return B2ND_EXEC_SUCCESS;
}

void b2nd_plan_free(b2nd_plan_t *plan) {
  if (!plan) {
    return;
  }
#if defined(_MSC_VER) && !defined(__clang__)
  if (_InterlockedDecrement((long volatile *)&plan->refcount) != 0) {
    return;
  }
#else
  if (__atomic_sub_fetch(&plan->refcount, 1, __ATOMIC_ACQ_REL) != 0) {
    return;
  }
#endif
  if (plan->is_staged) {
    b2nd_exec_staged_plan_free(plan->staged);
  } else {
    b2nd_exec_plan_free(plan->single);
  }
  for (size_t i = 0; i < plan->nretained; i++) {
    b2nd_plan_free(plan->retained[i]);
  }
  free(plan->retained);
  free(plan);
}

b2nd_exec_status_t b2nd_schedule_create(const b2nd_plan_t *plan,
                                        const b2nd_exec_input_meta_t *inputs, size_t ninputs,
                                        const b2nd_schedule_options_t *options,
                                        b2nd_schedule_t **out, b2nd_exec_error_t *error) {
  if (out) {
    *out = NULL;
  }
  api_clear_error(error);
  if (!out || !plan || (ninputs && !inputs) || ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid schedule specialization arguments");
  }
  b2nd_schedule_options_t sopts = B2ND_SCHEDULE_OPTIONS_INITIALIZER;
  if (options) {
    if (!api_copy_in(options, options->struct_size, B2ND_SCHEDULE_OPTIONS_V1_SIZE,
                     &sopts, sizeof(sopts))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid schedule options struct_size");
    }
  }
  b2nd_exec_input_meta meta[B2ND_EXEC_MAX_INPUTS] = {{0}};
  for (size_t i = 0; i < ninputs; i++) {
    b2nd_exec_input_meta_t m = B2ND_EXEC_INPUT_META_INITIALIZER;
    if (!api_copy_in(&inputs[i], inputs[i].struct_size, B2ND_EXEC_INPUT_META_V1_SIZE,
                     &m, sizeof(m))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid input metadata struct_size");
    }
    me_dtype dt = ME_AUTO;
    if (!api_to_me_dtype(m.dtype, &dt)) {
      return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid input metadata dtype");
    }
    meta[i].dtype = dt;
    meta[i].itemsize = m.itemsize ? m.itemsize : api_dtype_width(m.dtype);
    meta[i].ndim = m.ndim;
    if (m.ndim <= B2ND_EXEC_MAX_RANK) {
      memcpy(meta[i].shape, m.shape, m.ndim * sizeof(int64_t));
    }
  }
  b2nd_schedule_t *sched = calloc(1, sizeof(*sched));
  if (!sched) {
    return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Cannot allocate execution schedule");
  }
  me_error me_err = {0};
  if (sopts.partition_spec) {
    if (plan->is_staged) {
      free(sched);
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Partition schedules are not supported on multi-stage plans");
    }
    if (sopts.pairwise_sum) {
      free(sched);
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Pairwise sum is not supported on partition schedules");
    }
    b2nd_partition_spec_t pspec = B2ND_PARTITION_SPEC_INITIALIZER;
    if (!api_copy_in(sopts.partition_spec, sopts.partition_spec->struct_size,
                     B2ND_PARTITION_SPEC_V1_SIZE, &pspec, sizeof(pspec))) {
      free(sched);
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid partition specification struct_size");
    }
    b2nd_exec_partition_spec native_spec = {0};
    native_spec.struct_size = sizeof(native_spec);
    native_spec.ndim = pspec.ndim;
    if (pspec.ndim <= B2ND_EXEC_MAX_RANK) {
      memcpy(native_spec.domain, pspec.domain, sizeof(native_spec.domain));
      memcpy(native_spec.partitions, pspec.partitions, sizeof(native_spec.partitions));
      memcpy(native_spec.sel_start, pspec.sel_start, sizeof(native_spec.sel_start));
      memcpy(native_spec.sel_stop, pspec.sel_stop, sizeof(native_spec.sel_stop));
    }
    native_spec.has_selection = pspec.has_selection;
    me_status mst = b2nd_exec_specialize_partitions(
        plan->single, ninputs ? meta : NULL, ninputs, &native_spec,
        sopts.max_mapped_bytes, &sched->single, &me_err);
    if (mst != ME_SUCCESS) {
      free(sched);
      return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
    }
    sched->is_partition = true;
  } else if (plan->is_staged) {
    if (sopts.pairwise_sum) {
      free(sched);
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Pairwise sum is not supported on multi-stage schedules");
    }
    b2nd_exec_stage_diagnostic diag = {SIZE_MAX, UINT32_MAX};
    me_status mst = b2nd_exec_staged_specialize_ex(
        plan->staged, ninputs ? meta : NULL, ninputs, sopts.tile_items,
        sopts.max_mapped_bytes, sopts.max_intermediate_bytes,
        &sched->staged, &diag, &me_err);
    if (mst != ME_SUCCESS) {
      free(sched);
      return api_from_me_error(error, mst, &me_err, diag.node, diag.stage);
    }
    sched->is_staged = true;
  } else {
    me_status mst = b2nd_exec_specialize_ex(
        plan->single, ninputs ? meta : NULL, ninputs, sopts.tile_items,
        sopts.max_mapped_bytes, sopts.pairwise_sum, &sched->single, &me_err);
    if (mst != ME_SUCCESS) {
      free(sched);
      return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
    }
  }
  api_plan_retain(plan);
  sched->owner = plan;
  sched->ninputs = ninputs;
  if (ninputs) {
    memcpy(sched->inputs, meta, ninputs * sizeof(meta[0]));
  }
  *out = sched;
  return B2ND_EXEC_SUCCESS;
}

b2nd_exec_status_t b2nd_schedule_describe(const b2nd_schedule_t *schedule,
                                          b2nd_schedule_info_t *out,
                                          b2nd_stage_info_t *stages, size_t stages_cap) {
  if (!schedule || !out || out->struct_size < B2ND_SCHEDULE_INFO_V1_SIZE) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  b2nd_plan_info_t pinfo = B2ND_PLAN_INFO_INITIALIZER;
  if (b2nd_plan_describe(schedule->owner, &pinfo) != B2ND_EXEC_SUCCESS) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  size_t nstages = pinfo.nstages;
  if (stages_cap && !stages) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  for (size_t s = 0; s < stages_cap && s < nstages; s++) {
    if (stages[s].struct_size < B2ND_STAGE_INFO_V1_SIZE) {
      return B2ND_EXEC_ERR_INVALID_ARG;
    }
  }
  b2nd_schedule_info_t local;
  if (!schedule->is_staged) {
    b2nd_exec_schedule_info sinfo = {0};
    me_status mst = b2nd_exec_describe_schedule(schedule->single, &sinfo);
    if (mst != ME_SUCCESS) {
      return me_to_api_status(mst);
    }
    api_fill_sched_info(&sinfo, 1, 0, pinfo.dtype, pinfo.itemsize,
                        schedule->is_partition, &local);
    api_copy_out(&local, sizeof(local), B2ND_SCHEDULE_INFO_V1_SIZE, out, out->struct_size);
    if (stages && stages_cap >= 1) {
      b2nd_stage_info_t s_local = B2ND_STAGE_INFO_INITIALIZER;
      s_local.node = UINT32_MAX;
      s_local.last_consumer = SIZE_MAX;
      s_local.output_bytes = sinfo.output_items * pinfo.itemsize;
      s_local.plan = pinfo;
      s_local.schedule = local;
      api_copy_out(&s_local, sizeof(s_local), B2ND_STAGE_INFO_V1_SIZE,
                   &stages[0], stages[0].struct_size);
    }
    return B2ND_EXEC_SUCCESS;
  }

  size_t intermediate_bytes = 0;
  b2nd_exec_stage_info last_sinfo = {0};
  for (size_t s = 0; s < nstages; s++) {
    b2nd_exec_stage_info sinfo = {0};
    if (b2nd_exec_staged_describe(schedule->staged, s, &sinfo) != ME_SUCCESS) {
      return B2ND_EXEC_ERR_INVALID_ARG;
    }
    if (s + 1 < nstages) {
      intermediate_bytes += sinfo.output_bytes;
    }
    last_sinfo = sinfo;
    if (stages && s < stages_cap) {
      b2nd_stage_info_t s_local = B2ND_STAGE_INFO_INITIALIZER;
      const b2nd_exec_plan *child = NULL;
      const int *src = NULL;
      size_t cnt = 0;
      uint32_t node = UINT32_MAX;
      b2nd_exec_op child_op = B2ND_EXEC_INPUT;
      if (b2nd_exec_staged_child(schedule->owner->staged, s, &child, &src, &cnt, &node) == ME_SUCCESS) {
        child_op = b2nd_exec_plan_root_op(child);
      }
      s_local.node = sinfo.node;
      s_local.last_consumer = sinfo.last_consumer;
      s_local.output_bytes = sinfo.output_bytes;
      api_fill_plan_info(&sinfo.plan, 1, child_op, &s_local.plan);
      api_fill_sched_info(&sinfo.schedule, 1, 0, s_local.plan.dtype,
                          s_local.plan.itemsize, false, &s_local.schedule);
      api_copy_out(&s_local, sizeof(s_local), B2ND_STAGE_INFO_V1_SIZE,
                   &stages[s], stages[s].struct_size);
    }
  }
  api_fill_sched_info(&last_sinfo.schedule, nstages, intermediate_bytes,
                      pinfo.dtype, pinfo.itemsize, false, &local);
  local.schedule_metadata_bytes += sizeof(*schedule);
  api_copy_out(&local, sizeof(local), B2ND_SCHEDULE_INFO_V1_SIZE, out, out->struct_size);
  return B2ND_EXEC_SUCCESS;
}

void b2nd_schedule_free(b2nd_schedule_t *schedule) {
  if (!schedule) {
    return;
  }
  if (schedule->is_staged) {
    b2nd_exec_staged_schedule_free(schedule->staged);
  } else {
    b2nd_exec_schedule_free(schedule->single);
  }
  b2nd_plan_free((b2nd_plan_t *)schedule->owner);
  free(schedule);
}

static bool api_parse_numpy_dtype(const char *dtype, int8_t dtype_format, int32_t typesize,
                                  b2nd_exec_dtype_t *out_dt, size_t *out_width,
                                  bool *out_native_endian) {
  *out_dt = B2ND_DTYPE_AUTO;
  *out_width = 0;
  *out_native_endian = false;
  if (dtype_format != DTYPE_NUMPY_FORMAT || !dtype || !dtype[0] || !dtype[1] || typesize <= 0) {
    return false;
  }
  uint16_t endian = 1;
  char host = *(const unsigned char *)&endian ? '<' : '>';
  if (dtype[1] == 'S' || dtype[1] == 'U') {
    if (!dtype[2]) {
      return false;
    }
    char *endp = NULL;
    unsigned long long cnt = strtoull(dtype + 2, &endp, 10);
    if (!endp || *endp != '\0' || cnt == 0) {
      return false;
    }
    size_t unit = dtype[1] == 'U' ? 4 : 1;
    if (cnt > SIZE_MAX / unit || (size_t)cnt * unit != (size_t)typesize) {
      return false;
    }
    *out_native_endian = dtype[1] == 'S'
        ? (dtype[0] == '=' || dtype[0] == '|' || dtype[0] == host)
        : (dtype[0] == '=' || dtype[0] == host);
    *out_dt = B2ND_DTYPE_STRING;
    *out_width = (size_t)typesize;
    return true;
  }
  if (!dtype[2]) {
    return false;
  }
  b2nd_exec_dtype_t dt = B2ND_DTYPE_AUTO;
  size_t width = 0;
  if (strcmp(dtype + 1, "c16") == 0) {
    dt = B2ND_DTYPE_COMPLEX128;
    width = 16;
  } else if (dtype[3] == '\0') {
    const char *s = dtype + 1;
    if (strcmp(s, "b1") == 0) { dt = B2ND_DTYPE_BOOL; width = 1; }
    else if (strcmp(s, "i1") == 0) { dt = B2ND_DTYPE_INT8; width = 1; }
    else if (strcmp(s, "u1") == 0) { dt = B2ND_DTYPE_UINT8; width = 1; }
    else if (strcmp(s, "i2") == 0) { dt = B2ND_DTYPE_INT16; width = 2; }
    else if (strcmp(s, "u2") == 0) { dt = B2ND_DTYPE_UINT16; width = 2; }
    else if (strcmp(s, "i4") == 0) { dt = B2ND_DTYPE_INT32; width = 4; }
    else if (strcmp(s, "u4") == 0) { dt = B2ND_DTYPE_UINT32; width = 4; }
    else if (strcmp(s, "i8") == 0) { dt = B2ND_DTYPE_INT64; width = 8; }
    else if (strcmp(s, "u8") == 0) { dt = B2ND_DTYPE_UINT64; width = 8; }
    else if (strcmp(s, "f4") == 0) { dt = B2ND_DTYPE_FLOAT32; width = 4; }
    else if (strcmp(s, "f8") == 0) { dt = B2ND_DTYPE_FLOAT64; width = 8; }
    else if (strcmp(s, "c8") == 0) { dt = B2ND_DTYPE_COMPLEX64; width = 8; }
  }
  if (dt == B2ND_DTYPE_AUTO || width != (size_t)typesize) {
    return false;
  }
  if (width == 1) {
    *out_native_endian = (dtype[0] == '=' || dtype[0] == '|' || dtype[0] == host);
  } else {
    *out_native_endian = (dtype[0] == '=' || dtype[0] == host);
  }
  *out_dt = dt;
  *out_width = width;
  return true;
}

static b2nd_exec_status_t api_array_meta(const b2nd_array_t *arr, b2nd_exec_input_meta_t *out,
                                         b2nd_exec_error_t *error) {
  if (!arr || !arr->sc || arr->ndim < 0 || arr->ndim > B2ND_EXEC_MAX_RANK) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid b2nd_array_t operand");
  }
  b2nd_exec_dtype_t dt = B2ND_DTYPE_AUTO;
  size_t width = 0;
  bool native = false;
  if (!api_parse_numpy_dtype(arr->dtype, arr->dtype_format, arr->sc->typesize,
                             &dt, &width, &native)) {
    return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Unsupported b2nd_array_t dtype metadata");
  }
  if (!native) {
    return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Non-native endian b2nd_array_t is not supported");
  }
  *out = (b2nd_exec_input_meta_t)B2ND_EXEC_INPUT_META_INITIALIZER;
  out->dtype = dt;
  out->itemsize = width;
  out->ndim = (size_t)arr->ndim;
  for (int d = 0; d < arr->ndim; d++) {
    out->shape[d] = arr->shape[d];
  }
  return B2ND_EXEC_SUCCESS;
}

typedef struct {
  void *user_context;
  b2nd_source_read_fn read;
  b2nd_sink_write_fn write;
} api_cb_ctx;

static me_status api_source_trampoline(void *context, const size_t *indices, size_t count,
                                       me_mut_buffer dst, me_error *error) {
  const api_cb_ctx *cb = context;
  b2nd_exec_error_t pub_err = B2ND_EXEC_ERROR_INITIALIZER;
  b2nd_exec_status_t st = cb->read(cb->user_context, indices, count,
                                    dst.data, dst.capacity, &pub_err);
  if (st != B2ND_EXEC_SUCCESS) {
    if (error) {
      *error = (me_error){0};
      error->status = api_to_me_status(st);
      error->line = pub_err.line;
      error->column = pub_err.column;
      snprintf(error->message, sizeof(error->message), "%s",
               pub_err.message[0] ? pub_err.message : "Source callback error");
    }
    return api_to_me_status(st);
  }
  return ME_SUCCESS;
}

static me_status api_sink_trampoline(void *context, size_t offset_items, size_t count,
                                     me_buffer src, me_error *error) {
  const api_cb_ctx *cb = context;
  b2nd_exec_error_t pub_err = B2ND_EXEC_ERROR_INITIALIZER;
  b2nd_exec_status_t st = cb->write(cb->user_context, offset_items, count,
                                     src.data, src.capacity, &pub_err);
  if (st != B2ND_EXEC_SUCCESS) {
    if (error) {
      *error = (me_error){0};
      error->status = api_to_me_status(st);
      error->line = pub_err.line;
      error->column = pub_err.column;
      snprintf(error->message, sizeof(error->message), "%s",
               pub_err.message[0] ? pub_err.message : "Sink callback error");
    }
    return api_to_me_status(st);
  }
  return ME_SUCCESS;
}

static bool api_admit_array_metadata(const b2nd_array_t *arr, const b2nd_exec_input_meta *expected,
                                     b2nd_exec_status_t *reject_kind) {
  b2nd_exec_input_meta_t got;
  b2nd_exec_error_t dummy = B2ND_EXEC_ERROR_INITIALIZER;
  if (!arr || !arr->sc || arr->ndim < 0 || arr->ndim > B2ND_EXEC_MAX_RANK) {
    *reject_kind = B2ND_EXEC_ERR_INVALID_ARG;
    return false;
  }
  if (api_array_meta(arr, &got, &dummy) != B2ND_EXEC_SUCCESS ||
      got.dtype != me_to_api_dtype(expected->dtype) || got.itemsize != expected->itemsize ||
      got.ndim != expected->ndim || arr->sc->typesize != (int32_t)expected->itemsize ||
      (expected->itemsize > 8 && expected->dtype != ME_COMPLEX128 && expected->dtype != ME_STRING) ||
      (!arr->sc->frame && arr->sc->nchunks && !arr->sc->data) ||
      (arr->sc->frame && ((const blosc2_frame_s *)arr->sc->frame)->schunk != arr->sc) ||
      arr->sc->compcode > BLOSC_ZSTD) {
    *reject_kind = B2ND_EXEC_ERR_UNSUPPORTED;
    return false;
  }
  for (size_t d = 0; d < expected->ndim; d++) {
    if (arr->shape[d] != expected->shape[d]) {
      *reject_kind = B2ND_EXEC_ERR_INVALID_ARG;
      return false;
    }
  }
  blosc2_dparams dparams;
  if (!arr->sc->dctx || blosc2_ctx_get_dparams(arr->sc->dctx, &dparams) < 0 || dparams.postfilter) {
    *reject_kind = B2ND_EXEC_ERR_UNSUPPORTED;
    return false;
  }
  for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
    if (arr->sc->filters[f] > BLOSC_DELTA) {
      *reject_kind = B2ND_EXEC_ERR_UNSUPPORTED;
      return false;
    }
  }
  return true;
}

static bool api_admit_sink_array_metadata(const b2nd_array_t *arr, const b2nd_plan_info_t *pinfo,
                                          const b2nd_schedule_info_t *sinfo) {
  b2nd_exec_input_meta_t got;
  b2nd_exec_error_t dummy = B2ND_EXEC_ERROR_INITIALIZER;
  if (!arr || !arr->sc || arr->ndim < 1 || (size_t)arr->ndim != sinfo->output_ndim ||
      !pinfo->itemsize || pinfo->itemsize > 8 ||
      api_array_meta(arr, &got, &dummy) != B2ND_EXEC_SUCCESS ||
      got.dtype != pinfo->dtype || got.itemsize != pinfo->itemsize ||
      (arr->sc->frame && ((const blosc2_frame_s *)arr->sc->frame)->schunk != arr->sc) ||
      !arr->sc->cctx) {
    return false;
  }
  for (int d = 0; d < arr->ndim; d++) {
    if (arr->shape[d] != sinfo->output_shape[d]) {
      return false;
    }
  }
  blosc2_cparams cparams;
  if (blosc2_ctx_get_cparams(arr->sc->cctx, &cparams) < 0 || cparams.prefilter != NULL ||
      cparams.use_dict != 0 || cparams.compcode > BLOSC_ZSTD) {
    return false;
  }
  for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
    if (cparams.filters[f] > BLOSC_DELTA) {
      return false;
    }
  }
  return true;
}

static bool api_view_matches_meta(const b2nd_view_t *v, const b2nd_exec_input_meta *expected) {
  if (v->dtype != B2ND_DTYPE_AUTO) {
    me_dtype dt = ME_AUTO;
    if (!api_to_me_dtype(v->dtype, &dt) || dt != expected->dtype) {
      return false;
    }
    if (v->itemsize && v->itemsize != expected->itemsize) {
      return false;
    }
    if (v->ndim != expected->ndim) {
      return false;
    }
    for (size_t d = 0; d < expected->ndim; d++) {
      if (v->shape[d] != expected->shape[d]) {
        return false;
      }
    }
  } else if (v->itemsize && v->itemsize != expected->itemsize) {
    return false;
  }
  return true;
}

static void api_fill_native_view(const b2nd_view_t *src, b2nd_exec_view *dst) {
  *dst = (b2nd_exec_view){0};
  dst->struct_size = sizeof(*dst);
  dst->base = src->base;
  dst->capacity = src->capacity;
  dst->offset = src->offset;
  memcpy(dst->strides, src->strides, sizeof(dst->strides));
  dst->byte_order = (unsigned)src->byte_order;
}

static void api_fill_native_output_view(const b2nd_output_view_t *src, b2nd_exec_output_view *dst) {
  *dst = (b2nd_exec_output_view){0};
  dst->struct_size = sizeof(*dst);
  dst->base = src->base;
  dst->capacity = src->capacity;
  dst->offset = src->offset;
  memcpy(dst->strides, src->strides, sizeof(dst->strides));
}

static bool api_view_contiguous_mut_buffer(const b2nd_output_view_t *v,
                                           size_t out_ndim, const int64_t *out_shape,
                                           size_t out_items, size_t itemsize,
                                           me_mut_buffer *out) {
  if (out_items == 0) {
    *out = (me_mut_buffer){v->base, v->capacity};
    return true;
  }
  if (!v->base || v->offset > v->capacity || out_items > SIZE_MAX / itemsize ||
      (uintptr_t)v->base > UINTPTR_MAX - v->offset) {
    return false;
  }
  size_t need = out_items * itemsize;
  if (v->capacity - v->offset < need) {
    return false;
  }
  int64_t expected = (int64_t)itemsize;
  for (size_t d = out_ndim; d-- > 0;) {
    if (v->strides[d] != expected) {
      return false;
    }
    if (out_shape[d] > INT64_MAX / expected) {
      return false;
    }
    expected *= out_shape[d];
  }
  *out = (me_mut_buffer){(unsigned char *)v->base + v->offset, v->capacity - v->offset};
  return true;
}

static b2nd_exec_status_t api_read_schedule_input(const b2nd_schedule_t *schedule, size_t slot,
                                                  b2nd_exec_input_meta *out) {
  if (!schedule || !out || slot >= schedule->ninputs) {
    return B2ND_EXEC_ERR_INVALID_ARG;
  }
  *out = schedule->inputs[slot];
  return B2ND_EXEC_SUCCESS;
}

b2nd_exec_status_t b2nd_schedule_admit(const b2nd_schedule_t *schedule,
                                       const b2nd_operand_t *inputs, size_t ninputs,
                                       const b2nd_output_t *output, bool *admitted,
                                       b2nd_exec_error_t *error) {
  if (admitted) {
    *admitted = false;
  }
  api_clear_error(error);
  if (!schedule || !admitted || (ninputs && !inputs) || ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid admission arguments");
  }
  b2nd_plan_info_t pinfo = B2ND_PLAN_INFO_INITIALIZER;
  b2nd_schedule_info_t sinfo = B2ND_SCHEDULE_INFO_INITIALIZER;
  if (b2nd_plan_describe(schedule->owner, &pinfo) != B2ND_EXEC_SUCCESS ||
      b2nd_schedule_describe(schedule, &sinfo, NULL, 0) != B2ND_EXEC_SUCCESS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid schedule metadata");
  }
  if (ninputs != pinfo.ninputs) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Input count mismatch");
  }
  b2nd_operand_t ops[B2ND_EXEC_MAX_INPUTS];
  for (size_t i = 0; i < ninputs; i++) {
    ops[i] = (b2nd_operand_t)B2ND_OPERAND_INITIALIZER;
    if (!api_copy_in(&inputs[i], inputs[i].struct_size, B2ND_OPERAND_V1_SIZE,
                     &ops[i], sizeof(ops[i]))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid operand struct_size");
    }
    if (ops[i].kind < B2ND_OPERAND_ARRAY || ops[i].kind > B2ND_OPERAND_SOURCE) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid operand kind");
    }
  }
  b2nd_output_t out_op = B2ND_OUTPUT_INITIALIZER;
  if (output) {
    if (!api_copy_in(output, output->struct_size, B2ND_OUTPUT_V1_SIZE,
                     &out_op, sizeof(out_op))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid output struct_size");
    }
    if (out_op.kind < B2ND_OUTPUT_ARRAY || out_op.kind > B2ND_OUTPUT_SINK) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid output kind");
    }
  }

  if (schedule->is_partition) {
    const b2nd_array_t *arrays[B2ND_EXEC_MAX_INPUTS] = {NULL};
    b2nd_exec_view views[B2ND_EXEC_MAX_INPUTS] = {{0}};
    b2nd_exec_source sources[B2ND_EXEC_MAX_INPUTS] = {{0}};
    api_cb_ctx cb_ctxs[B2ND_EXEC_MAX_INPUTS] = {{0}};
    for (size_t i = 0; i < ninputs; i++) {
      b2nd_exec_input_meta meta = {0};
      b2nd_exec_schedule_input(schedule->single, i, &meta);
      if (ops[i].kind == B2ND_OPERAND_ARRAY) {
        if (!ops[i].as.array) {
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                               "NULL array operand");
        }
        arrays[i] = ops[i].as.array;
      } else if (ops[i].kind == B2ND_OPERAND_VIEW) {
        if (!api_view_matches_meta(&ops[i].as.view, &meta)) {
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                               "Operand view metadata mismatch");
        }
        api_fill_native_view(&ops[i].as.view, &views[i]);
      } else {
        if (!ops[i].as.source.read) {
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                               "Source operand requires read callback");
        }
        cb_ctxs[i].user_context = ops[i].as.source.context;
        cb_ctxs[i].read = ops[i].as.source.read;
        sources[i].struct_size = sizeof(sources[i]);
        if (!api_to_me_dtype(ops[i].as.source.dtype, &sources[i].dtype)) {
          return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                               "Invalid source dtype");
        }
        sources[i].itemsize = ops[i].as.source.itemsize;
        sources[i].ndim = ops[i].as.source.ndim;
        if (sources[i].ndim <= B2ND_EXEC_MAX_RANK) {
          memcpy(sources[i].shape, ops[i].as.source.shape,
                 sources[i].ndim * sizeof(int64_t));
        }
        sources[i].context = &cb_ctxs[i];
        sources[i].read = api_source_trampoline;
      }
    }
    bool native_adm = false;
    me_error me_err = {0};
    me_status mst = b2nd_exec_admit_partitions(
        schedule->single, ninputs ? arrays : NULL, ninputs ? views : NULL,
        ninputs ? sources : NULL, ninputs, &native_adm, &me_err);
    if (mst != ME_SUCCESS) {
      return api_from_me_error(error, mst, &me_err, UINT32_MAX, SIZE_MAX);
    }
    if (native_adm && output) {
      me_mut_buffer unused = {0};
      if (out_op.kind != B2ND_OUTPUT_VIEW || out_op.as.view.allow_alias ||
          !api_view_contiguous_mut_buffer(&out_op.as.view, sinfo.output_ndim,
                                          sinfo.output_shape, sinfo.output_items,
                                          pinfo.itemsize, &unused)) {
        native_adm = false;
      }
    }
    *admitted = native_adm;
    return B2ND_EXEC_SUCCESS;
  }

  bool has_array = false, has_view = false, has_source = false;
  for (size_t i = 0; i < ninputs; i++) {
    b2nd_exec_input_meta meta = {0};
    if (api_read_schedule_input(schedule, i, &meta) != B2ND_EXEC_SUCCESS) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Cannot read schedule input metadata");
    }
    if (ops[i].kind == B2ND_OPERAND_ARRAY) {
      has_array = true;
      if (schedule->is_staged) {
        return B2ND_EXEC_SUCCESS;
      }
      if (pinfo.has_where && i == pinfo.where_slot) {
        return B2ND_EXEC_SUCCESS;
      }
      b2nd_exec_status_t rej = B2ND_EXEC_SUCCESS;
      if (!api_admit_array_metadata(ops[i].as.array, &meta, &rej)) {
        if (rej == B2ND_EXEC_ERR_INVALID_ARG) {
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                               "Invalid array operand metadata");
        }
        return B2ND_EXEC_SUCCESS;
      }
    } else if (ops[i].kind == B2ND_OPERAND_VIEW) {
      has_view = true;
      if (!api_view_matches_meta(&ops[i].as.view, &meta)) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Operand view metadata mismatch");
      }
      if (!schedule->is_staged) {
        b2nd_exec_view v;
        api_fill_native_view(&ops[i].as.view, &v);
        if (b2nd_exec_validate_view(schedule->single, i, &v, NULL) != ME_SUCCESS) {
          return B2ND_EXEC_SUCCESS;
        }
      }
    } else {
      has_source = true;
      if (!ops[i].as.source.read) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Source operand requires read callback");
      }
      if (pinfo.has_where) {
        return B2ND_EXEC_SUCCESS;
      }
      if (me_to_api_dtype(meta.dtype) != ops[i].as.source.dtype ||
          meta.itemsize != ops[i].as.source.itemsize ||
          meta.ndim != ops[i].as.source.ndim) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Source operand metadata mismatch");
      }
      for (size_t d = 0; d < meta.ndim; d++) {
        if (meta.shape[d] != ops[i].as.source.shape[d]) {
          return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                               "Source operand shape mismatch");
        }
      }
    }
  }
  if (has_array && has_source) {
    return B2ND_EXEC_SUCCESS;
  }
  if (output) {
    if (out_op.kind == B2ND_OUTPUT_ARRAY) {
      if (schedule->is_staged || pinfo.reduction || has_source || !pinfo.ninputs ||
          !api_admit_sink_array_metadata(out_op.as.array, &pinfo, &sinfo)) {
        return B2ND_EXEC_SUCCESS;
      }
      for (size_t i = 0; i < ninputs; i++) {
        if (ops[i].kind == B2ND_OPERAND_ARRAY &&
            (ops[i].as.array == out_op.as.array ||
             ops[i].as.array->sc == out_op.as.array->sc)) {
          return B2ND_EXEC_SUCCESS;
        }
      }
    } else if (out_op.kind == B2ND_OUTPUT_SINK) {
      if (schedule->is_staged || has_array || !out_op.as.sink.write ||
          out_op.as.sink.dtype != pinfo.dtype || out_op.as.sink.itemsize != pinfo.itemsize ||
          out_op.as.sink.ndim != sinfo.output_ndim) {
        return B2ND_EXEC_SUCCESS;
      }
      for (size_t d = 0; d < sinfo.output_ndim; d++) {
        if (out_op.as.sink.shape[d] != sinfo.output_shape[d]) {
          return B2ND_EXEC_SUCCESS;
        }
      }
    } else {
      if (out_op.as.view.allow_alias && (schedule->is_staged || has_array || has_source)) {
        return B2ND_EXEC_SUCCESS;
      }
      if (has_array) {
        me_mut_buffer unused = {0};
        if (!api_view_contiguous_mut_buffer(&out_op.as.view, sinfo.output_ndim,
                                            sinfo.output_shape, sinfo.output_items,
                                            pinfo.itemsize, &unused)) {
          return B2ND_EXEC_SUCCESS;
        }
      }
    }
  }
  (void)has_view;
  *admitted = true;
  return B2ND_EXEC_SUCCESS;
}

static void api_fill_report_exec(b2nd_exec_report_t *dst, const b2nd_exec_report *src) {
  dst->tiles = src->tiles;
  dst->map_calls = src->map_calls;
  dst->direct_tiles = src->direct_tiles;
  dst->mapped_bytes = src->mapped_bytes;
  dst->fp.flags = src->fp.flags;
  dst->fp.supported = src->fp.supported;
  dst->fp_observed = src->fp_observed;
  dst->tasks = src->tasks;
  dst->worker_bytes = src->worker_bytes;
  dst->gather_bytes = src->gather_bytes;
  dst->gathered_bytes = src->gathered_bytes;
  dst->staging_bytes = src->staging_bytes;
  dst->interpreter_calls = src->interpreter_calls;
  dst->jit_calls = src->jit_calls;
}

static void api_fill_report_storage(b2nd_exec_report_t *dst,
                                    const b2nd_exec_storage_report *src) {
  api_fill_report_exec(dst, &src->execution);
  dst->decode_bytes = src->decode_bytes;
  dst->storage_metadata_bytes = src->storage_metadata_bytes;
  dst->decodes = src->decodes;
  dst->decoded_bytes = src->decoded_bytes;
  dst->cache_hits = src->cache_hits;
  dst->read_spans = src->read_spans;
  dst->evaluated_groups = src->evaluated_groups;
  dst->max_group_items = src->max_group_items;
}

static void api_fill_report_cb(b2nd_exec_report_t *dst,
                               const b2nd_exec_callback_report *src) {
  api_fill_report_exec(dst, &src->execution);
  dst->read_calls = src->read_calls;
  dst->read_items = src->read_items;
  dst->max_read_items = src->max_read_items;
  dst->read_bytes = src->read_bytes;
  dst->write_calls = src->write_calls;
  dst->write_items = src->write_items;
  dst->max_write_items = src->max_write_items;
  dst->written_bytes = src->written_bytes;
  dst->index_buffer_bytes = src->index_buffer_bytes;
  dst->output_buffer_bytes = src->output_buffer_bytes;
}

static void api_fill_report_staged(b2nd_exec_report_t *dst,
                                   const b2nd_exec_staged_report *src,
                                   size_t nstages) {
  dst->fp.flags = src->fp.flags;
  dst->fp.supported = src->fp.supported;
  dst->entered_stages = src->entered_stages;
  dst->intermediate_bytes = src->intermediate_bytes;
  dst->live_intermediate_bytes = src->live_intermediate_bytes;
  dst->failed_stage = src->failed_stage;
  dst->failed_node = src->failed_node;
  bool seen_fp = false;
  for (size_t s = 0; s < src->entered_stages && s < nstages; s++) {
    const b2nd_exec_callback_report *sr = &src->stages[s];
    dst->tiles += sr->execution.tiles;
    dst->map_calls += sr->execution.map_calls;
    dst->direct_tiles += sr->execution.direct_tiles;
    if (sr->execution.mapped_bytes > dst->mapped_bytes) {
      dst->mapped_bytes = sr->execution.mapped_bytes;
    }
    if (sr->execution.gather_bytes > dst->gather_bytes) {
      dst->gather_bytes = sr->execution.gather_bytes;
    }
    dst->gathered_bytes += sr->execution.gathered_bytes;
    dst->interpreter_calls += sr->execution.interpreter_calls;
    dst->jit_calls += sr->execution.jit_calls;
    dst->read_calls += sr->read_calls;
    dst->read_items += sr->read_items;
    if (sr->max_read_items > dst->max_read_items) {
      dst->max_read_items = sr->max_read_items;
    }
    dst->read_bytes += sr->read_bytes;
    dst->write_calls += sr->write_calls;
    dst->write_items += sr->write_items;
    if (sr->max_write_items > dst->max_write_items) {
      dst->max_write_items = sr->max_write_items;
    }
    dst->written_bytes += sr->written_bytes;
    if (sr->index_buffer_bytes > dst->index_buffer_bytes) {
      dst->index_buffer_bytes = sr->index_buffer_bytes;
    }
    if (sr->output_buffer_bytes > dst->output_buffer_bytes) {
      dst->output_buffer_bytes = sr->output_buffer_bytes;
    }
    if (sr->execution.fp_observed) {
      dst->fp_observed = true;
      seen_fp = true;
    }
    if (dst->stage_reports && s < dst->stage_reports_cap) {
      b2nd_stage_report_t item = B2ND_STAGE_REPORT_INITIALIZER;
      item.tiles = sr->execution.tiles;
      item.map_calls = sr->execution.map_calls;
      item.direct_tiles = sr->execution.direct_tiles;
      item.mapped_bytes = sr->execution.mapped_bytes;
      item.gather_bytes = sr->execution.gather_bytes;
      item.gathered_bytes = sr->execution.gathered_bytes;
      item.interpreter_calls = sr->execution.interpreter_calls;
      item.jit_calls = sr->execution.jit_calls;
      item.read_calls = sr->read_calls;
      item.read_items = sr->read_items;
      item.max_read_items = sr->max_read_items;
      item.read_bytes = sr->read_bytes;
      item.write_calls = sr->write_calls;
      item.write_items = sr->write_items;
      item.max_write_items = sr->max_write_items;
      item.written_bytes = sr->written_bytes;
      item.index_buffer_bytes = sr->index_buffer_bytes;
      item.output_buffer_bytes = sr->output_buffer_bytes;
      item.fp.flags = sr->execution.fp.flags;
      item.fp.supported = sr->execution.fp.supported;
      item.fp_observed = sr->execution.fp_observed;
      api_copy_out(&item, sizeof(item), B2ND_STAGE_REPORT_V1_SIZE,
                   &dst->stage_reports[s], dst->stage_reports[s].struct_size);
    }
  }
  (void)seen_fp;
}

b2nd_exec_status_t b2nd_execute(const b2nd_schedule_t *schedule,
                                const b2nd_operand_t *inputs, size_t ninputs,
                                const b2nd_output_t *output,
                                const b2nd_run_options_t *options,
                                b2nd_exec_report_t *report, b2nd_exec_error_t *error) {
  api_clear_error(error);
  b2nd_exec_report_t local_rep = B2ND_EXEC_REPORT_INITIALIZER;
  if (report) {
    if (report->struct_size < B2ND_EXEC_REPORT_V1_SIZE) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid execution report struct_size");
    }
    b2nd_exec_report_t user_in = B2ND_EXEC_REPORT_INITIALIZER;
    api_copy_in(report, report->struct_size, B2ND_EXEC_REPORT_V1_SIZE,
                &user_in, sizeof(user_in));
    local_rep.stage_reports = user_in.stage_reports;
    local_rep.stage_reports_cap = user_in.stage_reports_cap;
    if (local_rep.stage_reports_cap && !local_rep.stage_reports) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Missing stage_reports buffer");
    }
    for (size_t s = 0; s < local_rep.stage_reports_cap; s++) {
      if (local_rep.stage_reports[s].struct_size < B2ND_STAGE_REPORT_V1_SIZE) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Invalid stage_report struct_size");
      }
      b2nd_stage_report_t z = B2ND_STAGE_REPORT_INITIALIZER;
      api_copy_out(&z, sizeof(z), B2ND_STAGE_REPORT_V1_SIZE,
                   &local_rep.stage_reports[s], local_rep.stage_reports[s].struct_size);
    }
    api_copy_out(&local_rep, sizeof(local_rep), B2ND_EXEC_REPORT_V1_SIZE,
                 report, report->struct_size);
  }
  if (!schedule || !output || (ninputs && !inputs) || ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid execution bindings");
  }
  b2nd_run_options_t ropts = B2ND_RUN_OPTIONS_INITIALIZER;
  if (options) {
    if (!api_copy_in(options, options->struct_size, B2ND_RUN_OPTIONS_V1_SIZE,
                     &ropts, sizeof(ropts))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid run options struct_size");
    }
  }
  if (!ropts.workers ||
      (ropts.raise_mask & ~(B2ND_FP_INVALID | B2ND_FP_DIVIDE | B2ND_FP_OVERFLOW | B2ND_FP_UNDERFLOW))) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid worker count or raise mask");
  }
  b2nd_plan_info_t pinfo = B2ND_PLAN_INFO_INITIALIZER;
  b2nd_schedule_info_t sinfo = B2ND_SCHEDULE_INFO_INITIALIZER;
  if (b2nd_plan_describe(schedule->owner, &pinfo) != B2ND_EXEC_SUCCESS ||
      b2nd_schedule_describe(schedule, &sinfo, NULL, 0) != B2ND_EXEC_SUCCESS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid schedule metadata");
  }
  if (ninputs != pinfo.ninputs) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Execution input count mismatch");
  }
  b2nd_output_t out_op = B2ND_OUTPUT_INITIALIZER;
  if (!api_copy_in(output, output->struct_size, B2ND_OUTPUT_V1_SIZE,
                   &out_op, sizeof(out_op)) ||
      out_op.kind < B2ND_OUTPUT_ARRAY || out_op.kind > B2ND_OUTPUT_SINK) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid output descriptor");
  }

  const b2nd_array_t *arrays[B2ND_EXEC_MAX_INPUTS] = {NULL};
  b2nd_exec_view views[B2ND_EXEC_MAX_INPUTS] = {{0}};
  b2nd_exec_source sources[B2ND_EXEC_MAX_INPUTS] = {{0}};
  api_cb_ctx src_ctxs[B2ND_EXEC_MAX_INPUTS] = {{0}};
  bool has_array = false, has_view = false, has_source = false;

  for (size_t i = 0; i < ninputs; i++) {
    b2nd_operand_t op = B2ND_OPERAND_INITIALIZER;
    if (!api_copy_in(&inputs[i], inputs[i].struct_size, B2ND_OPERAND_V1_SIZE,
                     &op, sizeof(op)) ||
        op.kind < B2ND_OPERAND_ARRAY || op.kind > B2ND_OPERAND_SOURCE) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid operand descriptor");
    }
    b2nd_exec_input_meta meta = {0};
    if (api_read_schedule_input(schedule, i, &meta) != B2ND_EXEC_SUCCESS) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Cannot query input metadata");
    }
    sources[i].struct_size = sizeof(sources[i]);
    sources[i].dtype = meta.dtype;
    sources[i].itemsize = meta.itemsize;
    sources[i].ndim = meta.ndim;
    if (meta.ndim <= B2ND_EXEC_MAX_RANK) {
      memcpy(sources[i].shape, meta.shape, meta.ndim * sizeof(int64_t));
    }
    if (op.kind == B2ND_OPERAND_ARRAY) {
      if (!op.as.array) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "NULL array operand");
      }
      arrays[i] = op.as.array;
      has_array = true;
    } else if (op.kind == B2ND_OPERAND_VIEW) {
      if (!api_view_matches_meta(&op.as.view, &meta)) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "View operand dtype or shape mismatch");
      }
      api_fill_native_view(&op.as.view, &views[i]);
      has_view = true;
    } else {
      if (!op.as.source.read) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Source operand requires read callback");
      }
      me_dtype sdt = ME_AUTO;
      if (!api_to_me_dtype(op.as.source.dtype, &sdt)) {
        return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Invalid source dtype");
      }
      src_ctxs[i].user_context = op.as.source.context;
      src_ctxs[i].read = op.as.source.read;
      sources[i].dtype = sdt;
      sources[i].itemsize = op.as.source.itemsize;
      sources[i].ndim = op.as.source.ndim;
      if (op.as.source.ndim <= B2ND_EXEC_MAX_RANK) {
        memcpy(sources[i].shape, op.as.source.shape, op.as.source.ndim * sizeof(int64_t));
      }
      sources[i].context = &src_ctxs[i];
      sources[i].read = api_source_trampoline;
      has_source = true;
    }
  }

  b2nd_exec_jobs jobs = {sizeof(jobs), ropts.workers, 0};
  if (ropts.has_worker_scratch) {
    jobs.max_worker_scratch = ropts.max_worker_scratch;
  } else if (ropts.workers > 1) {
    jobs.max_worker_scratch = ropts.max_gather_bytes
        ? ropts.max_gather_bytes : ((size_t)64 * 1024 * 1024);
  }
  const b2nd_exec_jobs *jobs_ptr =
      (ropts.workers > 1 || ropts.has_worker_scratch) ? &jobs : NULL;

  me_error me_err = {0};
  me_status mst = ME_SUCCESS;
  uint32_t err_node = UINT32_MAX;
  size_t err_stage = SIZE_MAX;

  if (schedule->is_partition) {
    if (ropts.workers > 1) {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Multi-worker partition execution is not supported via this API");
    }
    if (out_op.kind != B2ND_OUTPUT_VIEW || out_op.as.view.allow_alias) {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Partition execution requires a non-aliased output view");
    }
    me_mut_buffer out_buf = {0};
    if (!api_view_contiguous_mut_buffer(&out_op.as.view, sinfo.output_ndim, sinfo.output_shape,
                                        sinfo.output_items, pinfo.itemsize, &out_buf)) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Partition output must be contiguous with matching extent");
    }
    b2nd_exec_storage_report srep = {0};
    mst = b2nd_exec_execute_partitions(
        schedule->single, has_array ? arrays : NULL, has_view ? views : NULL,
        has_source ? sources : NULL, ninputs, out_buf, ropts.raise_mask,
        ropts.max_decode_bytes, ropts.max_gather_bytes, &srep, &me_err);
    api_fill_report_storage(&local_rep, &srep);
  } else if (schedule->is_staged) {
    if (ropts.workers > 1) {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Multi-worker staged execution is not supported");
    }
    if (has_array || out_op.kind != B2ND_OUTPUT_VIEW || out_op.as.view.allow_alias) {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Staged execution requires view/source inputs and non-aliased view output");
    }
    b2nd_exec_output_view out_view;
    api_fill_native_output_view(&out_op.as.view, &out_view);
    b2nd_exec_staged_report srep = {0};
    mst = b2nd_exec_staged_execute_sources_into(
        schedule->staged, ninputs ? sources : NULL, has_view ? views : NULL, ninputs,
        &out_view, ropts.raise_mask, ropts.max_gather_bytes, &srep, &me_err);
    api_fill_report_staged(&local_rep, &srep, pinfo.nstages);
    err_stage = srep.failed_stage;
    err_node = srep.failed_node;
  } else if (has_array) {
    if (has_source) {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Mixing compressed arrays and source callbacks is not admitted");
    }
    b2nd_exec_storage_report srep = {0};
    if (out_op.kind == B2ND_OUTPUT_ARRAY) {
      mst = b2nd_exec_execute_to_array(
          schedule->single, arrays, has_view ? views : NULL, ninputs, out_op.as.array,
          ropts.raise_mask, ropts.max_decode_bytes, ropts.max_gather_bytes,
          jobs_ptr, &srep, &me_err);
    } else if (out_op.kind == B2ND_OUTPUT_VIEW && !out_op.as.view.allow_alias) {
      me_mut_buffer out_buf = {0};
      if (!api_view_contiguous_mut_buffer(&out_op.as.view, sinfo.output_ndim, sinfo.output_shape,
                                          sinfo.output_items, pinfo.itemsize, &out_buf)) {
        return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Storage array execution requires a contiguous output view");
      }
      mst = b2nd_exec_execute_arrays_ex(
          schedule->single, arrays, has_view ? views : NULL, ninputs, out_buf,
          ropts.raise_mask, ropts.max_decode_bytes, ropts.max_gather_bytes,
          jobs_ptr, &srep, &me_err);
    } else {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Unsupported output mode for compressed array inputs");
    }
    api_fill_report_storage(&local_rep, &srep);
  } else if (has_source || out_op.kind == B2ND_OUTPUT_SINK) {
    if (ropts.workers > 1) {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Multi-worker callback execution is not supported");
    }
    b2nd_exec_callback_report crep = {0};
    if (out_op.kind == B2ND_OUTPUT_SINK) {
      if (!out_op.as.sink.write) {
        return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Sink output requires write callback");
      }
      me_dtype sink_dt = ME_AUTO;
      if (!api_to_me_dtype(out_op.as.sink.dtype, &sink_dt)) {
        return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Invalid sink dtype");
      }
      api_cb_ctx sink_ctx = {out_op.as.sink.context, NULL, out_op.as.sink.write};
      b2nd_exec_sink native_sink = {0};
      native_sink.struct_size = sizeof(native_sink);
      native_sink.dtype = sink_dt;
      native_sink.itemsize = out_op.as.sink.itemsize;
      native_sink.ndim = out_op.as.sink.ndim;
      if (native_sink.ndim <= B2ND_EXEC_MAX_RANK) {
        memcpy(native_sink.shape, out_op.as.sink.shape, native_sink.ndim * sizeof(int64_t));
      }
      native_sink.context = &sink_ctx;
      native_sink.write = api_sink_trampoline;
      mst = b2nd_exec_execute_sources(
          schedule->single, ninputs ? sources : NULL, has_view ? views : NULL, ninputs,
          (me_mut_buffer){0}, &native_sink, ropts.raise_mask,
          ropts.max_gather_bytes, ropts.max_output_bytes, &crep, &me_err);
    } else if (out_op.kind == B2ND_OUTPUT_VIEW && !out_op.as.view.allow_alias) {
      b2nd_exec_output_view out_view;
      api_fill_native_output_view(&out_op.as.view, &out_view);
      mst = b2nd_exec_execute_sources_into(
          schedule->single, ninputs ? sources : NULL, has_view ? views : NULL, ninputs,
          &out_view, ropts.raise_mask, ropts.max_gather_bytes, &crep, &me_err);
    } else {
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Unsupported output mode for callback sources");
    }
    api_fill_report_cb(&local_rep, &crep);
  } else if (out_op.kind == B2ND_OUTPUT_ARRAY) {
    if (ninputs > 0) {
      b2nd_exec_storage_report srep = {0};
      mst = b2nd_exec_execute_to_array(
          schedule->single, arrays, views, ninputs, out_op.as.array,
          ropts.raise_mask, ropts.max_decode_bytes, ropts.max_gather_bytes,
          jobs_ptr, &srep, &me_err);
      api_fill_report_storage(&local_rep, &srep);
    } else {
      /* Zero-input constant plan into compressed array: materialize via scratch buffer. */
      if (!api_admit_sink_array_metadata(out_op.as.array, &pinfo, &sinfo)) {
        return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Unsupported compressed sink destination");
      }
      size_t out_bytes = sinfo.output_items * pinfo.itemsize;
      unsigned char *tmp = out_bytes ? malloc(out_bytes) : NULL;
      if (out_bytes && !tmp) {
        return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Cannot allocate zero-input sink buffer");
      }
      b2nd_exec_output_view tmp_view = {sizeof(tmp_view), tmp, out_bytes, 0, {0}};
      size_t stride = pinfo.itemsize;
      for (size_t d = sinfo.output_ndim; d-- > 0;) {
        tmp_view.strides[d] = (int64_t)stride;
        stride *= (size_t)sinfo.output_shape[d];
      }
      b2nd_exec_report erep = {0};
      mst = b2nd_exec_execute_into(schedule->single, views, 0, &tmp_view,
                                   ropts.raise_mask, ropts.max_gather_bytes,
                                   jobs_ptr, &erep, &me_err);
      api_fill_report_exec(&local_rep, &erep);
      if (mst == ME_SUCCESS && sinfo.output_items > 0) {
        int64_t start[B2ND_MAX_DIM] = {0};
        if (b2nd_set_slice_cbuffer(tmp, out_op.as.array->shape, (int64_t)out_bytes,
                                   start, out_op.as.array->shape, out_op.as.array) < 0) {
          mst = ME_ERR_EVAL;
          snprintf(me_err.message, sizeof(me_err.message), "Failed writing compressed sink array");
        }
      }
      free(tmp);
    }
  } else {
    b2nd_exec_output_view out_view;
    api_fill_native_output_view(&out_op.as.view, &out_view);
    b2nd_exec_report erep = {0};
    if (out_op.as.view.allow_alias) {
      mst = b2nd_exec_execute_alias(
          schedule->single, views, ninputs, &out_view, out_op.as.view.staging_budget,
          ropts.raise_mask, ropts.max_gather_bytes, jobs_ptr, &erep, &me_err);
    } else {
      mst = b2nd_exec_execute_into(
          schedule->single, views, ninputs, &out_view, ropts.raise_mask,
          ropts.max_gather_bytes, jobs_ptr, &erep, &me_err);
    }
    api_fill_report_exec(&local_rep, &erep);
  }

  if (report) {
    api_copy_out(&local_rep, sizeof(local_rep), B2ND_EXEC_REPORT_V1_SIZE,
                 report, report->struct_size);
  }
  return api_from_me_error(error, mst, &me_err, err_node, err_stage);
}

static bool api_numpy_dtype_str(b2nd_exec_dtype_t dtype, size_t itemsize,
                                char *buf, size_t buf_cap) {
  uint16_t endian = 1;
  char host = *(const unsigned char *)&endian ? '<' : '>';
  switch (dtype) {
    case B2ND_DTYPE_BOOL: snprintf(buf, buf_cap, "|b1"); return true;
    case B2ND_DTYPE_INT8: snprintf(buf, buf_cap, "|i1"); return true;
    case B2ND_DTYPE_UINT8: snprintf(buf, buf_cap, "|u1"); return true;
    case B2ND_DTYPE_INT16: snprintf(buf, buf_cap, "%ci2", host); return true;
    case B2ND_DTYPE_UINT16: snprintf(buf, buf_cap, "%cu2", host); return true;
    case B2ND_DTYPE_INT32: snprintf(buf, buf_cap, "%ci4", host); return true;
    case B2ND_DTYPE_UINT32: snprintf(buf, buf_cap, "%cu4", host); return true;
    case B2ND_DTYPE_INT64: snprintf(buf, buf_cap, "%ci8", host); return true;
    case B2ND_DTYPE_UINT64: snprintf(buf, buf_cap, "%cu8", host); return true;
    case B2ND_DTYPE_FLOAT32: snprintf(buf, buf_cap, "%cf4", host); return true;
    case B2ND_DTYPE_FLOAT64: snprintf(buf, buf_cap, "%cf8", host); return true;
    case B2ND_DTYPE_COMPLEX64: snprintf(buf, buf_cap, "%cc8", host); return true;
    case B2ND_DTYPE_COMPLEX128: snprintf(buf, buf_cap, "%cc16", host); return true;
    case B2ND_DTYPE_STRING: snprintf(buf, buf_cap, "|S%zu", itemsize); return true;
    default: return false;
  }
}

static b2nd_context_t *api_derive_out_ctx(const b2nd_array_t *const *inputs, size_t ninputs,
                                          size_t out_ndim, const int64_t *out_shape,
                                          b2nd_exec_dtype_t out_dtype, size_t out_itemsize) {
  char dtype_str[32];
  if (!api_numpy_dtype_str(out_dtype, out_itemsize, dtype_str, sizeof(dtype_str))) {
    return NULL;
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  if (ninputs > 0 && inputs && inputs[0] && inputs[0]->sc && inputs[0]->sc->cctx) {
    blosc2_cparams inherited;
    if (blosc2_ctx_get_cparams(inputs[0]->sc->cctx, &inherited) == 0) {
      cparams = inherited;
    }
  }
  cparams.typesize = (int32_t)out_itemsize;
  cparams.blocksize = 0;
  cparams.prefilter = NULL;
  cparams.preparams = NULL;
  cparams.use_dict = 0;
  if (cparams.compcode > BLOSC_ZSTD) {
    cparams.compcode = BLOSC2_CPARAMS_DEFAULTS.compcode;
  }
  for (size_t f = 0; f < BLOSC2_MAX_FILTERS; f++) {
    if (cparams.filters[f] > BLOSC_DELTA) {
      memcpy(cparams.filters, BLOSC2_CPARAMS_DEFAULTS.filters, sizeof(cparams.filters));
      memcpy(cparams.filters_meta, BLOSC2_CPARAMS_DEFAULTS.filters_meta,
             sizeof(cparams.filters_meta));
      break;
    }
  }
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;

  int32_t chunkshape[B2ND_MAX_DIM] = {0};
  int32_t blockshape[B2ND_MAX_DIM] = {0};
  bool copied_geom = false;
  if (inputs) {
    for (size_t i = 0; i < ninputs; i++) {
      const b2nd_array_t *a = inputs[i];
      if (!a || (size_t)a->ndim != out_ndim) {
        continue;
      }
      bool match = true;
      for (size_t d = 0; d < out_ndim; d++) {
        if (a->shape[d] != out_shape[d]) {
          match = false;
          break;
        }
      }
      if (match) {
        int32_t bn = 1;
        bool fits = true;
        for (size_t d = 0; d < out_ndim; d++) {
          chunkshape[d] = a->chunkshape[d];
          blockshape[d] = a->blockshape[d];
          if (out_shape[d] > 0 && !blosc2_checked_mul_i32_nonneg(bn, blockshape[d], &bn)) {
            fits = false;
          }
        }
        int32_t bbytes = 0;
        if (fits && blosc2_checked_mul_i32_nonneg(bn, cparams.typesize, &bbytes)) {
          copied_geom = true;
          break;
        }
      }
    }
  }
  if (!copied_geom && out_ndim > 0) {
    int64_t max_chunk_items = 262144;
    int64_t max_block_items = 4096;
    if (out_itemsize > 0) {
      int64_t cap = ((int64_t)1024 * 1024) / (int64_t)out_itemsize;
      if (cap >= 1 && cap < max_chunk_items) {
        max_chunk_items = cap;
      }
      int64_t bcap = ((int64_t)32 * 1024) / (int64_t)out_itemsize;
      if (bcap >= 1 && bcap < max_block_items) {
        max_block_items = bcap;
      }
    }
    int64_t cprod = 1, bprod = 1;
    for (size_t d = out_ndim; d-- > 0;) {
      if (out_shape[d] == 0) {
        chunkshape[d] = 0;
        blockshape[d] = 0;
        continue;
      }
      int64_t cbudget = max_chunk_items / cprod;
      if (cbudget < 1) cbudget = 1;
      int64_t cs = out_shape[d] < cbudget ? out_shape[d] : cbudget;
      if (cs > INT32_MAX) cs = INT32_MAX;
      chunkshape[d] = (int32_t)cs;
      cprod *= cs;

      int64_t bbudget = max_block_items / bprod;
      if (bbudget < 1) bbudget = 1;
      int64_t bs = cs < bbudget ? cs : bbudget;
      if (bs > INT32_MAX) bs = INT32_MAX;
      blockshape[d] = (int32_t)bs;
      bprod *= bs;
    }
  }
  return b2nd_create_ctx(&storage, (int8_t)out_ndim, out_shape, chunkshape,
                         blockshape, dtype_str, DTYPE_NUMPY_FORMAT, NULL, 0);
}

b2nd_exec_status_t b2nd_eval(const char *source, const char *const *names,
                             const b2nd_array_t *const *inputs, size_t ninputs,
                             const b2nd_exec_options_t *exec_opts,
                             const b2nd_expr_options_t *expr_opts,
                             const b2nd_schedule_options_t *sched_opts,
                             const b2nd_run_options_t *run_opts,
                             b2nd_context_t *out_ctx, b2nd_array_t **out,
                             b2nd_exec_report_t *report, b2nd_exec_error_t *error) {
  if (out) {
    *out = NULL;
  }
  api_clear_error(error);
  if (!out || !source || (ninputs && (!inputs || !names)) || ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid b2nd_eval arguments");
  }
  b2nd_expr_options_t xopts = B2ND_EXPR_OPTIONS_INITIALIZER;
  if (expr_opts) {
    if (!api_copy_in(expr_opts, expr_opts->struct_size, B2ND_EXPR_OPTIONS_V1_SIZE,
                     &xopts, sizeof(xopts))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid expression options struct_size");
    }
  }
  bool has_where = xopts.reduce.struct_size != 0 && xopts.reduce.has_where_slot;
  if (has_where && !ninputs) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Reduction where mask requires an explicit Boolean trailing input");
  }

  b2nd_exec_input_meta_t metas[B2ND_EXEC_MAX_INPUTS];
  b2nd_exec_input_spec_t specs[B2ND_EXEC_MAX_INPUTS] = {{0}};
  b2nd_operand_t operands[B2ND_EXEC_MAX_INPUTS];
  for (size_t i = 0; i < ninputs; i++) {
    b2nd_exec_status_t st = api_array_meta(inputs[i], &metas[i], error);
    if (st != B2ND_EXEC_SUCCESS) {
      return st;
    }
    specs[i].name = names[i];
    specs[i].dtype = metas[i].dtype;
    specs[i].itemsize = metas[i].itemsize;
    operands[i] = (b2nd_operand_t)B2ND_OPERAND_INITIALIZER;
    operands[i].kind = B2ND_OPERAND_ARRAY;
    operands[i].as.array = inputs[i];
  }
  size_t expr_ninputs = has_where ? ninputs - 1 : ninputs;
  if (has_where && metas[ninputs - 1].dtype != B2ND_DTYPE_BOOL) {
    return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Reduction where mask input must be bool");
  }

  b2nd_plan_t *plan = NULL;
  b2nd_exec_status_t st = b2nd_plan_from_expr(
      source, expr_ninputs ? specs : NULL, expr_ninputs,
      exec_opts, expr_opts, &plan, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }
  b2nd_schedule_t *sched = NULL;
  st = b2nd_schedule_create(plan, ninputs ? metas : NULL, ninputs,
                            sched_opts, &sched, error);
  if (st != B2ND_EXEC_SUCCESS) {
    b2nd_plan_free(plan);
    return st;
  }
  b2nd_plan_info_t pinfo = B2ND_PLAN_INFO_INITIALIZER;
  b2nd_schedule_info_t sinfo = B2ND_SCHEDULE_INFO_INITIALIZER;
  b2nd_plan_describe(plan, &pinfo);
  b2nd_schedule_describe(sched, &sinfo, NULL, 0);

  b2nd_context_t *owned_ctx = NULL;
  b2nd_context_t *use_ctx = out_ctx;
  if (!use_ctx) {
    owned_ctx = api_derive_out_ctx(inputs, ninputs, sinfo.output_ndim,
                                   sinfo.output_shape, pinfo.dtype, pinfo.itemsize);
    if (!owned_ctx) {
      b2nd_schedule_free(sched);
      b2nd_plan_free(plan);
      return api_set_error(error, B2ND_EXEC_ERR_UNSUPPORTED, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Cannot derive output b2nd context");
    }
    use_ctx = owned_ctx;
  }

  b2nd_array_t *dst = NULL;
  if (!pinfo.reduction && sinfo.output_ndim >= 1 && ninputs > 0) {
    if (b2nd_uninit(use_ctx, &dst) != BLOSC2_ERROR_SUCCESS) {
      if (owned_ctx) b2nd_free_ctx(owned_ctx);
      b2nd_schedule_free(sched);
      b2nd_plan_free(plan);
      return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Cannot allocate output b2nd_array_t");
    }
    b2nd_output_t out_desc = B2ND_OUTPUT_INITIALIZER;
    out_desc.kind = B2ND_OUTPUT_ARRAY;
    out_desc.as.array = dst;
    st = b2nd_execute(sched, operands, ninputs, &out_desc, run_opts, report, error);
    if (st != B2ND_EXEC_SUCCESS) {
      b2nd_free(dst);
      dst = NULL;
    }
  } else {
    size_t out_bytes = sinfo.output_items * pinfo.itemsize;
    unsigned char *buf = out_bytes ? malloc(out_bytes) : NULL;
    if (out_bytes && !buf) {
      if (owned_ctx) b2nd_free_ctx(owned_ctx);
      b2nd_schedule_free(sched);
      b2nd_plan_free(plan);
      return api_set_error(error, B2ND_EXEC_ERR_OOM, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Cannot allocate temporary evaluation output buffer");
    }
    b2nd_output_t out_desc = B2ND_OUTPUT_INITIALIZER;
    out_desc.kind = B2ND_OUTPUT_VIEW;
    out_desc.as.view.base = buf;
    out_desc.as.view.capacity = out_bytes;
    size_t stride = pinfo.itemsize;
    for (size_t d = sinfo.output_ndim; d-- > 0;) {
      out_desc.as.view.strides[d] = (int64_t)stride;
      stride *= (size_t)sinfo.output_shape[d];
    }
    st = b2nd_execute(sched, ninputs ? operands : NULL, ninputs, &out_desc,
                      run_opts, report, error);
    if (st == B2ND_EXEC_SUCCESS) {
      int rc = out_bytes
          ? b2nd_from_cbuffer(use_ctx, &dst, buf, (int64_t)out_bytes)
          : b2nd_empty(use_ctx, &dst);
      if (rc != BLOSC2_ERROR_SUCCESS) {
        if (dst) b2nd_free(dst);
        dst = NULL;
        st = api_set_error(error, B2ND_EXEC_ERR_EVAL, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Cannot materialize b2nd_array_t result");
      }
    }
    free(buf);
  }

  if (owned_ctx) {
    b2nd_free_ctx(owned_ctx);
  }
  b2nd_schedule_free(sched);
  b2nd_plan_free(plan);
  if (st == B2ND_EXEC_SUCCESS) {
    *out = dst;
  }
  return st;
}

b2nd_exec_status_t b2nd_eval_into(const char *source, const char *const *names,
                                  const b2nd_operand_t *inputs, size_t ninputs,
                                  const b2nd_exec_options_t *exec_opts,
                                  const b2nd_expr_options_t *expr_opts,
                                  const b2nd_schedule_options_t *sched_opts,
                                  const b2nd_run_options_t *run_opts,
                                  const b2nd_output_t *output,
                                  b2nd_exec_report_t *report, b2nd_exec_error_t *error) {
  api_clear_error(error);
  if (!source || !output || (ninputs && (!inputs || !names)) || ninputs > B2ND_EXEC_MAX_INPUTS) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Invalid b2nd_eval_into arguments");
  }
  b2nd_expr_options_t xopts = B2ND_EXPR_OPTIONS_INITIALIZER;
  if (expr_opts) {
    if (!api_copy_in(expr_opts, expr_opts->struct_size, B2ND_EXPR_OPTIONS_V1_SIZE,
                     &xopts, sizeof(xopts))) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid expression options struct_size");
    }
  }
  bool has_where = xopts.reduce.struct_size != 0 && xopts.reduce.has_where_slot;
  if (has_where && !ninputs) {
    return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Reduction where mask requires an explicit Boolean trailing input");
  }

  b2nd_exec_input_meta_t metas[B2ND_EXEC_MAX_INPUTS];
  b2nd_exec_input_spec_t specs[B2ND_EXEC_MAX_INPUTS] = {{0}};
  for (size_t i = 0; i < ninputs; i++) {
    b2nd_operand_t op = B2ND_OPERAND_INITIALIZER;
    if (!api_copy_in(&inputs[i], inputs[i].struct_size, B2ND_OPERAND_V1_SIZE,
                     &op, sizeof(op)) ||
        op.kind < B2ND_OPERAND_ARRAY || op.kind > B2ND_OPERAND_SOURCE) {
      return api_set_error(error, B2ND_EXEC_ERR_INVALID_ARG, 0, 0, UINT32_MAX, SIZE_MAX,
                           "Invalid operand descriptor");
    }
    metas[i] = (b2nd_exec_input_meta_t)B2ND_EXEC_INPUT_META_INITIALIZER;
    if (op.kind == B2ND_OPERAND_ARRAY) {
      b2nd_exec_status_t st = api_array_meta(op.as.array, &metas[i], error);
      if (st != B2ND_EXEC_SUCCESS) {
        return st;
      }
    } else if (op.kind == B2ND_OPERAND_VIEW) {
      if (op.as.view.dtype == B2ND_DTYPE_AUTO || op.as.view.ndim > B2ND_EXEC_MAX_RANK) {
        return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                             "b2nd_eval_into view operands require explicit dtype and rank");
      }
      size_t width = 0;
      b2nd_exec_status_t st = api_resolve_itemsize(op.as.view.dtype, op.as.view.itemsize,
                                                   &width, error);
      if (st != B2ND_EXEC_SUCCESS) {
        return st;
      }
      metas[i].dtype = op.as.view.dtype;
      metas[i].itemsize = width;
      metas[i].ndim = op.as.view.ndim;
      memcpy(metas[i].shape, op.as.view.shape, op.as.view.ndim * sizeof(int64_t));
    } else {
      if (op.as.source.dtype == B2ND_DTYPE_AUTO || op.as.source.ndim > B2ND_EXEC_MAX_RANK) {
        return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                             "Invalid source operand dtype or rank");
      }
      size_t width = 0;
      b2nd_exec_status_t st = api_resolve_itemsize(op.as.source.dtype, op.as.source.itemsize,
                                                   &width, error);
      if (st != B2ND_EXEC_SUCCESS) {
        return st;
      }
      metas[i].dtype = op.as.source.dtype;
      metas[i].itemsize = width;
      metas[i].ndim = op.as.source.ndim;
      memcpy(metas[i].shape, op.as.source.shape, op.as.source.ndim * sizeof(int64_t));
    }
    specs[i].name = names[i];
    specs[i].dtype = metas[i].dtype;
    specs[i].itemsize = metas[i].itemsize;
  }

  size_t expr_ninputs = has_where ? ninputs - 1 : ninputs;
  if (has_where && metas[ninputs - 1].dtype != B2ND_DTYPE_BOOL) {
    return api_set_error(error, B2ND_EXEC_ERR_SIGNATURE, 0, 0, UINT32_MAX, SIZE_MAX,
                         "Reduction where mask input must be bool");
  }
  b2nd_plan_t *plan = NULL;
  b2nd_exec_status_t st = b2nd_plan_from_expr(
      source, expr_ninputs ? specs : NULL, expr_ninputs,
      exec_opts, expr_opts, &plan, error);
  if (st != B2ND_EXEC_SUCCESS) {
    return st;
  }
  b2nd_schedule_t *sched = NULL;
  st = b2nd_schedule_create(plan, ninputs ? metas : NULL, ninputs,
                            sched_opts, &sched, error);
  if (st == B2ND_EXEC_SUCCESS) {
    st = b2nd_execute(sched, inputs, ninputs, output, run_opts, report, error);
  }
  b2nd_schedule_free(sched);
  b2nd_plan_free(plan);
  return st;
}

#endif /* BLOSC2_HAVE_MINIEXPR */

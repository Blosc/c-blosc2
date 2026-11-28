/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#undef NDEBUG
#include "b2nd_exec.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static b2nd_array_t *make_1d_f64(const double *data, int64_t n, int32_t chunk, int32_t block) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(double);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams};
  int64_t shape[1] = {n};
  int32_t chunkshape[1] = {chunk};
  int32_t blockshape[1] = {block};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape,
                                        "<f8", DTYPE_NUMPY_FORMAT, NULL, 0);
  assert(ctx != NULL);
  b2nd_array_t *arr = NULL;
  assert(b2nd_from_cbuffer(ctx, &arr, data, n * (int64_t)sizeof(double)) == BLOSC2_ERROR_SUCCESS);
  b2nd_free_ctx(ctx);
  return arr;
}

static b2nd_array_t *make_2d_i32(const int32_t *data, int64_t r, int64_t c) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams};
  int64_t shape[2] = {r, c};
  int32_t chunkshape[2] = {(int32_t)r, (int32_t)c};
  int32_t blockshape[2] = {(int32_t)r, (int32_t)c};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                        "<i4", DTYPE_NUMPY_FORMAT, NULL, 0);
  assert(ctx != NULL);
  b2nd_array_t *arr = NULL;
  assert(b2nd_from_cbuffer(ctx, &arr, data, r * c * (int64_t)sizeof(int32_t)) ==
         BLOSC2_ERROR_SUCCESS);
  b2nd_free_ctx(ctx);
  return arr;
}

static void test_compression_only_stubs(void) {
  assert(!b2nd_exec_available());

  b2nd_exec_error_t err = B2ND_EXEC_ERROR_INITIALIZER;
  b2nd_plan_t *plan = (b2nd_plan_t *)(uintptr_t)0x1;
  b2nd_schedule_t *sched = (b2nd_schedule_t *)(uintptr_t)0x1;
  b2nd_array_t *out = (b2nd_array_t *)(uintptr_t)0x1;
  bool admitted = true;

  assert(b2nd_plan_from_expr("x + 1", NULL, 0, NULL, NULL, &plan, &err) ==
         B2ND_EXEC_ERR_UNSUPPORTED);
  assert(plan == NULL);
  assert(err.status == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(err.message[0] != '\0');

  /* Truncated error struct_size must not be written past its boundary. */
  b2nd_exec_error_t trunc_err = B2ND_EXEC_ERROR_INITIALIZER;
  trunc_err.struct_size = B2ND_EXEC_ERROR_V1_SIZE - 1;
  trunc_err.status = B2ND_EXEC_SUCCESS;
  assert(b2nd_eval("x + 1", NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, &out,
                   NULL, &trunc_err) == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(out == NULL);
  assert(trunc_err.status == B2ND_EXEC_SUCCESS);

  assert(b2nd_plan_from_graph(NULL, NULL, &plan, &err) == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(b2nd_plan_compose(NULL, 0, 0, &plan, &err) == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(b2nd_plan_describe(NULL, NULL) == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(b2nd_plan_describe_stage(NULL, 0, NULL, NULL, 0, NULL, NULL) ==
         B2ND_EXEC_ERR_UNSUPPORTED);
  assert(b2nd_schedule_create(NULL, NULL, 0, NULL, &sched, &err) ==
         B2ND_EXEC_ERR_UNSUPPORTED);
  assert(sched == NULL);
  assert(b2nd_schedule_describe(NULL, NULL, NULL, 0) == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(b2nd_schedule_admit(NULL, NULL, 0, NULL, &admitted, &err) ==
         B2ND_EXEC_ERR_UNSUPPORTED);
  assert(!admitted);
  assert(b2nd_execute(NULL, NULL, 0, NULL, NULL, NULL, &err) == B2ND_EXEC_ERR_UNSUPPORTED);
  assert(b2nd_eval_into("x", NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, &err) ==
         B2ND_EXEC_ERR_UNSUPPORTED);
  b2nd_schedule_free(NULL);
  b2nd_plan_free(NULL);
}

typedef struct {
  const double *data;
  size_t calls;
  size_t items;
  size_t fail_at;
} test_cb_source_state;

typedef struct {
  double *data;
  size_t calls;
  size_t items;
} test_cb_sink_state;

static b2nd_exec_status_t test_cb_read(void *context, const size_t *indices, size_t count,
                                       void *dst, size_t dst_capacity,
                                       b2nd_exec_error_t *error) {
  test_cb_source_state *st = context;
  st->calls++;
  if (st->fail_at && st->calls == st->fail_at) {
    if (error) {
      error->status = B2ND_EXEC_ERR_SOURCE;
      snprintf(error->message, sizeof(error->message), "simulated source error");
    }
    return B2ND_EXEC_ERR_SOURCE;
  }
  assert(dst_capacity >= count * sizeof(double));
  double *out = dst;
  for (size_t i = 0; i < count; i++) {
    out[i] = st->data[indices[i]];
  }
  st->items += count;
  return B2ND_EXEC_SUCCESS;
}

static b2nd_exec_status_t test_cb_write(void *context, size_t offset_items, size_t count,
                                        const void *src, size_t src_capacity,
                                        b2nd_exec_error_t *error) {
  (void)error;
  test_cb_sink_state *st = context;
  assert(src_capacity >= count * sizeof(double));
  memcpy(st->data + offset_items, src, count * sizeof(double));
  st->calls++;
  st->items += count;
  return B2ND_EXEC_SUCCESS;
}

static void test_eval_and_reductions(void) {
  double x_vals[10], y_vals[10];
  for (int i = 0; i < 10; i++) {
    x_vals[i] = (double)(i + 1) * 0.25;
    y_vals[i] = (double)(10 - i);
  }
  b2nd_array_t *x = make_1d_f64(x_vals, 10, 4, 2);
  b2nd_array_t *y = make_1d_f64(y_vals, 10, 4, 2);

  /* 1. One-shot b2nd_eval with out_ctx == NULL */
  const char *names[2] = {"x", "y"};
  const b2nd_array_t *inputs[2] = {x, y};
  b2nd_array_t *out = NULL;
  b2nd_exec_report_t report = B2ND_EXEC_REPORT_INITIALIZER;
  b2nd_exec_error_t err = B2ND_EXEC_ERROR_INITIALIZER;
  assert(b2nd_eval("sin(x) * 2.5 + y", names, inputs, 2,
                   NULL, NULL, NULL, NULL, NULL, &out, &report, &err) == B2ND_EXEC_SUCCESS);
  assert(out != NULL && out->ndim == 1 && out->shape[0] == 10);
  double got[10];
  assert(b2nd_to_cbuffer(out, got, sizeof(got)) == BLOSC2_ERROR_SUCCESS);
  for (int i = 0; i < 10; i++) {
    double expected = sin(x_vals[i]) * 2.5 + y_vals[i];
    assert(fabs(got[i] - expected) < 1e-12);
  }
  assert(report.decodes > 0);
  b2nd_free(out);
  out = NULL;

  /* 2. Menudet 0.2 auto-detected with capture */
  double scale = 3.0;
  b2nd_exec_capture_t cap = {{"scale", B2ND_DTYPE_FLOAT64, 0}, &scale, B2ND_CAPTURE_TYPED};
  b2nd_expr_options_t xopts = B2ND_EXPR_OPTIONS_INITIALIZER;
  xopts.captures = &cap;
  xopts.ncaptures = 1;
  const char *md_src =
      "def kernel(x, scale):\n"
      "    return x * scale + 1.0\n";
  assert(b2nd_eval(md_src, names, inputs, 1, NULL, &xopts, NULL, NULL, NULL,
                   &out, NULL, &err) == B2ND_EXEC_SUCCESS);
  assert(b2nd_to_cbuffer(out, got, sizeof(got)) == BLOSC2_ERROR_SUCCESS);
  for (int i = 0; i < 10; i++) {
    assert(fabs(got[i] - (x_vals[i] * 3.0 + 1.0)) < 1e-12);
  }
  b2nd_free(out);
  out = NULL;

  /* 3. Reductions via b2nd_eval (SUM with accumulator, ARGMAX, STD ddof=1, masked MIN) */
  int32_t m_vals[6] = {1, 5, 2, 9, 3, 4};
  b2nd_array_t *m = make_2d_i32(m_vals, 2, 3);
  const char *m_names[1] = {"m"};
  const b2nd_array_t *m_inputs[1] = {m};

  b2nd_expr_options_t sum_opts = B2ND_EXPR_OPTIONS_INITIALIZER;
  sum_opts.reduce.reduction = B2ND_REDUCTION_SUM;
  sum_opts.reduce.has_axes = true;
  sum_opts.reduce.naxes = 1;
  sum_opts.reduce.axes[0] = 0;
  sum_opts.reduce.keepdims = true;
  sum_opts.reduce.accumulator = B2ND_DTYPE_FLOAT64;
  assert(b2nd_eval("m", m_names, m_inputs, 1, NULL, &sum_opts, NULL, NULL, NULL,
                   &out, NULL, &err) == B2ND_EXEC_SUCCESS);
  assert(out->ndim == 2 && out->shape[0] == 1 && out->shape[1] == 3);
  double sum_col[3];
  assert(b2nd_to_cbuffer(out, sum_col, sizeof(sum_col)) == BLOSC2_ERROR_SUCCESS);
  assert(sum_col[0] == 10.0 && sum_col[1] == 8.0 && sum_col[2] == 6.0);
  b2nd_free(out);
  out = NULL;

  b2nd_expr_options_t argmax_opts = B2ND_EXPR_OPTIONS_INITIALIZER;
  argmax_opts.reduce.reduction = B2ND_REDUCTION_ARGMAX;
  assert(b2nd_eval("m", m_names, m_inputs, 1, NULL, &argmax_opts, NULL, NULL, NULL,
                   &out, NULL, &err) == B2ND_EXEC_SUCCESS);
  assert(out->ndim == 0 && out->nitems == 1);
  int64_t argmax_idx = -1;
  assert(b2nd_to_cbuffer(out, &argmax_idx, sizeof(argmax_idx)) == BLOSC2_ERROR_SUCCESS);
  assert(argmax_idx == 3);
  b2nd_free(out);
  out = NULL;

  b2nd_expr_options_t std_opts = B2ND_EXPR_OPTIONS_INITIALIZER;
  std_opts.reduce.reduction = B2ND_REDUCTION_STD;
  std_opts.reduce.ddof = 1.0;
  assert(b2nd_eval("x", names, inputs, 1, NULL, &std_opts, NULL, NULL, NULL,
                   &out, NULL, &err) == B2ND_EXEC_SUCCESS);
  assert(out->ndim == 0);
  double std_got = 0.0;
  assert(b2nd_to_cbuffer(out, &std_got, sizeof(std_got)) == BLOSC2_ERROR_SUCCESS);
  double mean = 0.0, ss = 0.0;
  for (int i = 0; i < 10; i++) mean += x_vals[i];
  mean /= 10.0;
  for (int i = 0; i < 10; i++) ss += (x_vals[i] - mean) * (x_vals[i] - mean);
  assert(fabs(std_got - sqrt(ss / 9.0)) < 1e-12);
  b2nd_free(out);
  out = NULL;

  /* 4. b2nd_eval_into: mixed view + array with trailing where mask into view */
  uint8_t mask[10] = {0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
  double min_init = 100.0;
  b2nd_expr_options_t min_opts = B2ND_EXPR_OPTIONS_INITIALIZER;
  min_opts.reduce.reduction = B2ND_REDUCTION_MIN;
  min_opts.reduce.has_initial = true;
  min_opts.reduce.initial_dtype = B2ND_DTYPE_FLOAT64;
  min_opts.reduce.initial_category = B2ND_CAPTURE_TYPED;
  min_opts.reduce.initial_size = sizeof(double);
  memcpy(min_opts.reduce.initial_scalar, &min_init, sizeof(min_init));
  min_opts.reduce.has_where_slot = true;

  b2nd_operand_t where_ops[2] = {B2ND_OPERAND_INITIALIZER, B2ND_OPERAND_INITIALIZER};
  where_ops[0].kind = B2ND_OPERAND_VIEW;
  where_ops[0].as.view.base = x_vals;
  where_ops[0].as.view.capacity = sizeof(x_vals);
  where_ops[0].as.view.strides[0] = sizeof(double);
  where_ops[0].as.view.dtype = B2ND_DTYPE_FLOAT64;
  where_ops[0].as.view.ndim = 1;
  where_ops[0].as.view.shape[0] = 10;

  where_ops[1].kind = B2ND_OPERAND_VIEW;
  where_ops[1].as.view.base = mask;
  where_ops[1].as.view.capacity = sizeof(mask);
  where_ops[1].as.view.strides[0] = 1;
  where_ops[1].as.view.dtype = B2ND_DTYPE_BOOL;
  where_ops[1].as.view.ndim = 1;
  where_ops[1].as.view.shape[0] = 10;

  double min_out = -1.0;
  b2nd_output_t scalar_out = B2ND_OUTPUT_INITIALIZER;
  scalar_out.kind = B2ND_OUTPUT_VIEW;
  scalar_out.as.view.base = &min_out;
  scalar_out.as.view.capacity = sizeof(min_out);
  const char *w_names[2] = {"x", "mask"};
  assert(b2nd_eval_into("x * 2.0", w_names, where_ops, 2, NULL, &min_opts,
                        NULL, NULL, &scalar_out, NULL, &err) == B2ND_EXEC_SUCCESS);
  assert(fabs(min_out - (x_vals[1] * 2.0)) < 1e-12);

  /* 5. Callback source -> callback sink + error propagation */
  test_cb_source_state src_st = {x_vals, 0, 0, 0};
  double sink_buf[10] = {0};
  test_cb_sink_state sink_st = {sink_buf, 0, 0};
  b2nd_operand_t cb_in = B2ND_OPERAND_INITIALIZER;
  cb_in.kind = B2ND_OPERAND_SOURCE;
  cb_in.as.source.dtype = B2ND_DTYPE_FLOAT64;
  cb_in.as.source.itemsize = sizeof(double);
  cb_in.as.source.ndim = 1;
  cb_in.as.source.shape[0] = 10;
  cb_in.as.source.context = &src_st;
  cb_in.as.source.read = test_cb_read;

  b2nd_output_t cb_out = B2ND_OUTPUT_INITIALIZER;
  cb_out.kind = B2ND_OUTPUT_SINK;
  cb_out.as.sink.dtype = B2ND_DTYPE_FLOAT64;
  cb_out.as.sink.itemsize = sizeof(double);
  cb_out.as.sink.ndim = 1;
  cb_out.as.sink.shape[0] = 10;
  cb_out.as.sink.context = &sink_st;
  cb_out.as.sink.write = test_cb_write;

  b2nd_schedule_options_t sopts = B2ND_SCHEDULE_OPTIONS_INITIALIZER;
  sopts.tile_items = 4;
  assert(b2nd_eval_into("x + 5.0", names, &cb_in, 1, NULL, NULL, &sopts, NULL,
                        &cb_out, &report, &err) == B2ND_EXEC_SUCCESS);
  assert(src_st.calls == 3 && src_st.items == 10);
  assert(sink_st.calls == 3 && sink_st.items == 10);
  assert(report.read_items == 10 && report.write_items == 10);
  for (int i = 0; i < 10; i++) {
    assert(fabs(sink_buf[i] - (x_vals[i] + 5.0)) < 1e-12);
  }

  src_st = (test_cb_source_state){x_vals, 0, 0, 2};
  assert(b2nd_eval_into("x + 5.0", names, &cb_in, 1, NULL, NULL, &sopts, NULL,
                        &cb_out, NULL, &err) == B2ND_EXEC_ERR_SOURCE);
  assert(strstr(err.message, "simulated source error") != NULL);

  b2nd_free(m);
  b2nd_free(x);
  b2nd_free(y);
}

static void test_graph_compose_lifetimes_and_abi(void) {
  b2nd_exec_error_t err = B2ND_EXEC_ERROR_INITIALIZER;

  /* 1. Auto-staged graph: x - sum(x) */
  b2nd_exec_node_t nodes[3] = {
      B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER};
  nodes[0].id = 0;
  nodes[0].op = B2ND_OP_INPUT;
  nodes[0].dtype = B2ND_DTYPE_FLOAT64;
  nodes[0].itemsize = sizeof(double);
  nodes[1].id = 1;
  nodes[1].op = B2ND_OP_SUM;
  nodes[1].nargs = 1;
  nodes[1].args[0] = 0;
  nodes[2].id = 2;
  nodes[2].op = B2ND_OP_SUBTRACT;
  nodes[2].nargs = 2;
  nodes[2].args[0] = 0;
  nodes[2].args[1] = 1;
  b2nd_exec_graph_t graph = B2ND_EXEC_GRAPH_INITIALIZER;
  graph.nodes = nodes;
  graph.nnodes = 3;
  graph.ninputs = 1;
  graph.root = 2;

  b2nd_plan_t *gplan = NULL;
  assert(b2nd_plan_from_graph(&graph, NULL, &gplan, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_info_t pinfo = B2ND_PLAN_INFO_INITIALIZER;
  assert(b2nd_plan_describe(gplan, &pinfo) == B2ND_EXEC_SUCCESS);
  assert(pinfo.nstages == 2 && pinfo.dtype == B2ND_DTYPE_FLOAT64);

  int s_src[8];
  size_t s_n = 0;
  uint32_t s_node = UINT32_MAX;
  b2nd_plan_info_t stage0_info = B2ND_PLAN_INFO_INITIALIZER;
  assert(b2nd_plan_describe_stage(gplan, 0, &stage0_info, s_src, 8, &s_n, &s_node) ==
         B2ND_EXEC_SUCCESS);
  assert(s_n == 1 && s_src[0] == -1 && s_node == 1 && stage0_info.reducer == B2ND_REDUCTION_SUM);

  b2nd_exec_input_meta_t imeta = B2ND_EXEC_INPUT_META_INITIALIZER;
  imeta.dtype = B2ND_DTYPE_FLOAT64;
  imeta.itemsize = sizeof(double);
  imeta.ndim = 1;
  imeta.shape[0] = 6;
  b2nd_schedule_t *gsched = NULL;
  assert(b2nd_schedule_create(gplan, &imeta, 1, NULL, &gsched, &err) == B2ND_EXEC_SUCCESS);

  /* Refcounted safety: free the plan BEFORE describing/executing the schedule! */
  b2nd_plan_free(gplan);

  b2nd_schedule_info_t sinfo = B2ND_SCHEDULE_INFO_INITIALIZER;
  b2nd_stage_info_t stage_infos[2] = {B2ND_STAGE_INFO_INITIALIZER, B2ND_STAGE_INFO_INITIALIZER};
  assert(b2nd_schedule_describe(gsched, &sinfo, stage_infos, 2) == B2ND_EXEC_SUCCESS);
  assert(sinfo.nstages == 2 && sinfo.intermediate_bytes == sizeof(double));
  assert(stage_infos[0].last_consumer == 1);

  double in_buf[6] = {1, 2, 3, 4, 5, 6};
  double out_buf[6] = {0};
  b2nd_operand_t op_in = B2ND_OPERAND_INITIALIZER;
  op_in.kind = B2ND_OPERAND_VIEW;
  op_in.as.view.base = in_buf;
  op_in.as.view.capacity = sizeof(in_buf);
  op_in.as.view.strides[0] = sizeof(double);
  b2nd_output_t op_out = B2ND_OUTPUT_INITIALIZER;
  op_out.kind = B2ND_OUTPUT_VIEW;
  op_out.as.view.base = out_buf;
  op_out.as.view.capacity = sizeof(out_buf);
  op_out.as.view.strides[0] = sizeof(double);

  bool admitted = false;
  assert(b2nd_schedule_admit(gsched, &op_in, 1, &op_out, &admitted, &err) == B2ND_EXEC_SUCCESS);
  assert(admitted);

  b2nd_stage_report_t s_reps[2] = {B2ND_STAGE_REPORT_INITIALIZER, B2ND_STAGE_REPORT_INITIALIZER};
  b2nd_exec_report_t rep = B2ND_EXEC_REPORT_INITIALIZER;
  rep.stage_reports = s_reps;
  rep.stage_reports_cap = 2;
  assert(b2nd_execute(gsched, &op_in, 1, &op_out, NULL, &rep, &err) == B2ND_EXEC_SUCCESS);
  assert(rep.entered_stages == 2 && s_reps[0].tiles > 0 && s_reps[1].tiles > 0);
  for (int i = 0; i < 6; i++) {
    assert(out_buf[i] == in_buf[i] - 21.0);
  }
  b2nd_schedule_free(gsched);

  /* Shared computed node splits automatically; shared scalar computation rejects with node id. */
  b2nd_exec_node_t shared_nodes[3] = {
      B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER};
  shared_nodes[0].id = 0;
  shared_nodes[0].op = B2ND_OP_INPUT;
  shared_nodes[0].dtype = B2ND_DTYPE_FLOAT32;
  shared_nodes[0].itemsize = sizeof(float);
  shared_nodes[1].id = 1;
  shared_nodes[1].op = B2ND_OP_FUNCTION;
  snprintf(shared_nodes[1].function, sizeof(shared_nodes[1].function), "sqrt");
  shared_nodes[1].nargs = 1;
  shared_nodes[1].args[0] = 0;
  shared_nodes[2].id = 2;
  shared_nodes[2].op = B2ND_OP_ADD;
  shared_nodes[2].nargs = 2;
  shared_nodes[2].args[0] = 1;
  shared_nodes[2].args[1] = 1;
  b2nd_exec_graph_t shared_graph = B2ND_EXEC_GRAPH_INITIALIZER;
  shared_graph.nodes = shared_nodes;
  shared_graph.nnodes = 3;
  shared_graph.ninputs = 1;
  shared_graph.root = 2;
  b2nd_plan_t *shared_plan = NULL;
  assert(b2nd_plan_from_graph(&shared_graph, NULL, &shared_plan, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_info_t shared_info = B2ND_PLAN_INFO_INITIALIZER;
  assert(b2nd_plan_describe(shared_plan, &shared_info) == B2ND_EXEC_SUCCESS);
  assert(shared_info.nstages == 2);
  b2nd_plan_free(shared_plan);

  b2nd_exec_node_t scalar_nodes[6] = {
      B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER,
      B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER, B2ND_EXEC_NODE_INITIALIZER};
  int64_t one = 1, two = 2;
  scalar_nodes[0].id = 0;
  scalar_nodes[0].op = B2ND_OP_INPUT;
  scalar_nodes[0].dtype = B2ND_DTYPE_FLOAT32;
  scalar_nodes[0].itemsize = sizeof(float);
  scalar_nodes[1].id = 1;
  scalar_nodes[1].op = B2ND_OP_CAPTURE;
  scalar_nodes[1].dtype = B2ND_DTYPE_INT64;
  scalar_nodes[1].itemsize = sizeof(one);
  scalar_nodes[1].category = B2ND_CAPTURE_WEAK;
  scalar_nodes[1].scalar_size = sizeof(one);
  memcpy(scalar_nodes[1].scalar, &one, sizeof(one));
  scalar_nodes[2].id = 2;
  scalar_nodes[2].op = B2ND_OP_CAPTURE;
  scalar_nodes[2].dtype = B2ND_DTYPE_INT64;
  scalar_nodes[2].itemsize = sizeof(two);
  scalar_nodes[2].category = B2ND_CAPTURE_WEAK;
  scalar_nodes[2].scalar_size = sizeof(two);
  memcpy(scalar_nodes[2].scalar, &two, sizeof(two));
  scalar_nodes[3].id = 3;
  scalar_nodes[3].op = B2ND_OP_ADD;
  scalar_nodes[3].nargs = 2;
  scalar_nodes[3].args[0] = 1;
  scalar_nodes[3].args[1] = 2;
  scalar_nodes[4].id = 4;
  scalar_nodes[4].op = B2ND_OP_ADD;
  scalar_nodes[4].nargs = 2;
  scalar_nodes[4].args[0] = 3;
  scalar_nodes[4].args[1] = 3;
  scalar_nodes[5].id = 5;
  scalar_nodes[5].op = B2ND_OP_ADD;
  scalar_nodes[5].nargs = 2;
  scalar_nodes[5].args[0] = 0;
  scalar_nodes[5].args[1] = 4;
  b2nd_exec_graph_t scalar_graph = B2ND_EXEC_GRAPH_INITIALIZER;
  scalar_graph.nodes = scalar_nodes;
  scalar_graph.nnodes = 6;
  scalar_graph.ninputs = 1;
  scalar_graph.root = 5;
  assert(b2nd_plan_from_graph(&scalar_graph, NULL, &shared_plan, &err) ==
         B2ND_EXEC_ERR_UNSUPPORTED);
  assert(err.node == 3 && shared_plan == NULL);

  /* 2. Explicit composition + flattening nested staged child + child freed before parent */
  b2nd_exec_input_spec_t xspec = {"x", B2ND_DTYPE_FLOAT64, 0};
  b2nd_exec_input_spec_t abspec[2] = {{"a", B2ND_DTYPE_FLOAT64, 0},
                                      {"b", B2ND_DTYPE_FLOAT64, 0}};
  b2nd_plan_t *p0 = NULL, *p1 = NULL, *composed = NULL, *outer = NULL;
  assert(b2nd_plan_from_expr("x * 2.0", &xspec, 1, NULL, NULL, &p0, &err) == B2ND_EXEC_SUCCESS);
  assert(b2nd_plan_from_expr("a + b", abspec, 2, NULL, NULL, &p1, &err) == B2ND_EXEC_SUCCESS);

  int b0[1] = {-1};
  int b1[2] = {0, -1};
  b2nd_stage_binding_t binds[2] = {B2ND_STAGE_BINDING_INITIALIZER, B2ND_STAGE_BINDING_INITIALIZER};
  binds[0].plan = p0;
  binds[0].sources = b0;
  binds[0].node = 10;
  binds[1].plan = p1;
  binds[1].sources = b1;
  binds[1].node = 11;
  assert(b2nd_plan_compose(binds, 2, 1, &composed, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_free(p0);
  b2nd_plan_free(p1);

  /* Now compose a staged child (`composed`) with another stage (`p2`) and free `composed`! */
  b2nd_plan_t *p2 = NULL;
  assert(b2nd_plan_from_expr("x + 1.0", &xspec, 1, NULL, NULL, &p2, &err) == B2ND_EXEC_SUCCESS);
  b2nd_stage_binding_t obinds[2] = {B2ND_STAGE_BINDING_INITIALIZER, B2ND_STAGE_BINDING_INITIALIZER};
  obinds[0].plan = composed;
  obinds[0].sources = b0;
  obinds[0].node = 20;
  int ob1[1] = {0};
  obinds[1].plan = p2;
  obinds[1].sources = ob1;
  obinds[1].node = 21;
  assert(b2nd_plan_compose(obinds, 2, 1, &outer, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_free(composed);
  b2nd_plan_free(p2);

  assert(b2nd_schedule_create(outer, &imeta, 1, NULL, &gsched, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_free(outer);
  assert(b2nd_execute(gsched, &op_in, 1, &op_out, NULL, &rep, &err) == B2ND_EXEC_SUCCESS);
  for (int i = 0; i < 6; i++) {
    assert(out_buf[i] == (in_buf[i] * 2.0 + in_buf[i]) + 1.0);
  }
  b2nd_schedule_free(gsched);

  /* 3. Transactional alias + explicit zero worker scratch rejection */
  b2nd_plan_t *map_plan = NULL;
  assert(b2nd_plan_from_expr("x * 2.0", &xspec, 1, NULL, NULL, &map_plan, &err) ==
         B2ND_EXEC_SUCCESS);
  assert(b2nd_schedule_create(map_plan, &imeta, 1, NULL, &gsched, &err) == B2ND_EXEC_SUCCESS);

  double alias_buf[6] = {1, 2, 3, 4, 5, 6};
  b2nd_operand_t a_in = B2ND_OPERAND_INITIALIZER;
  a_in.kind = B2ND_OPERAND_VIEW;
  a_in.as.view.base = alias_buf;
  a_in.as.view.capacity = sizeof(alias_buf);
  a_in.as.view.strides[0] = sizeof(double);

  b2nd_output_t a_out = B2ND_OUTPUT_INITIALIZER;
  a_out.kind = B2ND_OUTPUT_VIEW;
  a_out.as.view.base = alias_buf;
  a_out.as.view.capacity = sizeof(alias_buf);
  a_out.as.view.strides[0] = sizeof(double);
  a_out.as.view.allow_alias = false;
  assert(b2nd_execute(gsched, &a_in, 1, &a_out, NULL, NULL, &err) == B2ND_EXEC_ERR_UNSUPPORTED);

  a_out.as.view.allow_alias = true;
  a_out.as.view.staging_budget = sizeof(alias_buf);
  assert(b2nd_execute(gsched, &a_in, 1, &a_out, NULL, &rep, &err) == B2ND_EXEC_SUCCESS);
  assert(rep.staging_bytes == sizeof(alias_buf));
  for (int i = 0; i < 6; i++) {
    assert(alias_buf[i] == (double)(i + 1) * 2.0);
  }

  /* Explicit max_worker_scratch == 0 with gather scratch required must reject cleanly. */
  b2nd_operand_t rev_in = B2ND_OPERAND_INITIALIZER;
  rev_in.kind = B2ND_OPERAND_VIEW;
  rev_in.as.view.base = in_buf;
  rev_in.as.view.capacity = sizeof(in_buf);
  rev_in.as.view.offset = 5 * sizeof(double);
  rev_in.as.view.strides[0] = -(int64_t)sizeof(double);
  b2nd_run_options_t zero_scratch = B2ND_RUN_OPTIONS_INITIALIZER;
  zero_scratch.workers = 2;
  zero_scratch.has_worker_scratch = true;
  zero_scratch.max_worker_scratch = 0;
  assert(b2nd_execute(gsched, &rev_in, 1, &op_out, &zero_scratch, NULL, &err) ==
         B2ND_EXEC_ERR_INVALID_ARG);

  /* Logical partition schedule */
  b2nd_partition_spec_t pspec = B2ND_PARTITION_SPEC_INITIALIZER;
  pspec.ndim = 1;
  pspec.domain[0] = 6;
  pspec.partitions[0] = 3;
  b2nd_schedule_options_t psopts = B2ND_SCHEDULE_OPTIONS_INITIALIZER;
  psopts.partition_spec = &pspec;
  b2nd_schedule_t *psched = NULL;
  assert(b2nd_schedule_create(map_plan, &imeta, 1, &psopts, &psched, &err) == B2ND_EXEC_SUCCESS);
  assert(b2nd_schedule_admit(psched, &op_in, 1, &op_out, &admitted, &err) == B2ND_EXEC_SUCCESS);
  assert(admitted);
  assert(b2nd_execute(psched, &op_in, 1, &op_out, NULL, &rep, &err) == B2ND_EXEC_SUCCESS);
  for (int i = 0; i < 6; i++) {
    assert(out_buf[i] == in_buf[i] * 2.0);
  }
  b2nd_schedule_free(psched);
  b2nd_schedule_free(gsched);

  /* 4. ABI struct_size / V1_SIZE tests: truncated fails, exact V1 succeeds, oversized preserves tail */
  b2nd_exec_options_t bad_opts = B2ND_EXEC_OPTIONS_INITIALIZER;
  bad_opts.struct_size = B2ND_EXEC_OPTIONS_V1_SIZE - 1;
  b2nd_plan_t *bad_plan = NULL;
  assert(b2nd_plan_from_expr("x", &xspec, 1, &bad_opts, NULL, &bad_plan, &err) ==
         B2ND_EXEC_ERR_INVALID_ARG);

  b2nd_exec_options_t v1_opts = B2ND_EXEC_OPTIONS_INITIALIZER;
  v1_opts.struct_size = B2ND_EXEC_OPTIONS_V1_SIZE;
  b2nd_plan_t *v1_plan = NULL;
  assert(b2nd_plan_from_expr("x", &xspec, 1, &v1_opts, NULL, &v1_plan, &err) ==
         B2ND_EXEC_SUCCESS);

  struct {
    b2nd_plan_info_t v1;
    uint64_t trailing_sentinel;
  } ext_info;
  ext_info.v1 = (b2nd_plan_info_t)B2ND_PLAN_INFO_INITIALIZER;
  ext_info.v1.struct_size = sizeof(ext_info);
  ext_info.trailing_sentinel = UINT64_C(0x123456789abcdef0);
  assert(b2nd_plan_describe(v1_plan, &ext_info.v1) == B2ND_EXEC_SUCCESS);
  assert(ext_info.v1.struct_size == sizeof(ext_info));
  assert(ext_info.v1.dtype == B2ND_DTYPE_FLOAT64);
  assert(ext_info.trailing_sentinel == UINT64_C(0x123456789abcdef0));

  b2nd_plan_free(v1_plan);
  b2nd_plan_free(map_plan);
}

int main(void) {
  blosc2_init();
  if (!b2nd_exec_available()) {
    test_compression_only_stubs();
  } else {
    test_eval_and_reductions();
    test_graph_compose_lifetimes_and_abi();
  }
  blosc2_destroy();
  return 0;
}

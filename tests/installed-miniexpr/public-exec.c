#undef NDEBUG
#include <b2nd_exec.h>

#include <assert.h>
#include <math.h>
#include <stdint.h>

int main(void) {
  blosc2_init();
  assert(b2nd_exec_available());

  double x_vals[6] = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
  double y_vals[6] = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0};
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = sizeof(double);
  blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {.cparams = &cp, .dparams = &dp};
  int64_t shape[1] = {6};
  int32_t chunkshape[1] = {3};
  int32_t blockshape[1] = {2};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape,
                                        "<f8", DTYPE_NUMPY_FORMAT, NULL, 0);
  assert(ctx != NULL);
  b2nd_array_t *x = NULL, *y = NULL, *out = NULL;
  assert(b2nd_from_cbuffer(ctx, &x, x_vals, sizeof(x_vals)) == BLOSC2_ERROR_SUCCESS);
  assert(b2nd_from_cbuffer(ctx, &y, y_vals, sizeof(y_vals)) == BLOSC2_ERROR_SUCCESS);
  b2nd_free_ctx(ctx);

  const char *names[2] = {"x", "y"};
  const b2nd_array_t *inputs[2] = {x, y};
  b2nd_exec_error_t err = B2ND_EXEC_ERROR_INITIALIZER;
  assert(b2nd_eval("sin(x) * 2.5 + y", names, inputs, 2,
                   NULL, NULL, NULL, NULL, NULL, &out, NULL, &err) == B2ND_EXEC_SUCCESS);
  double got[6];
  assert(b2nd_to_cbuffer(out, got, sizeof(got)) == BLOSC2_ERROR_SUCCESS);
  for (int i = 0; i < 6; i++) {
    assert(fabs(got[i] - (sin(x_vals[i]) * 2.5 + y_vals[i])) < 1e-12);
  }
  b2nd_free(out);
  b2nd_free(x);
  b2nd_free(y);

  /* Refcounted plan/schedule lifetime and V1_SIZE contract without miniexpr headers */
  b2nd_exec_input_spec_t spec = {"v", B2ND_DTYPE_FLOAT64, 0};
  b2nd_plan_t *plan = NULL;
  b2nd_exec_options_t eopts = B2ND_EXEC_OPTIONS_INITIALIZER;
  eopts.struct_size = B2ND_EXEC_OPTIONS_V1_SIZE;
  assert(b2nd_plan_from_expr("v * 3.0 + 1.0", &spec, 1, &eopts, NULL, &plan, &err) ==
         B2ND_EXEC_SUCCESS);

  b2nd_exec_input_meta_t meta = B2ND_EXEC_INPUT_META_INITIALIZER;
  meta.dtype = B2ND_DTYPE_FLOAT64;
  meta.itemsize = sizeof(double);
  meta.ndim = 1;
  meta.shape[0] = 6;
  b2nd_schedule_t *sched = NULL;
  assert(b2nd_schedule_create(plan, &meta, 1, NULL, &sched, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_free(plan);

  b2nd_operand_t in_op = B2ND_OPERAND_INITIALIZER;
  in_op.kind = B2ND_OPERAND_VIEW;
  in_op.as.view.base = x_vals;
  in_op.as.view.capacity = sizeof(x_vals);
  in_op.as.view.strides[0] = sizeof(double);

  b2nd_output_t out_op = B2ND_OUTPUT_INITIALIZER;
  out_op.kind = B2ND_OUTPUT_VIEW;
  out_op.as.view.base = got;
  out_op.as.view.capacity = sizeof(got);
  out_op.as.view.strides[0] = sizeof(double);

  assert(b2nd_execute(sched, &in_op, 1, &out_op, NULL, NULL, &err) == B2ND_EXEC_SUCCESS);
  for (int i = 0; i < 6; i++) {
    assert(fabs(got[i] - (x_vals[i] * 3.0 + 1.0)) < 1e-12);
  }
  b2nd_schedule_free(sched);
  blosc2_destroy();
  return 0;
}

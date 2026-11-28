#undef NDEBUG
#include <b2nd_exec_kernel.h>

#include <assert.h>
#include <math.h>
#include <string.h>

int main(void) {
  blosc2_init();
  b2nd_exec_error_t err = B2ND_EXEC_ERROR_INITIALIZER;

  me_input_spec in_spec = {"x", ME_FLOAT64, sizeof(double)};
  me_signature sig = ME_SIGNATURE_INITIALIZER;
  sig.inputs = &in_spec;
  sig.ninputs = 1;
  sig.output_dtype = ME_FLOAT64;
  sig.cardinality = ME_ELEMENTWISE;

  me_kernel *owned = NULL;
  assert(me_prepare_menudet("0.2", "def f(x):\n    return x * 2.0 + 1.0\n",
                            &sig, NULL, &owned, NULL) == ME_SUCCESS);

  /* Failure on take_ownership=true must leave caller's kernel pointer untouched. */
  b2nd_exec_reduce_opts_t bad_ropts = B2ND_EXEC_REDUCE_OPTS_INITIALIZER;
  bad_ropts.reduction = B2ND_REDUCTION_VAR;
  bad_ropts.ddof = -1.0;
  b2nd_plan_t *plan = NULL;
  assert(b2nd_plan_from_kernel(&owned, true, &bad_ropts, &plan, &err) ==
         B2ND_EXEC_ERR_INVALID_ARG);
  assert(owned != NULL && plan == NULL);

  /* Success on take_ownership=true transfers ownership and clears caller pointer. */
  assert(b2nd_plan_from_kernel(&owned, true, NULL, &plan, &err) == B2ND_EXEC_SUCCESS);
  assert(owned == NULL && plan != NULL);

  double x[4] = {1.0, 2.0, 3.0, 4.0};
  double out[4] = {0};
  b2nd_exec_input_meta_t meta = B2ND_EXEC_INPUT_META_INITIALIZER;
  meta.dtype = B2ND_DTYPE_FLOAT64;
  meta.itemsize = sizeof(double);
  meta.ndim = 1;
  meta.shape[0] = 4;

  b2nd_schedule_t *sched = NULL;
  assert(b2nd_schedule_create(plan, &meta, 1, NULL, &sched, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_free(plan);

  b2nd_operand_t in_op = B2ND_OPERAND_INITIALIZER;
  in_op.kind = B2ND_OPERAND_VIEW;
  in_op.as.view.base = x;
  in_op.as.view.capacity = sizeof(x);
  in_op.as.view.strides[0] = sizeof(double);

  b2nd_output_t out_op = B2ND_OUTPUT_INITIALIZER;
  out_op.kind = B2ND_OUTPUT_VIEW;
  out_op.as.view.base = out;
  out_op.as.view.capacity = sizeof(out);
  out_op.as.view.strides[0] = sizeof(double);

  assert(b2nd_execute(sched, &in_op, 1, &out_op, NULL, NULL, &err) == B2ND_EXEC_SUCCESS);
  for (int i = 0; i < 4; i++) {
    assert(fabs(out[i] - (x[i] * 2.0 + 1.0)) < 1e-12);
  }
  b2nd_schedule_free(sched);

  /* Borrowed kernel + masked sum reduction with explicit accumulator */
  me_kernel *borrowed = NULL;
  assert(me_prepare_formula("x", &sig, NULL, &borrowed, NULL) == ME_SUCCESS);
  b2nd_exec_reduce_opts_t ropts = B2ND_EXEC_REDUCE_OPTS_INITIALIZER;
  ropts.reduction = B2ND_REDUCTION_SUM;
  ropts.has_where_slot = true;
  ropts.accumulator = B2ND_DTYPE_FLOAT64;
  assert(b2nd_plan_from_kernel(&borrowed, false, &ropts, &plan, &err) == B2ND_EXEC_SUCCESS);
  assert(borrowed != NULL);

  b2nd_exec_input_meta_t metas[2] = {meta, B2ND_EXEC_INPUT_META_INITIALIZER};
  metas[1].dtype = B2ND_DTYPE_BOOL;
  metas[1].itemsize = 1;
  metas[1].ndim = 1;
  metas[1].shape[0] = 4;
  assert(b2nd_schedule_create(plan, metas, 2, NULL, &sched, &err) == B2ND_EXEC_SUCCESS);
  b2nd_plan_free(plan);

  uint8_t mask[4] = {1, 0, 1, 0};
  b2nd_operand_t ops[2] = {in_op, B2ND_OPERAND_INITIALIZER};
  ops[1].kind = B2ND_OPERAND_VIEW;
  ops[1].as.view.base = mask;
  ops[1].as.view.capacity = sizeof(mask);
  ops[1].as.view.strides[0] = 1;

  double total = 0.0;
  out_op.as.view.base = &total;
  out_op.as.view.capacity = sizeof(total);
  out_op.as.view.strides[0] = 0;
  assert(b2nd_execute(sched, ops, 2, &out_op, NULL, NULL, &err) == B2ND_EXEC_SUCCESS);
  assert(total == 4.0);

  b2nd_schedule_free(sched);
  me_kernel_free(borrowed);
  blosc2_destroy();
  return 0;
}

/* Executor semantics do not depend on submission/completion order or prefixes. */
#undef NDEBUG
#include "b2nd-exec-launch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int mode;
static size_t launches, eval_calls, eval_fail_at;
static me_status fixture_launch(b2nd_exec_run_job run, size_t count, size_t stride,
                                 void *data, me_error *error) {
    launches++;
    if (mode != 2) {
        for (size_t i = count; i-- > 0;) {
            run((unsigned char *)data + i * stride);
            if (mode == 1 || mode == 3) break; /* Only the last logical job ran. */
        }
    }
    me_status status = mode == 1 ? ME_ERR_UNSUPPORTED : mode == 2 ? ME_ERR_OOM : ME_SUCCESS;
    if (status != ME_SUCCESS && error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "Injected launcher failure");
    }
    return status;
}

static me_status fixture_eval(const me_kernel *kernel, const me_buffer *inputs,
                              size_t count, me_mut_buffer output, const me_block *block,
                              const me_run_options *options, me_execution_backend *backend,
                              me_error *error) {
    if (++eval_calls == eval_fail_at) {
        *backend = ME_BACKEND_INTERPRETER;
        *error = (me_error){0};
        error->status = ME_ERR_EVAL;
        snprintf(error->message, sizeof(error->message), "Injected worker failure");
        return ME_ERR_EVAL;
    }
    return me_eval_block_backend(kernel, inputs, count, output, block, options, backend, error);
}

#define B2ND_EXEC_LAUNCH_OVERRIDE fixture_launch
#define me_eval_block_backend fixture_eval
#include "../../blosc/b2nd-exec.c"
#undef me_eval_block_backend

int main(void) {
#ifdef _WIN32
    return 77; /* Windows admission remains serial pending platform qualification. */
#else
    b2nd_exec_node nodes[3] = {0};
    for (size_t i = 0; i < 3; i++) {
        nodes[i].id = (uint32_t)i;
        nodes[i].struct_size = sizeof(*nodes);
    }
    nodes[0].op = B2ND_EXEC_INPUT;
    nodes[0].dtype = ME_INT64;
    nodes[0].itemsize = 8;
    nodes[1].op = B2ND_EXEC_NEGATE;
    nodes[1].nargs = 1;
    nodes[2].op = B2ND_EXEC_MIN;
    nodes[2].nargs = 1;
    nodes[2].args[0] = 1;
    b2nd_exec_descriptor descriptor = {sizeof(descriptor), nodes, 3, 1, 2};
    b2nd_exec_plan *plan = NULL;
    b2nd_exec_schedule *schedule = NULL;
    me_error error;
    assert(b2nd_exec_prepare(&descriptor, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta meta = {ME_INT64, 8, 1, {513}};
    assert(b2nd_exec_specialize(plan, &meta, 1, 16, 128, &schedule, &error) == ME_SUCCESS);
    int64_t input[513], output;
    for (size_t i = 0; i < 513; i++) input[i] = (int64_t)i;
    me_buffer buffer = {input, sizeof(input)};
    b2nd_exec_jobs jobs = {sizeof(jobs), 3, 384};
    b2nd_exec_report report;
    for (mode = 0; mode <= 3; mode++) {
        output = 99;
        launches = eval_calls = 0;
        me_status status = b2nd_exec_execute(schedule, &buffer, 1,
            (me_mut_buffer){&output, sizeof(output)}, 0, &jobs, &report, &error);
        assert(launches == 1); /* Never replay a failed or incomplete launch. */
        if (mode == 0) {
            assert(status == ME_SUCCESS && output == -512);
            assert(report.tasks == 3 && report.tiles == 33);
        } else {
            assert(status == (mode == 2 ? ME_ERR_OOM : ME_ERR_UNSUPPORTED));
            assert(output == 99);
            assert(report.tasks == (mode == 2 ? 0u : 1u));
            assert(report.tiles == (mode == 2 ? 0u : 11u));
        }
    }
    mode = 0;
    launches = eval_calls = 0;
    eval_fail_at = 2;
    output = 99;
    assert(b2nd_exec_execute(schedule, &buffer, 1, (me_mut_buffer){&output, sizeof(output)},
        0, &jobs, &report, &error) == ME_ERR_EVAL);
    assert(output == 99 && report.tasks == 3 && launches == 1);
    assert(report.map_calls == 23 && report.interpreter_calls == 24);
    assert(strstr(error.message, "worker failure"));
    eval_fail_at = 0;
    assert(b2nd_exec_execute(schedule, &buffer, 1, (me_mut_buffer){&output, sizeof(output)},
        0, &jobs, &report, &error) == ME_SUCCESS && output == -512);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);
    return 0;
#endif
}

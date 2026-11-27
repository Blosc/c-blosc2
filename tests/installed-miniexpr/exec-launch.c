/* Actual installed production launcher honors callbacks, including nesting. */
#undef NDEBUG
#include "b2nd-exec.h"
#include "blosc2.h"
#include <assert.h>

#ifndef _WIN32
typedef struct {
    b2nd_exec_schedule *schedule;
    me_buffer input;
    size_t calls;
    int64_t nested_result;
    bool nested;
    bool skip_job;
} fixture;

static void reverse_callback(void *data, void (*run)(void *), int count,
                              size_t stride, void *jobs) {
    fixture *state = data;
    state->calls++;
    assert(count == 4);
    if (!state->nested) {
        state->nested = true;
        b2nd_exec_report report;
        me_error error;
        b2nd_exec_jobs request = {sizeof(request), 4, 256};
        assert(b2nd_exec_execute(state->schedule, &state->input, 1,
            (me_mut_buffer){&state->nested_result, sizeof(state->nested_result)},
            0, &request, &report, &error) == ME_SUCCESS);
        assert(state->calls == 1 && report.tasks == 4);
    }
    for (int i = count; i-- > 0;) {
        if (!state->skip_job || i != 1) run((unsigned char *)jobs + (size_t)i * stride);
    }
}
#endif

int main(void) {
#ifdef _WIN32
    return 77; /* Portable wiring is not Windows numerical qualification. */
#else
    fixture state = {0};
    blosc2_set_threads_callback(reverse_callback, &state);
    blosc2_init();
    b2nd_exec_node nodes[3] = {0};
    for (size_t i = 0; i < 3; i++) {
        nodes[i].struct_size = sizeof(*nodes);
        nodes[i].id = (uint32_t)i;
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
    me_error error;
    assert(b2nd_exec_prepare(&descriptor, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta meta = {ME_INT64, 8, 1, {65}};
    assert(b2nd_exec_specialize(plan, &meta, 1, 8, 64, &state.schedule, &error) == ME_SUCCESS);
    int64_t values[65], result = 99;
    for (size_t i = 0; i < 65; i++) values[i] = (int64_t)i;
    state.input = (me_buffer){values, sizeof(values)};
    b2nd_exec_jobs jobs = {sizeof(jobs), 4, 256};
    b2nd_exec_report report;
    assert(b2nd_exec_execute(state.schedule, &state.input, 1,
        (me_mut_buffer){&result, sizeof(result)}, 0, &jobs, &report, &error) == ME_SUCCESS);
    assert(result == -64 && state.nested_result == -64);
    assert(state.calls == 1 && report.tasks == 4 && report.tiles == 9);
    state.skip_job = true;
    result = 99;
    assert(b2nd_exec_execute(state.schedule, &state.input, 1,
        (me_mut_buffer){&result, sizeof(result)}, 0, &jobs, &report, &error) == ME_ERR_UNSUPPORTED);
    assert(result == 99 && state.calls == 2 && report.tasks == 3 && report.tiles == 7);
    state.skip_job = false;
    values[64] = 1000;
    assert(b2nd_exec_execute(state.schedule, &state.input, 1,
        (me_mut_buffer){&result, sizeof(result)}, 0, &jobs, &report, &error) == ME_SUCCESS);
    assert(result == -1000 && state.calls == 3 && report.tasks == 4);
    b2nd_exec_schedule_free(state.schedule);
    b2nd_exec_plan_free(plan);
    blosc2_destroy();
    return 0;
#endif
}

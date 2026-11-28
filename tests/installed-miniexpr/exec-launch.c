/* Actual installed production launcher honors callbacks, including nesting. */
#undef NDEBUG
#include "b2nd-exec.h"
#include "blosc-parallel.h"
#include "blosc2.h"
#include <assert.h>
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <pthread.h>
typedef struct {
    b2nd_exec_schedule *schedule;
    me_buffer input;
    size_t calls;
    int64_t nested_result;
    bool nested;
    bool skip_job;
} fixture;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t ready;
    int entered;
} probe_gate;

typedef struct {
    pthread_t caller;
    pthread_t seen;
    bool nest;
    int nested_sum;
    probe_gate *gate;
} pool_probe;

static void probe_run(void *data) {
    pool_probe *p = data;
    p->seen = pthread_self();
    if (p->gate != NULL) {
        /* Force all four jobs to overlap: without this, a fast worker may run
         * several jobs and the observed subset of persistent threads can vary. */
        pthread_mutex_lock(&p->gate->mutex);
        p->gate->entered++;
        pthread_cond_broadcast(&p->gate->ready);
        while (p->gate->entered < 4) {
            pthread_cond_wait(&p->gate->ready, &p->gate->mutex);
        }
        pthread_mutex_unlock(&p->gate->mutex);
    }
    if (p->nest) {
        pool_probe sub[3] = {{0}};
        assert(blosc2_run_parallel(3, probe_run, sizeof(*sub), sub) == BLOSC2_ERROR_SUCCESS);
        for (int i = 0; i < 3; i++) {
            assert(pthread_equal(sub[i].seen, p->seen));
            p->nested_sum += 1;
        }
    }
}

typedef struct {
    b2nd_exec_schedule *schedule;
    me_buffer input;
    int64_t out;
} outer_req;

static void *outer_exec(void *data) {
    outer_req *req = data;
    b2nd_exec_jobs jobs = {sizeof(jobs), 4, 256};
    b2nd_exec_report report;
    me_error error;
    assert(b2nd_exec_execute(req->schedule, &req->input, 1,
        (me_mut_buffer){&req->out, sizeof(req->out)}, 0, &jobs, &report, &error) == ME_SUCCESS);
    assert(report.tasks == 4 && report.tiles == 9);
    return NULL;
}

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

    /* Persistent shared-pool dispatch: reused workers, concurrent outer callers,
     * inline nesting on pool workers, and destroy/re-init recovery. */
    blosc2_set_threads_callback(NULL, NULL);
    pool_probe first[4] = {{0}}, second[4] = {{0}};
    probe_gate gate = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0};
    for (int i = 0; i < 4; i++) {
        first[i].gate = &gate;
        second[i].gate = &gate;
    }
    first[1].nest = true;
    assert(blosc2_run_parallel(4, probe_run, sizeof(*first), first) == BLOSC2_ERROR_SUCCESS);
    assert(first[1].nested_sum == 3);
    gate.entered = 0;
    assert(blosc2_run_parallel(4, probe_run, sizeof(*second), second) == BLOSC2_ERROR_SUCCESS);
    pthread_t self = pthread_self();
    assert(pthread_equal(first[0].seen, self) && pthread_equal(second[0].seen, self));
    for (int i = 1; i < 4; i++) {
        assert(!pthread_equal(first[i].seen, self));
        bool matched = false;
        for (int j = 1; j < 4; j++) {
            if (pthread_equal(first[i].seen, second[j].seen)) matched = true;
        }
        assert(matched);
    }
    pthread_cond_destroy(&gate.ready);
    pthread_mutex_destroy(&gate.mutex);

    outer_req reqs[4];
    pthread_t callers[4];
    for (int i = 0; i < 4; i++) {
        reqs[i] = (outer_req){state.schedule, state.input, 99};
        assert(pthread_create(&callers[i], NULL, outer_exec, &reqs[i]) == 0);
    }
    for (int i = 0; i < 4; i++) {
        assert(pthread_join(callers[i], NULL) == 0);
        assert(reqs[i].out == -1000);
    }

    blosc2_destroy();
    blosc2_init();
    result = 99;
    assert(b2nd_exec_execute(state.schedule, &state.input, 1,
        (me_mut_buffer){&result, sizeof(result)}, 0, &jobs, &report, &error) == ME_SUCCESS);
    assert(result == -1000 && report.tasks == 4);

    b2nd_exec_schedule_free(state.schedule);
    b2nd_exec_plan_free(plan);
    blosc2_destroy();
    return 0;
#endif
}

/* Host transport qualification without linking codec/pool internals. */
#undef NDEBUG
#include "b2nd-exec-launch.h"
#include "blosc2.h"
#include <assert.h>
#include <stdlib.h>
#ifndef _WIN32
#include <pthread.h>
typedef struct {
    void (*run)(void *);
    void *data;
} thread_job;
static void *thread_run(void *data) {
    thread_job *job = data;
    job->run(job->data);
    return NULL;
}
#endif

static int mode;
static size_t dispatches;
static bool fail_allocation;
static void *host_malloc(size_t size) {
    return fail_allocation ? NULL : malloc(size);
}

static int mock_parallel(int16_t count, void (*run)(void *), size_t stride, void *data) {
    dispatches++;
    if (mode == 2) return BLOSC2_ERROR_MEMORY_ALLOC;
    for (int i = count; i-- > 0;) {
#ifndef _WIN32
        if (mode == 3) {
            thread_job job = {run, (unsigned char *)data + (size_t)i * stride};
            pthread_t thread;
            assert(pthread_create(&thread, NULL, thread_run, &job) == 0);
            assert(pthread_join(thread, NULL) == 0);
            continue;
        }
#endif
        run((unsigned char *)data + (size_t)i * stride);
        if (mode == 1) return BLOSC2_ERROR_THREAD_CREATE;
    }
    return BLOSC2_ERROR_SUCCESS;
}
#define malloc host_malloc
#define blosc2_run_parallel mock_parallel
#include "../../blosc/b2nd-exec-launch.c"
#undef malloc
#undef blosc2_run_parallel

typedef struct {
    size_t hits;
    bool nest;
} task;

static void run_task(void *data) {
    task *job = data;
    job->hits++;
    if (job->nest) {
        task children[2] = {{0}};
        me_error error;
        assert(b2nd_exec_launch_host(run_task, 2, sizeof(*children), children, &error) == ME_SUCCESS);
        assert(children[0].hits == 1 && children[1].hits == 1);
    }
}

int main(void) {
    for (mode = 0; mode <= 2; mode++) {
        task jobs[3] = {{0}};
        jobs[2].nest = true;
        dispatches = 0;
        me_error error;
        me_status status = b2nd_exec_launch_host(run_task, 3, sizeof(*jobs), jobs, &error);
        assert(dispatches == 1); /* A nested job executes inline, not recursively on the pool. */
        if (!mode) {
            assert(status == ME_SUCCESS && jobs[0].hits == 1 && jobs[2].hits == 1);
        } else {
            assert(status == (mode == 1 ? ME_ERR_UNSUPPORTED : ME_ERR_OOM));
            assert(jobs[0].hits == 0 && jobs[2].hits == (mode == 1 ? 1u : 0u));
        }
    }
    mode = 3;
    task threaded[3] = {{0}};
    threaded[2].nest = true;
    dispatches = 0;
    me_error threaded_error;
    assert(b2nd_exec_launch_host(run_task, 3, sizeof(*threaded), threaded, &threaded_error) == ME_SUCCESS);
    assert(dispatches == 1 && threaded[0].hits == 1 && threaded[2].hits == 1);
    mode = 0;
    task job = {0};
    me_error error;
    fail_allocation = true;
    dispatches = 0;
    assert(b2nd_exec_launch_host(run_task, 1, sizeof(job), &job, &error) == ME_ERR_OOM);
    assert(!dispatches && !job.hits);
    fail_allocation = false;
    assert(b2nd_exec_launch_host(run_task, 1, sizeof(job), &job, &error) == ME_SUCCESS);
    assert(dispatches == 1 && job.hits == 1); /* Failure restored launcher TLS state. */
    assert(b2nd_exec_launch_host(NULL, 1, sizeof(job), &job, &error) == ME_ERR_INVALID_ARG);
    assert(b2nd_exec_launch_host(run_task, 0, sizeof(job), &job, &error) == ME_ERR_INVALID_ARG);
    assert(dispatches == 1);
    return 0;
}

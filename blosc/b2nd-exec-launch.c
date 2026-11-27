/* Production transport integration, deliberately independent of numerical jobs. */
#include "b2nd-exec-launch.h"
#include "blosc-parallel.h"
#include "blosc2.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
static __declspec(thread) unsigned host_launch_depth;
#else
static _Thread_local unsigned host_launch_depth;
#endif

typedef struct {
    b2nd_exec_run_job run;
    void *data;
} host_job;

static void host_run_job(void *data) {
    host_job *job = data;
    host_launch_depth++;
    job->run(job->data);
    host_launch_depth--;
}

me_status b2nd_exec_launch_host(b2nd_exec_run_job run, size_t count, size_t stride,
                               void *jobdata, me_error *error) {
    me_status status = ME_SUCCESS;
    if (!run || !count || count > INT16_MAX || !stride || !jobdata ||
        count > SIZE_MAX / stride) {
        status = ME_ERR_INVALID_ARG;
    } else if (host_launch_depth) {
        /* Choose inline transport before any nested submission. Preserve the
         * executor's logical jobs; never retry a failed pool submission.
         * Also avoids waiting recursively on a bounded host callback pool. */
        for (size_t i = 0; i < count; i++) run((unsigned char *)jobdata + i * stride);
    } else {
        host_job *jobs = malloc(count * sizeof(*jobs));
        if (!jobs) {
            status = ME_ERR_OOM;
        } else {
            for (size_t i = 0; i < count; i++) {
                jobs[i] = (host_job){run, (unsigned char *)jobdata + i * stride};
            }
            host_launch_depth++;
            int rc = blosc2_run_parallel((int16_t)count, host_run_job, sizeof(*jobs), jobs);
            host_launch_depth--;
            if (rc != BLOSC2_ERROR_SUCCESS) {
                status = rc == BLOSC2_ERROR_MEMORY_ALLOC ? ME_ERR_OOM : ME_ERR_UNSUPPORTED;
            }
            free(jobs);
        }
    }
    if (status != ME_SUCCESS && error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s",
            status == ME_ERR_OOM ? "Cannot allocate execution launcher workspace" :
            status == ME_ERR_INVALID_ARG ? "Invalid execution launcher request" :
            "Cannot start all execution jobs");
    }
    return status;
}

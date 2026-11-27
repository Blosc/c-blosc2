/* Internal synchronous job transport; not an installed API. */
#ifndef B2ND_EXEC_LAUNCH_H
#define B2ND_EXEC_LAUNCH_H
#include "miniexpr_kernel.h"

typedef void (*b2nd_exec_run_job)(void *job);

/* Invoke run on jobdata + i * stride at most once for each i < count.
 * Return only after EVERY accepted callback has returned, even on failure.
 * Submission order and completion order need not be logical index order.
 * No scheduling, numerical policy, result combination or retry belongs here.
 * The caller retains all job data and buffers until synchronous return. */
/* Production integration honors Blosc2's preconfigured synchronous callback
 * or persistent shared-pool dispatcher on POSIX.
 * Nested executor launches run their preplanned jobs inline before submission. */
me_status b2nd_exec_launch_host(b2nd_exec_run_job run, size_t count, size_t stride,
                               void *jobdata, me_error *error);
#endif

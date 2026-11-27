#include <stdlib.h>
#include "b2nd-exec-storage.h"
static size_t allocations, allocation_fail_at;
static size_t execution_calls, unsupported_fp_stage = SIZE_MAX, unobserved_fp_stage = SIZE_MAX;
static me_status stage_execute_sources(const b2nd_exec_schedule *schedule,
                                        const b2nd_exec_source *sources,
                                        const b2nd_exec_view *views, size_t ninputs,
                                        me_mut_buffer output, const b2nd_exec_sink *sink,
                                        unsigned raise_mask, size_t max_gather_bytes,
                                        size_t max_output_bytes,
                                        b2nd_exec_callback_report *report, me_error *error) {
    me_status st = b2nd_exec_execute_sources(schedule, sources, views, ninputs, output,
        sink, raise_mask, max_gather_bytes, max_output_bytes, report, error);
    size_t stage = execution_calls++;
    if (stage == unsupported_fp_stage) {
        report->execution.fp_observed = true;
        report->execution.fp.supported = false;
    }
    if (stage == unobserved_fp_stage) {
        report->execution.fp_observed = false;
        report->execution.fp.supported = false;
    }
    return st;
}
static void *stage_malloc(size_t size) {
    return ++allocations == allocation_fail_at ? NULL : malloc(size);
}
static void *stage_calloc(size_t count, size_t size) {
    return ++allocations == allocation_fail_at ? NULL : calloc(count, size);
}
#define malloc stage_malloc
#define calloc stage_calloc
#define b2nd_exec_execute_sources stage_execute_sources
#include "../../blosc/b2nd-exec-stages.c"
#undef malloc
#undef calloc
#undef b2nd_exec_execute_sources
#define STAGE_FAULT_TEST 1
#include "../installed-miniexpr/exec-stages.c"

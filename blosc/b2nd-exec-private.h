/* Invocation-local storage bridge, not a public callback/source ABI. */
#ifndef B2ND_EXEC_PRIVATE_H
#define B2ND_EXEC_PRIVATE_H
#include "b2nd-exec.h"

typedef struct {
    void *context;
    me_status (*read)(void *context, size_t start, size_t count,
                      me_mut_buffer output, me_error *error);
} b2nd_exec_reader;

me_status b2nd_exec_schedule_input(const b2nd_exec_schedule *schedule, size_t slot,
                                  b2nd_exec_input_meta *out);
/* Validate reader/view bindings, budgets and partitionability before allocating
 * per-task storage decode workspaces. Writes the clamped task count to *out_tasks. */
me_status b2nd_exec_plan_readers(const b2nd_exec_schedule *schedule,
                                 const b2nd_exec_view *views,
                                 const bool *is_reader, size_t ninputs,
                                 me_mut_buffer output, size_t max_gather_bytes,
                                 const b2nd_exec_jobs *jobs,
                                 size_t *out_tasks, me_error *error);
/* Readers are serial per-task logical C-order spans laid out as
 * readers[task * reader_stride + slot] (or shared at stride 0 when serial).
 * NULL read selects the corresponding resident view. */
me_status b2nd_exec_execute_readers(const b2nd_exec_schedule *schedule,
                                   const b2nd_exec_view *views,
                                   const b2nd_exec_reader *readers, size_t ninputs,
                                   me_mut_buffer output, unsigned raise_mask,
                                   size_t max_gather_bytes, b2nd_exec_report *report,
                                   me_error *error);
me_status b2nd_exec_execute_readers_ex(const b2nd_exec_schedule *schedule,
                                      const b2nd_exec_view *views,
                                      const b2nd_exec_reader *readers,
                                      size_t reader_stride, size_t reader_tasks,
                                      size_t ninputs, me_mut_buffer output,
                                      unsigned raise_mask, size_t max_gather_bytes,
                                      const b2nd_exec_jobs *jobs,
                                      b2nd_exec_report *report, me_error *error);
#endif

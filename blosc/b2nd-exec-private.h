/* Invocation-local storage bridge, not a public callback/source ABI. */
#ifndef B2ND_EXEC_PRIVATE_H
#define B2ND_EXEC_PRIVATE_H
#include "b2nd-exec.h"

/* Shared descriptor validation; staged admission additionally validates its
 * supported region topology before preparing any child kernels. */
me_status b2nd_exec_validate_descriptor(const b2nd_exec_descriptor *descriptor,
                                       bool allow_stages, me_error *error);

typedef struct {
    void *context;
    me_status (*read)(void *context, size_t group, size_t pos, size_t count,
                      me_mut_buffer output, me_error *error);
} b2nd_exec_reader;

me_status b2nd_exec_schedule_input(const b2nd_exec_schedule *schedule, size_t slot,
                                  b2nd_exec_input_meta *out);
void b2nd_exec_schedule_layout(const b2nd_exec_schedule *schedule, size_t slot,
                               bool *matches_shape_out, bool *suffix_reduction_out,
                               bool reduced_axis_out[B2ND_EXEC_MAX_RANK]);
/* Validate reader/view bindings, budgets and partitionability before allocating
 * per-task storage decode workspaces. Writes the clamped task count to *out_tasks. */
me_status b2nd_exec_plan_readers(const b2nd_exec_schedule *schedule,
                                 const b2nd_exec_view *views,
                                 const bool *is_reader, size_t ninputs,
                                 me_mut_buffer output, size_t max_gather_bytes,
                                 const b2nd_exec_jobs *jobs,
                                 size_t *out_tasks, me_error *error);
/* Stage-owned contiguous native allocations have validated shapes/strides but
 * no address yet. Only the planner may defer these two address checks; all
 * external bindings and tile/gather budgets still pass the common validator. */
me_status b2nd_exec_plan_stage_readers(const b2nd_exec_schedule *schedule,
                                       const b2nd_exec_view *views,
                                       const bool *is_reader, size_t ninputs,
                                       me_mut_buffer output, size_t max_gather_bytes,
                                       const b2nd_exec_jobs *jobs,
                                       size_t *out_tasks, me_error *error,
                                       const bool *planned_inputs, bool planned_output);
me_status b2nd_exec_plan_input(const b2nd_exec_plan *plan, size_t slot,
                               b2nd_exec_input_meta *out);
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
const me_kernel *b2nd_exec_schedule_kernel(const b2nd_exec_schedule *schedule,
                                           b2nd_exec_plan_info *plan_out,
                                           b2nd_exec_schedule_info *sched_out,
                                           uint32_t *direct_slot_out);
typedef struct {
    bool is_partition_schedule;
    size_t ndim;
    int64_t domain[B2ND_EXEC_MAX_RANK];
    int64_t partitions[B2ND_EXEC_MAX_RANK];
    int64_t grid[B2ND_EXEC_MAX_RANK];
    int64_t sel_start[B2ND_EXEC_MAX_RANK];
    int64_t sel_stop[B2ND_EXEC_MAX_RANK];
} b2nd_exec_partition_layout;
me_status b2nd_exec_schedule_partitions(const b2nd_exec_schedule *schedule,
                                        b2nd_exec_partition_layout *out);
me_status b2nd_exec_validate_view(const b2nd_exec_schedule *schedule, size_t slot,
                                   const b2nd_exec_view *view, me_error *error);
bool b2nd_exec_view_is_direct(const b2nd_exec_schedule *schedule, size_t slot,
                              const b2nd_exec_view *view);
void b2nd_exec_gather_span(const b2nd_exec_schedule *schedule, size_t slot,
                           const b2nd_exec_view *view, size_t group,
                           size_t pos, size_t count, unsigned char *dst);
void b2nd_exec_gather_box(const b2nd_exec_schedule *schedule, size_t slot,
                          const b2nd_exec_view *view, const size_t *origin,
                          const size_t *extent, unsigned char *dst);
#endif

/* Experimental typed stage planner; internal, not an installed ABI.
 *
 * First admission: exactly one reduction feeding an arithmetic map. Child
 * descriptors are remapped directly, never exported/imported as JSON. Execution
 * is serial over resident views and/or synchronous source callbacks. Compressed
 * bindings, masks, multiple/nested reductions and explicit stages remain outside
 * this slice. Unsupported topology fails during prepare, without backend retry.
 */
#ifndef B2ND_EXEC_STAGES_H
#define B2ND_EXEC_STAGES_H
#include "b2nd-exec-storage.h"

typedef struct b2nd_exec_staged_plan b2nd_exec_staged_plan;
typedef struct b2nd_exec_staged_schedule b2nd_exec_staged_schedule;

typedef struct {
    uint32_t node; /* Original producer/root node, not a child descriptor ID. */
    size_t last_consumer; /* SIZE_MAX for caller-owned final output. */
    b2nd_exec_plan_info plan;
    b2nd_exec_schedule_info schedule;
    size_t output_bytes;
} b2nd_exec_stage_info;

typedef struct {
    b2nd_exec_callback_report stages[2];
    size_t entered_stages;
    size_t intermediate_bytes; /* Peak owned stage results, excludes tile/core scratch. */
    size_t live_intermediate_bytes; /* Zero on every return, including failure. */
    size_t failed_stage; /* SIZE_MAX for success or a graph-wide error. */
    uint32_t failed_node; /* Original stage root, UINT32_MAX for graph-wide errors. */
    me_fp_status fp;
} b2nd_exec_staged_report;

me_status b2nd_exec_staged_prepare(const b2nd_exec_descriptor *descriptor,
                                  const b2nd_exec_options *options,
                                  b2nd_exec_staged_plan **out, me_error *error);
void b2nd_exec_staged_plan_free(b2nd_exec_staged_plan *plan);

/* Schedules borrow the immutable plan; no operand values or intermediate
 * allocations are retained. Budget rejection occurs before any source reads. */
me_status b2nd_exec_staged_specialize(const b2nd_exec_staged_plan *plan,
                                     const b2nd_exec_input_meta *inputs, size_t ninputs,
                                     size_t tile_items, size_t max_mapped_bytes,
                                     size_t intermediate_budget,
                                     b2nd_exec_staged_schedule **out, me_error *error);
void b2nd_exec_staged_schedule_free(b2nd_exec_staged_schedule *schedule);
me_status b2nd_exec_staged_describe(const b2nd_exec_staged_schedule *schedule,
                                   size_t stage, b2nd_exec_stage_info *out);

/* Validate BOTH stages' bindings, output and gather budgets before the first
 * callback. Callback failures propagate without replay. Output must be disjoint
 * from every resident operand allocation. As with direct maps, a failing final
 * map may have written a prefix; intermediate values never escape. As in the
 * adapter, raise_mask applies after each stage's traversal: a flagged producer
 * must not enter its consumer. Reports retain the union of entered stages. */
me_status b2nd_exec_staged_execute_sources(const b2nd_exec_staged_schedule *schedule,
                                          const b2nd_exec_source *sources,
                                          const b2nd_exec_view *views, size_t ninputs,
                                          me_mut_buffer output, unsigned raise_mask,
                                          size_t max_gather_bytes,
                                          b2nd_exec_staged_report *report,
                                          me_error *error);
#endif

/* Experimental typed stage planner; internal, not an installed ABI.
 *
 * Child descriptors are remapped directly, never exported/imported as JSON.
 * Automatic boundaries are reductions and shared computed values. Explicit
 * composition borrows prepared child plans and binds external/producer slots.
 * All plans and schedules are immutable; values are invocation-local.
 */
#ifndef B2ND_EXEC_STAGES_H
#define B2ND_EXEC_STAGES_H
#include "b2nd-exec-storage.h"

typedef struct b2nd_exec_staged_plan b2nd_exec_staged_plan;
typedef struct b2nd_exec_staged_schedule b2nd_exec_staged_schedule;
#define B2ND_EXEC_MAX_STAGES 32

typedef struct {
    const b2nd_exec_plan *plan; /* Borrowed until the enclosing plan is freed. */
    const int *sources; /* -1-slot: external input; >=0: earlier stage result. */
    uint32_t node; /* Original root for diagnostics, independent of stage ID. */
} b2nd_exec_stage_spec;

typedef struct {
    uint32_t node; /* Original producer/root node, not a child descriptor ID. */
    size_t last_consumer; /* SIZE_MAX for caller-owned final output. */
    b2nd_exec_plan_info plan;
    b2nd_exec_schedule_info schedule;
    size_t output_bytes;
} b2nd_exec_stage_info;

typedef struct {
    size_t stage;  /* SIZE_MAX for graph-wide errors. */
    uint32_t node; /* Original stage root / node, UINT32_MAX if graph-wide. */
} b2nd_exec_stage_diagnostic;

typedef struct {
    b2nd_exec_callback_report stages[B2ND_EXEC_MAX_STAGES];
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
me_status b2nd_exec_staged_prepare_ex(const b2nd_exec_descriptor *descriptor,
                                     const b2nd_exec_options *options,
                                     b2nd_exec_staged_plan **out,
                                     b2nd_exec_stage_diagnostic *diag,
                                     me_error *error);
void b2nd_exec_staged_plan_free(b2nd_exec_staged_plan *plan);
me_status b2nd_exec_staged_assemble(const b2nd_exec_stage_spec *stages, size_t nstages,
                                   size_t ninputs, b2nd_exec_staged_plan **out, me_error *error);
me_status b2nd_exec_staged_assemble_ex(const b2nd_exec_stage_spec *stages, size_t nstages,
                                      size_t ninputs, b2nd_exec_staged_plan **out,
                                      b2nd_exec_stage_diagnostic *diag, me_error *error);
size_t b2nd_exec_staged_count(const b2nd_exec_staged_plan *plan);
me_status b2nd_exec_staged_plan_describe(const b2nd_exec_staged_plan *plan,
                                        b2nd_exec_plan_info *out);
/* Borrow typed child plans/bindings, never an interchange subtree. The parent
 * must outlive every borrowed child and any composition that uses it. */
me_status b2nd_exec_staged_child(const b2nd_exec_staged_plan *plan, size_t stage,
                                const b2nd_exec_plan **child, const int **sources,
                                size_t *ninputs, uint32_t *node);

/* Schedules borrow the immutable plan; no operand values or intermediate
 * allocations are retained. Budget rejection occurs before any source reads. */
me_status b2nd_exec_staged_specialize(const b2nd_exec_staged_plan *plan,
                                     const b2nd_exec_input_meta *inputs, size_t ninputs,
                                     size_t tile_items, size_t max_mapped_bytes,
                                     size_t intermediate_budget,
                                     b2nd_exec_staged_schedule **out, me_error *error);
me_status b2nd_exec_staged_specialize_ex(const b2nd_exec_staged_plan *plan,
                                        const b2nd_exec_input_meta *inputs, size_t ninputs,
                                        size_t tile_items, size_t max_mapped_bytes,
                                        size_t intermediate_budget,
                                        b2nd_exec_staged_schedule **out,
                                        b2nd_exec_stage_diagnostic *diag,
                                        me_error *error);
void b2nd_exec_staged_schedule_free(b2nd_exec_staged_schedule *schedule);
me_status b2nd_exec_staged_describe(const b2nd_exec_staged_schedule *schedule,
                                   size_t stage, b2nd_exec_stage_info *out);

/* Validate ALL stages' bindings, output and gather budgets before the first
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
me_status b2nd_exec_staged_execute_sources_into(const b2nd_exec_staged_schedule *schedule,
                                               const b2nd_exec_source *sources,
                                               const b2nd_exec_view *views, size_t ninputs,
                                               const b2nd_exec_output_view *output,
                                               unsigned raise_mask,
                                               size_t max_gather_bytes,
                                               b2nd_exec_staged_report *report,
                                               me_error *error);
#endif

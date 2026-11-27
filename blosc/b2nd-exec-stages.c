#include "b2nd-exec-stages.h"
#include "b2nd-exec-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct b2nd_exec_staged_plan {
    b2nd_exec_plan *children[2];
    size_t ninputs;
    size_t counts[2];
    size_t slots[2][B2ND_EXEC_MAX_INPUTS]; /* SIZE_MAX denotes producer result. */
    uint32_t nodes[2];
};

struct b2nd_exec_staged_schedule {
    const b2nd_exec_staged_plan *plan;
    b2nd_exec_schedule *children[2];
    b2nd_exec_stage_info stages[2];
};

static me_status stage_error(me_error *error, me_status status, const char *message) {
    if (error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

static bool stage_reduction(b2nd_exec_op op) {
    return op >= B2ND_EXEC_MIN && op <= B2ND_EXEC_STD;
}

/* Reverse topological reachability, optionally cutting the producer edge. */
static void stage_region(const b2nd_exec_descriptor *d, uint32_t root,
                         uint32_t cut, bool *needed) {
    needed[root] = true;
    for (size_t i = d->nnodes; i-- > 0;) {
        if (!needed[i] || i == cut) continue;
        for (size_t a = 0; a < d->nodes[i].nargs; a++) {
            needed[d->nodes[i].args[a]] = true;
        }
    }
}

static me_status stage_prepare_child(const b2nd_exec_descriptor *d,
                                     const b2nd_exec_options *options,
                                     b2nd_exec_staged_plan *plan, size_t stage,
                                     const bool *needed, me_error *error) {
    b2nd_exec_node *nodes = malloc(d->nnodes * sizeof(*nodes));
    if (!nodes) return stage_error(error, ME_ERR_OOM, "Cannot allocate typed stage nodes");
    uint32_t remap[B2ND_EXEC_MAX_NODES];
    size_t count = 0, slots = 0;
    b2nd_exec_plan_info producer = {0};
    if (stage) b2nd_exec_describe(plan->children[0], &producer);
    for (size_t i = 0; i < d->nnodes; i++) {
        if (!needed[i]) continue;
        b2nd_exec_node n = d->nodes[i];
        if (stage && i == plan->nodes[0]) {
            n = (b2nd_exec_node){0};
            n.struct_size = sizeof(n);
            n.op = B2ND_EXEC_INPUT;
            n.dtype = producer.dtype;
            n.itemsize = producer.itemsize;
            plan->slots[stage][slots] = SIZE_MAX;
            n.slot = (uint32_t)slots++;
        } else if (n.op == B2ND_EXEC_INPUT) {
            plan->slots[stage][slots] = n.slot;
            n.slot = (uint32_t)slots++;
        } else {
            for (size_t a = 0; a < n.nargs; a++) n.args[a] = remap[n.args[a]];
        }
        n.id = remap[i] = (uint32_t)count;
        nodes[count++] = n;
    }
    b2nd_exec_descriptor child = {
        sizeof(child), nodes, count, slots, remap[plan->nodes[stage]]};
    plan->counts[stage] = slots;
    me_status status = b2nd_exec_prepare(&child, options, &plan->children[stage], error);
    free(nodes);
    return status;
}

me_status b2nd_exec_staged_prepare(const b2nd_exec_descriptor *d,
                                  const b2nd_exec_options *options,
                                  b2nd_exec_staged_plan **out, me_error *error) {
    if (out) *out = NULL;
    if (error) *error = (me_error){0};
    if (!out) return stage_error(error, ME_ERR_INVALID_ARG, "Missing staged plan output");
    me_status status = b2nd_exec_validate_descriptor(d, true, error);
    if (status != ME_SUCCESS) return status;
    bool needed[B2ND_EXEC_MAX_NODES] = {0};
    stage_region(d, d->root, UINT32_MAX, needed);
    uint32_t producer = UINT32_MAX;
    for (size_t i = 0; i < d->nnodes; i++) {
        if (!needed[i]) {
            return stage_error(error, ME_ERR_INVALID_ARG, "Unreachable staged execution node");
        }
        if (stage_reduction(d->nodes[i].op)) {
            if (producer != UINT32_MAX || i == d->root) {
                return stage_error(error, ME_ERR_UNSUPPORTED,
                                   "Staged slice requires one reduction feeding a map");
            }
            producer = (uint32_t)i;
        }
    }
    if (producer == UINT32_MAX) {
        return stage_error(error, ME_ERR_UNSUPPORTED, "Staged graph has no reduction edge");
    }
    bool regions[2][B2ND_EXEC_MAX_NODES] = {{0}};
    stage_region(d, producer, UINT32_MAX, regions[0]);
    stage_region(d, d->root, producer, regions[1]);
    /* Each child has at most the original inputs plus one producer binding. */
    size_t consumer_inputs = 1;
    for (size_t i = 0; i < d->nnodes; i++) {
        if (regions[1][i] && d->nodes[i].op == B2ND_EXEC_INPUT) consumer_inputs++;
    }
    if (consumer_inputs > B2ND_EXEC_MAX_INPUTS) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Stage input limit exceeded");
    }
    b2nd_exec_staged_plan *plan = calloc(1, sizeof(*plan));
    if (!plan) return stage_error(error, ME_ERR_OOM, "Cannot allocate staged plan");
    plan->ninputs = d->ninputs;
    plan->nodes[0] = producer;
    plan->nodes[1] = d->root;
    for (size_t stage = 0; stage < 2; stage++) {
        status = stage_prepare_child(d, options, plan, stage, regions[stage], error);
        if (status != ME_SUCCESS) {
            b2nd_exec_staged_plan_free(plan);
            return status;
        }
    }
    *out = plan;
    return ME_SUCCESS;
}

void b2nd_exec_staged_plan_free(b2nd_exec_staged_plan *plan) {
    if (!plan) return;
    for (size_t i = 0; i < 2; i++) b2nd_exec_plan_free(plan->children[i]);
    free(plan);
}

me_status b2nd_exec_staged_specialize(const b2nd_exec_staged_plan *plan,
                                     const b2nd_exec_input_meta *inputs, size_t ninputs,
                                     size_t tile_items, size_t max_mapped_bytes,
                                     size_t intermediate_budget,
                                     b2nd_exec_staged_schedule **out, me_error *error) {
    if (out) *out = NULL;
    if (error) *error = (me_error){0};
    if (!plan || !out || !inputs || ninputs != plan->ninputs) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Invalid staged specialization bindings");
    }
    b2nd_exec_staged_schedule *schedule = calloc(1, sizeof(*schedule));
    if (!schedule) return stage_error(error, ME_ERR_OOM, "Cannot allocate staged schedule");
    schedule->plan = plan;
    me_status status = ME_SUCCESS;
    b2nd_exec_input_meta producer = {0};
    for (size_t stage = 0; stage < 2; stage++) {
        b2nd_exec_input_meta child_inputs[B2ND_EXEC_MAX_INPUTS];
        for (size_t i = 0; i < plan->counts[stage]; i++) {
            size_t slot = plan->slots[stage][i];
            child_inputs[i] = slot == SIZE_MAX ? producer : inputs[slot];
        }
        status = b2nd_exec_specialize(plan->children[stage], child_inputs, plan->counts[stage],
                                      tile_items, max_mapped_bytes, &schedule->children[stage], error);
        if (status != ME_SUCCESS) break;
        b2nd_exec_stage_info *info = &schedule->stages[stage];
        info->node = plan->nodes[stage];
        info->last_consumer = stage == 0 ? 1 : SIZE_MAX;
        b2nd_exec_describe(plan->children[stage], &info->plan);
        b2nd_exec_describe_schedule(schedule->children[stage], &info->schedule);
        if (info->schedule.output_items > SIZE_MAX / info->plan.itemsize) {
            status = stage_error(error, ME_ERR_INVALID_ARG, "Stage output extent overflow");
            break;
        }
        info->output_bytes = info->schedule.output_items * info->plan.itemsize;
        if (stage == 0) {
            if (info->output_bytes > intermediate_budget) {
                status = stage_error(error, ME_ERR_INVALID_ARG, "Stage intermediate budget exceeded");
                break;
            }
            size_t stride = info->plan.itemsize;
            for (size_t d = info->schedule.output_ndim; d-- > 0;) {
                size_t extent = (size_t)info->schedule.output_shape[d];
                if (stride > INT64_MAX || (extent && stride > (size_t)INT64_MAX / extent)) {
                    status = stage_error(error, ME_ERR_INVALID_ARG, "Stage intermediate stride overflow");
                    break;
                }
                stride *= extent;
            }
            if (status != ME_SUCCESS) break;
            producer.dtype = info->plan.dtype;
            producer.itemsize = info->plan.itemsize;
            producer.ndim = info->schedule.output_ndim;
            memcpy(producer.shape, info->schedule.output_shape, sizeof(producer.shape));
        }
    }
    if (status != ME_SUCCESS) {
        b2nd_exec_staged_schedule_free(schedule);
        return status;
    }
    *out = schedule;
    return ME_SUCCESS;
}

void b2nd_exec_staged_schedule_free(b2nd_exec_staged_schedule *schedule) {
    if (!schedule) return;
    for (size_t i = 0; i < 2; i++) b2nd_exec_schedule_free(schedule->children[i]);
    free(schedule);
}

me_status b2nd_exec_staged_describe(const b2nd_exec_staged_schedule *schedule,
                                   size_t stage, b2nd_exec_stage_info *out) {
    if (!schedule || !out || stage >= 2) return ME_ERR_INVALID_ARG;
    *out = schedule->stages[stage];
    return ME_SUCCESS;
}

static me_status stage_bind(const b2nd_exec_staged_schedule *schedule, size_t stage,
                            const b2nd_exec_source *sources, const b2nd_exec_view *views,
                            const b2nd_exec_view *intermediate,
                            b2nd_exec_source *child_sources, b2nd_exec_view *child_views,
                            me_mut_buffer output, size_t max_gather_bytes, me_error *error) {
    const b2nd_exec_staged_plan *plan = schedule->plan;
    bool readers[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t planned_slot = SIZE_MAX;
    for (size_t i = 0; i < plan->counts[stage]; i++) {
        b2nd_exec_input_meta meta;
        b2nd_exec_schedule_input(schedule->children[stage], i, &meta);
        size_t slot = plan->slots[stage][i];
        b2nd_exec_source *src = &child_sources[i];
        if (slot == SIZE_MAX) {
            *src = (b2nd_exec_source){0};
            src->struct_size = sizeof(*src);
            src->dtype = meta.dtype;
            src->itemsize = meta.itemsize;
            src->ndim = meta.ndim;
            memcpy(src->shape, meta.shape, sizeof(src->shape));
            child_views[i] = *intermediate;
            planned_slot = i;
        } else {
            *src = sources[slot];
            if (!src->read && !views) {
                return stage_error(error, ME_ERR_INVALID_ARG, "Missing staged resident view");
            }
            if (!src->read) child_views[i] = views[slot];
        }
        if (src->struct_size != sizeof(*src) || src->dtype != meta.dtype ||
            src->itemsize != meta.itemsize || src->ndim != meta.ndim ||
            memcmp(src->shape, meta.shape, meta.ndim * sizeof(meta.shape[0]))) {
            return stage_error(error, ME_ERR_INVALID_ARG, "Stage source metadata mismatch");
        }
        readers[i] = src->read != NULL;
    }
    size_t tasks = 1;
    return b2nd_exec_plan_stage_readers(schedule->children[stage], child_views, readers,
                                  plan->counts[stage], output, max_gather_bytes,
                                  NULL, &tasks, error, planned_slot, stage == 0);
}

me_status b2nd_exec_staged_execute_sources(const b2nd_exec_staged_schedule *schedule,
                                          const b2nd_exec_source *sources,
                                          const b2nd_exec_view *views, size_t ninputs,
                                          me_mut_buffer output, unsigned raise_mask,
                                          size_t max_gather_bytes,
                                          b2nd_exec_staged_report *report,
                                          me_error *error) {
    b2nd_exec_staged_report local = {0};
    local.failed_stage = SIZE_MAX;
    local.failed_node = UINT32_MAX;
    if (report) *report = local;
    if (error) *error = (me_error){0};
    if (!schedule || !sources || ninputs != schedule->plan->ninputs || (raise_mask & ~15u)) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Invalid staged execution bindings");
    }
    if (output.capacity > UINTPTR_MAX - (uintptr_t)output.data) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Staged output extent overflow");
    }
    for (size_t i = 0; views && i < ninputs; i++) {
        if (sources[i].read) continue;
        if (views[i].capacity > UINTPTR_MAX - (uintptr_t)views[i].base) {
            return stage_error(error, ME_ERR_INVALID_ARG, "Staged operand extent overflow");
        }
        uintptr_t begin = (uintptr_t)views[i].base, end = begin + views[i].capacity;
        uintptr_t out_begin = (uintptr_t)output.data, out_end = out_begin + output.capacity;
        if (views[i].capacity && output.capacity && schedule->stages[1].output_bytes &&
            out_begin < end && begin < out_end) {
            return stage_error(error, ME_ERR_UNSUPPORTED, "Staged output overlaps resident operand");
        }
    }
    /* Validate FP observation before any callback, even if stage 0 is integer. */
    if (raise_mask) {
        me_fp_scope scope;
        me_fp_status fp = {0};
        me_status status = me_fp_scope_begin(&scope, &fp, error);
        if (status != ME_SUCCESS) return status;
        status = me_fp_scope_end(&scope, error);
        if (status != ME_SUCCESS) return status;
        if (!fp.supported) {
            return stage_error(error, ME_ERR_UNSUPPORTED, "Staged FP observation unsupported");
        }
    }
    size_t bytes = schedule->stages[0].output_bytes;
    b2nd_exec_view intermediate = {0};
    intermediate.struct_size = sizeof(intermediate);
    intermediate.capacity = bytes;
    size_t stride = schedule->stages[0].plan.itemsize;
    for (size_t d = schedule->stages[0].schedule.output_ndim; d-- > 0;) {
        intermediate.strides[d] = (int64_t)stride;
        stride *= (size_t)schedule->stages[0].schedule.output_shape[d];
    }
    size_t count = schedule->plan->counts[0] + schedule->plan->counts[1];
    b2nd_exec_source *source_buf = calloc(count, sizeof(*source_buf));
    b2nd_exec_view *view_buf = calloc(count, sizeof(*view_buf));
    if (!source_buf || !view_buf) {
        free(source_buf);
        free(view_buf);
        return stage_error(error, ME_ERR_OOM, "Cannot allocate stage bindings");
    }
    b2nd_exec_source *child_sources[] = {source_buf, source_buf + schedule->plan->counts[0]};
    b2nd_exec_view *child_views[] = {view_buf, view_buf + schedule->plan->counts[0]};
    me_status status = ME_SUCCESS;
    for (size_t stage = 0; stage < 2; stage++) {
        status = stage_bind(schedule, stage, sources, views, &intermediate,
                            child_sources[stage], child_views[stage],
                            stage == 0 ? (me_mut_buffer){NULL, bytes} : output,
                            max_gather_bytes, error);
        if (status != ME_SUCCESS) {
            local.failed_stage = stage;
            local.failed_node = schedule->plan->nodes[stage];
            break;
        }
    }
    unsigned char *data = NULL;
    if (status == ME_SUCCESS && bytes) {
        data = malloc(bytes);
        if (!data) status = stage_error(error, ME_ERR_OOM, "Cannot allocate stage intermediate");
        else local.intermediate_bytes = local.live_intermediate_bytes = bytes;
    }
    for (size_t i = 0; i < schedule->plan->counts[1]; i++) {
        if (schedule->plan->slots[1][i] == SIZE_MAX) child_views[1][i].base = data;
    }
    bool fp_seen = false;
    for (size_t stage = 0; status == ME_SUCCESS && stage < 2; stage++) {
        local.entered_stages++;
        status = b2nd_exec_execute_sources(schedule->children[stage], child_sources[stage],
                                           child_views[stage], schedule->plan->counts[stage],
                                           stage == 0 ? (me_mut_buffer){data, bytes} : output,
                                           NULL, raise_mask, max_gather_bytes, 0,
                                           &local.stages[stage], error);
        me_fp_status fp = local.stages[stage].execution.fp;
        local.fp.flags |= fp.flags;
        if (local.stages[stage].execution.fp_observed) {
            local.fp.supported = fp_seen ? local.fp.supported && fp.supported : fp.supported;
            fp_seen = true;
        }
        if (status != ME_SUCCESS) {
            local.failed_stage = stage;
            local.failed_node = schedule->plan->nodes[stage];
        }
    }
    free(data); /* Producer's last consumer is the final map. */
    free(source_buf);
    free(view_buf);
    local.live_intermediate_bytes = 0;
    if (report) *report = local;
    return status;
}

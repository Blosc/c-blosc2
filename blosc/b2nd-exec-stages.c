#include "b2nd-exec-stages.h"
#include "b2nd-exec-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct b2nd_exec_staged_plan {
    const b2nd_exec_plan *children[B2ND_EXEC_MAX_STAGES];
    size_t ninputs, nstages;
    size_t counts[B2ND_EXEC_MAX_STAGES], last[B2ND_EXEC_MAX_STAGES];
    int sources[B2ND_EXEC_MAX_STAGES][B2ND_EXEC_MAX_INPUTS];
    uint32_t nodes[B2ND_EXEC_MAX_STAGES];
    bool owns_children;
};

struct b2nd_exec_staged_schedule {
    const b2nd_exec_staged_plan *plan;
    b2nd_exec_schedule *children[B2ND_EXEC_MAX_STAGES];
    b2nd_exec_stage_info stages[B2ND_EXEC_MAX_STAGES];
    size_t intermediate_bytes;
};

static me_status stage_error(me_error *error, me_status status, const char *message) {
    if (error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

static void stage_diag(b2nd_exec_stage_diagnostic *diag, size_t stage, uint32_t node) {
    if (diag) {
        diag->stage = stage;
        diag->node = node;
    }
}

static bool stage_reduction(b2nd_exec_op op) {
    return op >= B2ND_EXEC_MIN && op <= B2ND_EXEC_STD;
}

static void stage_region(const b2nd_exec_descriptor *d, uint32_t root,
                         const int *boundaries, bool *needed) {
    needed[root] = true;
    for (size_t i = d->nnodes; i-- > 0;) {
        if (!needed[i] || (i != root && boundaries && boundaries[i] >= 0)) continue;
        for (size_t a = 0; a < d->nodes[i].nargs; a++) needed[d->nodes[i].args[a]] = true;
        if (d->nodes[i].has_where) needed[d->nodes[i].where] = true;
    }
}

static me_status stage_links(b2nd_exec_staged_plan *plan, b2nd_exec_stage_diagnostic *diag,
                             me_error *error) {
    bool inputs[B2ND_EXEC_MAX_INPUTS] = {0}, reachable[B2ND_EXEC_MAX_STAGES] = {0};
    b2nd_exec_input_meta signatures[B2ND_EXEC_MAX_INPUTS] = {{0}};
    for (size_t s = 0; s < plan->nstages; s++) {
        b2nd_exec_plan_info child;
        if (b2nd_exec_describe(plan->children[s], &child) != ME_SUCCESS) {
            stage_diag(diag, s, plan->nodes[s]);
            return stage_error(error, ME_ERR_INVALID_ARG, "Missing explicit stage plan");
        }
        plan->counts[s] = child.ninputs;
        plan->last[s] = s;
        for (size_t i = 0; i < child.ninputs; i++) {
            int source = plan->sources[s][i];
            b2nd_exec_input_meta expected, actual = {0};
            b2nd_exec_plan_input(plan->children[s], i, &expected);
            if (source >= 0) {
                if ((size_t)source >= s) {
                    stage_diag(diag, s, plan->nodes[s]);
                    return stage_error(error, ME_ERR_INVALID_ARG, "Forward or cyclic stage binding");
                }
                b2nd_exec_plan_info producer;
                b2nd_exec_describe(plan->children[source], &producer);
                actual.dtype = producer.dtype;
                actual.itemsize = producer.itemsize;
                plan->last[source] = s;
            } else {
                size_t slot = (size_t)(-(int64_t)source - 1);
                if (slot >= plan->ninputs) {
                    stage_diag(diag, s, plan->nodes[s]);
                    return stage_error(error, ME_ERR_INVALID_ARG, "Invalid external stage binding");
                }
                if (!inputs[slot]) signatures[slot] = expected;
                actual = signatures[slot];
                inputs[slot] = true;
            }
            if (actual.dtype != expected.dtype || actual.itemsize != expected.itemsize) {
                stage_diag(diag, s, plan->nodes[s]);
                return stage_error(error, ME_ERR_SIGNATURE, "Stage binding dtype mismatch");
            }
        }
    }
    reachable[plan->nstages - 1] = true;
    for (size_t s = plan->nstages; s-- > 0;) {
        if (!reachable[s]) return stage_error(error, ME_ERR_INVALID_ARG, "Unreachable execution stage");
        for (size_t i = 0; i < plan->counts[s]; i++) {
            int source = plan->sources[s][i];
            if (source >= 0) reachable[source] = true;
        }
    }
    for (size_t i = 0; i < plan->ninputs; i++) {
        if (!inputs[i]) return stage_error(error, ME_ERR_INVALID_ARG, "Unreachable external stage input");
    }
    plan->last[plan->nstages - 1] = SIZE_MAX;
    return ME_SUCCESS;
}

me_status b2nd_exec_staged_assemble_ex(const b2nd_exec_stage_spec *stages, size_t nstages,
                                      size_t ninputs, b2nd_exec_staged_plan **out,
                                      b2nd_exec_stage_diagnostic *diag, me_error *error) {
    if (out) *out = NULL;
    if (error) *error = (me_error){0};
    stage_diag(diag, SIZE_MAX, UINT32_MAX);
    if (!out || !stages || !nstages || nstages > B2ND_EXEC_MAX_STAGES ||
        ninputs > B2ND_EXEC_MAX_INPUTS) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Invalid explicit stage bounds");
    }
    b2nd_exec_staged_plan *plan = calloc(1, sizeof(*plan));
    if (!plan) return stage_error(error, ME_ERR_OOM, "Cannot allocate staged plan");
    plan->nstages = nstages;
    plan->ninputs = ninputs;
    for (size_t s = 0; s < nstages; s++) {
        b2nd_exec_plan_info info;
        if (b2nd_exec_describe(stages[s].plan, &info) != ME_SUCCESS ||
            (info.ninputs && !stages[s].sources)) {
            stage_diag(diag, s, stages[s].node);
            b2nd_exec_staged_plan_free(plan);
            return stage_error(error, ME_ERR_INVALID_ARG, "Invalid explicit stage bindings");
        }
        plan->children[s] = stages[s].plan;
        plan->nodes[s] = stages[s].node;
        if (info.ninputs) memcpy(plan->sources[s], stages[s].sources, info.ninputs * sizeof(int));
    }
    me_status status = stage_links(plan, diag, error);
    if (status != ME_SUCCESS) {
        b2nd_exec_staged_plan_free(plan);
        return status;
    }
    *out = plan;
    return ME_SUCCESS;
}

me_status b2nd_exec_staged_assemble(const b2nd_exec_stage_spec *stages, size_t nstages,
                                   size_t ninputs, b2nd_exec_staged_plan **out, me_error *error) {
    return b2nd_exec_staged_assemble_ex(stages, nstages, ninputs, out, NULL, error);
}

me_status b2nd_exec_staged_prepare_ex(const b2nd_exec_descriptor *d,
                                     const b2nd_exec_options *options,
                                     b2nd_exec_staged_plan **out,
                                     b2nd_exec_stage_diagnostic *diag,
                                     me_error *error) {
    if (out) *out = NULL;
    if (error) *error = (me_error){0};
    stage_diag(diag, SIZE_MAX, UINT32_MAX);
    if (!out) return stage_error(error, ME_ERR_INVALID_ARG, "Missing staged plan output");
    me_status status = b2nd_exec_validate_descriptor(d, true, error);
    if (status != ME_SUCCESS) return status;
    bool needed[B2ND_EXEC_MAX_NODES] = {0};
    bool strong[B2ND_EXEC_MAX_NODES] = {0};
    unsigned uses[B2ND_EXEC_MAX_NODES] = {0};
    int boundaries[B2ND_EXEC_MAX_NODES];
    uint32_t roots[B2ND_EXEC_MAX_STAGES];
    stage_region(d, d->root, NULL, needed);
    size_t nstages = 0;
    for (size_t i = 0; i < d->nnodes; i++) {
        if (!needed[i]) {
            stage_diag(diag, SIZE_MAX, (uint32_t)i);
            return stage_error(error, ME_ERR_INVALID_ARG, "Unreachable staged execution node");
        }
        strong[i] = d->nodes[i].op == B2ND_EXEC_INPUT || stage_reduction(d->nodes[i].op);
        for (size_t a = 0; a < d->nodes[i].nargs; a++) {
            uint32_t arg = d->nodes[i].args[a];
            uses[arg]++;
            strong[i] |= strong[arg];
        }
        if (d->nodes[i].has_where) {
            uses[d->nodes[i].where]++;
            strong[i] |= strong[d->nodes[i].where];
        }
        boundaries[i] = -1;
    }
    for (size_t i = 0; i < d->nnodes; i++) {
        b2nd_exec_op op = d->nodes[i].op;
        if (i == d->root || stage_reduction(op) || (d->nodes[i].nargs && uses[i] > 1)) {
            if (i != d->root && !stage_reduction(op) && !strong[i]) {
                stage_diag(diag, SIZE_MAX, (uint32_t)i);
                return stage_error(error, ME_ERR_UNSUPPORTED,
                                   "Shared scalar-only computations cannot be materialized as strong stage inputs");
            }
            if (nstages == B2ND_EXEC_MAX_STAGES) {
                stage_diag(diag, SIZE_MAX, (uint32_t)i);
                return stage_error(error, ME_ERR_INVALID_ARG, "Execution stage limit exceeded");
            }
            boundaries[i] = (int)nstages;
            roots[nstages++] = (uint32_t)i;
        }
    }
    /* Lifting an ordered/lazy branch into an unconditional stage changes FP
     * participation and failure order. Explicit composition declares that order
     * separately; automatic conditional/masked regions retain the adapter restriction. */
    if (nstages > 1) {
        for (size_t i = 0; i < d->nnodes; i++) {
            b2nd_exec_op op = d->nodes[i].op;
            if (d->nodes[i].has_where || op == B2ND_EXEC_SELECT ||
                op == B2ND_EXEC_AND || op == B2ND_EXEC_OR) {
                stage_diag(diag, SIZE_MAX, (uint32_t)i);
                return stage_error(error, ME_ERR_UNSUPPORTED,
                                   "Unsupported automatic conditional or masked stage domain");
            }
        }
    }
    b2nd_exec_staged_plan *plan = calloc(1, sizeof(*plan));
    b2nd_exec_node *nodes = malloc(d->nnodes * sizeof(*nodes));
    if (!plan || !nodes) {
        free(plan);
        free(nodes);
        return stage_error(error, ME_ERR_OOM, "Cannot allocate typed stage planner");
    }
    plan->nstages = nstages;
    plan->ninputs = d->ninputs;
    plan->owns_children = true;
    for (size_t s = 0; s < nstages; s++) {
        memset(needed, 0, sizeof(needed));
        stage_region(d, roots[s], boundaries, needed);
        uint32_t remap[B2ND_EXEC_MAX_NODES];
        size_t count = 0, slots = 0;
        plan->nodes[s] = roots[s];
        for (size_t i = 0; i < d->nnodes; i++) {
            if (!needed[i]) continue;
            b2nd_exec_node n = d->nodes[i];
            bool producer = i != roots[s] && boundaries[i] >= 0;
            if (producer || n.op == B2ND_EXEC_INPUT) {
                if (slots == B2ND_EXEC_MAX_INPUTS) {
                    status = stage_error(error, ME_ERR_INVALID_ARG, "Stage input limit exceeded");
                    stage_diag(diag, s, roots[s]);
                    break;
                }
                if (producer) {
                    b2nd_exec_plan_info info;
                    b2nd_exec_describe(plan->children[boundaries[i]], &info);
                    n = (b2nd_exec_node){0};
                    n.struct_size = sizeof(n);
                    n.op = B2ND_EXEC_INPUT;
                    n.dtype = info.dtype;
                    n.itemsize = info.itemsize;
                }
                plan->sources[s][slots] = producer ? boundaries[i] : -1 - (int)n.slot;
                n.slot = (uint32_t)slots++;
            } else {
                for (size_t a = 0; a < n.nargs; a++) n.args[a] = remap[n.args[a]];
                if (n.has_where) n.where = remap[n.where];
            }
            n.id = remap[i] = (uint32_t)count;
            nodes[count++] = n;
        }
        if (status != ME_SUCCESS) break;
        b2nd_exec_descriptor child = {sizeof(child), nodes, count, slots, remap[roots[s]]};
        b2nd_exec_plan *prepared = NULL;
        status = b2nd_exec_prepare(&child, options, &prepared, error);
        if (status != ME_SUCCESS) {
            stage_diag(diag, s, roots[s]);
            break;
        }
        plan->children[s] = prepared;
    }
    free(nodes);
    if (status == ME_SUCCESS) status = stage_links(plan, diag, error);
    if (status != ME_SUCCESS) {
        b2nd_exec_staged_plan_free(plan);
        return status;
    }
    *out = plan;
    return ME_SUCCESS;
}

me_status b2nd_exec_staged_prepare(const b2nd_exec_descriptor *d,
                                  const b2nd_exec_options *options,
                                  b2nd_exec_staged_plan **out, me_error *error) {
    return b2nd_exec_staged_prepare_ex(d, options, out, NULL, error);
}

size_t b2nd_exec_staged_count(const b2nd_exec_staged_plan *plan) {
    return plan ? plan->nstages : 0;
}

me_status b2nd_exec_staged_child(const b2nd_exec_staged_plan *plan, size_t stage,
                                const b2nd_exec_plan **child, const int **sources,
                                size_t *ninputs, uint32_t *node) {
    if (!plan || stage >= plan->nstages || !child || !sources || !ninputs || !node) return ME_ERR_INVALID_ARG;
    *child = plan->children[stage];
    *sources = plan->sources[stage];
    *ninputs = plan->counts[stage];
    *node = plan->nodes[stage];
    return ME_SUCCESS;
}

me_status b2nd_exec_staged_plan_describe(const b2nd_exec_staged_plan *plan,
                                        b2nd_exec_plan_info *out) {
    if (!plan || !out) return ME_ERR_INVALID_ARG;
    b2nd_exec_describe(plan->children[plan->nstages - 1], out);
    out->ninputs = plan->ninputs;
    out->partitionable = false;
    out->plan_metadata_bytes = sizeof(*plan);
    out->has_jit = plan->nstages > 0;
    for (size_t s = 0; s < plan->nstages; s++) {
        b2nd_exec_plan_info info;
        b2nd_exec_describe(plan->children[s], &info);
        out->plan_metadata_bytes += info.plan_metadata_bytes;
        out->has_jit &= info.has_jit;
    }
    return ME_SUCCESS;
}

void b2nd_exec_staged_plan_free(b2nd_exec_staged_plan *plan) {
    if (!plan) return;
    if (plan->owns_children) {
        for (size_t i = 0; i < plan->nstages; i++) {
            b2nd_exec_plan_free((b2nd_exec_plan *)plan->children[i]);
        }
    }
    free(plan);
}

me_status b2nd_exec_staged_specialize_ex(const b2nd_exec_staged_plan *plan,
                                        const b2nd_exec_input_meta *inputs, size_t ninputs,
                                        size_t tile_items, size_t max_mapped_bytes,
                                        size_t intermediate_budget,
                                        b2nd_exec_staged_schedule **out,
                                        b2nd_exec_stage_diagnostic *diag,
                                        me_error *error) {
    if (out) *out = NULL;
    if (error) *error = (me_error){0};
    stage_diag(diag, SIZE_MAX, UINT32_MAX);
    if (!plan || !out || (!inputs && ninputs) || ninputs != plan->ninputs) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Invalid staged specialization bindings");
    }
    b2nd_exec_staged_schedule *schedule = calloc(1, sizeof(*schedule));
    if (!schedule) return stage_error(error, ME_ERR_OOM, "Cannot allocate staged schedule");
    schedule->plan = plan;
    me_status status = ME_SUCCESS;
    for (size_t s = 0; s < plan->nstages; s++) {
        b2nd_exec_input_meta child_inputs[B2ND_EXEC_MAX_INPUTS] = {{0}};
        for (size_t i = 0; i < plan->counts[s]; i++) {
            int source = plan->sources[s][i];
            if (source < 0) child_inputs[i] = inputs[-1 - source];
            else {
                const b2nd_exec_stage_info *producer = &schedule->stages[source];
                child_inputs[i].dtype = producer->plan.dtype;
                child_inputs[i].itemsize = producer->plan.itemsize;
                child_inputs[i].ndim = producer->schedule.output_ndim;
                memcpy(child_inputs[i].shape, producer->schedule.output_shape, sizeof(child_inputs[i].shape));
            }
        }
        status = b2nd_exec_specialize(plan->children[s], child_inputs, plan->counts[s],
                                      tile_items, max_mapped_bytes, &schedule->children[s], error);
        if (status != ME_SUCCESS) {
            stage_diag(diag, s, plan->nodes[s]);
            break;
        }
        b2nd_exec_stage_info *info = &schedule->stages[s];
        info->node = plan->nodes[s];
        info->last_consumer = plan->last[s];
        b2nd_exec_describe(plan->children[s], &info->plan);
        b2nd_exec_describe_schedule(schedule->children[s], &info->schedule);
        if (info->schedule.output_items > SIZE_MAX / info->plan.itemsize) {
            status = stage_error(error, ME_ERR_INVALID_ARG, "Stage output extent overflow");
            stage_diag(diag, s, plan->nodes[s]);
            break;
        }
        info->output_bytes = info->schedule.output_items * info->plan.itemsize;
        size_t stride = info->plan.itemsize;
        for (size_t d = info->schedule.output_ndim; d-- > 0;) {
            size_t extent = (size_t)info->schedule.output_shape[d];
            if (stride > INT64_MAX || (extent && stride > (size_t)INT64_MAX / extent)) {
                status = stage_error(error, ME_ERR_INVALID_ARG, "Stage intermediate stride overflow");
                stage_diag(diag, s, plan->nodes[s]);
                break;
            }
            stride *= extent;
        }
        if (status != ME_SUCCESS) break;
        if (s + 1 < plan->nstages) {
            if (info->output_bytes > intermediate_budget - schedule->intermediate_bytes) {
                status = stage_error(error, ME_ERR_INVALID_ARG, "Stage intermediate budget exceeded");
                stage_diag(diag, s, plan->nodes[s]);
                break;
            }
            schedule->intermediate_bytes += info->output_bytes;
        }
    }
    if (status != ME_SUCCESS) {
        b2nd_exec_staged_schedule_free(schedule);
        return status;
    }
    *out = schedule;
    return ME_SUCCESS;
}

me_status b2nd_exec_staged_specialize(const b2nd_exec_staged_plan *plan,
                                     const b2nd_exec_input_meta *inputs, size_t ninputs,
                                     size_t tile_items, size_t max_mapped_bytes,
                                     size_t intermediate_budget,
                                     b2nd_exec_staged_schedule **out, me_error *error) {
    return b2nd_exec_staged_specialize_ex(plan, inputs, ninputs, tile_items, max_mapped_bytes,
                                          intermediate_budget, out, NULL, error);
}

void b2nd_exec_staged_schedule_free(b2nd_exec_staged_schedule *schedule) {
    if (!schedule) return;
    for (size_t i = 0; i < schedule->plan->nstages; i++) b2nd_exec_schedule_free(schedule->children[i]);
    free(schedule);
}

me_status b2nd_exec_staged_describe(const b2nd_exec_staged_schedule *schedule,
                                   size_t stage, b2nd_exec_stage_info *out) {
    if (!schedule || !out || stage >= schedule->plan->nstages) return ME_ERR_INVALID_ARG;
    *out = schedule->stages[stage];
    return ME_SUCCESS;
}

me_status b2nd_exec_staged_execute_sources_into(const b2nd_exec_staged_schedule *schedule,
                                               const b2nd_exec_source *sources,
                                               const b2nd_exec_view *views, size_t ninputs,
                                               const b2nd_exec_output_view *output,
                                               unsigned raise_mask,
                                               size_t max_gather_bytes,
                                               b2nd_exec_staged_report *report,
                                               me_error *error) {
    b2nd_exec_staged_report local = {0};
    local.failed_stage = SIZE_MAX;
    local.failed_node = UINT32_MAX;
    if (report) *report = local;
    if (error) *error = (me_error){0};
    if (!schedule || (!sources && ninputs) || ninputs != schedule->plan->ninputs ||
        !output || output->struct_size != sizeof(*output) || (raise_mask & ~15u)) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Invalid staged execution bindings");
    }
    const b2nd_exec_staged_plan *plan = schedule->plan;
    size_t final = plan->nstages - 1;
    bool masked = false;
    for (size_t s = 0; s < plan->nstages; s++) masked |= schedule->stages[s].plan.has_where;
    for (size_t i = 0; masked && i < ninputs; i++) {
        if (sources[i].read) {
            return stage_error(error, ME_ERR_UNSUPPORTED, "Reader-backed masked stages are not admitted");
        }
    }
    if (output->capacity > UINTPTR_MAX - (uintptr_t)output->base) {
        return stage_error(error, ME_ERR_INVALID_ARG, "Staged output extent overflow");
    }
    for (size_t i = 0; views && i < ninputs; i++) {
        if (sources[i].read) continue;
        if (views[i].capacity > UINTPTR_MAX - (uintptr_t)views[i].base) {
            return stage_error(error, ME_ERR_INVALID_ARG, "Staged operand extent overflow");
        }
        uintptr_t begin = (uintptr_t)views[i].base, end = begin + views[i].capacity;
        uintptr_t out_begin = (uintptr_t)output->base, out_end = out_begin + output->capacity;
        if (views[i].capacity && output->capacity && schedule->stages[final].output_bytes &&
            out_begin < end && begin < out_end) {
            return stage_error(error, ME_ERR_UNSUPPORTED, "Staged output overlaps resident operand");
        }
    }
    if (raise_mask) {
        me_fp_scope scope;
        me_fp_status fp = {0};
        me_status status = me_fp_scope_begin(&scope, &fp, error);
        if (status != ME_SUCCESS) return status;
        status = me_fp_scope_end(&scope, error);
        if (status != ME_SUCCESS) return status;
        if (!fp.supported) return stage_error(error, ME_ERR_UNSUPPORTED, "Staged FP observation unsupported");
    }
    b2nd_exec_view results[B2ND_EXEC_MAX_STAGES] = {{0}};
    size_t offsets[B2ND_EXEC_MAX_STAGES], count = 0;
    for (size_t s = 0; s < plan->nstages; s++) {
        offsets[s] = count;
        count += plan->counts[s];
        results[s].struct_size = sizeof(results[s]);
        results[s].capacity = schedule->stages[s].output_bytes;
        size_t stride = schedule->stages[s].plan.itemsize;
        for (size_t d = schedule->stages[s].schedule.output_ndim; d-- > 0;) {
            results[s].strides[d] = (int64_t)stride;
            stride *= (size_t)schedule->stages[s].schedule.output_shape[d];
        }
    }
    b2nd_exec_source *child_sources = calloc(count ? count : 1, sizeof(*child_sources));
    b2nd_exec_view *child_views = calloc(count ? count : 1, sizeof(*child_views));
    if (!child_sources || !child_views) {
        free(child_sources);
        free(child_views);
        return stage_error(error, ME_ERR_OOM, "Cannot allocate stage bindings");
    }
    me_status status = ME_SUCCESS;
    for (size_t s = 0; status == ME_SUCCESS && s < plan->nstages; s++) {
        bool readers[B2ND_EXEC_MAX_INPUTS] = {0}, planned[B2ND_EXEC_MAX_INPUTS] = {0};
        for (size_t i = 0; i < plan->counts[s]; i++) {
            b2nd_exec_input_meta meta;
            b2nd_exec_schedule_input(schedule->children[s], i, &meta);
            b2nd_exec_source *src = &child_sources[offsets[s] + i];
            b2nd_exec_view *view = &child_views[offsets[s] + i];
            int source = plan->sources[s][i];
            if (source >= 0) {
                src->struct_size = sizeof(*src);
                src->dtype = meta.dtype; src->itemsize = meta.itemsize; src->ndim = meta.ndim;
                memcpy(src->shape, meta.shape, sizeof(src->shape));
                *view = results[source];
                planned[i] = true;
            } else {
                size_t slot = (size_t)(-1 - source);
                *src = sources[slot];
                if (!src->read && !views) {
                    status = stage_error(error, ME_ERR_INVALID_ARG, "Missing staged resident view");
                    break;
                }
                if (!src->read) *view = views[slot];
            }
            if (src->struct_size != sizeof(*src) || src->dtype != meta.dtype ||
                src->itemsize != meta.itemsize || src->ndim != meta.ndim ||
                memcmp(src->shape, meta.shape, meta.ndim * sizeof(meta.shape[0]))) {
                status = stage_error(error, ME_ERR_INVALID_ARG, "Stage source metadata mismatch");
                break;
            }
            readers[i] = src->read != NULL;
        }
        size_t tasks = 1;
        if (status == ME_SUCCESS) {
            if (s == final) {
                status = b2nd_exec_plan_stage_readers_into(schedule->children[s], child_views + offsets[s],
                    readers, plan->counts[s], output, max_gather_bytes, NULL, &tasks, error, planned, false);
            } else {
                status = b2nd_exec_plan_stage_readers(schedule->children[s], child_views + offsets[s],
                    readers, plan->counts[s], (me_mut_buffer){NULL, results[s].capacity},
                    max_gather_bytes, NULL, &tasks, error, planned, true);
            }
        }
        if (status != ME_SUCCESS) {
            local.failed_stage = s;
            local.failed_node = plan->nodes[s];
        }
    }
    /* Reserve every declared dependency after all binding/budget preflight,
     * before any source read. This matches the adapter's honest reservation. */
    for (size_t s = 0; status == ME_SUCCESS && s < final; s++) {
        if (!results[s].capacity) continue;
        results[s].base = malloc(results[s].capacity);
        if (!results[s].base) {
            status = stage_error(error, ME_ERR_OOM, "Cannot allocate stage intermediate");
            local.failed_stage = s;
            local.failed_node = plan->nodes[s];
            break;
        }
        local.live_intermediate_bytes += results[s].capacity;
        local.intermediate_bytes = local.live_intermediate_bytes;
    }
    bool fp_seen = false;
    for (size_t s = 0; status == ME_SUCCESS && s < plan->nstages; s++) {
        for (size_t i = 0; i < plan->counts[s]; i++) {
            int source = plan->sources[s][i];
            if (source >= 0) child_views[offsets[s] + i] = results[source];
        }
        local.entered_stages++;
        if (s == final) {
            status = b2nd_exec_execute_sources_into(schedule->children[s], child_sources + offsets[s],
                child_views + offsets[s], plan->counts[s], output, raise_mask, max_gather_bytes,
                &local.stages[s], error);
        } else {
            status = b2nd_exec_execute_sources(schedule->children[s], child_sources + offsets[s],
                child_views + offsets[s], plan->counts[s],
                (me_mut_buffer){(void *)results[s].base, results[s].capacity},
                NULL, raise_mask, max_gather_bytes, 0, &local.stages[s], error);
        }
        local.fp.flags |= local.stages[s].execution.fp.flags;
        if (local.stages[s].execution.fp_observed) {
            local.fp.supported = fp_seen ? local.fp.supported && local.stages[s].execution.fp.supported :
                local.stages[s].execution.fp.supported;
            fp_seen = true;
        }
        if (status != ME_SUCCESS) {
            local.failed_stage = s;
            local.failed_node = plan->nodes[s];
        }
        for (size_t p = 0; p < s; p++) {
            if (results[p].base && plan->last[p] == s) {
                free((void *)results[p].base);
                results[p].base = NULL;
                local.live_intermediate_bytes -= results[p].capacity;
            }
        }
    }
    for (size_t s = 0; s < final; s++) free((void *)results[s].base);
    free(child_sources);
    free(child_views);
    local.live_intermediate_bytes = 0;
    if (report) *report = local;
    return status;
}

me_status b2nd_exec_staged_execute_sources(const b2nd_exec_staged_schedule *schedule,
                                          const b2nd_exec_source *sources,
                                          const b2nd_exec_view *views, size_t ninputs,
                                          me_mut_buffer output, unsigned raise_mask,
                                          size_t max_gather_bytes,
                                          b2nd_exec_staged_report *report,
                                          me_error *error) {
    if (!schedule) {
        if (report) {
            *report = (b2nd_exec_staged_report){.failed_stage = SIZE_MAX, .failed_node = UINT32_MAX};
        }
        return stage_error(error, ME_ERR_INVALID_ARG, "Invalid staged execution bindings");
    }
    size_t final = schedule->plan->nstages - 1;
    b2nd_exec_output_view out_view = {sizeof(out_view), output.data, output.capacity, 0, {0}};
    size_t stride = schedule->stages[final].plan.itemsize;
    for (size_t d = schedule->stages[final].schedule.output_ndim; d-- > 0;) {
        out_view.strides[d] = (int64_t)stride;
        stride *= (size_t)schedule->stages[final].schedule.output_shape[d];
    }
    return b2nd_exec_staged_execute_sources_into(schedule, sources, views, ninputs, &out_view,
                                                 raise_mask, max_gather_bytes, report, error);
}

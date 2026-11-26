/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#include "b2nd-exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct b2nd_exec_plan {
    me_kernel *kernel;
    b2nd_exec_plan_info info;
    b2nd_exec_node *nodes;
    size_t nnodes;
    uint32_t root;
    b2nd_exec_input_meta inputs[B2ND_EXEC_MAX_INPUTS];
};

struct b2nd_exec_schedule {
    const b2nd_exec_plan *plan;
    b2nd_exec_schedule_info info;
    b2nd_exec_input_meta inputs[B2ND_EXEC_MAX_INPUTS];
};

static me_status exec_error(me_error *error, me_status status, const char *message) {
    if (error) {
        *error = (me_error){0};
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

/* Descriptor admission only: arithmetic inference remains in the typed core.
 * Strings/complex extrema are deliberately not admitted by this first slice. */
static size_t numeric_width(me_dtype dtype) {
    switch (dtype) {
        case ME_BOOL: case ME_INT8: case ME_UINT8: return 1;
        case ME_INT16: case ME_UINT16: return 2;
        case ME_INT32: case ME_UINT32: case ME_FLOAT32: return 4;
        case ME_INT64: case ME_UINT64: case ME_FLOAT64: return 8;
        default: return 0;
    }
}

static me_status validate(const b2nd_exec_descriptor *d, me_error *error) {
    if (!d || d->struct_size != sizeof(*d) || !d->nodes ||
        !d->nnodes || d->nnodes > B2ND_EXEC_MAX_NODES ||
        !d->ninputs || d->ninputs > B2ND_EXEC_MAX_INPUTS || d->root >= d->nnodes) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution descriptor bounds");
    }
    bool slots[B2ND_EXEC_MAX_INPUTS] = {0};
    unsigned depth[B2ND_EXEC_MAX_NODES] = {0};
    size_t captures = 0, inputs = 0;
    for (size_t i = 0; i < d->nnodes; i++) {
        const b2nd_exec_node *n = &d->nodes[i];
        if (n->struct_size != sizeof(*n) || n->id != i || n->nargs > 2) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution node ID or size");
        }
        size_t arity;
        switch (n->op) {
            case B2ND_EXEC_INPUT: case B2ND_EXEC_CAPTURE: arity = 0; break;
            case B2ND_EXEC_NEGATE: case B2ND_EXEC_MIN: arity = 1; break;
            case B2ND_EXEC_ADD: case B2ND_EXEC_SUBTRACT: case B2ND_EXEC_MULTIPLY: arity = 2; break;
            default: return exec_error(error, ME_ERR_UNSUPPORTED, "Unsupported execution operation");
        }
        if (n->nargs != arity) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution operation arity");
        }
        depth[i] = 1;
        for (size_t a = 0; a < n->nargs; a++) {
            if (n->args[a] >= i || d->nodes[n->args[a]].op == B2ND_EXEC_MIN) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Forward, cyclic or nested reduction reference");
            }
            unsigned next = depth[n->args[a]] + 1;
            if (next > depth[i]) {
                depth[i] = next;
            }
        }
        if (depth[i] > B2ND_EXEC_MAX_DEPTH) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Execution graph depth exceeded");
        }
        if (n->op == B2ND_EXEC_INPUT || n->op == B2ND_EXEC_CAPTURE) {
            size_t width = numeric_width(n->dtype);
            if (!width || n->itemsize != width) {
                return exec_error(error, ME_ERR_SIGNATURE, "Unsupported dtype or invalid scalar width");
            }
        }
        if (n->op == B2ND_EXEC_INPUT) {
            if (n->slot >= d->ninputs || slots[n->slot] || n->scalar_size) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Invalid or duplicate execution input slot");
            }
            slots[n->slot] = true;
            inputs++;
        } else if (n->op == B2ND_EXEC_CAPTURE) {
            if (++captures > B2ND_EXEC_MAX_CAPTURES || n->scalar_size != n->itemsize ||
                (n->category != ME_CAPTURE_TYPED && n->category != ME_CAPTURE_WEAK) ||
                (n->dtype == ME_BOOL && n->scalar[0] > 1)) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Malformed execution scalar capture");
            }
        } else if (n->dtype != ME_AUTO || n->itemsize || n->scalar_size) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Operation dtype constraints are not yet supported");
        }
        if (n->op == B2ND_EXEC_MIN && i != d->root) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Only root reductions are supported");
        }
    }
    if (inputs != d->ninputs) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Missing execution input slot");
    }
    return ME_SUCCESS;
}

void b2nd_exec_plan_free(b2nd_exec_plan *plan) {
    if (plan) {
        me_kernel_free(plan->kernel);
        free(plan->nodes);
        free(plan);
    }
}

me_status b2nd_exec_prepare(const b2nd_exec_descriptor *d, const b2nd_exec_options *options,
                           b2nd_exec_plan **out, me_error *error) {
    if (out) {
        *out = NULL;
    }
    if (error) {
        *error = (me_error){0};
    }
    b2nd_exec_options defaults = B2ND_EXEC_OPTIONS_INITIALIZER;
    if (!options) {
        options = &defaults;
    }
    if (!out || options->struct_size != sizeof(*options) ||
        options->execution < ME_EXEC_INTERPRET || options->execution > ME_EXEC_REQUIRE_JIT ||
        options->casting < ME_CAST_UNSAFE || options->casting > ME_CAST_SAFE) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution preparation options");
    }
    me_status status = validate(d, error);
    if (status != ME_SUCCESS) {
        return status;
    }
    b2nd_exec_plan *plan = calloc(1, sizeof(*plan));
    if (!plan) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution plan");
    }
    plan->nodes = malloc(d->nnodes * sizeof(*d->nodes));
    if (!plan->nodes) {
        b2nd_exec_plan_free(plan);
        return exec_error(error, ME_ERR_OOM, "Cannot copy execution descriptor");
    }
    memcpy(plan->nodes, d->nodes, d->nnodes * sizeof(*d->nodes));
    plan->nnodes = d->nnodes;
    plan->root = d->root;
    plan->info.ninputs = d->ninputs;
    plan->info.reduction = d->nodes[d->root].op == B2ND_EXEC_MIN;
    uint32_t map = plan->info.reduction ? d->nodes[d->root].args[0] : d->root;
    plan->info.identity = d->nodes[map].op == B2ND_EXEC_INPUT;
    bool reachable[B2ND_EXEC_MAX_NODES] = {0};
    reachable[map] = true;
    for (size_t i = d->nnodes; i-- > 0;) {
        if (reachable[i]) {
            for (size_t a = 0; a < d->nodes[i].nargs; a++) {
                reachable[d->nodes[i].args[a]] = true;
            }
        }
    }

    /* Bounded node IDs yield bounded generated source. This is a wire-free
     * lowering to the existing Menudet grammar, not another arithmetic engine. */
    size_t capacity = 128 + d->nnodes * 96 + d->ninputs * 24;
    char *source = malloc(capacity);
    if (!source) {
        b2nd_exec_plan_free(plan);
        return exec_error(error, ME_ERR_OOM, "Cannot allocate map lowering");
    }
    me_input_spec inputs[B2ND_EXEC_MAX_INPUTS];
    me_capture captures[B2ND_EXEC_MAX_CAPTURES];
    char input_names[B2ND_EXEC_MAX_INPUTS][24];
    char capture_names[B2ND_EXEC_MAX_CAPTURES][24];
    size_t ncaptures = 0;
    size_t used = (size_t)snprintf(source, capacity, "def mapped(");
    for (size_t slot = 0; slot < d->ninputs; slot++) {
        snprintf(input_names[slot], sizeof(input_names[slot]), "i%zu", slot);
        used += (size_t)snprintf(source + used, capacity - used, "%si%zu", slot ? ", " : "", slot);
    }
    /* Captures are source parameters, but never runtime input slots. */
    for (size_t i = 0; i < d->nnodes; i++) {
        const b2nd_exec_node *n = &plan->nodes[i];
        if (n->op == B2ND_EXEC_INPUT) {
            inputs[n->slot] = (me_input_spec){input_names[n->slot], n->dtype, n->itemsize};
            plan->inputs[n->slot].dtype = n->dtype;
            plan->inputs[n->slot].itemsize = n->itemsize;
        } else if (n->op == B2ND_EXEC_CAPTURE) {
            snprintf(capture_names[ncaptures], sizeof(capture_names[ncaptures]), "c%zu", i);
            captures[ncaptures] = (me_capture){
                {capture_names[ncaptures], n->dtype, n->itemsize}, n->scalar, n->category};
            used += (size_t)snprintf(source + used, capacity - used, ", c%zu", i);
            ncaptures++;
        }
    }
    used += (size_t)snprintf(source + used, capacity - used, "):\n");
    for (size_t i = 0; i < d->nnodes; i++) {
        const b2nd_exec_node *n = &plan->nodes[i];
        if (!reachable[i]) {
            continue;
        }
        switch (n->op) {
            case B2ND_EXEC_INPUT:
                used += (size_t)snprintf(source + used, capacity - used, "    v%zu = i%u\n", i, n->slot);
                break;
            case B2ND_EXEC_CAPTURE:
                used += (size_t)snprintf(source + used, capacity - used, "    v%zu = c%zu\n", i, i);
                break;
            case B2ND_EXEC_NEGATE:
                used += (size_t)snprintf(source + used, capacity - used, "    v%zu = -v%u\n", i, n->args[0]);
                break;
            case B2ND_EXEC_ADD: case B2ND_EXEC_SUBTRACT: case B2ND_EXEC_MULTIPLY:
                used += (size_t)snprintf(source + used, capacity - used, "    v%zu = v%u %c v%u\n",
                    i, n->args[0], n->op == B2ND_EXEC_ADD ? '+' : n->op == B2ND_EXEC_SUBTRACT ? '-' : '*',
                    n->args[1]);
                break;
            case B2ND_EXEC_MIN: break;
        }
    }
    snprintf(source + used, capacity - used, "    return v%u\n", map);
    me_signature signature = ME_SIGNATURE_INITIALIZER;
    signature.inputs = inputs;
    signature.ninputs = d->ninputs;
    signature.captures = captures;
    signature.ncaptures = ncaptures;
    signature.cardinality = ME_ELEMENTWISE;
    signature.casting = options->casting;
    signature.integer_powers = options->integer_powers;
    me_prepare_options policy = ME_PREPARE_OPTIONS_INITIALIZER;
    policy.execution = options->execution;
    policy.inherit_environment = options->inherit_environment;
    policy.menudet_forbidden_features = ME_MENUDET_FEATURE_BLOCK_REDUCTIONS |
        ME_MENUDET_FEATURE_CONTROL_FLOW | ME_MENUDET_FEATURE_FIXED_STRINGS;
    status = me_prepare_menudet("0.2", source, &signature, &policy, &plan->kernel, error);
    free(source);
    if (status == ME_SUCCESS) {
        me_kernel_info info = ME_KERNEL_INFO_INITIALIZER;
        status = me_describe(plan->kernel, &info);
        if (status == ME_SUCCESS && (!numeric_width(info.output_dtype) ||
            info.cardinality != ME_ELEMENTWISE || info.context_ndim)) {
            status = exec_error(error, ME_ERR_UNSUPPORTED, "Unsupported execution map result");
        }
        plan->info.dtype = info.output_dtype;
        plan->info.itemsize = info.output_itemsize;
    }
    if (status != ME_SUCCESS) {
        b2nd_exec_plan_free(plan);
        return status;
    }
    *out = plan;
    return ME_SUCCESS;
}

me_status b2nd_exec_describe(const b2nd_exec_plan *plan, b2nd_exec_plan_info *out) {
    if (!plan || !out) {
        return ME_ERR_INVALID_ARG;
    }
    *out = plan->info;
    return ME_SUCCESS;
}

me_status b2nd_exec_specialize(const b2nd_exec_plan *plan,
                              const b2nd_exec_input_meta *inputs, size_t ninputs,
                              size_t tile_items, size_t max_mapped_bytes,
                              b2nd_exec_schedule **out, me_error *error) {
    if (out) {
        *out = NULL;
    }
    if (!out || !plan || !inputs || ninputs != plan->info.ninputs || !tile_items) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution specialization");
    }
    size_t items = 1;
    if (!inputs[0].ndim || inputs[0].ndim > B2ND_EXEC_MAX_RANK) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution rank");
    }
    for (size_t axis = 0; axis < inputs[0].ndim; axis++) {
        int64_t extent = inputs[0].shape[axis];
        if (extent < 0 || (extent && items > SIZE_MAX / (uint64_t)extent)) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Execution shape size overflow");
        }
        items *= (size_t)extent;
    }
    for (size_t i = 0; i < ninputs; i++) {
        if (inputs[i].dtype != plan->inputs[i].dtype ||
            inputs[i].itemsize != plan->inputs[i].itemsize ||
            inputs[i].ndim != inputs[0].ndim ||
            memcmp(inputs[i].shape, inputs[0].shape, inputs[0].ndim * sizeof(int64_t))) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Execution requires same-shape typed inputs");
        }
        if (items > SIZE_MAX / inputs[i].itemsize) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Execution input byte size overflow");
        }
    }
    if (plan->info.reduction && !items) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Minimum of an empty execution domain");
    }
    if (items > SIZE_MAX / plan->info.itemsize) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution output byte size overflow");
    }
    size_t lanes = items < tile_items ? items : tile_items;
    size_t mapped = plan->info.identity ? 0 : lanes * plan->info.itemsize;
    if (mapped > max_mapped_bytes) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution mapped tile budget exceeded");
    }
    b2nd_exec_schedule *schedule = calloc(1, sizeof(*schedule));
    if (!schedule) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution schedule");
    }
    schedule->plan = plan;
    schedule->info = (b2nd_exec_schedule_info){
        items, lanes, mapped, plan->info.reduction ? sizeof(uint64_t) : 0,
        plan->info.reduction ? 1 : items};
    memcpy(schedule->inputs, inputs, ninputs * sizeof(*inputs));
    *out = schedule;
    return ME_SUCCESS;
}

me_status b2nd_exec_describe_schedule(const b2nd_exec_schedule *schedule, b2nd_exec_schedule_info *out) {
    if (!schedule || !out) {
        return ME_ERR_INVALID_ARG;
    }
    *out = schedule->info;
    return ME_SUCCESS;
}

void b2nd_exec_schedule_free(b2nd_exec_schedule *schedule) {
    free(schedule);
}

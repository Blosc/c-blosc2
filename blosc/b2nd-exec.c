/*********************************************************************
  Copyright (c) 2026 Blosc Development Team
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/
#include "b2nd-exec.h"
#include "miniexpr_reduce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct b2nd_exec_plan {
    me_kernel *kernel;
    me_kernel *initial_kernel;
    uint64_t initial_value;
    b2nd_exec_plan_info info;
    b2nd_exec_node *nodes;
    size_t nnodes;
    uint32_t root;
    uint32_t direct_slot;
    b2nd_exec_input_meta inputs[B2ND_EXEC_MAX_INPUTS];
};

struct b2nd_exec_schedule {
    const b2nd_exec_plan *plan;
    b2nd_exec_schedule_info info;
    b2nd_exec_input_meta inputs[B2ND_EXEC_MAX_INPUTS];
    bool matches_shape[B2ND_EXEC_MAX_INPUTS];
    bool reduced_axis[B2ND_EXEC_MAX_RANK];
    bool suffix_reduction;
    size_t max_mapped_bytes;
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
    return me_extrema_itemsize(dtype);
}

static me_reduction_kind reduction_kind(b2nd_exec_op op) {
    switch (op) {
        case B2ND_EXEC_MIN: return ME_REDUCTION_MIN;
        case B2ND_EXEC_MAX: return ME_REDUCTION_MAX;
        case B2ND_EXEC_SUM: return ME_REDUCTION_SUM;
        case B2ND_EXEC_PROD: return ME_REDUCTION_PROD;
        case B2ND_EXEC_ANY: return ME_REDUCTION_ANY;
        case B2ND_EXEC_ALL: return ME_REDUCTION_ALL;
        default: return ME_REDUCTION_NONE;
    }
}

static void result_info(b2nd_exec_plan_info *out, const me_kernel_info *map) {
    out->map_dtype = map->output_dtype;
    out->map_itemsize = map->output_itemsize;
    out->dtype = me_reduce_accumulator_dtype(map->output_dtype, out->reducer, ME_AUTO);
    out->itemsize = numeric_width(out->dtype);
    out->cardinality = map->cardinality;
    out->context_ndim = map->context_ndim;
}

static bool load_scalar_u64(me_dtype dtype, const void *src, uint64_t *out) {
    size_t width = numeric_width(dtype);
    if (!width || !src || !out) {
        return false;
    }
    if (dtype == ME_FLOAT32 || dtype == ME_FLOAT64) {
        *out = 0;
        memcpy(out, src, width);
        return true;
    }
    switch (dtype) {
        case ME_BOOL: {
            uint8_t v;
            memcpy(&v, src, sizeof(v));
            *out = v ? 1 : 0;
            return true;
        }
        case ME_INT8: {
            int8_t v;
            memcpy(&v, src, sizeof(v));
            *out = (uint64_t)(int64_t)v;
            return true;
        }
        case ME_INT16: {
            int16_t v;
            memcpy(&v, src, sizeof(v));
            *out = (uint64_t)(int64_t)v;
            return true;
        }
        case ME_INT32: {
            int32_t v;
            memcpy(&v, src, sizeof(v));
            *out = (uint64_t)(int64_t)v;
            return true;
        }
        case ME_INT64: {
            int64_t v;
            memcpy(&v, src, sizeof(v));
            *out = (uint64_t)v;
            return true;
        }
        case ME_UINT8: {
            uint8_t v;
            memcpy(&v, src, sizeof(v));
            *out = v;
            return true;
        }
        case ME_UINT16: {
            uint16_t v;
            memcpy(&v, src, sizeof(v));
            *out = v;
            return true;
        }
        case ME_UINT32: {
            uint32_t v;
            memcpy(&v, src, sizeof(v));
            *out = v;
            return true;
        }
        case ME_UINT64: {
            uint64_t v;
            memcpy(&v, src, sizeof(v));
            *out = v;
            return true;
        }
        default:
            return false;
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
            case B2ND_EXEC_NEGATE: case B2ND_EXEC_MIN: case B2ND_EXEC_MAX:
            case B2ND_EXEC_SUM: case B2ND_EXEC_PROD:
            case B2ND_EXEC_ANY: case B2ND_EXEC_ALL: arity = 1; break;
            case B2ND_EXEC_ADD: case B2ND_EXEC_SUBTRACT: case B2ND_EXEC_MULTIPLY: arity = 2; break;
            default: return exec_error(error, ME_ERR_UNSUPPORTED, "Unsupported execution operation");
        }
        if (n->nargs != arity) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution operation arity");
        }
        depth[i] = 1;
        for (size_t a = 0; a < n->nargs; a++) {
            if (n->args[a] >= i || reduction_kind(d->nodes[n->args[a]].op) != ME_REDUCTION_NONE) {
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
        if (reduction_kind(n->op) != ME_REDUCTION_NONE) {
            if (i != d->root) {
                return exec_error(error, ME_ERR_UNSUPPORTED, "Only root reductions are supported");
            }
            if ((!n->has_axes && n->naxes) || n->naxes > B2ND_EXEC_MAX_RANK) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution reduction axes");
            }
            for (size_t k = 0; k < n->naxes; k++) {
                if (n->axes[k] < -B2ND_EXEC_MAX_RANK || n->axes[k] >= B2ND_EXEC_MAX_RANK) {
                    return exec_error(error, ME_ERR_INVALID_ARG, "Reduction axis exceeds maximum rank");
                }
            }
            if (n->has_initial) {
                if (n->op == B2ND_EXEC_ANY || n->op == B2ND_EXEC_ALL) {
                    return exec_error(error, ME_ERR_INVALID_ARG,
                                      "Truth reductions do not accept initial values");
                }
                size_t iw = numeric_width(n->initial_dtype);
                if (!iw || n->initial_size != iw ||
                    (n->initial_category != ME_CAPTURE_TYPED &&
                     n->initial_category != ME_CAPTURE_WEAK) ||
                    (n->initial_dtype == ME_BOOL && n->initial_scalar[0] > 1)) {
                    return exec_error(error, ME_ERR_INVALID_ARG,
                                      "Malformed execution reduction initial scalar");
                }
            } else if (n->initial_size || n->initial_dtype != ME_AUTO) {
                return exec_error(error, ME_ERR_INVALID_ARG,
                                  "Unexpected initial payload without has_initial");
            }
        } else if (n->has_axes || n->naxes || n->keepdims || n->has_initial ||
                   n->initial_size || n->initial_dtype != ME_AUTO) {
            return exec_error(error, ME_ERR_INVALID_ARG,
                              "Unexpected reduction options on non-reduction node");
        }
    }
    if (inputs != d->ninputs) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Missing execution input slot");
    }
    return ME_SUCCESS;
}

static bool emit_text(char *source, size_t capacity, size_t *used, const char *text) {
    size_t length = strlen(text);
    if (length >= capacity - *used) {
        return false;
    }
    memcpy(source + *used, text, length + 1);
    *used += length;
    return true;
}

/* Inline single-use arithmetic into one fused expression. Only shared
 * computed nodes need temporaries: blindly expanding a DAG is exponential,
 * while assigning every input/capture inhibits core batch fusion. */
static bool emit_map(char *source, size_t capacity, size_t *used,
                     const b2nd_exec_node *nodes, const unsigned *uses,
                     uint32_t id, bool definition) {
    const b2nd_exec_node *n = &nodes[id];
    char name[24];
    if (n->op == B2ND_EXEC_INPUT || n->op == B2ND_EXEC_CAPTURE || (uses[id] > 1 && !definition)) {
        snprintf(name, sizeof(name), "%c%u",
            n->op == B2ND_EXEC_INPUT ? 'i' : n->op == B2ND_EXEC_CAPTURE ? 'c' : 'v',
            n->op == B2ND_EXEC_INPUT ? n->slot : id);
        return emit_text(source, capacity, used, name);
    }
    if (!emit_text(source, capacity, used, n->op == B2ND_EXEC_NEGATE ? "(-" : "(") ||
        !emit_map(source, capacity, used, nodes, uses, n->args[0], false)) {
        return false;
    }
    if (n->nargs == 2) {
        const char *op = n->op == B2ND_EXEC_ADD ? " + " : n->op == B2ND_EXEC_SUBTRACT ? " - " : " * ";
        if (!emit_text(source, capacity, used, op) ||
            !emit_map(source, capacity, used, nodes, uses, n->args[1], false)) {
            return false;
        }
    }
    return emit_text(source, capacity, used, ")");
}

void b2nd_exec_plan_free(b2nd_exec_plan *plan) {
    if (plan) {
        me_kernel_free(plan->kernel);
        me_kernel_free(plan->initial_kernel);
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
    plan->info.reducer = reduction_kind(d->nodes[d->root].op);
    plan->info.reduction = plan->info.reducer != ME_REDUCTION_NONE;
    /* Current DAG map ops are all lane-local; ordered sums/products stay serial. */
    plan->info.partitionable = plan->info.reducer != ME_REDUCTION_SUM &&
        plan->info.reducer != ME_REDUCTION_PROD;
    uint32_t map = plan->info.reduction ? d->nodes[d->root].args[0] : d->root;
    plan->info.identity = d->nodes[map].op == B2ND_EXEC_INPUT;
    plan->direct_slot = plan->info.identity ? d->nodes[map].slot : 0;
    bool reachable[B2ND_EXEC_MAX_NODES] = {0};
    unsigned uses[B2ND_EXEC_MAX_NODES] = {0};
    reachable[map] = true;
    for (size_t i = d->nnodes; i-- > 0;) {
        if (reachable[i]) {
            for (size_t a = 0; a < d->nodes[i].nargs; a++) {
                reachable[d->nodes[i].args[a]] = true;
                uses[d->nodes[i].args[a]]++;
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
    size_t used = (size_t)snprintf(source, capacity, "# me:fp=strict\ndef mapped(");
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
    bool emitted = true;
    for (size_t i = 0; i < d->nnodes && emitted; i++) {
        if (reachable[i] && uses[i] > 1 && d->nodes[i].nargs) {
            char prefix[32];
            snprintf(prefix, sizeof(prefix), "    v%zu = ", i);
            emitted = emit_text(source, capacity, &used, prefix) &&
                emit_map(source, capacity, &used, d->nodes, uses, (uint32_t)i, true) &&
                emit_text(source, capacity, &used, "\n");
        }
    }
    emitted = emitted && emit_text(source, capacity, &used, "    return ") &&
        emit_map(source, capacity, &used, d->nodes, uses, map, false) &&
        emit_text(source, capacity, &used, "\n");
    if (!emitted) {
        free(source);
        b2nd_exec_plan_free(plan);
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution map lowering exceeded source bound");
    }
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
        result_info(&plan->info, &info);
    }
    if (status == ME_SUCCESS && plan->info.reduction) {
        const b2nd_exec_node *root_node = &d->nodes[d->root];
        plan->info.has_axes = root_node->has_axes;
        plan->info.naxes = root_node->naxes;
        if (root_node->naxes) {
            memcpy(plan->info.axes, root_node->axes, root_node->naxes * sizeof(int));
        }
        plan->info.keepdims = root_node->keepdims;
        plan->info.has_initial = root_node->has_initial;
        if (root_node->has_initial) {
            if (root_node->initial_dtype == plan->info.dtype) {
                if (!load_scalar_u64(plan->info.dtype, root_node->initial_scalar,
                                     &plan->initial_value)) {
                    status = exec_error(error, ME_ERR_INVALID_ARG,
                                        "Invalid reduction initial scalar");
                }
            } else {
                me_capture init_capture = {
                    {"initial", root_node->initial_dtype, root_node->initial_size},
                    root_node->initial_scalar, root_node->initial_category};
                me_signature init_sig = ME_SIGNATURE_INITIALIZER;
                init_sig.captures = &init_capture;
                init_sig.ncaptures = 1;
                init_sig.output_dtype = plan->info.dtype;
                init_sig.cardinality = ME_ELEMENTWISE;
                init_sig.casting = ME_CAST_UNSAFE;
                me_prepare_options init_policy = ME_PREPARE_OPTIONS_INITIALIZER;
                init_policy.execution = ME_EXEC_INTERPRET;
                status = me_prepare_menudet(
                    "0.2", "# me:fp=strict\ndef convert(initial):\n    return initial\n",
                    &init_sig, &init_policy, &plan->initial_kernel, error);
            }
        }
    }
    if (status != ME_SUCCESS) {
        b2nd_exec_plan_free(plan);
        return status;
    }
    plan->info.plan_metadata_bytes = sizeof(*plan) + plan->nnodes * sizeof(*plan->nodes);
    *out = plan;
    return ME_SUCCESS;
}

me_status b2nd_exec_prepare_kernel(me_kernel **kernel, me_reduction_kind reducer,
                                  b2nd_exec_plan **out, me_error *error) {
    if (out) {
        *out = NULL;
    }
    if (!kernel || !*kernel || !out) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid direct execution kernel");
    }
    me_kernel_info info = ME_KERNEL_INFO_INITIALIZER;
    size_t count = 0;
    if (me_describe(*kernel, &info) != ME_SUCCESS ||
        me_describe_input_count(*kernel, &count) != ME_SUCCESS ||
        count > B2ND_EXEC_MAX_INPUTS || !info.output_itemsize) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid direct kernel metadata");
    }
    if (reducer < ME_REDUCTION_NONE || reducer > ME_REDUCTION_ALL ||
        (reducer != ME_REDUCTION_NONE &&
         (info.cardinality != ME_ELEMENTWISE || !numeric_width(info.output_dtype)))) {
        return exec_error(error, ME_ERR_UNSUPPORTED, "Direct kernel cannot feed this reducer");
    }
    b2nd_exec_plan *plan = calloc(1, sizeof(*plan));
    if (!plan) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate direct execution plan");
    }
    plan->info.ninputs = count;
    plan->info.reducer = reducer;
    plan->info.reduction = reducer != ME_REDUCTION_NONE;
    plan->info.partitionable = false;
    result_info(&plan->info, &info);
    /* Direct complex/string kernels remain inspectable, not resident-admitted. */
    if (reducer == ME_REDUCTION_NONE) {
        plan->info.dtype = info.output_dtype;
        plan->info.itemsize = info.output_itemsize;
    }
    for (size_t i = 0; i < count; i++) {
        me_input_spec input;
        if (me_describe_input(*kernel, i, &input) != ME_SUCCESS) {
            free(plan);
            return exec_error(error, ME_ERR_INVALID_ARG, "Cannot describe direct kernel input");
        }
        plan->inputs[i].dtype = input.dtype;
        plan->inputs[i].itemsize = input.itemsize;
    }
    plan->kernel = *kernel;
    *kernel = NULL;
    plan->info.plan_metadata_bytes = sizeof(*plan);
    *out = plan;
    return ME_SUCCESS;
}

me_status b2nd_exec_describe(const b2nd_exec_plan *plan, b2nd_exec_plan_info *out) {
    if (!plan || !out) {
        return ME_ERR_INVALID_ARG;
    }
    *out = plan->info;
    out->has_jit = me_kernel_has_jit(plan->kernel);
    return ME_SUCCESS;
}

me_status b2nd_exec_specialize_ex(const b2nd_exec_plan *plan,
                                 const b2nd_exec_input_meta *inputs, size_t ninputs,
                                 size_t tile_items, size_t max_mapped_bytes,
                                 bool pairwise_sum,
                                 b2nd_exec_schedule **out, me_error *error) {
    if (out) {
        *out = NULL;
    }
    if (!out || !plan || !inputs || ninputs != plan->info.ninputs || !tile_items) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution specialization");
    }
    if (!ninputs || plan->info.cardinality != ME_ELEMENTWISE || plan->info.context_ndim ||
        !numeric_width(plan->info.map_dtype)) {
        return exec_error(error, ME_ERR_UNSUPPORTED, "Resident execution requires a context-free elementwise domain");
    }
    if (pairwise_sum &&
        (plan->info.reducer != ME_REDUCTION_SUM ||
         (plan->info.map_dtype != ME_FLOAT32 && plan->info.map_dtype != ME_FLOAT64) ||
         plan->info.dtype != plan->info.map_dtype)) {
        return exec_error(error, ME_ERR_UNSUPPORTED,
                          "Pairwise sum requires matching floating map and accumulator dtypes");
    }
    size_t out_ndim = 0;
    for (size_t i = 0; i < ninputs; i++) {
        if (!numeric_width(inputs[i].dtype)) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Resident execution requires real numeric inputs");
        }
        if (inputs[i].dtype != plan->inputs[i].dtype ||
            inputs[i].itemsize != plan->inputs[i].itemsize) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Execution input dtype mismatch");
        }
        if (inputs[i].ndim > B2ND_EXEC_MAX_RANK) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution rank");
        }
        size_t in_items = 1;
        for (size_t axis = 0; axis < inputs[i].ndim; axis++) {
            int64_t extent = inputs[i].shape[axis];
            if (extent < 0 || (extent && in_items > SIZE_MAX / (uint64_t)extent)) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Execution shape size overflow");
            }
            in_items *= (size_t)extent;
        }
        if (in_items > SIZE_MAX / inputs[i].itemsize) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Execution input byte size overflow");
        }
        if (inputs[i].ndim > out_ndim) {
            out_ndim = inputs[i].ndim;
        }
    }
    int64_t out_shape[B2ND_EXEC_MAX_RANK] = {0};
    size_t items = 1;
    for (size_t axis = 0; axis < out_ndim; axis++) {
        int64_t extent = 1;
        for (size_t i = 0; i < ninputs; i++) {
            if (inputs[i].ndim + axis >= out_ndim) {
                int64_t in_extent = inputs[i].shape[inputs[i].ndim + axis - out_ndim];
                if (in_extent != 1) {
                    if (extent == 1) {
                        extent = in_extent;
                    } else if (extent != in_extent) {
                        return exec_error(error, ME_ERR_INVALID_ARG, "Incompatible execution broadcast shapes");
                    }
                }
            }
        }
        out_shape[axis] = extent;
        if (extent && items > SIZE_MAX / (uint64_t)extent) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Execution shape size overflow");
        }
        items *= (size_t)extent;
    }
    bool reduced_axis[B2ND_EXEC_MAX_RANK] = {0};
    bool suffix = true;
    size_t output_ndim = out_ndim;
    int64_t output_shape[B2ND_EXEC_MAX_RANK] = {0};
    size_t groups = 1;
    size_t red_count = items;
    size_t output_items = items;
    if (plan->info.reduction) {
        if (!plan->info.has_axes) {
            for (size_t a = 0; a < out_ndim; a++) {
                reduced_axis[a] = true;
            }
        } else {
            if (plan->info.naxes > out_ndim) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Reduction axis out of range");
            }
            for (size_t k = 0; k < plan->info.naxes; k++) {
                int axis = plan->info.axes[k] < 0 ?
                    plan->info.axes[k] + (int)out_ndim : plan->info.axes[k];
                if (axis < 0 || (size_t)axis >= out_ndim || reduced_axis[axis]) {
                    return exec_error(error, ME_ERR_INVALID_ARG, "Invalid or duplicate reduction axis");
                }
                reduced_axis[axis] = true;
            }
        }
        bool seen_reduced = false;
        output_ndim = 0;
        red_count = 1;
        for (size_t a = 0; a < out_ndim; a++) {
            if (reduced_axis[a]) {
                seen_reduced = true;
                if (out_shape[a] && red_count > SIZE_MAX / (uint64_t)out_shape[a]) {
                    return exec_error(error, ME_ERR_INVALID_ARG, "Execution reduction size overflow");
                }
                red_count *= (size_t)out_shape[a];
            } else if (seen_reduced) {
                suffix = false;
            }
            if (!reduced_axis[a] || plan->info.keepdims) {
                int64_t extent = reduced_axis[a] ? 1 : out_shape[a];
                output_shape[output_ndim++] = extent;
                if (extent && groups > SIZE_MAX / (uint64_t)extent) {
                    return exec_error(error, ME_ERR_INVALID_ARG, "Execution output size overflow");
                }
                groups *= (size_t)extent;
            }
        }
        output_items = groups;
        if ((plan->info.reducer == ME_REDUCTION_MIN || plan->info.reducer == ME_REDUCTION_MAX) &&
            !plan->info.has_initial && !red_count && groups) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Extremum of an empty execution domain");
        }
    } else if (out_ndim) {
        memcpy(output_shape, out_shape, out_ndim * sizeof(int64_t));
    }
    if (output_items > SIZE_MAX / plan->info.itemsize) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution output byte size overflow");
    }
    size_t group_lanes = plan->info.reduction ? red_count : items;
    size_t lanes = !items ? 0 : (group_lanes < tile_items ? group_lanes : tile_items);
    if (!plan->info.identity && lanes > INT_MAX) {
        return exec_error(error, ME_ERR_UNSUPPORTED, "Execution tile exceeds the core lane limit");
    }
    if (lanes > SIZE_MAX / plan->info.map_itemsize) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution map tile size overflow");
    }
    size_t mapped = (plan->info.identity || !plan->info.reduction || !lanes) ?
        0 : lanes * plan->info.map_itemsize;
    if (mapped > max_mapped_bytes) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution mapped tile budget exceeded");
    }
    bool matches[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t broadcast_gather = 0;
    for (size_t i = 0; i < ninputs; i++) {
        matches[i] = inputs[i].ndim == out_ndim &&
            (!out_ndim || memcmp(inputs[i].shape, out_shape, out_ndim * sizeof(int64_t)) == 0);
        if (lanes && (!matches[i] || !suffix) && (!plan->info.identity || i == plan->direct_slot)) {
            if (lanes > SIZE_MAX / inputs[i].itemsize) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Execution broadcast gather size overflow");
            }
            size_t tile_bytes = lanes * inputs[i].itemsize;
            if (broadcast_gather > SIZE_MAX - tile_bytes) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Execution broadcast gather size overflow");
            }
            broadcast_gather += tile_bytes;
        }
    }
    b2nd_exec_schedule *schedule = calloc(1, sizeof(*schedule));
    if (!schedule) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution schedule");
    }
    schedule->plan = plan;
    schedule->info = (b2nd_exec_schedule_info){
        items, lanes, mapped,
        pairwise_sum ? sizeof(me_pairwise_sum_state) : (plan->info.reduction ? sizeof(uint64_t) : 0),
        output_items, sizeof(*schedule),
        out_ndim, {0}, broadcast_gather,
        output_ndim, {0}, groups, red_count, pairwise_sum};
    if (out_ndim) {
        memcpy(schedule->info.shape, out_shape, out_ndim * sizeof(int64_t));
    }
    if (output_ndim) {
        memcpy(schedule->info.output_shape, output_shape, output_ndim * sizeof(int64_t));
    }
    memcpy(schedule->inputs, inputs, ninputs * sizeof(*inputs));
    memcpy(schedule->matches_shape, matches, ninputs * sizeof(bool));
    memcpy(schedule->reduced_axis, reduced_axis, sizeof(reduced_axis));
    schedule->suffix_reduction = suffix;
    schedule->max_mapped_bytes = max_mapped_bytes;
    *out = schedule;
    return ME_SUCCESS;
}

me_status b2nd_exec_specialize(const b2nd_exec_plan *plan,
                              const b2nd_exec_input_meta *inputs, size_t ninputs,
                              size_t tile_items, size_t max_mapped_bytes,
                              b2nd_exec_schedule **out, me_error *error) {
    return b2nd_exec_specialize_ex(plan, inputs, ninputs, tile_items, max_mapped_bytes,
                                   false, out, error);
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

static bool valid_extent(const void *data, size_t capacity, size_t bytes, size_t alignment) {
    return capacity >= bytes && (!bytes ||
        (data && (uintptr_t)data % alignment == 0 && (uintptr_t)data <= UINTPTR_MAX - bytes));
}

static bool view_add(size_t base, int64_t stride, uint64_t count, size_t *out) {
    uint64_t magnitude = stride < 0 ? (uint64_t)(-(stride + 1)) + 1 : (uint64_t)stride;
    if (magnitude && count > SIZE_MAX / magnitude) {
        return false;
    }
    size_t bytes = (size_t)(magnitude * count);
    if (stride < 0) {
        if (bytes > base) {
            return false;
        }
        *out = base - bytes;
    } else {
        if (base > SIZE_MAX - bytes) {
            return false;
        }
        *out = base + bytes;
    }
    return true;
}

static bool view_valid(const b2nd_exec_view *v, const b2nd_exec_input_meta *m) {
    if (!v || v->struct_size != sizeof(*v) || v->byte_order > 2 || v->offset > v->capacity) {
        return false;
    }
    if (v->capacity && (!v->base || (uintptr_t)v->base > UINTPTR_MAX - v->capacity)) {
        return false;
    }
    size_t n = 1;
    for (size_t i = 0; i < m->ndim; i++) {
        n *= (size_t)m->shape[i];
    }
    if (!n) {
        return true;
    }
    size_t low = v->offset, high = v->offset;
    for (size_t i = 0; i < m->ndim; i++) {
        if (!view_add(v->strides[i] < 0 ? low : high, v->strides[i],
                      (uint64_t)m->shape[i] - 1,
                      v->strides[i] < 0 ? &low : &high)) {
            return false;
        }
    }
    return v->base && high <= v->capacity && m->itemsize <= v->capacity - high;
}

static bool view_native(const b2nd_exec_view *v) {
    uint16_t x = 1;
    return !v->byte_order || v->byte_order == (*(const uint8_t *)&x ? 1u : 2u);
}

static bool view_contiguous(const b2nd_exec_view *v, const b2nd_exec_input_meta *m) {
    size_t stride = m->itemsize;
    for (size_t i = m->ndim; i-- > 0;) {
        if (m->shape[i] > 1 && (v->strides[i] < 0 || (uint64_t)v->strides[i] != stride)) {
            return false;
        }
        if (m->shape[i] && stride > INT64_MAX / (uint64_t)m->shape[i]) {
            return false;
        }
        stride *= (size_t)m->shape[i];
    }
    return true;
}

static void view_load(const b2nd_exec_view *v, size_t offset, void *dst, size_t width) {
    const uint8_t *p = (const uint8_t *)v->base + offset;
    if (view_native(v)) {
        memcpy(dst, p, width);
    } else {
        for (size_t i = 0; i < width; i++) {
            ((uint8_t *)dst)[i] = p[width - i - 1];
        }
    }
}

static bool output_valid(const b2nd_exec_output_view *out, size_t ndim,
                         const int64_t *shape, size_t width, size_t nitems) {
    if (!out || out->struct_size != sizeof(*out) || out->offset > out->capacity) {
        return false;
    }
    if (out->capacity && (!out->base || (uintptr_t)out->base > UINTPTR_MAX - out->capacity)) {
        return false;
    }
    if (!nitems) {
        return true;
    }
    size_t low = out->offset, high = out->offset;
    for (size_t i = 0; i < ndim; i++) {
        if (!view_add(out->strides[i] < 0 ? low : high, out->strides[i],
                      (uint64_t)shape[i] - 1, out->strides[i] < 0 ? &low : &high)) {
            return false;
        }
    }
    if (!out->base || high > out->capacity || width > out->capacity - high ||
        ((uintptr_t)out->base + out->offset) % width != 0) {
        return false;
    }
    uint64_t strides[B2ND_EXEC_MAX_RANK], extents[B2ND_EXEC_MAX_RANK];
    size_t axes = 0;
    for (size_t i = 0; i < ndim; i++) {
        if (shape[i] > 1) {
            int64_t stride = out->strides[i];
            uint64_t magnitude = stride < 0 ? (uint64_t)(-(stride + 1)) + 1 : (uint64_t)stride;
            if (!magnitude || magnitude % width) {
                return false;
            }
            size_t j = axes++;
            while (j && strides[j - 1] > magnitude) {
                strides[j] = strides[j - 1];
                extents[j] = extents[j - 1];
                j--;
            }
            strides[j] = magnitude;
            extents[j] = (uint64_t)shape[i];
        }
    }
    size_t span = width;
    for (size_t i = 0; i < axes; i++) {
        if (strides[i] < span || strides[i] > SIZE_MAX ||
            extents[i] - 1 > (SIZE_MAX - span) / strides[i]) {
            return false;
        }
        span += (size_t)((extents[i] - 1) * strides[i]);
    }
    return true;
}

static bool output_contiguous(const b2nd_exec_output_view *out, size_t ndim,
                              const int64_t *shape, size_t width) {
    size_t stride = width;
    for (size_t i = ndim; i-- > 0;) {
        if (shape[i] > 1 && (out->strides[i] < 0 || (uint64_t)out->strides[i] != stride)) {
            return false;
        }
        if (shape[i] && stride > INT64_MAX / (uint64_t)shape[i]) {
            return false;
        }
        stride *= (size_t)shape[i];
    }
    return true;
}

static size_t output_offset(const b2nd_exec_output_view *out, size_t ndim,
                            const int64_t *shape, size_t linear_index) {
    int64_t coords[B2ND_EXEC_MAX_RANK] = {0};
    size_t tmp = linear_index;
    for (size_t a = ndim; a-- > 0;) {
        size_t ext = (size_t)shape[a];
        coords[a] = (int64_t)(tmp % ext);
        tmp /= ext;
    }
    size_t offset = out->offset;
    for (size_t a = 0; a < ndim; a++) {
        if (shape[a] != 1) {
            view_add(offset, out->strides[a], (uint64_t)coords[a], &offset);
        }
    }
    return offset;
}

static void output_scatter_tile(const b2nd_exec_output_view *out, size_t ndim,
                                const int64_t *shape, size_t pos, size_t count,
                                const unsigned char *src, size_t width) {
    int64_t coords[B2ND_EXEC_MAX_RANK] = {0};
    size_t tmp = pos;
    for (size_t a = ndim; a-- > 0;) {
        size_t ext = (size_t)shape[a];
        coords[a] = (int64_t)(tmp % ext);
        tmp /= ext;
    }
    for (size_t lane = 0; lane < count; lane++) {
        size_t offset = out->offset;
        for (size_t a = 0; a < ndim; a++) {
            if (shape[a] != 1) {
                view_add(offset, out->strides[a], (uint64_t)coords[a], &offset);
            }
        }
        memcpy((unsigned char *)out->base + offset, src + lane * width, width);
        for (size_t a = ndim; a-- > 0;) {
            if (++coords[a] < shape[a]) {
                break;
            }
            coords[a] = 0;
        }
    }
}

typedef struct {
    const b2nd_exec_schedule *schedule;
    const b2nd_exec_view *views;
    const bool *direct;
    size_t ninputs;
    const b2nd_exec_output_view *out_view;
    bool scatter;
    unsigned raise_mask;
    unsigned char *mapped; /* This job's mapped tile, already bounded by the caller. */
    unsigned char *gather_slots[B2ND_EXEC_MAX_INPUTS];
    size_t start;          /* First tile (groups <= 1) or first group (groups > 1). */
    size_t end;            /* Past-last tile or group. */
    bool has_initial;
    uint64_t initial_val;
    bool has_init_state;
    uint64_t partial;
    size_t tiles, map_calls, direct_tiles, gathered_bytes;
    size_t interpreter_calls, jit_calls;
    me_fp_status fp;
    bool fp_seen;
    bool fp_scoped;
    me_status status;
    me_error error;
} exec_job;

static void view_gather_tile(const b2nd_exec_schedule *schedule, size_t input_idx,
                             const b2nd_exec_view *view, size_t group, size_t pos,
                             size_t count, unsigned char *dst) {
    const b2nd_exec_plan *plan = schedule->plan;
    const b2nd_exec_input_meta *meta = &schedule->inputs[input_idx];
    size_t width = meta->itemsize;
    size_t out_ndim = schedule->info.ndim;
    const int64_t *out_shape = schedule->info.shape;
    if (schedule->suffix_reduction && schedule->matches_shape[input_idx] && out_ndim == 1) {
        size_t linear = group * schedule->info.reduction_lanes + pos;
        size_t offset = view->offset;
        view_add(offset, view->strides[0], (uint64_t)linear, &offset);
        for (size_t lane = 0; lane < count; lane++) {
            view_load(view, offset, dst + lane * width, width);
            if (lane + 1 < count) {
                view_add(offset, view->strides[0], 1, &offset);
            }
        }
        return;
    }
    int64_t coords[B2ND_EXEC_MAX_RANK] = {0};
    size_t g_rem = group;
    size_t l_rem = pos;
    for (size_t a = out_ndim; a-- > 0;) {
        size_t *idx = (plan->info.reduction && !schedule->reduced_axis[a]) ? &g_rem : &l_rem;
        coords[a] = (int64_t)(*idx % (size_t)out_shape[a]);
        *idx /= (size_t)out_shape[a];
    }
    size_t shift = out_ndim - meta->ndim;
    for (size_t lane = 0; lane < count; lane++) {
        size_t offset = view->offset;
        for (size_t a = 0; a < meta->ndim; a++) {
            if (meta->shape[a] != 1) {
                view_add(offset, view->strides[a], (uint64_t)coords[shift + a], &offset);
            }
        }
        view_load(view, offset, dst + lane * width, width);
        if (lane + 1 < count) {
            for (size_t a = out_ndim; a-- > 0;) {
                if (plan->info.reduction && !schedule->reduced_axis[a]) {
                    continue;
                }
                if (++coords[a] < out_shape[a]) {
                    break;
                }
                coords[a] = 0;
            }
        }
    }
}

static void exec_reduce_values(const b2nd_exec_plan *plan, const void *values, size_t count,
                               uint64_t *acc, bool initialized, me_error *error, me_status *status) {
    if (plan->info.reducer == ME_REDUCTION_SUM) {
        if (!me_sum_span(plan->info.map_dtype, values, count, *acc, false, acc)) {
            *status = exec_error(error, ME_ERR_EVAL, "Execution reduction failed");
        }
    } else if (plan->info.reducer == ME_REDUCTION_PROD) {
        if (!me_prod_span(plan->info.map_dtype, values, count, *acc, false, acc)) {
            *status = exec_error(error, ME_ERR_EVAL, "Execution reduction failed");
        }
    } else if (plan->info.reducer == ME_REDUCTION_ANY || plan->info.reducer == ME_REDUCTION_ALL) {
        bool truth = false;
        if (!me_truth_span(plan->info.map_dtype, values, count, *acc != 0,
                           plan->info.reducer == ME_REDUCTION_ALL, &truth)) {
            *status = exec_error(error, ME_ERR_EVAL, "Execution reduction failed");
        } else {
            *acc = truth ? 1 : 0;
        }
    } else if (!me_extrema_span(plan->info.map_dtype, values, count, *acc, initialized,
                                plan->info.reducer == ME_REDUCTION_MIN, acc)) {
        *status = exec_error(error, ME_ERR_EVAL, "Execution reduction failed");
    }
}

static bool exec_combine_partial(me_reduction_kind reducer, me_dtype dtype,
                                 uint64_t *acc, uint64_t part, bool initialized) {
    if (reducer == ME_REDUCTION_ANY) {
        *acc = (*acc || part) ? 1 : 0;
        return true;
    }
    if (reducer == ME_REDUCTION_ALL) {
        *acc = (*acc && part) ? 1 : 0;
        return true;
    }
    uint8_t buf[16] = {0};
    if (!me_reduce_store_scalar(dtype, part, buf)) {
        return false;
    }
    return me_extrema_span(dtype, buf, 1, *acc, initialized, reducer == ME_REDUCTION_MIN, acc);
}

static uint64_t exec_pairwise_finish(me_dtype dtype, me_pairwise_sum_state *pw,
                                     bool has_initial, uint64_t initial_val, bool has_any) {
    if (!has_any) {
        return has_initial ? initial_val : 0;
    }
    uint64_t tail = 0;
    me_pairwise_sum_finish(pw, &tail);
    if (!has_initial) {
        return tail;
    }
    uint64_t out = 0;
    if (dtype == ME_FLOAT32) {
        float a, b;
        memcpy(&a, &initial_val, sizeof(a));
        memcpy(&b, &tail, sizeof(b));
        float sum = a + b;
        memcpy(&out, &sum, sizeof(sum));
    } else {
        double a, b;
        memcpy(&a, &initial_val, sizeof(a));
        memcpy(&b, &tail, sizeof(b));
        double sum = a + b;
        memcpy(&out, &sum, sizeof(sum));
    }
    return out;
}

static void exec_eval_tile(exec_job *job, size_t group, size_t pos, size_t count,
                           void *direct_out, const void **out_values) {
    const b2nd_exec_schedule *schedule = job->schedule;
    const b2nd_exec_plan *plan = schedule->plan;
    size_t width = plan->info.itemsize;
    size_t map_width = plan->info.map_itemsize;
    size_t red_count = schedule->info.reduction_lanes;
    uint32_t direct_slot = plan->direct_slot;
    const void *values;
    if (plan->info.identity) {
        if (job->direct[direct_slot]) {
            size_t linear = group * red_count + pos;
            values = (const unsigned char *)job->views[direct_slot].base +
                job->views[direct_slot].offset + linear * map_width;
        } else {
            view_gather_tile(schedule, direct_slot, &job->views[direct_slot],
                             group, pos, count, job->gather_slots[direct_slot]);
            job->gathered_bytes += count * map_width;
            values = job->gather_slots[direct_slot];
        }
        job->direct_tiles++;
        if (!plan->info.reduction) {
            if (job->scatter) {
                output_scatter_tile(job->out_view, schedule->info.output_ndim,
                                    schedule->info.output_shape, pos, count,
                                    (const unsigned char *)values, width);
            } else {
                memcpy(direct_out, values, count * width);
            }
        }
        *out_values = values;
        return;
    }
    me_buffer spans[B2ND_EXEC_MAX_INPUTS];
    for (size_t i = 0; i < job->ninputs; i++) {
        size_t input_width = schedule->inputs[i].itemsize;
        if (job->direct[i]) {
            size_t linear = group * red_count + pos;
            spans[i] = (me_buffer){
                (const unsigned char *)job->views[i].base +
                    job->views[i].offset + linear * input_width,
                count * input_width};
        } else {
            view_gather_tile(schedule, i, &job->views[i],
                             group, pos, count, job->gather_slots[i]);
            job->gathered_bytes += count * input_width;
            spans[i] = (me_buffer){job->gather_slots[i], count * input_width};
        }
    }
    me_block block = ME_BLOCK_INITIALIZER;
    block.nitems = count;
    me_run_options options = ME_RUN_OPTIONS_INITIALIZER;
    me_fp_status fp = {0};
    options.fp_status = &fp;
    /* The graph adapter checks the union after traversal, including
     * reducer flags. A raise mask must not truncate later tiles. */
    options.raise_mask = 0;
    void *target = (plan->info.reduction || job->scatter) ? job->mapped : direct_out;
    me_execution_backend backend = ME_BACKEND_NONE;
    job->status = me_eval_block_backend(plan->kernel, spans, job->ninputs,
        (me_mut_buffer){target, count * map_width}, &block, &options, &backend, &job->error);
    job->interpreter_calls += backend == ME_BACKEND_INTERPRETER;
    job->jit_calls += backend == ME_BACKEND_JIT;
    job->fp.flags |= fp.flags;
    job->fp.supported = job->fp_seen ? job->fp.supported && fp.supported : fp.supported;
    job->fp_seen = true;
    if (job->status == ME_SUCCESS) {
        job->map_calls++;
        if (!plan->info.reduction && job->scatter) {
            output_scatter_tile(job->out_view, schedule->info.output_ndim,
                                schedule->info.output_shape, pos, count,
                                (const unsigned char *)target, width);
        }
    }
    *out_values = target;
}

/* Traverse this job's tile or group range. Each job owns its mapped tile,
 * gather scratch and FP scope, so jobs may run on different host threads. */
static void exec_run(exec_job *job) {
    const b2nd_exec_schedule *schedule = job->schedule;
    const b2nd_exec_plan *plan = schedule->plan;
    size_t width = plan->info.itemsize;
    size_t tile_items = schedule->info.tile_items;
    size_t groups = schedule->info.reduction_groups;
    size_t red_count = schedule->info.reduction_lanes;
    bool pairwise = schedule->info.pairwise_sum;
    bool floating = plan->info.reduction &&
        (plan->info.map_dtype == ME_FLOAT32 || plan->info.map_dtype == ME_FLOAT64);
    bool scoped = floating || job->raise_mask != 0;
    job->fp_scoped = scoped;
    me_fp_scope scope;
    me_fp_status reducer_fp = {0};
    if (scoped) {
        job->status = me_fp_scope_begin(&scope, &reducer_fp, &job->error);
        if (job->status != ME_SUCCESS) {
            return;
        }
        if (job->raise_mask && !reducer_fp.supported) {
            me_fp_scope_end(&scope, NULL);
            job->status = exec_error(&job->error, ME_ERR_UNSUPPORTED,
                                     "Floating exception reporting is unavailable");
            return;
        }
    }
    if (plan->info.reduction && groups > 1) {
        size_t tiles_g = (red_count && tile_items) ?
            (red_count + tile_items - 1) / tile_items : 0;
        for (size_t g = job->start; g < job->end && job->status == ME_SUCCESS; g++) {
            uint64_t acc = job->initial_val;
            bool acc_init = job->has_initial ||
                (plan->info.reducer != ME_REDUCTION_MIN && plan->info.reducer != ME_REDUCTION_MAX);
            me_pairwise_sum_state pw;
            if (pairwise && me_pairwise_sum_init(&pw, plan->info.dtype) != ME_PAIRWISE_SUM_SUCCESS) {
                job->status = exec_error(&job->error, ME_ERR_EVAL, "Cannot initialize pairwise sum");
                break;
            }
            for (size_t t = 0; t < tiles_g && job->status == ME_SUCCESS; t++) {
                size_t pos = t * tile_items;
                size_t count = red_count - pos < tile_items ? red_count - pos : tile_items;
                const void *values = NULL;
                exec_eval_tile(job, g, pos, count, NULL, &values);
                if (job->status != ME_SUCCESS) {
                    break;
                }
                if (pairwise) {
                    if (me_pairwise_sum_update(&pw, values, count) != ME_PAIRWISE_SUM_SUCCESS) {
                        job->status = exec_error(&job->error, ME_ERR_EVAL, "Pairwise sum update failed");
                        break;
                    }
                } else {
                    exec_reduce_values(plan, values, count, &acc, acc_init, &job->error, &job->status);
                    acc_init = true;
                }
                if (job->status == ME_SUCCESS) {
                    job->tiles++;
                }
            }
            if (job->status == ME_SUCCESS) {
                if (pairwise) {
                    acc = exec_pairwise_finish(plan->info.dtype, &pw, job->has_initial,
                                               job->initial_val, red_count > 0);
                }
                size_t out_off = job->scatter ?
                    output_offset(job->out_view, schedule->info.output_ndim,
                                  schedule->info.output_shape, g) :
                    job->out_view->offset + g * width;
                if (!me_reduce_store_scalar(plan->info.dtype, acc,
                                            (unsigned char *)job->out_view->base + out_off)) {
                    job->status = exec_error(&job->error, ME_ERR_EVAL, "Execution scalar store failed");
                }
            }
        }
    } else {
        size_t domain_lanes = plan->info.reduction ? red_count : schedule->info.nitems;
        me_pairwise_sum_state pw;
        if (pairwise && me_pairwise_sum_init(&pw, plan->info.dtype) != ME_PAIRWISE_SUM_SUCCESS) {
            job->status = exec_error(&job->error, ME_ERR_EVAL, "Cannot initialize pairwise sum");
        }
        for (size_t t = job->start; t < job->end && job->status == ME_SUCCESS; t++) {
            size_t pos = t * tile_items;
            size_t count = domain_lanes - pos < tile_items ? domain_lanes - pos : tile_items;
            void *direct_out = (!plan->info.reduction && !job->scatter) ?
                ((unsigned char *)job->out_view->base + job->out_view->offset + pos * width) : NULL;
            const void *values = NULL;
            exec_eval_tile(job, 0, pos, count, direct_out, &values);
            if (job->status != ME_SUCCESS) {
                break;
            }
            if (plan->info.reduction) {
                if (pairwise) {
                    if (me_pairwise_sum_update(&pw, values, count) != ME_PAIRWISE_SUM_SUCCESS) {
                        job->status = exec_error(&job->error, ME_ERR_EVAL, "Pairwise sum update failed");
                        break;
                    }
                } else {
                    exec_reduce_values(plan, values, count, &job->partial,
                                       job->has_init_state, &job->error, &job->status);
                    job->has_init_state = true;
                }
            }
            if (job->status == ME_SUCCESS) {
                job->tiles++;
            }
        }
        if (job->status == ME_SUCCESS && pairwise) {
            job->partial = exec_pairwise_finish(plan->info.dtype, &pw, job->has_initial,
                                                job->initial_val, domain_lanes > 0);
        }
    }
    if (scoped) {
        me_status restored = me_fp_scope_end(&scope, job->status == ME_SUCCESS ? &job->error : NULL);
        job->fp.flags |= reducer_fp.flags;
        job->fp.supported = job->fp_seen ? job->fp.supported && reducer_fp.supported :
            reducer_fp.supported;
        job->fp_seen = true;
        if (job->status == ME_SUCCESS) {
            job->status = restored;
        }
    }
}

#if !defined(_WIN32)
#include <pthread.h>
typedef struct {
    exec_job *job;
    pthread_t thread;
} exec_thread;
static void *exec_thread_main(void *argument) {
    exec_run(((exec_thread *)argument)->job);
    return NULL;
}
#endif

/* Ordered combination: partials are merged by logical task ID, never by
 * completion order. Only order-independent reducers are partitioned. */
static void exec_combine(b2nd_exec_report *report, const exec_job *jobs, size_t ntasks) {
    bool seen = false;
    for (size_t i = 0; i < ntasks; i++) {
        report->tiles += jobs[i].tiles;
        report->map_calls += jobs[i].map_calls;
        report->interpreter_calls += jobs[i].interpreter_calls;
        report->jit_calls += jobs[i].jit_calls;
        report->direct_tiles += jobs[i].direct_tiles;
        report->gathered_bytes += jobs[i].gathered_bytes;
        report->fp.flags |= jobs[i].fp.flags;
        if (jobs[i].fp_seen || jobs[i].fp_scoped) {
            report->fp.supported = seen ? report->fp.supported && jobs[i].fp.supported :
                jobs[i].fp.supported;
            seen = true;
        }
    }
}

me_status b2nd_exec_execute_into(const b2nd_exec_schedule *schedule,
                                 const b2nd_exec_view *inputs, size_t ninputs,
                                 const b2nd_exec_output_view *output,
                                 unsigned raise_mask, size_t max_gather_bytes,
                                 const b2nd_exec_jobs *jobs, b2nd_exec_report *report,
                                 me_error *error) {
    if (report) {
        *report = (b2nd_exec_report){0};
    }
    if (error) {
        *error = (me_error){0};
    }
    if (!schedule || !inputs || !output || !report || ninputs != schedule->plan->info.ninputs ||
        (raise_mask & ~(ME_FP_INVALID | ME_FP_DIVIDE | ME_FP_OVERFLOW | ME_FP_UNDERFLOW)) ||
        (jobs && (jobs->struct_size != sizeof(*jobs) || !jobs->workers))) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution bindings or FP mask");
    }
    const b2nd_exec_plan *plan = schedule->plan;
    size_t items = schedule->info.nitems;
    size_t width = plan->info.itemsize;
    size_t output_items = schedule->info.output_items;
    size_t output_bytes = output_items * width;
    if (!output_valid(output, schedule->info.output_ndim, schedule->info.output_shape,
                      width, output_items)) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution output extent, alignment or strides");
    }
    bool direct[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t gather_offsets[B2ND_EXEC_MAX_INPUTS] = {0};
    size_t per_job_gather = 0;
    for (size_t i = 0; i < ninputs; i++) {
        if (!view_valid(&inputs[i], &schedule->inputs[i])) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution input view or extent");
        }
        if (inputs[i].capacity && output_bytes && output->capacity &&
            (uintptr_t)inputs[i].base < (uintptr_t)output->base + output->capacity &&
            (uintptr_t)output->base < (uintptr_t)inputs[i].base + inputs[i].capacity) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Aliased execution output is not yet supported");
        }
        bool needed = !plan->info.identity || i == plan->direct_slot;
        direct[i] = schedule->suffix_reduction && schedule->matches_shape[i] &&
            view_native(&inputs[i]) && view_contiguous(&inputs[i], &schedule->inputs[i]) &&
            (!items || ((uintptr_t)inputs[i].base + inputs[i].offset) % schedule->inputs[i].itemsize == 0);
        if (needed && !direct[i] && items && schedule->info.tile_items) {
            size_t tile_bytes = schedule->info.tile_items * schedule->inputs[i].itemsize;
            if (per_job_gather > SIZE_MAX - tile_bytes) {
                return exec_error(error, ME_ERR_INVALID_ARG, "Execution gather buffer size overflow");
            }
            gather_offsets[i] = per_job_gather;
            per_job_gather += tile_bytes;
        }
    }
    if (per_job_gather > max_gather_bytes) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution gather buffer budget exceeded");
    }
    bool scatter = !output_contiguous(output, schedule->info.output_ndim,
                                      schedule->info.output_shape, width);
    size_t per_job_mapped = schedule->info.mapped_bytes;
    if (!plan->info.reduction && scatter && !plan->info.identity && items) {
        per_job_mapped = schedule->info.tile_items * width;
        if (per_job_mapped > schedule->max_mapped_bytes) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Execution mapped tile budget exceeded");
        }
    }
    size_t groups = schedule->info.reduction_groups;
    size_t red_count = schedule->info.reduction_lanes;
    size_t work_items = (plan->info.reduction && groups > 1) ? groups :
        ((plan->info.reduction && !groups) ? 0 :
         (schedule->info.tile_items ?
          ((plan->info.reduction ? red_count : items) + schedule->info.tile_items - 1) /
          schedule->info.tile_items : 0));
    size_t ntasks = jobs ? jobs->workers : 1;
    if (ntasks > B2ND_EXEC_MAX_TASKS) {
        ntasks = B2ND_EXEC_MAX_TASKS;
    }
    if (ntasks > work_items) {
        ntasks = work_items ? work_items : 1;
    }
    if (ntasks > 1 && (plan->info.reducer == ME_REDUCTION_SUM ||
                       plan->info.reducer == ME_REDUCTION_PROD)) {
        /* Partitioning would redefine strict serial leaves/combine order. */
        return exec_error(error, ME_ERR_UNSUPPORTED,
                          "Ordered reductions require serial execution");
    }
    if (ntasks > 1 && !plan->info.partitionable) {
        return exec_error(error, ME_ERR_UNSUPPORTED,
                          "Execution plan is not proven partitionable");
    }
    if (per_job_mapped > SIZE_MAX - per_job_gather) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Aggregate worker scratch size overflow");
    }
    size_t per_job_total = per_job_mapped + per_job_gather;
    if (ntasks > 1 && per_job_total) {
        /* Cap the job count to the aggregate budget; a budget that cannot hold
         * even one job is a rejected request, not a silent serial fallback. */
        size_t fit = jobs->max_worker_scratch / per_job_total;
        if (!fit) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Aggregate worker scratch budget exceeded");
        }
        if (ntasks > fit) {
            ntasks = fit;
        }
    }
#ifdef _WIN32
    ntasks = 1;
#endif
    uint64_t identity_val = 0;
    if (plan->info.reduction &&
        (plan->info.reducer == ME_REDUCTION_SUM || plan->info.reducer == ME_REDUCTION_PROD ||
         plan->info.reducer == ME_REDUCTION_ANY || plan->info.reducer == ME_REDUCTION_ALL)) {
        me_reduce_identity(plan->info.reducer, plan->info.dtype, &identity_val);
    }
    uint64_t initial_val = identity_val;
    me_fp_status init_fp = {0};
    bool init_fp_seen = false;
    if (plan->info.has_initial) {
        if (plan->initial_kernel) {
            uint64_t init_buf[2] = {0};
            me_block init_block = ME_BLOCK_INITIALIZER;
            init_block.nitems = 1;
            me_run_options init_opts = ME_RUN_OPTIONS_INITIALIZER;
            init_opts.fp_status = &init_fp;
            me_status init_status = me_eval_block(
                plan->initial_kernel, NULL, 0,
                (me_mut_buffer){init_buf, width}, &init_block, &init_opts, error);
            init_fp_seen = true;
            if (init_status != ME_SUCCESS) {
                report->fp = init_fp;
                return init_status;
            }
            if (!load_scalar_u64(plan->info.dtype, init_buf, &initial_val)) {
                return exec_error(error, ME_ERR_EVAL, "Cannot decode converted initial scalar");
            }
        } else {
            initial_val = plan->initial_value;
        }
    }
    size_t mapped_bytes = per_job_mapped ? per_job_mapped * ntasks : 0;
    size_t gather_bytes = per_job_gather ? per_job_gather * ntasks : 0;
    size_t worker_bytes = per_job_total ? per_job_total * ntasks : 0;
    unsigned char *mapped = mapped_bytes ? malloc(mapped_bytes) : NULL;
    unsigned char *gather = gather_bytes ? malloc(gather_bytes) : NULL;
    exec_job *parts = calloc(ntasks, sizeof(*parts));
#if !defined(_WIN32)
    exec_thread *threads = ntasks > 1 ? calloc(ntasks, sizeof(*threads)) : NULL;
    if (ntasks > 1 && !threads) {
        free(mapped);
        free(gather);
        free(parts);
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution jobs");
    }
#endif
    if (!parts || (mapped_bytes && !mapped) || (gather_bytes && !gather)) {
        free(mapped);
        free(gather);
        free(parts);
#if !defined(_WIN32)
        free(threads);
#endif
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution jobs");
    }
    bool single_group_acc = plan->info.reduction && groups == 1 && ntasks == 1;
    size_t per_task = work_items / ntasks, extra = work_items % ntasks, cursor = 0;
    for (size_t i = 0; i < ntasks; i++) {
        size_t count = per_task + (i < extra ? 1 : 0);
        parts[i] = (exec_job){
            schedule, inputs, direct, ninputs, output, scatter, raise_mask,
            mapped ? mapped + i * per_job_mapped : NULL, {0},
            cursor, cursor + count,
            plan->info.has_initial, initial_val,
            single_group_acc && (plan->info.has_initial ||
                (plan->info.reducer != ME_REDUCTION_MIN && plan->info.reducer != ME_REDUCTION_MAX)),
            single_group_acc ? initial_val : identity_val,
            0, 0, 0, 0, 0, 0, {0}, false, false, ME_SUCCESS, {0}};
        if (gather) {
            unsigned char *base = gather + i * per_job_gather;
            for (size_t s = 0; s < ninputs; s++) {
                bool needed = !plan->info.identity || s == plan->direct_slot;
                if (needed && !direct[s] && items && schedule->info.tile_items) {
                    parts[i].gather_slots[s] = base + gather_offsets[s];
                }
            }
        }
        cursor += count;
    }
    me_status status = ME_SUCCESS;
#if !defined(_WIN32)
    if (ntasks > 1) {
        size_t started = 0;
        for (; started < ntasks; started++) {
            threads[started].job = &parts[started];
            if (pthread_create(&threads[started].thread, NULL, exec_thread_main,
                               &threads[started]) != 0) {
                break;
            }
        }
        if (started != ntasks) {
            /* Join every started task before returning; no retry/replay. */
            for (size_t i = 0; i < started; i++) {
                pthread_join(threads[i].thread, NULL);
            }
            exec_combine(report, parts, started);
            if (init_fp_seen) {
                report->fp.flags |= init_fp.flags;
                report->fp.supported = report->tiles ?
                    (report->fp.supported && init_fp.supported) : init_fp.supported;
            }
            report->tasks = started;
            report->worker_bytes = worker_bytes;
            report->mapped_bytes = per_job_mapped;
            report->gather_bytes = per_job_gather;
            free(threads);
            free(mapped);
            free(gather);
            free(parts);
            return exec_error(error, ME_ERR_UNSUPPORTED, "Cannot start all execution jobs");
        }
        for (size_t i = 0; i < ntasks; i++) {
            pthread_join(threads[i].thread, NULL);
        }
        free(threads);
    } else
#endif
    {
        exec_run(&parts[0]);
    }
    exec_combine(report, parts, ntasks);
    if (init_fp_seen) {
        bool any_fp = false;
        for (size_t i = 0; i < ntasks; i++) {
            any_fp |= parts[i].fp_seen || parts[i].fp_scoped;
        }
        report->fp.flags |= init_fp.flags;
        report->fp.supported = any_fp ? (report->fp.supported && init_fp.supported) : init_fp.supported;
    }
    report->tasks = ntasks;
    report->worker_bytes = worker_bytes;
    report->mapped_bytes = per_job_mapped;
    report->gather_bytes = per_job_gather;
    for (size_t i = 0; i < ntasks; i++) {
        if (parts[i].status != ME_SUCCESS) {
            if (status == ME_SUCCESS) {
                if (error) {
                    *error = parts[i].error;
                }
                status = parts[i].status;
            }
        }
    }
    if (status == ME_SUCCESS && plan->info.reduction && groups == 1) {
        uint64_t result = initial_val;
        if (ntasks == 1) {
            if (parts[0].tiles) {
                result = parts[0].partial;
            }
        } else {
            bool floating_extrema =
                (plan->info.reducer == ME_REDUCTION_MIN || plan->info.reducer == ME_REDUCTION_MAX) &&
                (plan->info.dtype == ME_FLOAT32 || plan->info.dtype == ME_FLOAT64);
            bool scoped = floating_extrema || raise_mask != 0;
            me_fp_scope combine_scope;
            me_fp_status combine_fp = {0};
            if (scoped) {
                status = me_fp_scope_begin(&combine_scope, &combine_fp, error);
                if (status == ME_SUCCESS && raise_mask && !combine_fp.supported) {
                    me_fp_scope_end(&combine_scope, NULL);
                    status = exec_error(error, ME_ERR_UNSUPPORTED,
                                        "Floating exception reporting is unavailable");
                }
            }
            if (status == ME_SUCCESS) {
                bool initialized = plan->info.has_initial;
                for (size_t i = 0; i < ntasks; i++) {
                    if (!parts[i].tiles) {
                        continue; /* An empty domain contributes no value, not an identity. */
                    }
                    if (!exec_combine_partial(plan->info.reducer, plan->info.dtype,
                                              &result, parts[i].partial, initialized)) {
                        status = exec_error(error, ME_ERR_EVAL,
                                            "Execution reduction combination failed");
                        break;
                    }
                    initialized = true;
                }
            }
            if (scoped && status != ME_ERR_UNSUPPORTED) {
                me_status restored = me_fp_scope_end(&combine_scope,
                                                     status == ME_SUCCESS ? error : NULL);
                report->fp.flags |= combine_fp.flags;
                report->fp.supported = report->fp.supported && combine_fp.supported;
                if (status == ME_SUCCESS) {
                    status = restored;
                }
            }
        }
        if (status == ME_SUCCESS && (report->fp.flags & raise_mask)) {
            status = exec_error(error, ME_ERR_EVAL, "Execution raised floating-point status");
        }
        if (status == ME_SUCCESS &&
            !me_reduce_store_scalar(plan->info.dtype, result,
                                    (unsigned char *)output->base + output->offset)) {
            status = exec_error(error, ME_ERR_EVAL, "Execution scalar store failed");
        }
    } else if (status == ME_SUCCESS && (report->fp.flags & raise_mask)) {
        status = exec_error(error, ME_ERR_EVAL, "Execution raised floating-point status");
    }
    free(mapped);
    free(gather);
    free(parts);
    return status;
}

me_status b2nd_exec_execute_views(const b2nd_exec_schedule *schedule,
                                  const b2nd_exec_view *inputs, size_t ninputs,
                                  me_mut_buffer output, unsigned raise_mask,
                                  size_t max_gather_bytes,
                                  const b2nd_exec_jobs *jobs, b2nd_exec_report *report,
                                  me_error *error) {
    if (report) {
        *report = (b2nd_exec_report){0};
    }
    if (error) {
        *error = (me_error){0};
    }
    if (!schedule || !inputs || !report) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution bindings or FP mask");
    }
    const b2nd_exec_plan *plan = schedule->plan;
    size_t width = plan->info.itemsize;
    size_t output_bytes = schedule->info.output_items * width;
    if (!valid_extent(output.data, output.capacity, output_bytes, width)) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution output extent or alignment");
    }
    b2nd_exec_output_view out_view = {sizeof(out_view), output.data, output.capacity, 0, {0}};
    size_t stride = width;
    for (size_t a = schedule->info.output_ndim; a-- > 0;) {
        out_view.strides[a] = (int64_t)stride;
        if (schedule->info.output_shape[a]) {
            stride *= (size_t)schedule->info.output_shape[a];
        }
    }
    return b2nd_exec_execute_into(schedule, inputs, ninputs, &out_view, raise_mask,
                                  max_gather_bytes, jobs, report, error);
}

me_status b2nd_exec_execute_alias(const b2nd_exec_schedule *schedule,
                                   const b2nd_exec_view *inputs, size_t ninputs,
                                   const b2nd_exec_output_view *output,
                                   size_t staging_budget,
                                   unsigned raise_mask, size_t max_gather_bytes,
                                   const b2nd_exec_jobs *jobs, b2nd_exec_report *report,
                                   me_error *error) {
    if (report) {
        *report = (b2nd_exec_report){0};
    }
    if (error) {
        *error = (me_error){0};
    }
    if (!schedule || !inputs || !output || !report || ninputs != schedule->plan->info.ninputs ||
        (raise_mask & ~(ME_FP_INVALID | ME_FP_DIVIDE | ME_FP_OVERFLOW | ME_FP_UNDERFLOW)) ||
        (jobs && (jobs->struct_size != sizeof(*jobs) || !jobs->workers))) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution bindings or FP mask");
    }
    size_t width = schedule->plan->info.itemsize;
    size_t output_items = schedule->info.output_items;
    size_t output_bytes = output_items * width;
    if (!output_valid(output, schedule->info.output_ndim, schedule->info.output_shape,
                      width, output_items)) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution output extent, alignment or strides");
    }
    for (size_t i = 0; i < ninputs; i++) {
        if (!view_valid(&inputs[i], &schedule->inputs[i])) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution input view or extent");
        }
    }
    if (output_bytes > staging_budget) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Execution alias staging budget exceeded");
    }
    unsigned char *staging = output_bytes ? malloc(output_bytes) : NULL;
    if (output_bytes && !staging) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution alias staging buffer");
    }
    b2nd_exec_output_view staged_view = {sizeof(staged_view), staging, output_bytes, 0, {0}};
    size_t stride = width;
    for (size_t a = schedule->info.output_ndim; a-- > 0;) {
        staged_view.strides[a] = (int64_t)stride;
        if (schedule->info.output_shape[a]) {
            stride *= (size_t)schedule->info.output_shape[a];
        }
    }
    me_status status = b2nd_exec_execute_into(
        schedule, inputs, ninputs, &staged_view, raise_mask, max_gather_bytes, jobs, report, error);
    report->staging_bytes = output_bytes;
    if (status == ME_SUCCESS && output_bytes) {
        if (output_contiguous(output, schedule->info.output_ndim,
                              schedule->info.output_shape, width)) {
            memcpy((unsigned char *)output->base + output->offset, staging, output_bytes);
        } else {
            output_scatter_tile(output, schedule->info.output_ndim,
                                schedule->info.output_shape, 0, output_items, staging, width);
        }
    }
    free(staging);
    return status;
}

me_status b2nd_exec_execute(const b2nd_exec_schedule *schedule,
                            const me_buffer *inputs, size_t ninputs,
                            me_mut_buffer output, unsigned raise_mask,
                            const b2nd_exec_jobs *jobs, b2nd_exec_report *report,
                            me_error *error) {
    if (report) {
        *report = (b2nd_exec_report){0};
    }
    if (error) {
        *error = (me_error){0};
    }
    if (!schedule || !inputs || !report || ninputs != schedule->plan->info.ninputs) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution bindings or FP mask");
    }
    b2nd_exec_view views[B2ND_EXEC_MAX_INPUTS] = {0};
    for (size_t i = 0; i < ninputs; i++) {
        size_t in_items = 1;
        for (size_t a = 0; a < schedule->inputs[i].ndim; a++) {
            in_items *= (size_t)schedule->inputs[i].shape[a];
        }
        size_t bytes = in_items * schedule->inputs[i].itemsize;
        if (!valid_extent(inputs[i].data, inputs[i].capacity, bytes, schedule->inputs[i].itemsize)) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution input extent or alignment");
        }
        size_t stride = schedule->inputs[i].itemsize;
        for (size_t a = schedule->inputs[i].ndim; a-- > 0;) {
            views[i].strides[a] = (int64_t)stride;
            if (schedule->inputs[i].shape[a]) {
                stride *= (size_t)schedule->inputs[i].shape[a];
            }
        }
        views[i].struct_size = sizeof(views[i]);
        views[i].base = inputs[i].data;
        views[i].capacity = inputs[i].capacity;
        views[i].offset = 0;
        views[i].byte_order = 0;
    }
    return b2nd_exec_execute_views(schedule, views, ninputs, output, raise_mask,
                                   schedule->info.broadcast_gather_bytes, jobs, report, error);
}

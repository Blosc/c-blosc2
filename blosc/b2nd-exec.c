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
        plan->info.dtype = info.output_dtype;
        plan->info.itemsize = info.output_itemsize;
        plan->info.cardinality = info.cardinality;
        plan->info.context_ndim = info.context_ndim;
    }
    if (status != ME_SUCCESS) {
        b2nd_exec_plan_free(plan);
        return status;
    }
    *out = plan;
    return ME_SUCCESS;
}

me_status b2nd_exec_prepare_kernel(me_kernel **kernel, bool minimum,
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
    if (minimum && (info.cardinality != ME_ELEMENTWISE || !numeric_width(info.output_dtype))) {
        return exec_error(error, ME_ERR_UNSUPPORTED, "Direct kernel is not an elementwise minimum map");
    }
    b2nd_exec_plan *plan = calloc(1, sizeof(*plan));
    if (!plan) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate direct execution plan");
    }
    plan->info = (b2nd_exec_plan_info){
        info.output_dtype, info.output_itemsize, count, minimum, false,
        info.cardinality, info.context_ndim};
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
    if (!ninputs || plan->info.cardinality != ME_ELEMENTWISE || plan->info.context_ndim) {
        return exec_error(error, ME_ERR_UNSUPPORTED, "Resident execution requires a context-free elementwise domain");
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
    size_t mapped = plan->info.identity || !plan->info.reduction ? 0 : lanes * plan->info.itemsize;
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

static bool valid_extent(const void *data, size_t capacity, size_t bytes, size_t alignment) {
    return capacity >= bytes && (!bytes ||
        (data && (uintptr_t)data % alignment == 0 && (uintptr_t)data <= UINTPTR_MAX - bytes));
}

me_status b2nd_exec_execute(const b2nd_exec_schedule *schedule,
                            const me_buffer *inputs, size_t ninputs,
                            me_mut_buffer output, unsigned raise_mask,
                            b2nd_exec_report *report, me_error *error) {
    if (report) {
        *report = (b2nd_exec_report){0};
    }
    if (error) {
        *error = (me_error){0};
    }
    if (!schedule || !inputs || !report || ninputs != schedule->plan->info.ninputs ||
        (raise_mask & ~(ME_FP_INVALID | ME_FP_DIVIDE | ME_FP_OVERFLOW | ME_FP_UNDERFLOW))) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution bindings or FP mask");
    }
    const b2nd_exec_plan *plan = schedule->plan;
    size_t items = schedule->info.nitems;
    size_t width = plan->info.itemsize;
    size_t output_bytes = schedule->info.output_items * width;
    if (!valid_extent(output.data, output.capacity, output_bytes, width)) {
        return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution output extent or alignment");
    }
    for (size_t i = 0; i < ninputs; i++) {
        size_t bytes = items * schedule->inputs[i].itemsize;
        if (!valid_extent(inputs[i].data, inputs[i].capacity, bytes, schedule->inputs[i].itemsize)) {
            return exec_error(error, ME_ERR_INVALID_ARG, "Invalid execution input extent or alignment");
        }
        if (bytes && output_bytes &&
            (uintptr_t)inputs[i].data < (uintptr_t)output.data + output_bytes &&
            (uintptr_t)output.data < (uintptr_t)inputs[i].data + bytes) {
            return exec_error(error, ME_ERR_UNSUPPORTED, "Aliased execution output is not yet supported");
        }
    }
    void *mapped = schedule->info.mapped_bytes ? malloc(schedule->info.mapped_bytes) : NULL;
    if (schedule->info.mapped_bytes && !mapped) {
        return exec_error(error, ME_ERR_OOM, "Cannot allocate execution mapped tile");
    }
    report->mapped_bytes = schedule->info.mapped_bytes;
    uint32_t direct_slot = plan->direct_slot;
    uint64_t best = 0;
    me_status status = ME_SUCCESS;
    /* Only floating reduction arithmetic needs a host FP scope. Integer
     * identity reductions deliberately avoid probing floating-point status. */
    bool floating = plan->info.reduction &&
        (plan->info.dtype == ME_FLOAT32 || plan->info.dtype == ME_FLOAT64);
    bool scoped = floating || raise_mask != 0;
    me_fp_scope scope;
    me_fp_status reducer_fp = {0};
    if (scoped) {
        status = me_fp_scope_begin(&scope, &reducer_fp, error);
        if (status != ME_SUCCESS) {
            free(mapped);
            return status;
        }
        if (raise_mask && !reducer_fp.supported) {
            me_fp_scope_end(&scope, NULL);
            free(mapped);
            return exec_error(error, ME_ERR_UNSUPPORTED, "Floating exception reporting is unavailable");
        }
    }
    bool fp_seen = false;
    for (size_t pos = 0; pos < items;) {
        size_t count = items - pos < schedule->info.tile_items ?
            items - pos : schedule->info.tile_items;
        const void *values;
        if (plan->info.identity) {
            values = (const unsigned char *)inputs[direct_slot].data + pos * width;
            report->direct_tiles++;
        } else {
            me_buffer spans[B2ND_EXEC_MAX_INPUTS];
            for (size_t i = 0; i < ninputs; i++) {
                size_t input_width = schedule->inputs[i].itemsize;
                spans[i] = (me_buffer){
                    (const unsigned char *)inputs[i].data + pos * input_width, count * input_width};
            }
            me_block block = ME_BLOCK_INITIALIZER;
            block.nitems = count;
            me_run_options options = ME_RUN_OPTIONS_INITIALIZER;
            me_fp_status fp = {0};
            options.fp_status = &fp;
            /* The graph adapter checks the union after traversal, including
             * reducer flags. A raise mask must not truncate later tiles. */
            options.raise_mask = 0;
            void *target = plan->info.reduction ? mapped :
                (unsigned char *)output.data + pos * width;
            status = me_eval_block(plan->kernel, spans, ninputs,
                (me_mut_buffer){target, count * width}, &block, &options, error);
            report->fp.flags |= fp.flags;
            report->fp.supported = fp_seen ? report->fp.supported && fp.supported : fp.supported;
            fp_seen = true;
            if (status != ME_SUCCESS) {
                break;
            }
            report->map_calls++;
            values = target;
        }
        if (plan->info.reduction) {
            if (!me_extrema_span(plan->info.dtype, values, count, best, pos != 0, true, &best)) {
                status = exec_error(error, ME_ERR_EVAL, "Execution minimum failed");
                break;
            }
        } else if (plan->info.identity) {
            memcpy((unsigned char *)output.data + pos * width, values, count * width);
        }
        report->tiles++;
        pos += count;
    }
    if (scoped) {
        me_status restored = me_fp_scope_end(&scope, status == ME_SUCCESS ? error : NULL);
        report->fp.flags |= reducer_fp.flags;
        report->fp.supported = fp_seen ? report->fp.supported && reducer_fp.supported : reducer_fp.supported;
        if (status == ME_SUCCESS) {
            status = restored;
        }
    }
    free(mapped);
    if (status == ME_SUCCESS && (report->fp.flags & raise_mask)) {
        status = exec_error(error, ME_ERR_EVAL, "Execution raised floating-point status");
    }
    if (status == ME_SUCCESS && plan->info.reduction) {
        memcpy(output.data, &best, width);
    }
    return status;
}

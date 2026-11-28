/* Wire-free descriptor and borrowed-schedule contract. No artifact headers. */
#undef NDEBUG
#include "b2nd-exec.h"
#include <assert.h>

/* Resident parallel jobs are conservative on Windows: one admitted task. */
static size_t expected_tasks(size_t requested) {
#ifdef _WIN32
    (void)requested;
    return 1;
#else
    return requested;
#endif
}

#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static b2nd_exec_node node(uint32_t id, b2nd_exec_op op) {
    b2nd_exec_node n = {0};
    n.struct_size = sizeof(n);
    n.id = id;
    n.op = op;
    return n;
}

static void borrowed_mask_contract(void) {
    me_input_spec input = {"x", ME_FLOAT64, sizeof(double)};
    me_signature sig = ME_SIGNATURE_INITIALIZER;
    sig.inputs = &input;
    sig.ninputs = 1;
    me_kernel *kernel = NULL;
    me_error error;
    assert(me_prepare_formula("sqrt(x)", &sig, NULL, &kernel, &error) == ME_SUCCESS);
    b2nd_exec_plan *plan = NULL;
    assert(b2nd_exec_prepare_borrowed_kernel_ex(kernel, ME_REDUCTION_SUM, NULL,
        ME_AUTO, true, &plan, &error) == ME_SUCCESS);
    b2nd_exec_plan_info info;
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
    assert(info.ninputs == 2 && info.has_where && !info.partitionable);
    b2nd_exec_input_meta meta[] = {
        {ME_FLOAT64, sizeof(double), 1, {2}}, {ME_BOOL, 1, 1, {2}}
    };
    b2nd_exec_schedule *schedule = NULL;
    assert(b2nd_exec_specialize(plan, meta, 2, 1, 1024, &schedule, &error) == ME_SUCCESS);
    double values[] = {-1, 4}, output = -1;
    bool mask[] = {false, true};
    me_buffer buffers[] = {{values, sizeof(values)}, {mask, sizeof(mask)}};
    b2nd_exec_report report;
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){&output, sizeof(output)},
                             0, NULL, &report, &error) == ME_SUCCESS);
    assert(output == 2 && report.fp.flags == 0);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);
    assert(b2nd_exec_prepare_borrowed_kernel_ex(kernel, ME_REDUCTION_SUM, NULL,
        ME_FLOAT32, true, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
    assert(info.dtype == ME_FLOAT32 && info.map_dtype == ME_FLOAT32);
    assert(b2nd_exec_specialize(plan, meta, 2, 2, 1, &schedule, &error) == ME_ERR_INVALID_ARG);
    assert(b2nd_exec_specialize(plan, meta, 2, 2, 1024, &schedule, &error) == ME_SUCCESS);
    float cast_output = -1;
    assert(b2nd_exec_execute(schedule, buffers, 2,
        (me_mut_buffer){&cast_output, sizeof(cast_output)}, 0, NULL, &report, &error) == ME_SUCCESS);
    if (cast_output != 2) {
        fprintf(stderr, "Borrowed accumulator conversion: output=%a scratch=%zu flags=%u\n",
                (double)cast_output, report.mapped_bytes, report.fp.flags);
    }
    assert(cast_output == 2);
    assert(report.fp.flags == 0);
    assert(report.mapped_bytes == 24);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);
    me_kernel_free(kernel);
}

static double scalar_value(me_dtype dtype, uint64_t bits) {
    if (dtype == ME_INT16) {
        int16_t value; memcpy(&value, &bits, sizeof(value)); return value;
    }
    if (dtype == ME_INT64) {
        int64_t value; memcpy(&value, &bits, sizeof(value)); return (double)value;
    }
    if (dtype == ME_FLOAT32) {
        float value; memcpy(&value, &bits, sizeof(value)); return value;
    }
    double value; memcpy(&value, &bits, sizeof(value)); return value;
}

static void backend_contracts(bool require_jit) {
    b2nd_exec_node nodes[3];
    nodes[0] = node(0, B2ND_EXEC_INPUT);
    nodes[0].dtype = ME_FLOAT64;
    nodes[0].itemsize = sizeof(double);
    nodes[1] = node(1, B2ND_EXEC_NEGATE);
    nodes[1].nargs = 1;
    nodes[1].args[0] = 0;
    nodes[2] = node(2, B2ND_EXEC_MIN);
    nodes[2].nargs = 1;
    nodes[2].args[0] = 1;
    b2nd_exec_descriptor descriptor = {sizeof(descriptor), nodes, 3, 1, 2};
    b2nd_exec_input_meta meta = {ME_FLOAT64, sizeof(double), 1, {17}};
    double values[17], result;
    for (size_t i = 0; i < 17; i++) values[i] = (double)i;
    me_buffer input = {values, sizeof(values)};
    for (int policy = ME_EXEC_INTERPRET; policy <= ME_EXEC_AUTO; policy++) {
        if (policy == ME_EXEC_REQUIRE_JIT && !require_jit) {
            continue;
        }
        b2nd_exec_options options = B2ND_EXEC_OPTIONS_INITIALIZER;
        options.execution = (me_execution_policy)policy;
        options.jit_compiler = ME_COMPILER_CC;
        options.math_backend = ME_MATH_SYSTEM;
        b2nd_exec_plan *plan = NULL;
        b2nd_exec_schedule *schedule = NULL;
        b2nd_exec_plan_info info;
        b2nd_exec_report report;
        me_error error;
        me_status status = b2nd_exec_prepare(&descriptor, &options, &plan, &error);
        if (status != ME_SUCCESS) {
            fprintf(stderr, "Backend policy %d: %s\n", policy, error.message);
        }
        assert(status == ME_SUCCESS);
        assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
        assert(policy != ME_EXEC_INTERPRET || !info.has_jit);
        assert(policy != ME_EXEC_REQUIRE_JIT || info.has_jit);
        assert(b2nd_exec_specialize(plan, &meta, 1, 4, 32, &schedule, &error) == ME_SUCCESS);
        b2nd_exec_jobs jobs = {sizeof(jobs), 3, 96};
        me_status exec_status = b2nd_exec_execute(schedule, &input, 1,
            (me_mut_buffer){&result, sizeof(result)}, 0, &jobs, &report, &error);
        if (exec_status != ME_SUCCESS) {
            fprintf(stderr, "Backend policy %d execute: %d %s\n", policy,
                    (int)exec_status, error.message);
        }
        assert(exec_status == ME_SUCCESS);
        assert(result == -16 && report.map_calls == 5);
        assert(report.jit_calls == (info.has_jit ? 5u : 0u));
        assert(report.interpreter_calls == (info.has_jit ? 0u : 5u));
#ifndef __EMSCRIPTEN__
        uint64_t snan = UINT64_C(0x7ff0000000000001);
        memcpy(&values[16], &snan, sizeof(snan));
        result = 99;
        assert(b2nd_exec_execute(schedule, &input, 1,
            (me_mut_buffer){&result, sizeof(result)}, ME_FP_INVALID,
            &jobs, &report, &error) == ME_ERR_EVAL);
        assert(result == 99 && report.fp.supported && (report.fp.flags & ME_FP_INVALID));
        assert(report.map_calls == 5);
        assert(report.jit_calls == (info.has_jit ? 5u : 0u));
        assert(report.interpreter_calls == (info.has_jit ? 0u : 5u));
        values[16] = 16;
#endif
        jobs.workers = 1;
        jobs.max_worker_scratch = 31;
        result = 99;
        assert(b2nd_exec_execute(schedule, &input, 1,
            (me_mut_buffer){&result, sizeof(result)}, 0, &jobs, &report, &error) == ME_ERR_INVALID_ARG);
        assert(result == 99 && !report.tasks && !report.tiles && !report.jit_calls &&
               !report.interpreter_calls);
        jobs.workers = 3;
        jobs.max_worker_scratch = 96;
        /* Identity traversal must not claim JIT use merely because its
         * prepared numerical kernel has generated code available. */
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
        descriptor.root = 0;
        descriptor.nnodes = 1;
        assert(b2nd_exec_prepare(&descriptor, &options, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(plan, &meta, 1, 4, 0, &schedule, &error) == ME_SUCCESS);
        double copied[17];
        assert(b2nd_exec_execute(schedule, &input, 1,
            (me_mut_buffer){copied, sizeof(copied)}, 0, &jobs, &report, &error) == ME_SUCCESS);
        assert(!memcmp(copied, values, sizeof(values)));
        assert(report.direct_tiles == 5 && !report.jit_calls && !report.interpreter_calls);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
        descriptor.root = 2;
        descriptor.nnodes = 3;
    }
    /* Lane locality alone is not a proof: weak narrow-integer conversion can
     * fail. Reject partitioning before reads or destination writes. */
    nodes[0].dtype = ME_UINT8;
    nodes[0].itemsize = 1;
    nodes[1] = node(1, B2ND_EXEC_CAPTURE);
    nodes[1].dtype = ME_INT64;
    nodes[1].itemsize = nodes[1].scalar_size = 8;
    nodes[1].category = ME_CAPTURE_WEAK;
    int64_t capture = 300;
    memcpy(nodes[1].scalar, &capture, sizeof(capture));
    nodes[2] = node(2, B2ND_EXEC_ADD);
    nodes[2].nargs = 2;
    nodes[2].args[1] = 1;
    b2nd_exec_plan *plan = NULL;
    b2nd_exec_schedule *schedule = NULL;
    b2nd_exec_plan_info info;
    b2nd_exec_report report;
    me_error error;
    assert(b2nd_exec_prepare(&descriptor, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS && !info.partitionable);
    meta.dtype = ME_UINT8;
    meta.itemsize = 1;
    meta.shape[0] = 4;
    assert(b2nd_exec_specialize(plan, &meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
    uint8_t data[4] = {0}, output[4] = {55, 55, 55, 55};
    input = (me_buffer){data, sizeof(data)};
    b2nd_exec_jobs jobs = {sizeof(jobs), 2, 0};
    assert(b2nd_exec_execute(schedule, &input, 1,
        (me_mut_buffer){output, sizeof(output)}, 0, &jobs, &report, &error) == ME_ERR_UNSUPPORTED);
    assert(!report.tasks && !report.tiles && !report.map_calls);
    assert(output[0] == 55 && output[3] == 55);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);
}

int main(int argc, char **argv) {
    borrowed_mask_contract();
    backend_contracts(argc > 1 && !strcmp(argv[1], "--require-jit"));
    /* The shared scope restores caller rounding/flags and nests collectors. */
#ifndef __EMSCRIPTEN__
    int rounding = fegetround();
    assert(fesetround(FE_DOWNWARD) == 0);
    feclearexcept(FE_ALL_EXCEPT);
    feraiseexcept(FE_DIVBYZERO);
    me_fp_scope outer, inner;
    me_fp_status outer_status, inner_status;
    me_error scope_error;
    assert(me_fp_scope_begin(&outer, &outer_status, &scope_error) == ME_SUCCESS);
    assert(fegetround() == FE_TONEAREST);
    feraiseexcept(FE_OVERFLOW);
    assert(me_fp_scope_begin(&inner, &inner_status, &scope_error) == ME_SUCCESS);
    feraiseexcept(FE_INVALID);
    assert(me_fp_scope_end(&inner, &scope_error) == ME_SUCCESS);
    assert(inner_status.supported && inner_status.flags == ME_FP_INVALID);
    assert(me_fp_scope_end(&outer, &scope_error) == ME_SUCCESS);
    assert(outer_status.flags == ME_FP_OVERFLOW);
    assert(fegetround() == FE_DOWNWARD);
    assert(fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
    assert(fesetround(rounding) == 0);
    feclearexcept(FE_ALL_EXCEPT);
#else
    me_fp_scope scope;
    me_fp_status status;
    assert(me_fp_scope_begin(&scope, &status, NULL) == ME_SUCCESS);
    assert(!status.supported);
    assert(me_fp_scope_end(&scope, NULL) == ME_SUCCESS);
#endif
    b2nd_exec_node nodes[7];
    nodes[0] = node(0, B2ND_EXEC_INPUT);
    nodes[0].dtype = ME_INT64;
    nodes[0].itemsize = 8;
    nodes[1] = nodes[0];
    nodes[1].id = 1;
    nodes[1].slot = 1;
    nodes[2] = node(2, B2ND_EXEC_CAPTURE);
    nodes[2].dtype = ME_INT64;
    nodes[2].itemsize = nodes[2].scalar_size = 8;
    nodes[2].category = ME_CAPTURE_WEAK;
    int64_t scale = INT64_MAX;
    memcpy(nodes[2].scalar, &scale, 8);
    nodes[3] = node(3, B2ND_EXEC_MULTIPLY);
    nodes[3].nargs = 2;
    nodes[3].args[0] = 1;
    nodes[3].args[1] = 2;
    nodes[4] = node(4, B2ND_EXEC_ADD);
    nodes[4].nargs = 2;
    nodes[4].args[0] = 0;
    nodes[4].args[1] = 3;
    nodes[5] = node(5, B2ND_EXEC_MIN);
    nodes[5].nargs = 1;
    nodes[5].args[0] = 4;
    b2nd_exec_descriptor d = {sizeof(d), nodes, 6, 2, 5};
    b2nd_exec_options options = B2ND_EXEC_OPTIONS_INITIALIZER;
    b2nd_exec_plan *plan = NULL;
    me_error error;
    assert(b2nd_exec_prepare(&d, &options, &plan, &error) == ME_SUCCESS);
    b2nd_exec_plan_info info;
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
    assert(info.dtype == ME_INT64 && info.reduction && !info.identity && info.ninputs == 2 && info.partitionable);
    b2nd_exec_input_meta metadata[2] = {
        {ME_INT64, 8, 1, {17}}, {ME_INT64, 8, 1, {17}}
    };
    b2nd_exec_schedule *schedule = NULL;
    assert(b2nd_exec_specialize(plan, metadata, 2, 8, 64, &schedule, &error) == ME_SUCCESS);
    b2nd_exec_schedule_info geometry;
    assert(b2nd_exec_describe_schedule(schedule, &geometry) == ME_SUCCESS);
    assert(geometry.nitems == 17 && geometry.tile_items == 8 && geometry.mapped_bytes == 64);
    int64_t x[17], y[17] = {0}, result = 99;
    for (size_t i = 0; i < 17; i++) {
        x[i] = 100;
    }
    x[16] = 7;
    y[16] = 2;
    me_buffer buffers[2] = {{x, sizeof(x)}, {y, sizeof(y)}};
    b2nd_exec_report report;
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){&result, 8}, 0,
        NULL, &report, &error) == ME_SUCCESS);
    assert(result == 5 && report.tiles == 3 && report.map_calls == 3 && report.mapped_bytes == 64);
    assert(!report.fp.flags);
    result = 99;
    buffers[1].capacity--;
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){&result, 8}, 0,
        NULL, &report, &error) == ME_ERR_INVALID_ARG);
    assert(result == 99 && !report.tiles);
    buffers[1].capacity++;
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){x, sizeof(x)}, 0,
        NULL, &report, &error) == ME_ERR_UNSUPPORTED);
    assert(x[0] == 100 && !report.tiles);
    b2nd_exec_schedule_free(schedule);
    assert(b2nd_exec_specialize(plan, metadata, 2, 8, 63, &schedule, &error) == ME_ERR_INVALID_ARG);
    assert(!schedule);
    metadata[0].ndim = B2ND_EXEC_MAX_RANK + 1;
    assert(b2nd_exec_specialize(plan, metadata, 2, 8, 64, &schedule, &error) == ME_ERR_INVALID_ARG);
    metadata[0].ndim = 2;
    metadata[0].shape[0] = INT64_MAX;
    metadata[0].shape[1] = INT64_MAX;
    assert(b2nd_exec_specialize(plan, metadata, 2, 8, 64, &schedule, &error) == ME_ERR_INVALID_ARG);
    b2nd_exec_plan_free(plan);

    /* Map overflow wraps in int64 before min reduction without FP flags. */
    int64_t wrap_x[17];
    for (size_t i = 0; i < 17; i++) {
        wrap_x[i] = 100;
    }
    wrap_x[4] = INT64_MAX;
    me_buffer wrap_buf = {wrap_x, sizeof(wrap_x)};
    b2nd_exec_node wrap_nodes[4];
    wrap_nodes[0] = node(0, B2ND_EXEC_INPUT);
    wrap_nodes[0].dtype = ME_INT64;
    wrap_nodes[0].itemsize = 8;
    wrap_nodes[1] = node(1, B2ND_EXEC_CAPTURE);
    wrap_nodes[1].dtype = ME_INT64;
    wrap_nodes[1].itemsize = wrap_nodes[1].scalar_size = 8;
    wrap_nodes[1].category = ME_CAPTURE_WEAK;
    int64_t one = 1;
    memcpy(wrap_nodes[1].scalar, &one, 8);
    wrap_nodes[2] = node(2, B2ND_EXEC_ADD);
    wrap_nodes[2].nargs = 2;
    wrap_nodes[2].args[0] = 0;
    wrap_nodes[2].args[1] = 1;
    wrap_nodes[3] = node(3, B2ND_EXEC_MIN);
    wrap_nodes[3].nargs = 1;
    wrap_nodes[3].args[0] = 2;
    b2nd_exec_descriptor wrap_d = {sizeof(wrap_d), wrap_nodes, 4, 1, 3};
    b2nd_exec_input_meta wrap_meta = {ME_INT64, 8, 1, {17}};
    assert(b2nd_exec_prepare(&wrap_d, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_specialize(plan, &wrap_meta, 1, 8, 64, &schedule, &error) == ME_SUCCESS);
    result = 0;
    assert(b2nd_exec_execute(schedule, &wrap_buf, 1, (me_mut_buffer){&result, 8}, 0,
        NULL, &report, &error) == ME_SUCCESS);
    assert(result == INT64_MIN && report.tiles == 3 && !report.fp.flags);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);

    /* Malformed descriptors must fail and always clear the caller's out slot. */
    for (int invalid = 0; invalid < 15; invalid++) {
        b2nd_exec_node saved[6];
        memcpy(saved, nodes, sizeof(saved));
        b2nd_exec_descriptor bad = d;
        b2nd_exec_options opts = options;
        switch (invalid) {
            case 0: nodes[4].args[1] = 4; break; /* Self cycle. */
            case 1: nodes[3].args[0] = 4; break; /* Forward reference. */
            case 2: nodes[1].slot = 0; break;
            case 3: nodes[2].scalar_size = 7; break;
            case 4: nodes[3].nargs = 1; break;
            case 5: bad.root = 6; break;
            case 6: bad.nnodes = SIZE_MAX; break;
            case 7: bad.ninputs = B2ND_EXEC_MAX_INPUTS + 1; break;
            case 8: nodes[0].dtype = ME_STRING; break;
            case 9: opts.struct_size--; break;
            case 10: opts.execution = (me_execution_policy)99; break;
            case 11: nodes[2].category = (me_capture_category)99; break;
            case 12: nodes[2].id = 1; break;
            case 13: nodes[2].struct_size--; break;
            case 14: opts.casting = (me_casting)99; break;
        }
        plan = (b2nd_exec_plan *)(uintptr_t)1;
        assert(b2nd_exec_prepare(&bad, &opts, &plan, &error) != ME_SUCCESS);
        assert(!plan && error.status != ME_SUCCESS);
        memcpy(nodes, saved, sizeof(saved));
    }

    b2nd_exec_node deep[B2ND_EXEC_MAX_DEPTH + 1];
    deep[0] = nodes[0];
    for (uint32_t i = 1; i <= B2ND_EXEC_MAX_DEPTH; i++) {
        deep[i] = node(i, B2ND_EXEC_NEGATE);
        deep[i].nargs = 1;
        deep[i].args[0] = i - 1;
    }
    b2nd_exec_descriptor excessive = {
        sizeof(excessive), deep, B2ND_EXEC_MAX_DEPTH + 1, 1, B2ND_EXEC_MAX_DEPTH};
    assert(b2nd_exec_prepare(&excessive, NULL, &plan, &error) == ME_ERR_INVALID_ARG);
    assert(!plan);

    /* The descriptor is temporary: poison it before metadata queries. */
    assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
    memset(nodes, 0xff, sizeof(nodes));
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
    assert(info.dtype == ME_INT64 && info.ninputs == 2);
    metadata[0] = metadata[1];
    assert(b2nd_exec_specialize(plan, metadata, 2, 8, 64, &schedule, &error) == ME_SUCCESS);
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){&result, 8}, 0,
        NULL, &report, &error) == ME_SUCCESS);
    assert(result == 5);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);

    /* Native representative width/capture cases, with the artifact build off. */
    for (int kind = 0; kind < 3; kind++) {
        me_dtype dtype = kind == 0 ? ME_INT16 : kind == 1 ? ME_FLOAT32 : ME_FLOAT64;
        size_t width = kind == 0 ? 2 : kind == 1 ? 4 : 8;
        nodes[0] = node(0, B2ND_EXEC_INPUT);
        nodes[0].dtype = dtype;
        nodes[0].itemsize = width;
        nodes[1] = node(1, B2ND_EXEC_CAPTURE);
        nodes[1].dtype = ME_INT64;
        nodes[1].itemsize = nodes[1].scalar_size = 8;
        nodes[1].category = ME_CAPTURE_WEAK;
        scale = 3;
        memcpy(nodes[1].scalar, &scale, 8);
        nodes[2] = node(2, B2ND_EXEC_MULTIPLY);
        nodes[2].nargs = 2;
        nodes[2].args[0] = 0;
        nodes[2].args[1] = 1;
        nodes[3] = node(3, B2ND_EXEC_MIN);
        nodes[3].nargs = 1;
        nodes[3].args[0] = 2;
        d = (b2nd_exec_descriptor){sizeof(d), nodes, 4, 1, 3};
        assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS && info.dtype == dtype);
        b2nd_exec_input_meta meta = {dtype, width, 1, {3}};
        assert(b2nd_exec_specialize(plan, &meta, 1, 2, 16, &schedule, &error) == ME_SUCCESS);
        int16_t i16[] = {7, -2, 5}, out16 = 0;
        float f32[] = {7, -2, 5}, out32 = 0;
        double f64[] = {7, -2, 5}, out64 = 0;
        me_buffer input = {kind == 0 ? (void *)i16 : kind == 1 ? (void *)f32 : (void *)f64, 3 * width};
        me_mut_buffer output = {kind == 0 ? (void *)&out16 : kind == 1 ? (void *)&out32 : (void *)&out64, width};
        assert(b2nd_exec_execute(schedule, &input, 1, output, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(kind == 0 ? out16 == -6 : kind == 1 ? out32 == -6 : out64 == -6);
        assert(report.tiles == 2 && report.mapped_bytes == 2 * width);
        if (kind == 1) {
            f32[2] = FLT_MAX;
            out32 = 11;
#ifndef __EMSCRIPTEN__
            feclearexcept(FE_ALL_EXCEPT);
            feraiseexcept(FE_DIVBYZERO);
            assert(b2nd_exec_execute(schedule, &input, 1, output, ME_FP_OVERFLOW,
                NULL, &report, &error) == ME_ERR_EVAL);
            assert(out32 == 11 && (report.fp.flags & ME_FP_OVERFLOW));
            assert(report.tiles == 2 && report.map_calls == 2);
            assert(fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
            feclearexcept(FE_ALL_EXCEPT);
#else
            assert(b2nd_exec_execute(schedule, &input, 1, output, ME_FP_OVERFLOW,
                NULL, &report, &error) == ME_ERR_UNSUPPORTED);
            assert(out32 == 11 && !report.tiles && !report.fp.supported);
#endif
        }
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
        b2nd_exec_op reducers[] = {B2ND_EXEC_SUM, B2ND_EXEC_PROD, B2ND_EXEC_MAX};
        double expected[] = {30, -1890, 21};
        f32[2] = 5;
        for (size_t r = 0; r < 3; r++) {
            nodes[3].op = reducers[r];
            d.nnodes = 4;
            d.root = 3;
            meta.shape[0] = 3;
            assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
            assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
            assert(info.map_dtype == dtype && info.map_itemsize == width);
            assert(info.dtype == (kind == 0 && r < 2 ? ME_INT64 : dtype));
            assert(b2nd_exec_specialize(plan, &meta, 1, 2, 16, &schedule, &error) == ME_SUCCESS);
            uint64_t scalar = 0;
            assert(b2nd_exec_execute(schedule, &input, 1, (me_mut_buffer){&scalar, sizeof(scalar)},
                0, NULL, &report, &error) == ME_SUCCESS);
            assert(scalar_value(info.dtype, scalar) == expected[r]);
            assert(report.mapped_bytes == 2 * width);
            b2nd_exec_schedule_free(schedule);
            if (r < 2) {
                meta.shape[0] = 0;
                assert(b2nd_exec_specialize(plan, &meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
                me_buffer empty = {NULL, 0};
                assert(b2nd_exec_execute(schedule, &empty, 1, (me_mut_buffer){&scalar, sizeof(scalar)},
                    0, NULL, &report, &error) == ME_SUCCESS);
                assert(scalar_value(info.dtype, scalar) == (r == 0 ? 0 : 1));
                assert(!report.tiles && !report.map_calls);
                b2nd_exec_schedule_free(schedule);
            }
            b2nd_exec_plan_free(plan);
            nodes[3].args[0] = 0;
            meta.shape[0] = 3;
            assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
            assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS && info.identity);
            assert(b2nd_exec_specialize(plan, &meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
            assert(b2nd_exec_execute(schedule, &input, 1, (me_mut_buffer){&scalar, sizeof(scalar)},
                0, NULL, &report, &error) == ME_SUCCESS);
            assert(scalar_value(info.dtype, scalar) == (r == 0 ? 10 : r == 1 ? -70 : 7));
            assert(!report.map_calls && !report.mapped_bytes && report.direct_tiles == 2);
            b2nd_exec_schedule_free(schedule);
            b2nd_exec_plan_free(plan);
            nodes[3].args[0] = 2;
        }
        d.nnodes = 3;
        d.root = 2;
        assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(plan, &meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
        int16_t map16[3];
        float map32[3];
        double map64[3];
        output = (me_mut_buffer){
            kind == 0 ? (void *)map16 : kind == 1 ? (void *)map32 : (void *)map64, 3 * width};
        f32[2] = 5;
        assert(b2nd_exec_execute(schedule, &input, 1, output, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(kind == 0 ? map16[2] == 15 : kind == 1 ? map32[2] == 15 : map64[2] == 15);
        assert(report.map_calls == 2);
        assert(report.mapped_bytes == 0);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
    }
    /* Partitioned jobs: ordered combine, per-job FP scope, bounded scratch. */
    {
        b2nd_exec_node part[3];
        part[0] = node(0, B2ND_EXEC_INPUT);
        part[0].dtype = ME_INT64;
        part[0].itemsize = 8;
        part[1] = node(1, B2ND_EXEC_MIN);
        part[1].nargs = 1;
        part[1].args[0] = 0;
        b2nd_exec_descriptor jobs_descriptor = {sizeof(jobs_descriptor), part, 2, 1, 1};
        assert(b2nd_exec_prepare(&jobs_descriptor, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta jobs_meta = {ME_INT64, 8, 1, {1000}};
        assert(b2nd_exec_specialize(plan, &jobs_meta, 1, 16, 0, &schedule, &error) == ME_SUCCESS);
        int64_t *many = malloc(1000 * sizeof(*many));
        assert(many);
        for (size_t i = 0; i < 1000; i++) {
            many[i] = 100000 - (int64_t)i;
        }
        many[511] = -7;
        me_buffer many_buffer = {many, 1000 * sizeof(*many)};
        b2nd_exec_jobs jobs = {sizeof(jobs), 1, 0};
        int64_t parallel_result = 0;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_SUCCESS);
        assert(parallel_result == -7 && report.tasks == 1);
        jobs.workers = 4;
        parallel_result = 0;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_SUCCESS);
        assert(parallel_result == -7 && report.tasks == expected_tasks(4));
        assert(report.tiles == 63 && report.direct_tiles == 63 && report.map_calls == 0);
        /* Same result for every admitted worker count. */
        for (size_t workers = 1; workers <= 7; workers++) {
            jobs.workers = workers;
            parallel_result = 0;
            assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
                0, &jobs, &report, &error) == ME_SUCCESS);
            assert(parallel_result == -7);
            assert(report.tasks == expected_tasks(workers));
        }
        /* A truncated input must fail before any job starts. */
        many_buffer.capacity--;
        jobs.workers = 4;
        parallel_result = 99;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_ERR_INVALID_ARG);
        assert(parallel_result == 99 && !report.tasks);
        many_buffer.capacity++;
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Ordered sums keep serial leaves: partitioning is rejected, not reordered. */
        part[1].op = B2ND_EXEC_SUM;
        assert(b2nd_exec_prepare(&jobs_descriptor, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(plan, &jobs_meta, 1, 16, 0, &schedule, &error) == ME_SUCCESS);
        jobs.workers = 2;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_ERR_UNSUPPORTED);
        jobs.workers = 1;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_SUCCESS);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Mapped reductions need one tile per job, charged to an aggregate budget. */
        part[0] = node(0, B2ND_EXEC_INPUT);
        part[0].dtype = ME_INT64;
        part[0].itemsize = 8;
        part[1] = node(1, B2ND_EXEC_CAPTURE);
        part[1].dtype = ME_INT64;
        part[1].itemsize = part[1].scalar_size = 8;
        part[1].category = ME_CAPTURE_WEAK;
        scale = 2;
        memcpy(part[1].scalar, &scale, 8);
        part[2] = node(3, B2ND_EXEC_MAX);
        part[2].nargs = 1;
        part[2].args[0] = 2;
        b2nd_exec_node mapped_nodes[4] = {part[0], part[1],
            node(2, B2ND_EXEC_ADD), part[2]};
        mapped_nodes[2].nargs = 2;
        mapped_nodes[2].args[0] = 0;
        mapped_nodes[2].args[1] = 1;
        b2nd_exec_descriptor mapped_descriptor = {sizeof(mapped_descriptor), mapped_nodes, 4, 1, 3};
        assert(b2nd_exec_prepare(&mapped_descriptor, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(plan, &jobs_meta, 1, 16, 1024, &schedule, &error) == ME_SUCCESS);
        jobs.workers = 4;
        jobs.max_worker_scratch = 127; /* 4 * 128 bytes needed. */
        parallel_result = 0;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_ERR_INVALID_ARG);
        assert(parallel_result == 0 && !report.tasks);
        jobs.max_worker_scratch = 512;
        assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
            0, &jobs, &report, &error) == ME_SUCCESS);
        assert(parallel_result == 100002);
        assert(report.tasks == expected_tasks(4) && report.worker_bytes == 512 &&
               report.map_calls == 63);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
        free(many);
    }
    /* Authored formula kernels share the executor; no Menudet substitution. */
    me_input_spec slot = {"x", ME_INT64, 8};
    me_signature signature = ME_SIGNATURE_INITIALIZER;
    signature.inputs = &slot;
    signature.ninputs = 1;
    signature.cardinality = ME_ELEMENTWISE;
    me_kernel *kernel = NULL;
    assert(me_prepare_formula("x * 2", &signature, NULL, &kernel, &error) == ME_SUCCESS);
    size_t slots = 0;
    me_input_spec described;
    assert(me_describe_input_count(kernel, &slots) == ME_SUCCESS && slots == 1);
    assert(me_describe_input(kernel, 0, &described) == ME_SUCCESS);
    assert(!described.name && described.dtype == ME_INT64 && described.itemsize == 8);
    assert(me_describe_input(kernel, 1, &described) == ME_ERR_INVALID_ARG);
    assert(b2nd_exec_prepare_kernel(&kernel, ME_REDUCTION_NONE, NULL, &error) == ME_ERR_INVALID_ARG);
    assert(kernel);
    assert(b2nd_exec_prepare_kernel(&kernel, ME_REDUCTION_NONE, &plan, &error) == ME_SUCCESS);
    assert(!kernel);
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS && info.partitionable);
    b2nd_exec_input_meta direct_meta = {ME_INT64, 8, 1, {3}};
    assert(b2nd_exec_specialize(plan, &direct_meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
    int64_t direct_input[] = {7, -2, 5}, direct_output[3];
    me_buffer direct_buffer = {direct_input, sizeof(direct_input)};
    b2nd_exec_jobs direct_jobs = {sizeof(direct_jobs), 2, 0};
    assert(b2nd_exec_execute(schedule, &direct_buffer, 1,
        (me_mut_buffer){direct_output, sizeof(direct_output)}, 0, &direct_jobs, &report, &error) == ME_SUCCESS);
    assert(b2nd_exec_execute(schedule, &direct_buffer, 1,
        (me_mut_buffer){direct_output, sizeof(direct_output)}, 0, NULL, &report, &error) == ME_SUCCESS);
    assert(direct_output[0] == 14 && direct_output[1] == -4 && direct_output[2] == 10);
    assert(report.map_calls == 2 && !report.mapped_bytes);
    b2nd_exec_schedule_free(schedule);
    direct_meta.ndim = 0;
    assert(b2nd_exec_specialize(plan, &direct_meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
    assert(b2nd_exec_execute(schedule, &direct_buffer, 1,
        (me_mut_buffer){direct_output, sizeof(direct_output)}, 0, NULL, &report, &error) == ME_SUCCESS);
    assert(report.tiles == 1 && direct_output[0] == 14);
    b2nd_exec_schedule_free(schedule);
    direct_meta.ndim = 2;
    direct_meta.shape[0] = 0;
    direct_meta.shape[1] = 3;
    assert(b2nd_exec_specialize(plan, &direct_meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
    me_buffer empty = {NULL, 0};
    assert(b2nd_exec_execute(schedule, &empty, 1,
        (me_mut_buffer){NULL, 0}, 0, NULL, &report, &error) == ME_SUCCESS);
    assert(!report.tiles && !report.map_calls);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);

    /* Narrow-integer sum/prod overflow past input dtype width. */
    {
        int8_t i8_ones[200];
        uint8_t u8_ones[300];
        uint8_t bool_ones[256];
        int8_t i8_twos[10];
        for (size_t i = 0; i < 200; i++) i8_ones[i] = 1;
        for (size_t i = 0; i < 300; i++) u8_ones[i] = 1;
        for (size_t i = 0; i < 256; i++) bool_ones[i] = 1;
        for (size_t i = 0; i < 10; i++) i8_twos[i] = 2;
        b2nd_exec_node narrow[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        narrow[1].nargs = 1;
        narrow[1].args[0] = 0;
        b2nd_exec_descriptor nd = {sizeof(nd), narrow, 2, 1, 1};

        narrow[0].dtype = ME_INT8;
        narrow[0].itemsize = 1;
        assert(b2nd_exec_prepare(&nd, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta nm = {ME_INT8, 1, 1, {200}};
        assert(b2nd_exec_specialize(plan, &nm, 1, 64, 0, &schedule, &error) == ME_SUCCESS);
        int64_t s_out = 0;
        assert(b2nd_exec_execute(schedule, &(me_buffer){i8_ones, sizeof(i8_ones)}, 1,
            (me_mut_buffer){&s_out, sizeof(s_out)}, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(s_out == 200);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        narrow[0].dtype = ME_UINT8;
        assert(b2nd_exec_prepare(&nd, NULL, &plan, &error) == ME_SUCCESS);
        nm.dtype = ME_UINT8;
        nm.shape[0] = 300;
        assert(b2nd_exec_specialize(plan, &nm, 1, 64, 0, &schedule, &error) == ME_SUCCESS);
        uint64_t u_out = 0;
        assert(b2nd_exec_execute(schedule, &(me_buffer){u8_ones, sizeof(u8_ones)}, 1,
            (me_mut_buffer){&u_out, sizeof(u_out)}, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(u_out == 300);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        narrow[0].dtype = ME_BOOL;
        assert(b2nd_exec_prepare(&nd, NULL, &plan, &error) == ME_SUCCESS);
        nm.dtype = ME_BOOL;
        nm.shape[0] = 256;
        assert(b2nd_exec_specialize(plan, &nm, 1, 64, 0, &schedule, &error) == ME_SUCCESS);
        s_out = 0;
        assert(b2nd_exec_execute(schedule, &(me_buffer){bool_ones, sizeof(bool_ones)}, 1,
            (me_mut_buffer){&s_out, sizeof(s_out)}, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(s_out == 256);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        narrow[0].dtype = ME_INT8;
        narrow[1].op = B2ND_EXEC_PROD;
        assert(b2nd_exec_prepare(&nd, NULL, &plan, &error) == ME_SUCCESS);
        nm.dtype = ME_INT8;
        nm.shape[0] = 10;
        assert(b2nd_exec_specialize(plan, &nm, 1, 4, 0, &schedule, &error) == ME_SUCCESS);
        s_out = 0;
        assert(b2nd_exec_execute(schedule, &(me_buffer){i8_twos, sizeof(i8_twos)}, 1,
            (me_mut_buffer){&s_out, sizeof(s_out)}, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(s_out == 1024);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
    }

    /* Truth reductions (any/all), multi-task FP combine, and task cap. */
    {
        b2nd_exec_node truth[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_ANY)};
        truth[0].dtype = ME_FLOAT32;
        truth[0].itemsize = 4;
        truth[1].nargs = 1;
        truth[1].args[0] = 0;
        b2nd_exec_descriptor td = {sizeof(td), truth, 2, 1, 1};
        assert(b2nd_exec_prepare(&td, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS &&
               info.dtype == ME_BOOL && info.partitionable && info.reducer == ME_REDUCTION_ANY);
        b2nd_exec_input_meta tm = {ME_FLOAT32, 4, 1, {0}};
        assert(b2nd_exec_specialize(plan, &tm, 1, 4, 0, &schedule, &error) == ME_SUCCESS);
        uint8_t b_out = 9;
        assert(b2nd_exec_execute(schedule, &(me_buffer){NULL, 0}, 1,
            (me_mut_buffer){&b_out, 1}, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(b_out == 0);
        b2nd_exec_schedule_free(schedule);

        float vals[100];
        for (size_t i = 0; i < 100; i++) vals[i] = 0.0f;
        vals[0] = 1.0f;
#ifndef __EMSCRIPTEN__
        uint32_t snan_bits = UINT32_C(0x7f800001);
        memcpy(&vals[99], &snan_bits, sizeof(snan_bits));
#endif
        tm.shape[0] = 100;
        assert(b2nd_exec_specialize(plan, &tm, 1, 1, 0, &schedule, &error) == ME_SUCCESS);
        b2nd_exec_jobs jobs = {sizeof(jobs), 1000, 0};
        b_out = 0;
#ifndef __EMSCRIPTEN__
        assert(b2nd_exec_execute(schedule, &(me_buffer){vals, sizeof(vals)}, 1,
            (me_mut_buffer){&b_out, 1}, ME_FP_INVALID, &jobs, &report, &error) == ME_ERR_EVAL);
        assert(report.tasks <= B2ND_EXEC_MAX_TASKS && report.fp.supported && (report.fp.flags & ME_FP_INVALID));
#endif
        vals[99] = 0.0f;
        assert(b2nd_exec_execute(schedule, &(me_buffer){vals, sizeof(vals)}, 1,
            (me_mut_buffer){&b_out, 1}, 0, &jobs, &report, &error) == ME_SUCCESS);
        assert(b_out == 1 && report.tasks == expected_tasks(B2ND_EXEC_MAX_TASKS));
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        truth[1].op = B2ND_EXEC_ALL;
        assert(b2nd_exec_prepare(&td, NULL, &plan, &error) == ME_SUCCESS);
        tm.shape[0] = 0;
        assert(b2nd_exec_specialize(plan, &tm, 1, 4, 0, &schedule, &error) == ME_SUCCESS);
        b_out = 9;
        assert(b2nd_exec_execute(schedule, &(me_buffer){NULL, 0}, 1,
            (me_mut_buffer){&b_out, 1}, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(b_out == 1);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Multi-task float min checks sNaN and -0.0/+0.0 across tasks. */
        truth[1].op = B2ND_EXEC_MIN;
        assert(b2nd_exec_prepare(&td, NULL, &plan, &error) == ME_SUCCESS);
        tm.shape[0] = 8;
        assert(b2nd_exec_specialize(plan, &tm, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
        float zvals[8] = {0.0f, 1.0f, 2.0f, 3.0f, -0.0f, 4.0f, 5.0f, 6.0f};
        float f_out = 99.0f;
        jobs.workers = 4;
        assert(b2nd_exec_execute(schedule, &(me_buffer){zvals, sizeof(zvals)}, 1,
            (me_mut_buffer){&f_out, sizeof(f_out)}, 0, &jobs, &report, &error) == ME_SUCCESS);
        uint32_t f_bits = 0;
        memcpy(&f_bits, &f_out, sizeof(f_bits));
        assert(f_bits == UINT32_C(0x80000000));
#if defined(__EMSCRIPTEN__)
        /* This runtime cannot observe the FP environment: reporting
         * unavailability must replace fabricating supported zero flags. */
        assert(!report.fp.supported);
#else
        assert(report.fp.supported);
#endif
#ifndef __EMSCRIPTEN__
        memcpy(&zvals[7], &snan_bits, sizeof(snan_bits));
        f_out = 99.0f;
        assert(b2nd_exec_execute(schedule, &(me_buffer){zvals, sizeof(zvals)}, 1,
            (me_mut_buffer){&f_out, sizeof(f_out)}, ME_FP_INVALID, &jobs, &report, &error) == ME_ERR_EVAL);
        assert(f_out == 99.0f && (report.fp.flags & ME_FP_INVALID));
#endif
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
    }

    /* Broadcast, strided, reversed, unaligned and byte-swapped views. */
    {
        b2nd_exec_node bnodes[3];
        bnodes[0] = node(0, B2ND_EXEC_INPUT);
        bnodes[0].dtype = ME_INT32;
        bnodes[0].itemsize = 4;
        bnodes[1] = bnodes[0];
        bnodes[1].id = 1;
        bnodes[1].slot = 1;
        bnodes[2] = node(2, B2ND_EXEC_ADD);
        bnodes[2].nargs = 2;
        bnodes[2].args[0] = 0;
        bnodes[2].args[1] = 1;
        b2nd_exec_descriptor bd = {sizeof(bd), bnodes, 3, 2, 2};
        assert(b2nd_exec_prepare(&bd, NULL, &plan, &error) == ME_SUCCESS);

        b2nd_exec_input_meta bad_bcast[2] = {
            {ME_INT32, 4, 2, {3, 2}}, {ME_INT32, 4, 2, {3, 3}}
        };
        assert(b2nd_exec_specialize(plan, bad_bcast, 2, 4, 0, &schedule, &error) == ME_ERR_INVALID_ARG);

        b2nd_exec_input_meta bmeta[2] = {
            {ME_INT32, 4, 2, {3, 1}}, {ME_INT32, 4, 2, {1, 5}}
        };
        assert(b2nd_exec_specialize(plan, bmeta, 2, 4, 0, &schedule, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe_schedule(schedule, &geometry) == ME_SUCCESS);
        assert(geometry.ndim == 2 && geometry.shape[0] == 3 && geometry.shape[1] == 5 &&
               geometry.nitems == 15 && geometry.broadcast_gather_bytes == 32);

        /* Non-native endianness on input 0 + unaligned offset on input 1. */
        uint8_t buf0[1 + 3 * 4] = {0};
        uint8_t buf1[3 + 5 * 8] = {0};
        uint16_t endian = 1;
        unsigned non_native = *(const uint8_t *)&endian ? 2u : 1u;
        for (int32_t i = 0; i < 3; i++) {
            int32_t val = (i + 1) * 10;
            uint8_t raw[4];
            memcpy(raw, &val, 4);
            for (size_t b = 0; b < 4; b++) {
                buf0[1 + i * 4 + b] = raw[3 - b];
            }
        }
        for (int32_t j = 0; j < 5; j++) {
            int32_t val = j + 1;
            memcpy(buf1 + 3 + (size_t)(4 - j) * 8, &val, 4); /* Reversed stride -8! */
        }
        b2nd_exec_view views[2] = {
            {sizeof(b2nd_exec_view), buf0, sizeof(buf0), 1, {4, 4}, non_native},
            {sizeof(b2nd_exec_view), buf1, sizeof(buf1), 3 + 4 * 8, {0, -8}, 0}
        };
        int32_t b_out[15] = {0};
        assert(b2nd_exec_execute_views(schedule, views, 2, (me_mut_buffer){b_out, sizeof(b_out)},
            0, 31, NULL, &report, &error) == ME_ERR_INVALID_ARG);
        assert(b_out[0] == 0);
        assert(b2nd_exec_execute_views(schedule, views, 2, (me_mut_buffer){b_out, sizeof(b_out)},
            0, 32, NULL, &report, &error) == ME_SUCCESS);
        for (int32_t i = 0; i < 3; i++) {
            for (int32_t j = 0; j < 5; j++) {
                assert(b_out[i * 5 + j] == (i + 1) * 10 + (j + 1));
            }
        }
        assert(report.gather_bytes == 32 && report.gathered_bytes == 120);

        /* Forged underflow/overflow views must fail before touching output. */
        b_out[0] = -99;
        views[1].offset = 3; /* Reversed stride -8 from offset 3 underflows buf1. */
        assert(b2nd_exec_execute_views(schedule, views, 2, (me_mut_buffer){b_out, sizeof(b_out)},
            0, 64, NULL, &report, &error) == ME_ERR_INVALID_ARG);
        assert(b_out[0] == -99);
        views[1].offset = 3 + 4 * 8;
        views[0].capacity -= 1; /* One byte short on last element. */
        assert(b2nd_exec_execute_views(schedule, views, 2, (me_mut_buffer){b_out, sizeof(b_out)},
            0, 64, NULL, &report, &error) == ME_ERR_INVALID_ARG);
        assert(b_out[0] == -99);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Identity reduction over a strided view gathers without map calls. */
        b2nd_exec_node id_min[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_MIN)};
        id_min[0].dtype = ME_INT32;
        id_min[0].itemsize = 4;
        id_min[1].nargs = 1;
        id_min[1].args[0] = 0;
        b2nd_exec_descriptor id_d = {sizeof(id_d), id_min, 2, 1, 1};
        assert(b2nd_exec_prepare(&id_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta id_meta = {ME_INT32, 4, 1, {4}};
        assert(b2nd_exec_specialize(plan, &id_meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
        int32_t strided_vals[] = {10, -999, 3, -999, -4, -999, 8};
        b2nd_exec_view sv = {sizeof(sv), strided_vals, sizeof(strided_vals), 0, {8}, 0};
        int32_t min_res = 0;
        assert(b2nd_exec_execute_views(schedule, &sv, 1, (me_mut_buffer){&min_res, 4},
            0, 8, NULL, &report, &error) == ME_SUCCESS);
        assert(min_res == -4 && report.map_calls == 0 && report.direct_tiles == 2 &&
               report.gathered_bytes == 16);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
    }

    {
        /* Reduction axes, keepdims, initial conversion, pairwise sum, strided output and alias staging. */
        b2nd_exec_node ax[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        ax[0].dtype = ME_INT16;
        ax[0].itemsize = 2;
        ax[1].nargs = 1;
        ax[1].args[0] = 0;
        ax[1].has_axes = true;
        ax[1].naxes = 1;
        ax[1].axes[0] = 1;
        ax[1].keepdims = true;
        ax[1].has_initial = true;
        ax[1].initial_dtype = ME_FLOAT32;
        ax[1].initial_category = ME_CAPTURE_TYPED;
        float init_f32 = 1e38f;
        memcpy(ax[1].initial_scalar, &init_f32, 4);
        ax[1].initial_size = 4;
        b2nd_exec_descriptor ax_d = {sizeof(ax_d), ax, 2, 1, 1};
        assert(b2nd_exec_prepare(&ax_d, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
        assert(info.has_axes && info.naxes == 1 && info.axes[0] == 1 && info.keepdims && info.has_initial);
        assert(info.dtype == ME_INT64 && info.itemsize == 8);

        /* Invalid/duplicate axis and initial on truth reductions must fail cleanly. */
        b2nd_exec_node bad_init[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_ANY)};
        bad_init[0].dtype = ME_INT32;
        bad_init[0].itemsize = 4;
        bad_init[1].nargs = 1;
        bad_init[1].args[0] = 0;
        bad_init[1].has_initial = true;
        bad_init[1].initial_dtype = ME_BOOL;
        bad_init[1].initial_category = ME_CAPTURE_TYPED;
        bad_init[1].initial_scalar[0] = 1;
        bad_init[1].initial_size = 1;
        b2nd_exec_descriptor bad_d = {sizeof(bad_d), bad_init, 2, 1, 1};
        b2nd_exec_plan *bad_plan = NULL;
        assert(b2nd_exec_prepare(&bad_d, NULL, &bad_plan, &error) == ME_ERR_INVALID_ARG && !bad_plan);

        b2nd_exec_input_meta dup_meta = {ME_INT16, 2, 2, {2, 3}};
        ax[1].naxes = 2;
        ax[1].axes[0] = 1;
        ax[1].axes[1] = -1;
        assert(b2nd_exec_prepare(&ax_d, NULL, &bad_plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(bad_plan, &dup_meta, 1, 4, 32, &schedule, &error) == ME_ERR_INVALID_ARG);
        assert(!schedule);
        dup_meta.shape[1] = 2;
        assert(b2nd_exec_specialize(bad_plan, &dup_meta, 1, 4, 32, &schedule, &error) == ME_ERR_INVALID_ARG);
        b2nd_exec_plan_free(bad_plan);

        /* Initial value conversion overflows int64 and surfaces ME_FP_INVALID / overflow. */
        b2nd_exec_input_meta m2d = {ME_INT16, 2, 2, {2, 3}};
        assert(b2nd_exec_specialize(plan, &m2d, 1, 2, 16, &schedule, &error) == ME_SUCCESS);
        b2nd_exec_schedule_info g2d;
        assert(b2nd_exec_describe_schedule(schedule, &g2d) == ME_SUCCESS);
        assert(g2d.output_ndim == 2 && g2d.output_shape[0] == 2 && g2d.output_shape[1] == 1);
        assert(g2d.reduction_groups == 2 && g2d.reduction_lanes == 3 && !g2d.pairwise_sum);
        int16_t a2d[6] = {1, 2, 3, 4, 5, 6};
        int64_t o2d[2] = {0};
        b2nd_exec_view v2d = {sizeof(v2d), a2d, sizeof(a2d), 0, {6, 2}, 0};
        b2nd_exec_output_view ov2d = {sizeof(ov2d), o2d, sizeof(o2d), 0, {8, 8}};
        assert(b2nd_exec_execute_into(schedule, &v2d, 1, &ov2d, 0, 16,
            NULL, &report, &error) == ME_ERR_EVAL && !report.fp.flags);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Float64 -> float32 overflowing initial raises ME_FP_OVERFLOW. */
        b2nd_exec_node ax_f32[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        ax_f32[0].dtype = ME_FLOAT32;
        ax_f32[0].itemsize = 4;
        ax_f32[1].nargs = 1;
        ax_f32[1].args[0] = 0;
        ax_f32[1].has_initial = true;
        ax_f32[1].initial_dtype = ME_FLOAT64;
        ax_f32[1].initial_category = ME_CAPTURE_TYPED;
        double init_huge = 1e308;
        memcpy(ax_f32[1].initial_scalar, &init_huge, 8);
        ax_f32[1].initial_size = 8;
        b2nd_exec_descriptor ax_f32_d = {sizeof(ax_f32_d), ax_f32, 2, 1, 1};
        assert(b2nd_exec_prepare(&ax_f32_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta m_f32 = {ME_FLOAT32, 4, 1, {2}};
        assert(b2nd_exec_specialize(plan, &m_f32, 1, 2, 8, &schedule, &error) == ME_SUCCESS);
        float in_f32[2] = {1.0f, 2.0f};
        float out_f32 = 0.0f;
        b2nd_exec_view v_f32 = {sizeof(v_f32), in_f32, sizeof(in_f32), 0, {4}, 0};
        b2nd_exec_output_view ov_f32 = {sizeof(ov_f32), &out_f32, sizeof(out_f32), 0, {0}};
#if defined(__EMSCRIPTEN__)
        /* No FP observation: the raise request is rejected before publishing. */
        assert(b2nd_exec_execute_into(schedule, &v_f32, 1, &ov_f32, ME_FP_OVERFLOW, 8,
            NULL, &report, &error) == ME_ERR_UNSUPPORTED);
        assert(out_f32 == 0.0f && !report.fp.supported);
#else
        assert(b2nd_exec_execute_into(schedule, &v_f32, 1, &ov_f32, ME_FP_OVERFLOW, 8,
            NULL, &report, &error) == ME_ERR_EVAL && (report.fp.flags & ME_FP_OVERFLOW));
#endif
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Non-suffix axis=0 and negative axis=-1 with normal int64 initial and strided output. */
        b2nd_exec_node ax0[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        ax0[0].dtype = ME_INT32;
        ax0[0].itemsize = 4;
        ax0[1].nargs = 1;
        ax0[1].args[0] = 0;
        ax0[1].has_axes = true;
        ax0[1].naxes = 1;
        ax0[1].axes[0] = 0;
        ax0[1].has_initial = true;
        ax0[1].initial_dtype = ME_INT16;
        ax0[1].initial_category = ME_CAPTURE_TYPED;
        int16_t init10 = 10;
        memcpy(ax0[1].initial_scalar, &init10, 2);
        ax0[1].initial_size = 2;
        b2nd_exec_descriptor ax0_d = {sizeof(ax0_d), ax0, 2, 1, 1};
        assert(b2nd_exec_prepare(&ax0_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta m3 = {ME_INT32, 4, 2, {2, 3}};
        assert(b2nd_exec_specialize(plan, &m3, 1, 2, 32, &schedule, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe_schedule(schedule, &g2d) == ME_SUCCESS);
        assert(g2d.output_ndim == 1 && g2d.output_shape[0] == 3 &&
               g2d.reduction_groups == 3 && g2d.reduction_lanes == 2);
        int32_t in3[6] = {1, 2, 3, 4, 5, 6};
        int64_t strided_out[6] = {99, 99, 99, 99, 99, 99};
        b2nd_exec_view vin3 = {sizeof(vin3), in3, sizeof(in3), 0, {12, 4}, 0};
        /* Reverse strided output: indices 0,1,2 land at strided_out[4, 2, 0]. */
        b2nd_exec_output_view ov_rev = {sizeof(ov_rev), strided_out, sizeof(strided_out), 4 * 8, {-16}};
        assert(b2nd_exec_execute_into(schedule, &vin3, 1, &ov_rev, 0, 32, NULL, &report, &error) == ME_SUCCESS);
        assert(strided_out[4] == 15 && strided_out[2] == 17 && strided_out[0] == 19);
        assert(strided_out[1] == 99 && strided_out[3] == 99 && strided_out[5] == 99);
        /* Overlapping output view (stride 0) must fail before touching output. */
        b2nd_exec_output_view ov_zero = {sizeof(ov_zero), strided_out, sizeof(strided_out), 0, {0}};
        assert(b2nd_exec_execute_into(schedule, &vin3, 1, &ov_zero, 0, 32, NULL, &report, &error) == ME_ERR_INVALID_ARG);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Multi-axis (0, 2) on 3D and empty axis=() reduction = map semantics. */
        b2nd_exec_node ax_empty[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        ax_empty[0].dtype = ME_INT32;
        ax_empty[0].itemsize = 4;
        ax_empty[1].nargs = 1;
        ax_empty[1].args[0] = 0;
        ax_empty[1].has_axes = true;
        ax_empty[1].naxes = 0;
        b2nd_exec_descriptor axe_d = {sizeof(axe_d), ax_empty, 2, 1, 1};
        assert(b2nd_exec_prepare(&axe_d, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(plan, &m3, 1, 3, 32, &schedule, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe_schedule(schedule, &g2d) == ME_SUCCESS);
        assert(g2d.output_ndim == 2 && g2d.reduction_groups == 6 && g2d.reduction_lanes == 1);
        int64_t map_red_out[6] = {0};
        b2nd_exec_output_view ov_map_red = {sizeof(ov_map_red), map_red_out, sizeof(map_red_out), 0, {24, 8}};
        assert(b2nd_exec_execute_into(schedule, &vin3, 1, &ov_map_red, 0, 32, NULL, &report, &error) == ME_SUCCESS);
        for (int i = 0; i < 6; i++) {
            assert(map_red_out[i] == in3[i]);
        }
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        b2nd_exec_node ax3d[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        ax3d[0].dtype = ME_INT32;
        ax3d[0].itemsize = 4;
        ax3d[1].nargs = 1;
        ax3d[1].args[0] = 0;
        ax3d[1].has_axes = true;
        ax3d[1].naxes = 2;
        ax3d[1].axes[0] = 0;
        ax3d[1].axes[1] = 2;
        b2nd_exec_descriptor ax3d_d = {sizeof(ax3d_d), ax3d, 2, 1, 1};
        assert(b2nd_exec_prepare(&ax3d_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta m_empty_red = {ME_INT32, 4, 3, {0, 3, 2}};
        assert(b2nd_exec_specialize(plan, &m_empty_red, 1, 4, 32, &schedule, &error) == ME_SUCCESS);
        assert(b2nd_exec_describe_schedule(schedule, &g2d) == ME_SUCCESS);
        assert(g2d.output_ndim == 1 && g2d.output_shape[0] == 3 &&
               g2d.reduction_groups == 3 && g2d.reduction_lanes == 0);
        int64_t empty_group_out[3] = {9, 9, 9};
        b2nd_exec_view v_empty_red = {sizeof(v_empty_red), in3, 0, 0, {24, 8, 4}, 0};
        b2nd_exec_output_view ov_empty_group = {sizeof(ov_empty_group), empty_group_out, sizeof(empty_group_out), 0, {8}};
        assert(b2nd_exec_execute_into(schedule, &v_empty_red, 1, &ov_empty_group, 0, 0, NULL, &report, &error) == ME_SUCCESS);
        assert(empty_group_out[0] == 0 && empty_group_out[1] == 0 && empty_group_out[2] == 0);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Pairwise sum invariance across tile sizes (1, 7, 128, 300) with and without initial. */
        b2nd_exec_node pw[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_SUM)};
        pw[0].dtype = ME_FLOAT64;
        pw[0].itemsize = 8;
        pw[1].nargs = 1;
        pw[1].args[0] = 0;
        pw[1].has_initial = true;
        pw[1].initial_dtype = ME_FLOAT64;
        pw[1].initial_category = ME_CAPTURE_TYPED;
        double pw_init = 0.25;
        memcpy(pw[1].initial_scalar, &pw_init, 8);
        pw[1].initial_size = 8;
        b2nd_exec_descriptor pw_d = {sizeof(pw_d), pw, 2, 1, 1};
        assert(b2nd_exec_prepare(&pw_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta m_pw = {ME_FLOAT64, 8, 1, {300}};
        double pw_data[300];
        for (size_t i = 0; i < 300; i++) {
            pw_data[i] = (double)(i + 1) * 0.1 + 1e-7;
        }
        size_t tiles[4] = {1, 7, 128, 300};
        double pw_ref = 0.0;
        for (size_t t = 0; t < 4; t++) {
            assert(b2nd_exec_specialize_ex(plan, &m_pw, 1, tiles[t], tiles[t] * 8, true, &schedule, &error) == ME_SUCCESS);
            assert(b2nd_exec_describe_schedule(schedule, &g2d) == ME_SUCCESS && g2d.pairwise_sum);
            me_buffer pw_buf = {pw_data, sizeof(pw_data)};
            double got = 0.0;
            assert(b2nd_exec_execute(schedule, &pw_buf, 1, (me_mut_buffer){&got, sizeof(got)},
                0, NULL, &report, &error) == ME_SUCCESS);
            if (t == 0) {
                pw_ref = got;
            } else {
                assert(memcmp(&got, &pw_ref, sizeof(got)) == 0);
            }
            b2nd_exec_schedule_free(schedule);
        }
        b2nd_exec_plan_free(plan);

        /* Aliasing staging adapter: exact reverse in-place, over-budget rejection, raise_mask unchanged. */
        b2nd_exec_node rev[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_ADD)};
        rev[0].dtype = ME_FLOAT64;
        rev[0].itemsize = 8;
        rev[1].nargs = 2;
        rev[1].args[0] = 0;
        rev[1].args[1] = 0;
        b2nd_exec_descriptor rev_d = {sizeof(rev_d), rev, 2, 1, 1};
        assert(b2nd_exec_prepare(&rev_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta m_rev = {ME_FLOAT64, 8, 1, {5}};
        assert(b2nd_exec_specialize(plan, &m_rev, 1, 2, 32, &schedule, &error) == ME_SUCCESS);
        double alias_buf[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
        b2nd_exec_view v_alias = {sizeof(v_alias), alias_buf, sizeof(alias_buf), 0, {8}, 0};
        /* Write in reverse into alias_buf: out[0..4] = alias_buf[4..0]. */
        b2nd_exec_output_view ov_alias = {sizeof(ov_alias), alias_buf, sizeof(alias_buf), 4 * 8, {-8}};
        assert(b2nd_exec_execute_into(schedule, &v_alias, 1, &ov_alias, 0, 16, NULL, &report, &error) == ME_ERR_UNSUPPORTED);
        assert(alias_buf[0] == 1.0 && alias_buf[4] == 5.0);
        assert(b2nd_exec_execute_alias(schedule, &v_alias, 1, &ov_alias, 39, 0, 16, NULL, &report, &error) == ME_ERR_INVALID_ARG);
        assert(alias_buf[0] == 1.0 && alias_buf[4] == 5.0);
        assert(b2nd_exec_execute_alias(schedule, &v_alias, 1, &ov_alias, 40, 0, 16, NULL, &report, &error) == ME_SUCCESS);
        assert(report.staging_bytes == 40);
        assert(alias_buf[4] == 2.0 && alias_buf[3] == 4.0 && alias_buf[2] == 6.0 &&
               alias_buf[1] == 8.0 && alias_buf[0] == 10.0);
        /* FP exception in alias mode leaves destination completely untouched. */
        alias_buf[0] = DBL_MAX;
        alias_buf[4] = -5.0;
#if defined(__EMSCRIPTEN__)
        /* Unobservable FP environment rejects the raise mask; the alias
         * destination must remain exactly untouched. */
        assert(b2nd_exec_execute_alias(schedule, &v_alias, 1, &ov_alias, 40, ME_FP_OVERFLOW, 16,
            NULL, &report, &error) == ME_ERR_UNSUPPORTED);
#else
        assert(b2nd_exec_execute_alias(schedule, &v_alias, 1, &ov_alias, 40, ME_FP_OVERFLOW, 16,
            NULL, &report, &error) == ME_ERR_EVAL && (report.fp.flags & ME_FP_OVERFLOW));
#endif
        assert(alias_buf[0] == DBL_MAX && alias_buf[4] == -5.0);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);

        /* Multi-worker B2ND_EXEC_ALL initializes each worker partial to identity true. */
        b2nd_exec_node all_n[2] = {node(0, B2ND_EXEC_INPUT), node(1, B2ND_EXEC_ALL)};
        all_n[0].dtype = ME_INT32;
        all_n[0].itemsize = 4;
        all_n[1].nargs = 1;
        all_n[1].args[0] = 0;
        b2nd_exec_descriptor all_d = {sizeof(all_d), all_n, 2, 1, 1};
        assert(b2nd_exec_prepare(&all_d, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_input_meta m_all = {ME_INT32, 4, 1, {6}};
        assert(b2nd_exec_specialize(plan, &m_all, 1, 2, 16, &schedule, &error) == ME_SUCCESS);
        int32_t all_vals[6] = {1, 2, 3, 4, 5, 6};
        me_buffer all_buf = {all_vals, sizeof(all_vals)};
        uint8_t all_out = 0;
        b2nd_exec_jobs all_jobs = {sizeof(all_jobs), 3, 128};
        assert(b2nd_exec_execute(schedule, &all_buf, 1, (me_mut_buffer){&all_out, 1}, 0,
            &all_jobs, &report, &error) == ME_SUCCESS);
        assert(all_out == 1);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
    }

    direct_meta = (b2nd_exec_input_meta){ME_INT64, 8, 1, {3}};
    signature.cardinality = ME_BLOCK_SCALAR;
    assert(me_prepare_menudet("0.2", "def group(x):\n    return block_sum(x)\n",
        &signature, NULL, &kernel, &error) == ME_SUCCESS);
    assert(b2nd_exec_prepare_kernel(&kernel, ME_REDUCTION_MIN, &plan, &error) == ME_ERR_UNSUPPORTED);
    assert(kernel && !plan);
    assert(b2nd_exec_prepare_kernel(&kernel, ME_REDUCTION_NONE, &plan, &error) == ME_SUCCESS);
    assert(!kernel);
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS && info.cardinality == ME_BLOCK_SCALAR);
    assert(b2nd_exec_specialize(plan, &direct_meta, 1, 2, 0, &schedule, &error) == ME_ERR_UNSUPPORTED);
    assert(!schedule);
    b2nd_exec_plan_free(plan);
    b2nd_exec_plan_free(NULL);
    b2nd_exec_schedule_free(NULL);

    /* Explicit reduction dtype accumulator casts (valid and incompatible). */
    b2nd_exec_node rnodes[2] = {
        {.struct_size = sizeof(b2nd_exec_node), .id = 0, .op = B2ND_EXEC_INPUT,
         .dtype = ME_INT64, .itemsize = 8},
        {.struct_size = sizeof(b2nd_exec_node), .id = 1, .op = B2ND_EXEC_SUM,
         .dtype = ME_INT8, .nargs = 1, .itemsize = 1},
    };
    b2nd_exec_descriptor rd = {sizeof(rd), rnodes, 2, 1, 1};
    assert(b2nd_exec_prepare(&rd, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS && info.dtype == ME_INT8 && info.itemsize == 1);
    b2nd_exec_input_meta rm = {ME_INT64, 8, 1, {3}};
    assert(b2nd_exec_specialize(plan, &rm, 1, 2, 64, &schedule, &error) == ME_SUCCESS);
    int64_t rvals[3] = {120, 120, 120};
    me_buffer rbuf = {rvals, sizeof(rvals)};
    int8_t rout = 0;
    assert(b2nd_exec_execute(schedule, &rbuf, 1, (me_mut_buffer){&rout, 1}, 0, NULL, &report, &error) == ME_SUCCESS);
    assert(rout == (int8_t)(int64_t)360);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);

    rnodes[0].dtype = ME_FLOAT64; rnodes[0].itemsize = 8;
    rnodes[1].op = B2ND_EXEC_PROD; rnodes[1].dtype = ME_FLOAT32; rnodes[1].itemsize = 4;
    rnodes[1].has_initial = true; rnodes[1].initial_dtype = ME_INT64;
    rnodes[1].initial_category = ME_CAPTURE_TYPED; rnodes[1].initial_size = 8;
    int64_t init3 = 3; memcpy(rnodes[1].initial_scalar, &init3, sizeof(init3));
    assert(b2nd_exec_prepare(&rd, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta rm_f64 = {ME_FLOAT64, 8, 1, {3}};
    assert(b2nd_exec_specialize(plan, &rm_f64, 1, 2, 64, &schedule, &error) == ME_SUCCESS);
    double pfvals[3] = {12.0, 12.0, 12.0};
    me_buffer pbuf = {pfvals, sizeof(pfvals)};
    float pfout = 0.0f;
    assert(b2nd_exec_execute(schedule, &pbuf, 1, (me_mut_buffer){&pfout, sizeof(pfout)}, 0, NULL, &report, &error) == ME_SUCCESS);
    assert(pfout == 5184.0f);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);

    rnodes[1].has_initial = false; rnodes[1].initial_dtype = ME_AUTO; rnodes[1].initial_size = 0;
    rnodes[1].op = B2ND_EXEC_SUM; rnodes[1].dtype = ME_INT64; rnodes[1].itemsize = 8;
    assert(b2nd_exec_prepare(&rd, NULL, &plan, &error) == ME_ERR_SIGNATURE);
    assert(!plan);
    return 0;
}

/* Wire-free descriptor and borrowed-schedule contract. No artifact headers. */
#include "b2nd-exec.h"
#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static b2nd_exec_node node(uint32_t id, b2nd_exec_op op) {
    b2nd_exec_node n = {0};
    n.struct_size = sizeof(n);
    n.id = id;
    n.op = op;
    return n;
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

int main(void) {
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
    assert(info.dtype == ME_INT64 && info.reduction && !info.identity && info.ninputs == 2);
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
        assert(parallel_result == -7 && report.tasks == 4);
        assert(report.tiles == 63 && report.direct_tiles == 63 && report.map_calls == 0);
        /* Same result for every admitted worker count. */
        for (size_t workers = 1; workers <= 7; workers++) {
            jobs.workers = workers;
            parallel_result = 0;
            assert(b2nd_exec_execute(schedule, &many_buffer, 1, (me_mut_buffer){&parallel_result, 8},
                0, &jobs, &report, &error) == ME_SUCCESS);
            assert(parallel_result == -7);
            assert(report.tasks == workers);
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
        assert(report.tasks == 4 && report.worker_bytes == 512 && report.map_calls == 63);
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
    b2nd_exec_input_meta direct_meta = {ME_INT64, 8, 1, {3}};
    assert(b2nd_exec_specialize(plan, &direct_meta, 1, 2, 0, &schedule, &error) == ME_SUCCESS);
    int64_t direct_input[] = {7, -2, 5}, direct_output[3];
    me_buffer direct_buffer = {direct_input, sizeof(direct_input)};
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
    return 0;
}

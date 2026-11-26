/* Wire-free descriptor and borrowed-schedule contract. No artifact headers. */
#include "b2nd-exec.h"
#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <string.h>

static b2nd_exec_node node(uint32_t id, b2nd_exec_op op) {
    b2nd_exec_node n = {0};
    n.struct_size = sizeof(n);
    n.id = id;
    n.op = op;
    return n;
}

int main(void) {
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
        &report, &error) == ME_SUCCESS);
    assert(result == 5 && report.tiles == 3 && report.map_calls == 3 && report.mapped_bytes == 64);
    assert(!report.fp.flags);
    result = 99;
    buffers[1].capacity--;
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){&result, 8}, 0,
        &report, &error) == ME_ERR_INVALID_ARG);
    assert(result == 99 && !report.tiles);
    buffers[1].capacity++;
    assert(b2nd_exec_execute(schedule, buffers, 2, (me_mut_buffer){x, sizeof(x)}, 0,
        &report, &error) == ME_ERR_UNSUPPORTED);
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
        &report, &error) == ME_SUCCESS);
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
        assert(b2nd_exec_execute(schedule, &input, 1, output, 0, &report, &error) == ME_SUCCESS);
        assert(kind == 0 ? out16 == -6 : kind == 1 ? out32 == -6 : out64 == -6);
        assert(report.tiles == 2 && report.mapped_bytes == 2 * width);
        if (kind == 1) {
            f32[2] = FLT_MAX;
            out32 = 11;
            feclearexcept(FE_ALL_EXCEPT);
            feraiseexcept(FE_DIVBYZERO);
            assert(b2nd_exec_execute(schedule, &input, 1, output, ME_FP_OVERFLOW,
                &report, &error) == ME_ERR_EVAL);
            assert(out32 == 11 && (report.fp.flags & ME_FP_OVERFLOW));
            assert(fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
            feclearexcept(FE_ALL_EXCEPT);
        }
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
        d.nnodes = 3;
        d.root = 2;
        assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
        assert(b2nd_exec_specialize(plan, &meta, 1, 2, 16, &schedule, &error) == ME_SUCCESS);
        int16_t map16[3];
        float map32[3];
        double map64[3];
        output = (me_mut_buffer){
            kind == 0 ? (void *)map16 : kind == 1 ? (void *)map32 : (void *)map64, 3 * width};
        f32[2] = 5;
        assert(b2nd_exec_execute(schedule, &input, 1, output, 0, &report, &error) == ME_SUCCESS);
        assert(kind == 0 ? map16[2] == 15 : kind == 1 ? map32[2] == 15 : map64[2] == 15);
        assert(report.map_calls == 2);
        b2nd_exec_schedule_free(schedule);
        b2nd_exec_plan_free(plan);
    }
    b2nd_exec_plan_free(NULL);
    b2nd_exec_schedule_free(NULL);
    return 0;
}

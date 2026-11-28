/* Differential storage/resident execution through exactly the same schedule. */
#undef NDEBUG
#include "b2nd-exec-storage.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static b2nd_exec_node make_node(uint32_t id, b2nd_exec_op op) {
    b2nd_exec_node node = {0};
    node.struct_size = sizeof(node);
    node.id = id;
    node.op = op;
    return node;
}

static void poison_padding(b2nd_array_t *array) {
    size_t bytes = (size_t)array->extchunknitems * 8;
    uint64_t *chunk = malloc(bytes);
    assert(chunk);
    for (int64_t c = 0; c < array->sc->nchunks; c++) {
        assert(blosc2_schunk_decompress_chunk(array->sc, c, chunk, (int32_t)bytes) == (int)bytes);
        for (size_t i = 0; i < bytes / 8; i++) {
            if (chunk[i] == 0) chunk[i] = UINT64_C(0x7ff0000000000001);
        }
        assert(blosc2_schunk_update_buffer(array->sc, c, chunk, (int32_t)bytes) >= 0);
    }
    free(chunk);
}

typedef struct {
    size_t calls;
    size_t fail_on_call;
    size_t expected_next;
    bool check_sequential;
    double scale;
} test_cb_source_ctx;

static me_status test_cb_read(void *ctx, const size_t *indices, size_t count,
                              me_mut_buffer dst, me_error *error) {
    test_cb_source_ctx *s = ctx;
    s->calls++;
    if (s->fail_on_call && s->calls == s->fail_on_call) {
        if (error) {
            error->status = ME_ERR_EVAL;
            snprintf(error->message, sizeof(error->message), "injected source failure");
        }
        return ME_ERR_EVAL;
    }
    double *out = dst.data;
    for (size_t i = 0; i < count; i++) {
        if (s->check_sequential) {
            assert(indices[i] == s->expected_next);
            s->expected_next++;
        }
        out[i] = (double)(indices[i] % 97) * s->scale;
    }
    return ME_SUCCESS;
}

typedef struct {
    size_t calls;
    size_t fail_on_call;
    size_t expected_offset;
    double accum;
} test_cb_sink_ctx;

static me_status test_cb_write(void *ctx, size_t offset_items, size_t count,
                               me_buffer src, me_error *error) {
    test_cb_sink_ctx *s = ctx;
    s->calls++;
    if (s->fail_on_call && s->calls == s->fail_on_call) {
        if (error) {
            error->status = ME_ERR_EVAL;
            snprintf(error->message, sizeof(error->message), "injected sink failure");
        }
        return ME_ERR_EVAL;
    }
    assert(offset_items == s->expected_offset);
    s->expected_offset += count;
    const double *vals = src.data;
    for (size_t i = 0; i < count; i++) s->accum += vals[i];
    return ME_SUCCESS;
}

int main(void) {
    blosc2_init();
    const int64_t shape[] = {5, 7};
    const int32_t chunks[][2] = {{3, 4}, {4, 3}};
    const int32_t blocks[][2] = {{2, 3}, {3, 2}};
    double x[35], y[35];
    for (size_t i = 0; i < 35; i++) {
        x[i] = 10 + (double)i;
        y[i] = 100 + (double)i;
    }
    const double *data[] = {x, y};
    b2nd_array_t *arrays[2] = {NULL, NULL};
    for (size_t i = 0; i < 2; i++) {
        blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
        b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunks[i], blocks[i], "=f8", 0, NULL, 0);
        assert(ctx && b2nd_from_cbuffer(ctx, &arrays[i], data[i], sizeof(x)) == 0);
        b2nd_free_ctx(ctx);
        poison_padding(arrays[i]);
    }
    const b2nd_array_t *inputs[] = {arrays[0], arrays[1]};
    size_t decode_budget = (size_t)(arrays[0]->extchunknitems + arrays[1]->extchunknitems) * 8;
    b2nd_exec_node nodes[4];
    nodes[0] = make_node(0, B2ND_EXEC_INPUT);
    nodes[0].dtype = ME_FLOAT64;
    nodes[0].itemsize = 8;
    nodes[1] = nodes[0];
    nodes[1].id = nodes[1].slot = 1;
    nodes[2] = make_node(2, B2ND_EXEC_ADD);
    nodes[2].nargs = 2;
    nodes[2].args[1] = 1;
    nodes[3] = make_node(3, B2ND_EXEC_MIN);
    nodes[3].nargs = 1;
    nodes[3].args[0] = 2;
    b2nd_exec_descriptor descriptor = {sizeof(descriptor), nodes, 4, 2, 3};
    b2nd_exec_input_meta meta[] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 2, {5, 7}}};
    const b2nd_exec_op operations[] = {B2ND_EXEC_MIN, B2ND_EXEC_SUM, B2ND_EXEC_PROD,
                                      B2ND_EXEC_MAX, B2ND_EXEC_ANY, B2ND_EXEC_ALL, B2ND_EXEC_ADD};
    for (size_t op = 0; op < sizeof(operations) / sizeof(*operations); op++) {
        descriptor.root = operations[op] == B2ND_EXEC_ADD ? 2 : 3;
        descriptor.nnodes = descriptor.root + 1;
        nodes[3].op = operations[op];
        b2nd_exec_plan *plan = NULL;
        me_error error;
        assert(b2nd_exec_prepare(&descriptor, NULL, &plan, &error) == ME_SUCCESS);
        b2nd_exec_plan_info info;
        assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
        const size_t tiles[] = {1, 8, 16, 64};
        for (size_t t = 0; t < sizeof(tiles) / sizeof(*tiles); t++) {
            b2nd_exec_schedule *schedule = NULL;
            assert(b2nd_exec_specialize(plan, meta, 2, tiles[t], 512, &schedule, &error) == ME_SUCCESS);
            b2nd_exec_schedule_info geometry;
            assert(b2nd_exec_describe_schedule(schedule, &geometry) == ME_SUCCESS);
            uint64_t actual[35], expected[35];
            memset(actual, 0x55, sizeof(actual));
            memset(expected, 0x55, sizeof(expected));
            size_t bytes = geometry.output_items * info.itemsize;
            me_buffer resident[] = {{x, sizeof(x)}, {y, sizeof(y)}};
            b2nd_exec_report reference;
            assert(b2nd_exec_execute(schedule, resident, 2, (me_mut_buffer){expected, bytes},
                0, NULL, &reference, &error) == ME_SUCCESS);
            size_t gather_budget = geometry.tile_items * 8 * 2;
            b2nd_exec_storage_report report;
            int64_t current_chunks[] = {arrays[0]->sc->current_nchunk, arrays[1]->sc->current_nchunk};
#ifdef __EMSCRIPTEN__
            /* Unobservable FP flags must reject raise masks before storage reads. */
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                ME_FP_INVALID, decode_budget, gather_budget, &report, &error) == ME_ERR_UNSUPPORTED);
            assert(!report.decodes && !report.execution.tiles);
            unsigned raise_mask = 0;
#else
            unsigned raise_mask = ME_FP_INVALID;
#endif
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                raise_mask, decode_budget, gather_budget, &report, &error) == ME_SUCCESS);
            assert(!memcmp(actual, expected, bytes));
            assert(report.execution.tiles == reference.tiles);
            assert(report.execution.map_calls == reference.map_calls);
            assert(report.execution.fp.flags == reference.fp.flags);
            assert(report.decode_bytes == decode_budget && report.decodes > 0);
            assert(report.execution.gather_bytes == gather_budget);
            assert(report.execution.gathered_bytes == sizeof(x) + sizeof(y));
            assert(report.read_spans == report.execution.tiles * 2);
            assert(report.cache_hits + report.decodes == 70);
            assert(report.execution.tasks == 1);
            assert(arrays[0]->sc->current_nchunk == current_chunks[0]);
            assert(arrays[1]->sc->current_nchunk == current_chunks[1]);
            b2nd_array_t wrong_shape = *arrays[1];
            wrong_shape.shape[0]++;
            const b2nd_array_t *wrong_inputs[] = {arrays[0], &wrong_shape};
            assert(b2nd_exec_execute_arrays(schedule, wrong_inputs, 2, (me_mut_buffer){actual, bytes},
                0, decode_budget, gather_budget, &report, &error) == ME_ERR_INVALID_ARG);
            assert(!report.read_spans && !report.decodes);
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2,
                (me_mut_buffer){arrays[0]->sc->data[0], bytes},
                0, decode_budget, gather_budget, &report, &error) == ME_ERR_UNSUPPORTED);
            assert(!report.read_spans && !report.decodes);
            memset(actual, 0x55, sizeof(actual));
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                0, decode_budget - 1, gather_budget, &report, &error) == ME_ERR_INVALID_ARG);
            assert(!report.decodes && !report.read_spans && !report.execution.tiles);
            assert(actual[0] == UINT64_C(0x5555555555555555));
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                0, decode_budget, gather_budget - 1, &report, &error) == ME_ERR_INVALID_ARG);
            assert(!report.decodes && !report.read_spans && !report.execution.tiles);
            assert(actual[0] == UINT64_C(0x5555555555555555));
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes - 1},
                0, decode_budget, gather_budget, &report, &error) == ME_ERR_INVALID_ARG);
            assert(!report.decodes && !report.read_spans);
#ifdef STORAGE_FAULT_TEST
            /* Owner allocation failures precede every storage read. */
            for (size_t failure = 1; failure <= 3; failure++) {
                allocations = 0;
                allocation_fail_at = failure;
                assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                    0, decode_budget, gather_budget, &report, &error) == ME_ERR_OOM);
                assert(!report.decodes && !report.read_spans);
            }
            allocation_fail_at = 0;
            if (tiles[t] == 8) {
                decode_calls = decode_fail_at = 0;
                assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                    0, decode_budget, gather_budget, &report, &error) == ME_SUCCESS);
                size_t total_calls = decode_calls;
                assert(total_calls > 1);
                for (size_t failure = 1; failure <= total_calls; failure += total_calls - 1) {
                    memset(actual, 0x55, sizeof(actual));
                    decode_calls = 0;
                    decode_fail_at = failure;
                    assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                        0, decode_budget, gather_budget, &report, &error) == ME_ERR_EVAL);
                    assert(decode_calls == failure && report.decodes == failure - 1);
                    if (info.reduction || failure == 1) {
                        assert(actual[0] == UINT64_C(0x5555555555555555));
                    } else {
                        assert(report.execution.tiles > 0);
                        assert(!memcmp(actual, expected, report.execution.tiles * tiles[t] * 8));
                    }
                }
                decode_fail_at = 0;
            }
#endif
            b2nd_exec_schedule_free(schedule);
        }
        b2nd_exec_plan_free(plan);
    }
    for (size_t i = 0; i < 2; i++) b2nd_free(arrays[i]);

    {
        /* Multi-dtype (int16, float32, bool), mixed compressed+resident, and parallel storage. */
        int16_t s16[35];
        for (size_t i = 0; i < 35; i++) s16[i] = (int16_t)(i - 5);
        blosc2_cparams cp16 = BLOSC2_CPARAMS_DEFAULTS;
        cp16.typesize = 2;
        blosc2_storage st16 = {.cparams = &cp16};
        b2nd_context_t *c16 = b2nd_create_ctx(&st16, 2, shape, chunks[0], blocks[0], "=i2", 0, NULL, 0);
        b2nd_array_t *a16 = NULL;
        assert(c16 && b2nd_from_cbuffer(c16, &a16, s16, sizeof(s16)) == 0);
        b2nd_free_ctx(c16);

        b2nd_exec_node n16[2] = {make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_SUM)};
        n16[0].dtype = ME_INT16; n16[0].itemsize = 2;
        n16[1].nargs = 1; n16[1].args[0] = 0;
        b2nd_exec_descriptor d16 = {sizeof(d16), n16, 2, 1, 1};
        b2nd_exec_plan *p16 = NULL; me_error err16;
        assert(b2nd_exec_prepare(&d16, NULL, &p16, &err16) == ME_SUCCESS);
        b2nd_exec_input_meta m16 = {ME_INT16, 2, 2, {5, 7}};
        b2nd_exec_schedule *sc16 = NULL;
        assert(b2nd_exec_specialize(p16, &m16, 1, 8, 64, &sc16, &err16) == ME_SUCCESS);
        int64_t sum16 = 0;
        b2nd_exec_storage_report r16;
        const b2nd_array_t *in16[] = {a16};
        size_t db16 = (size_t)a16->extchunknitems * 2;
        assert(b2nd_exec_execute_arrays(sc16, in16, 1, (me_mut_buffer){&sum16, sizeof(sum16)},
            0, db16, 16, &r16, &err16) == ME_SUCCESS);
        assert(sum16 == 420);
        b2nd_exec_jobs bad_j = {sizeof(bad_j), 2, 1024};
        assert(b2nd_exec_execute_arrays_ex(sc16, in16, NULL, 1, (me_mut_buffer){&sum16, sizeof(sum16)},
            0, db16 * 2, 16, &bad_j, &r16, &err16) == ME_ERR_UNSUPPORTED && !r16.decodes);
        assert(b2nd_exec_execute_arrays_ex(sc16, in16, NULL, 1, (me_mut_buffer){&sum16, sizeof(sum16)},
            0, db16, 16, &bad_j, &r16, &err16) == ME_ERR_UNSUPPORTED && !r16.decodes);
        b2nd_exec_schedule_free(sc16);
        b2nd_exec_plan_free(p16);
        b2nd_free(a16);

        float f32[35];
        for (size_t i = 0; i < 35; i++) f32[i] = (float)(i + 1) * 0.5f;
        blosc2_cparams cpf = BLOSC2_CPARAMS_DEFAULTS;
        cpf.typesize = 4;
        blosc2_storage stf = {.cparams = &cpf};
        b2nd_context_t *cf = b2nd_create_ctx(&stf, 2, shape, chunks[0], blocks[0], "=f4", 0, NULL, 0);
        b2nd_array_t *af = NULL;
        assert(cf && b2nd_from_cbuffer(cf, &af, f32, sizeof(f32)) == 0);
        b2nd_free_ctx(cf);
        b2nd_exec_node nf[4] = {
            make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_INPUT),
            make_node(2, B2ND_EXEC_ADD), make_node(3, B2ND_EXEC_MIN)};
        nf[0].dtype = ME_FLOAT32; nf[0].itemsize = 4;
        nf[1].slot = 1; nf[1].dtype = ME_FLOAT32; nf[1].itemsize = 4;
        nf[2].nargs = 2; nf[2].args[1] = 1;
        nf[3].nargs = 1; nf[3].args[0] = 2;
        b2nd_exec_descriptor df = {sizeof(df), nf, 4, 2, 3};
        b2nd_exec_plan *pf = NULL;
        assert(b2nd_exec_prepare(&df, NULL, &pf, &err16) == ME_SUCCESS);
        b2nd_exec_input_meta mf[] = {{ME_FLOAT32, 4, 2, {5, 7}}, {ME_FLOAT32, 4, 2, {5, 7}}};
        b2nd_exec_schedule *sf = NULL;
        assert(b2nd_exec_specialize(pf, mf, 2, 8, 64, &sf, &err16) == ME_SUCCESS);
        const b2nd_array_t *mixed_arr[] = {af, NULL};
        b2nd_exec_view mixed_views[2] = {
            {sizeof(b2nd_exec_view), NULL, 0, 0, {0}, 0},
            {sizeof(b2nd_exec_view), f32, sizeof(f32), 0, {28, 4}, 0}};
        float min_f32 = 0.0f;
        b2nd_exec_jobs fj = {sizeof(fj), 3, 256};
        size_t db_f = (size_t)af->extchunknitems * 4;
        assert(b2nd_exec_execute_arrays_ex(sf, mixed_arr, mixed_views, 2,
            (me_mut_buffer){&min_f32, sizeof(min_f32)}, 0, db_f * 2, 64, &fj, &r16, &err16) == ME_SUCCESS);
        assert(min_f32 == 1.0f && r16.decode_bytes == db_f * 2 && r16.execution.tasks == 2);
        b2nd_exec_schedule_free(sf);
        b2nd_exec_plan_free(pf);
        b2nd_free(af);
    }

    {
        /* Compressed output sinks: matched layout, mismatched layout, mixed, and preflight rejections. */
        double y_swap_buf[35];
        memcpy(y_swap_buf, y, sizeof(y));
        blosc2_cparams cp64 = BLOSC2_CPARAMS_DEFAULTS;
        cp64.typesize = 8;
        blosc2_storage st64 = {.cparams = &cp64};
        b2nd_array_t *ax = NULL, *ay_match = NULL, *ay_mis = NULL, *out_match = NULL, *out_mis = NULL;
        b2nd_context_t *cx = b2nd_create_ctx(&st64, 2, shape, chunks[0], blocks[0], "=f8", 0, NULL, 0);
        b2nd_context_t *cy = b2nd_create_ctx(&st64, 2, shape, chunks[1], blocks[1], "=f8", 0, NULL, 0);
        const int32_t out_c[] = {2, 5}, out_b[] = {2, 2};
        b2nd_context_t *com = b2nd_create_ctx(&st64, 2, shape, chunks[0], blocks[0], "=f8", 0, NULL, 0);
        b2nd_context_t *co2 = b2nd_create_ctx(&st64, 2, shape, out_c, out_b, "=f8", 0, NULL, 0);
        assert(cx && cy && com && co2);
        assert(b2nd_from_cbuffer(cx, &ax, x, sizeof(x)) == 0);
        assert(b2nd_from_cbuffer(cx, &ay_match, y, sizeof(y)) == 0);
        assert(b2nd_from_cbuffer(cy, &ay_mis, y, sizeof(y)) == 0);
        assert(b2nd_uninit(com, &out_match) == 0);
        assert(b2nd_uninit(co2, &out_mis) == 0);
        b2nd_free_ctx(cx); b2nd_free_ctx(cy); b2nd_free_ctx(com); b2nd_free_ctx(co2);
        poison_padding(ax);
        poison_padding(ay_match);
        poison_padding(ay_mis);

        b2nd_exec_node sn[3] = {
            make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_INPUT), make_node(2, B2ND_EXEC_ADD)};
        sn[0].dtype = ME_FLOAT64; sn[0].itemsize = 8;
        sn[1].slot = 1; sn[1].dtype = ME_FLOAT64; sn[1].itemsize = 8;
        sn[2].nargs = 2; sn[2].args[1] = 1;
        b2nd_exec_descriptor sd = {sizeof(sd), sn, 3, 2, 2};
        b2nd_exec_plan *sp = NULL;
        me_error serr;
        assert(b2nd_exec_prepare(&sd, NULL, &sp, &serr) == ME_SUCCESS);
        b2nd_exec_input_meta sm[] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 2, {5, 7}}};
        b2nd_exec_schedule *ss = NULL;
        assert(b2nd_exec_specialize(sp, sm, 2, 8, 64, &ss, &serr) == ME_SUCCESS);

        const b2nd_array_t *in_match[] = {ax, ay_match};
        b2nd_exec_storage_report sr;
        b2nd_exec_jobs sj = {sizeof(sj), 3, 65536};
        size_t match_dec_b = 2 * (size_t)out_match->blocknitems * 8;
        assert(b2nd_exec_execute_to_array(ss, in_match, NULL, 2, out_match, 0,
                                          match_dec_b * 3, 64, &sj, &sr, &serr) == ME_SUCCESS);
        double got[35];
        assert(b2nd_to_cbuffer(out_match, got, sizeof(got)) == 0);
        for (size_t i = 0; i < 35; i++) assert(got[i] == x[i] + y_swap_buf[i]);
        assert(sr.execution.fp.flags == 0 && sr.execution.gather_bytes == 0);
        /* Decode buffers are allocated for planned workers. The codec may
         * dispatch work to fewer workers; tasks counts only entered workers. */
        assert(sr.execution.tasks > 0 && sr.execution.tasks <= sj.workers);
        assert(sr.decode_bytes >= sr.execution.tasks * match_dec_b);
        assert(sr.decode_bytes <= sj.workers * match_dec_b);
        assert(sr.decode_bytes % match_dec_b == 0);

        const b2nd_array_t *in_mis[] = {ax, ay_mis};
        size_t mis_dec_b = (size_t)(ax->extchunknitems + ay_mis->extchunknitems) * 8;
        assert(b2nd_exec_execute_to_array(ss, in_mis, NULL, 2, out_mis, 0,
                                          mis_dec_b * 3, 2 * 4 * 8, &sj, &sr, &serr) == ME_SUCCESS);
        assert(b2nd_to_cbuffer(out_mis, got, sizeof(got)) == 0);
        for (size_t i = 0; i < 35; i++) assert(got[i] == x[i] + y_swap_buf[i]);

        /* Rejections before sink mutation: tight decode/gather budgets and aliased sink. */
        assert(b2nd_exec_execute_to_array(ss, in_match, NULL, 2, out_match, 0,
                                          match_dec_b - 1, 64, NULL, &sr, &serr) == ME_ERR_INVALID_ARG);
        assert(!sr.decodes);
        assert(b2nd_exec_execute_to_array(ss, in_mis, NULL, 2, out_mis, 0,
                                          mis_dec_b, 2 * 4 * 8 - 1, NULL, &sr, &serr) == ME_ERR_INVALID_ARG);
        assert(!sr.decodes);
        /* A present zero worker cap is not an unlimited allowance. Reject
         * before decoding or publishing any replacement sink chunks. */
        b2nd_exec_jobs zero_scratch = {sizeof(zero_scratch), 3, 0};
        assert(b2nd_exec_execute_to_array(ss, in_mis, NULL, 2, out_mis, 0,
                                          mis_dec_b * 3, 65536, &zero_scratch,
                                          &sr, &serr) == ME_ERR_INVALID_ARG);
        assert(!sr.decodes && !sr.execution.tiles);
        assert(b2nd_to_cbuffer(out_mis, got, sizeof(got)) == 0);
        for (size_t i = 0; i < 35; i++) assert(got[i] == x[i] + y_swap_buf[i]);
        const b2nd_array_t *alias_in[] = {out_match, ay_match};
        assert(b2nd_exec_execute_to_array(ss, alias_in, NULL, 2, out_match, 0,
                                          match_dec_b, 64, NULL, &sr, &serr) == ME_ERR_UNSUPPORTED);
        assert(!sr.decodes);

        /* Reject reductions cleanly as compressed sink destination. */
        b2nd_exec_node rn[2] = {make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_MIN)};
        rn[0].dtype = ME_FLOAT64; rn[0].itemsize = 8;
        rn[1].nargs = 1; rn[1].args[0] = 0;
        b2nd_exec_descriptor rd = {sizeof(rd), rn, 2, 1, 1};
        b2nd_exec_plan *rp = NULL;
        b2nd_exec_schedule *rs = NULL;
        assert(b2nd_exec_prepare(&rd, NULL, &rp, &serr) == ME_SUCCESS);
        assert(b2nd_exec_specialize(rp, sm, 1, 8, 64, &rs, &serr) == ME_SUCCESS);
        const b2nd_array_t *rin = ax;
        assert(b2nd_exec_execute_to_array(rs, &rin, NULL, 1, out_match, 0,
                                          match_dec_b, 64, NULL, &sr, &serr) == ME_ERR_UNSUPPORTED);
        b2nd_exec_schedule_free(rs); b2nd_exec_plan_free(rp);

        /* Late logical sNaN raises ME_ERR_EVAL when requested. */
        double x_nan[35];
        memcpy(x_nan, x, sizeof(x_nan));
        uint64_t snan = UINT64_C(0x7ff0000000000001);
        memcpy(&x_nan[34], &snan, sizeof(snan));
        b2nd_context_t *cnan = b2nd_create_ctx(&st64, 2, shape, chunks[0], blocks[0], "=f8", 0, NULL, 0);
        b2nd_array_t *anan = NULL;
        assert(cnan && b2nd_from_cbuffer(cnan, &anan, x_nan, sizeof(x_nan)) == 0);
        b2nd_free_ctx(cnan);
        const b2nd_array_t *in_nan[] = {anan, ay_match};
#ifdef __EMSCRIPTEN__
        assert(b2nd_exec_execute_to_array(ss, in_nan, NULL, 2, out_match, ME_FP_INVALID,
                                          match_dec_b, 64, NULL, &sr, &serr) == ME_ERR_UNSUPPORTED);
        assert(!sr.decodes && !sr.execution.tiles);
#else
        assert(b2nd_exec_execute_to_array(ss, in_nan, NULL, 2, out_match, ME_FP_INVALID,
                                          match_dec_b, 64, NULL, &sr, &serr) == ME_ERR_EVAL);
        assert(sr.execution.fp.flags & ME_FP_INVALID);
#endif
        b2nd_free(anan);

        /* Empty 0-item sink succeeds cleanly. */
        const int64_t eshape[] = {0, 7};
        b2nd_context_t *ce = b2nd_create_ctx(&st64, 2, eshape, chunks[0], blocks[0], "=f8", 0, NULL, 0);
        b2nd_array_t *ae = NULL, *oe = NULL;
        assert(ce && b2nd_empty(ce, &ae) == 0 && b2nd_uninit(ce, &oe) == 0);
        b2nd_free_ctx(ce);
        b2nd_exec_input_meta em[] = {{ME_FLOAT64, 8, 2, {0, 7}}, {ME_FLOAT64, 8, 2, {0, 7}}};
        b2nd_exec_schedule *es = NULL;
        assert(b2nd_exec_specialize(sp, em, 2, 8, 64, &es, &serr) == ME_SUCCESS);
        const b2nd_array_t *ine[] = {ae, ae};
        assert(b2nd_exec_execute_to_array(es, ine, NULL, 2, oe, 0, 64, 64, NULL, &sr, &serr) == ME_SUCCESS);
        assert(!sr.decodes && !sr.execution.tiles);
        b2nd_exec_schedule_free(es); b2nd_free(ae); b2nd_free(oe);

        /* Verify mismatched sink used bounded block cache. */
        assert(b2nd_exec_execute_to_array(ss, in_mis, NULL, 2, out_mis, 0,
                                          2 * 4 * 6 * 8 * 3, 2 * 4 * 8, &sj, &sr, &serr) == ME_SUCCESS);
        assert(sr.decode_bytes <= 2 * 4 * 6 * 8 * sr.execution.tasks);

        /* Compressed N-D broadcasting: (5, 7) + (7,) and (5, 7) + (5, 1). */
        const int64_t row_s[] = {7}, col_s[] = {5, 1};
        const int32_t row_c[] = {4}, row_b[] = {3}, col_c[] = {3, 1}, col_b[] = {2, 1};
        double brow[7], bcol[5];
        for (size_t i = 0; i < 7; i++) brow[i] = (double)(i + 1) * 10.0;
        for (size_t i = 0; i < 5; i++) bcol[i] = (double)(i + 1) * 100.0;
        b2nd_context_t *cr = b2nd_create_ctx(&st64, 1, row_s, row_c, row_b, "=f8", 0, NULL, 0);
        b2nd_context_t *cc = b2nd_create_ctx(&st64, 2, col_s, col_c, col_b, "=f8", 0, NULL, 0);
        b2nd_array_t *arow = NULL, *acol = NULL;
        assert(cr && cc && b2nd_from_cbuffer(cr, &arow, brow, sizeof(brow)) == 0);
        assert(b2nd_from_cbuffer(cc, &acol, bcol, sizeof(bcol)) == 0);
        b2nd_free_ctx(cr); b2nd_free_ctx(cc);
        poison_padding(arow); poison_padding(acol);
        b2nd_exec_input_meta bm1[] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 1, {7}}};
        b2nd_exec_schedule *bs1 = NULL;
        assert(b2nd_exec_specialize(sp, bm1, 2, 8, 64, &bs1, &serr) == ME_SUCCESS);
        const b2nd_array_t *in_b1[] = {ax, arow};
        double res_bcast[35];
        size_t dec_b1 = (size_t)(ax->extchunknitems + arow->extchunknitems) * 8;
        assert(b2nd_exec_execute_arrays_ex(bs1, in_b1, NULL, 2, (me_mut_buffer){res_bcast, sizeof(res_bcast)},
            0, dec_b1 * 3, 2 * 8 * 8, &sj, &sr, &serr) == ME_SUCCESS);
        for (size_t i = 0; i < 35; i++) assert(res_bcast[i] == x[i] + brow[i % 7]);
        assert(b2nd_exec_execute_to_array(bs1, in_b1, NULL, 2, out_mis, 0,
            dec_b1 * 3, 2 * 4 * 8, &sj, &sr, &serr) == ME_SUCCESS);
        assert(b2nd_to_cbuffer(out_mis, got, sizeof(got)) == 0);
        for (size_t i = 0; i < 35; i++) assert(got[i] == x[i] + brow[i % 7]);
        b2nd_exec_schedule_free(bs1);

        b2nd_exec_input_meta bm2[] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 2, {5, 1}}};
        b2nd_exec_schedule *bs2 = NULL;
        assert(b2nd_exec_specialize(sp, bm2, 2, 8, 64, &bs2, &serr) == ME_SUCCESS);
        const b2nd_array_t *in_b2[] = {ax, acol};
        assert(b2nd_exec_execute_to_array(bs2, in_b2, NULL, 2, out_mis, 0,
            dec_b1 * 3, 2 * 4 * 8, &sj, &sr, &serr) == ME_SUCCESS);
        assert(b2nd_to_cbuffer(out_mis, got, sizeof(got)) == 0);
        for (size_t r = 0; r < 5; r++)
            for (size_t c_idx = 0; c_idx < 7; c_idx++)
                assert(got[r * 7 + c_idx] == x[r * 7 + c_idx] + bcol[r]);
        b2nd_exec_schedule_free(bs2);

        /* Compressed partial-axis reductions compared bitwise with resident views. */
        b2nd_exec_node pn[2] = {make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_SUM)};
        pn[0].dtype = ME_FLOAT64; pn[0].itemsize = 8;
        pn[1].nargs = 1; pn[1].args[0] = 0;
        pn[1].has_axes = true; pn[1].naxes = 1; pn[1].axes[0] = 0; pn[1].keepdims = false;
        b2nd_exec_descriptor pd = {sizeof(pd), pn, 2, 1, 1};
        b2nd_exec_plan *pp0 = NULL;
        b2nd_exec_schedule *ps0 = NULL;
        assert(b2nd_exec_prepare(&pd, NULL, &pp0, &serr) == ME_SUCCESS);
        assert(b2nd_exec_specialize(pp0, &sm[0], 1, 4, 64, &ps0, &serr) == ME_SUCCESS);
        double sum_a0[7], sum_ref[7];
        b2nd_exec_view vx = {sizeof(vx), x, sizeof(x), 0, {56, 8}, 0};
        b2nd_exec_report vr;
        const b2nd_array_t *p_ax = ax;
        assert(b2nd_exec_execute_views(ps0, &vx, 1, (me_mut_buffer){sum_ref, sizeof(sum_ref)},
            0, 64, NULL, &vr, &serr) == ME_SUCCESS);
        assert(b2nd_exec_execute_arrays(ps0, &p_ax, 1, (me_mut_buffer){sum_a0, sizeof(sum_a0)},
            0, (size_t)ax->extchunknitems * 8, 64, &sr, &serr) == ME_SUCCESS);
        assert(!memcmp(sum_a0, sum_ref, sizeof(sum_ref)));
        b2nd_exec_schedule_free(ps0); b2nd_exec_plan_free(pp0);

        pn[1].axes[0] = 1; pn[1].keepdims = true;
        b2nd_exec_plan *pp1 = NULL;
        b2nd_exec_schedule *ps1 = NULL;
        assert(b2nd_exec_prepare(&pd, NULL, &pp1, &serr) == ME_SUCCESS);
        assert(b2nd_exec_specialize(pp1, &sm[0], 1, 4, 64, &ps1, &serr) == ME_SUCCESS);
        double sum_a1[5], sum_ref1[5];
        assert(b2nd_exec_execute_views(ps1, &vx, 1, (me_mut_buffer){sum_ref1, sizeof(sum_ref1)},
            0, 64, NULL, &vr, &serr) == ME_SUCCESS);
        assert(b2nd_exec_execute_arrays(ps1, &p_ax, 1, (me_mut_buffer){sum_a1, sizeof(sum_a1)},
            0, (size_t)ax->extchunknitems * 8, 64, &sr, &serr) == ME_SUCCESS);
        assert(!memcmp(sum_a1, sum_ref1, sizeof(sum_ref1)));
        b2nd_exec_schedule_free(ps1); b2nd_exec_plan_free(pp1);

        b2nd_exec_node mn[4] = {
            make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_INPUT),
            make_node(2, B2ND_EXEC_ADD), make_node(3, B2ND_EXEC_MIN)};
        mn[0].dtype = ME_FLOAT64; mn[0].itemsize = 8;
        mn[1].slot = 1; mn[1].dtype = ME_FLOAT64; mn[1].itemsize = 8;
        mn[2].nargs = 2; mn[2].args[1] = 1;
        mn[3].nargs = 1; mn[3].args[0] = 2;
        mn[3].has_axes = true; mn[3].naxes = 1; mn[3].axes[0] = 0;
        b2nd_exec_descriptor md = {sizeof(md), mn, 4, 2, 3};
        b2nd_exec_plan *mp = NULL;
        b2nd_exec_schedule *ms = NULL;
        assert(b2nd_exec_prepare(&md, NULL, &mp, &serr) == ME_SUCCESS);
        assert(b2nd_exec_specialize(mp, sm, 2, 3, 64, &ms, &serr) == ME_SUCCESS);
        double min_a0[7], min_ref[7];
        b2nd_exec_view vy = {sizeof(vy), y, sizeof(y), 0, {56, 8}, 0};
        b2nd_exec_view vxy[2] = {vx, vy};
        assert(b2nd_exec_execute_views(ms, vxy, 2, (me_mut_buffer){min_ref, sizeof(min_ref)},
            0, 64, &sj, &vr, &serr) == ME_SUCCESS);
        assert(b2nd_exec_execute_arrays_ex(ms, in_mis, NULL, 2, (me_mut_buffer){min_a0, sizeof(min_a0)},
            0, mis_dec_b * 3, 64, &sj, &sr, &serr) == ME_SUCCESS);
        assert(!memcmp(min_a0, min_ref, sizeof(min_ref)));
        b2nd_exec_schedule_free(ms); b2nd_exec_plan_free(mp);

        b2nd_free(arow); b2nd_free(acol);
        b2nd_exec_schedule_free(ss); b2nd_exec_plan_free(sp);
        b2nd_free(ax); b2nd_free(ay_match); b2nd_free(ay_mis);
        b2nd_free(out_match); b2nd_free(out_mis);
    }

    {
        /* Frame-backed (cframe & urlpath) inputs/sinks + argmin/argmax + var/std. */
        double fx[35], fy[35];
        for (size_t i = 0; i < 35; i++) {
            fx[i] = (double)((i * 7) % 13) - 4.0;
            fy[i] = (double)(i % 5) * 0.5;
        }
        fx[3] = -0.0; fx[9] = 0.0; fx[17] = -4.0;
        blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
        cp.typesize = 8;
        blosc2_storage st_cf = {.contiguous = true, .cparams = &cp};
#ifdef STORAGE_FAULT_TEST
        const char *p_in = "/tmp/b2_exec_storage_fault_in.b2nd";
        const char *p_out = "/tmp/b2_exec_storage_fault_out.b2nd";
#else
        const char *p_in = "/tmp/b2_exec_storage_test_in.b2nd";
        const char *p_out = "/tmp/b2_exec_storage_test_out.b2nd";
#endif
        remove(p_in); remove(p_out);
        blosc2_storage st_disk_in = {.contiguous = true, .urlpath = (char *)p_in, .cparams = &cp};
        blosc2_storage st_disk_out = {.contiguous = true, .urlpath = (char *)p_out, .cparams = &cp};
        b2nd_context_t *c_cf = b2nd_create_ctx(&st_cf, 2, shape, chunks[0], blocks[0], "=f8", 0, NULL, 0);
        b2nd_context_t *c_din = b2nd_create_ctx(&st_disk_in, 2, shape, chunks[1], blocks[1], "=f8", 0, NULL, 0);
        b2nd_context_t *c_dout = b2nd_create_ctx(&st_disk_out, 2, shape, chunks[0], blocks[0], "=f8", 0, NULL, 0);
        b2nd_array_t *acf = NULL, *adin = NULL, *adout = NULL, *ocf = NULL;
        assert(c_cf && c_din && c_dout);
        assert(b2nd_from_cbuffer(c_cf, &acf, fx, sizeof(fx)) == 0);
        assert(b2nd_from_cbuffer(c_din, &adin, fy, sizeof(fy)) == 0);
        assert(b2nd_from_cbuffer(c_cf, &ocf, fy, sizeof(fy)) == 0);
        assert(b2nd_uninit(c_dout, &adout) == 0);
        b2nd_free_ctx(c_cf); b2nd_free_ctx(c_din); b2nd_free_ctx(c_dout);
        assert(acf->sc->frame && adin->sc->frame && adout->sc->frame && ocf->sc->frame);

        b2nd_exec_node fn[3] = {
            make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_INPUT), make_node(2, B2ND_EXEC_ADD)};
        fn[0].dtype = ME_FLOAT64; fn[0].itemsize = 8;
        fn[1].slot = 1; fn[1].dtype = ME_FLOAT64; fn[1].itemsize = 8;
        fn[2].nargs = 2; fn[2].args[1] = 1;
        b2nd_exec_descriptor fd = {sizeof(fd), fn, 3, 2, 2};
        b2nd_exec_plan *fp = NULL;
        me_error ferr;
        assert(b2nd_exec_prepare(&fd, NULL, &fp, &ferr) == ME_SUCCESS);
        b2nd_exec_input_meta fm[] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 2, {5, 7}}};
        b2nd_exec_schedule *fs = NULL;
        assert(b2nd_exec_specialize(fp, fm, 2, 8, 64, &fs, &ferr) == ME_SUCCESS);

        const b2nd_array_t *fin[] = {acf, adin};
        b2nd_exec_storage_report fsr;
        b2nd_exec_jobs fj = {sizeof(fj), 3, 65536};
        size_t fdec = (size_t)(acf->extchunknitems + adin->extchunknitems) * 8 * 4;
        double fout[35], got_cf[35], got_disk[35];
        int64_t cnc0 = acf->sc->current_nchunk, cnc1 = adin->sc->current_nchunk;
        assert(b2nd_exec_execute_arrays_ex(fs, fin, NULL, 2, (me_mut_buffer){fout, sizeof(fout)},
            0, fdec, 128, &fj, &fsr, &ferr) == ME_SUCCESS);
        assert(acf->sc->current_nchunk == cnc0 && adin->sc->current_nchunk == cnc1);
        for (size_t i = 0; i < 35; i++) assert(fout[i] == fx[i] + fy[i]);

        /* Serial and mixed frame/resident calls leave no uninitialized cleanup
         * slots; failed initialization/allocation releases only live mutexes. */
        b2nd_exec_view fviews[2] = {
            {sizeof(b2nd_exec_view), fx, sizeof(fx), 0, {56, 8}, 0},
            {sizeof(b2nd_exec_view), fy, sizeof(fy), 0, {56, 8}, 0}};
        for (size_t resident = 0; resident < 2; resident++) {
            const b2nd_array_t *mixed[2] = {acf, adin};
            mixed[resident] = NULL;
            for (size_t workers = 1; workers <= 3; workers += 2) {
                b2nd_exec_jobs mixed_jobs = {sizeof(mixed_jobs), workers, 65536};
                assert(b2nd_exec_execute_arrays_ex(fs, mixed, fviews, 2,
                    (me_mut_buffer){fout, sizeof(fout)}, 0, fdec, 128,
                    &mixed_jobs, &fsr, &ferr) == ME_SUCCESS);
                for (size_t i = 0; i < 35; i++) assert(fout[i] == fx[i] + fy[i]);
#ifdef STORAGE_FAULT_TEST
                assert(live_mutexes == 0);
                allocations = 0;
                allocation_fail_at = 1;
                assert(b2nd_exec_execute_arrays_ex(fs, mixed, fviews, 2,
                    (me_mut_buffer){fout, sizeof(fout)}, 0, fdec, 128,
                    &mixed_jobs, &fsr, &ferr) == ME_ERR_OOM);
                assert(live_mutexes == 0 && !fsr.read_spans && !fsr.decodes);
                allocation_fail_at = 0;
#endif
            }
        }
#if defined(STORAGE_FAULT_TEST) && !defined(_WIN32)
        for (size_t failure = 1; failure <= 2; failure++) {
            mutex_inits = 0;
            mutex_fail_at = failure;
            assert(b2nd_exec_execute_arrays_ex(fs, fin, NULL, 2,
                (me_mut_buffer){fout, sizeof(fout)}, 0, fdec, 128,
                &fj, &fsr, &ferr) == ME_ERR_OOM);
            assert(live_mutexes == 0 && !fsr.read_spans && !fsr.decodes);
        }
        mutex_fail_at = 0;
        assert(b2nd_exec_execute_arrays_ex(fs, fin, NULL, 2,
            (me_mut_buffer){fout, sizeof(fout)}, 0, fdec, 128,
            &fj, &fsr, &ferr) == ME_SUCCESS);
        assert(live_mutexes == 0);
#endif

#if defined(STORAGE_FAULT_TEST) && !defined(_WIN32)
        /* Sink admission must clean up earlier mutexes and leave compressed
         * output untouched when any frame-mutex initialization fails. */
        double sink_before[35];
        assert(b2nd_to_cbuffer(ocf, sink_before, sizeof(sink_before)) == 0);
        for (size_t failure = 1; failure <= 2; failure++) {
            mutex_inits = 0;
            mutex_fail_at = failure;
            assert(b2nd_exec_execute_to_array(fs, fin, NULL, 2, ocf, 0, fdec, 64,
                &fj, &fsr, &ferr) == ME_ERR_OOM);
            assert(!live_mutexes && !fsr.read_spans && !fsr.decodes);
            assert(b2nd_to_cbuffer(ocf, got_cf, sizeof(got_cf)) == 0);
            assert(!memcmp(got_cf, sink_before, sizeof(got_cf)));
        }
        mutex_fail_at = 0;
#endif
        assert(b2nd_exec_execute_to_array(fs, fin, NULL, 2, ocf, 0, fdec, 64, &fj, &fsr, &ferr) == ME_SUCCESS);
        assert(b2nd_to_cbuffer(ocf, got_cf, sizeof(got_cf)) == 0);
        for (size_t i = 0; i < 35; i++) assert(got_cf[i] == fx[i] + fy[i]);

        assert(b2nd_exec_execute_to_array(fs, fin, NULL, 2, adout, 0, fdec, 64, &fj, &fsr, &ferr) == ME_SUCCESS);
        assert(b2nd_to_cbuffer(adout, got_disk, sizeof(got_disk)) == 0);
        for (size_t i = 0; i < 35; i++) assert(got_disk[i] == fx[i] + fy[i]);

        const b2nd_array_t *falias[] = {acf, adin};
        assert(b2nd_exec_execute_to_array(fs, falias, NULL, 2, acf, 0, fdec, 64, NULL, &fsr, &ferr) == ME_ERR_UNSUPPORTED);
        b2nd_exec_schedule_free(fs); b2nd_exec_plan_free(fp);

        const b2nd_array_t *p_acf = acf;
        /* Arg-extrema: ties choose earliest, parallel matches resident views, partial-axis supported. */
        for (int is_min = 0; is_min <= 1; is_min++) {
            b2nd_exec_node an[2] = {
                make_node(0, B2ND_EXEC_INPUT),
                make_node(1, is_min ? B2ND_EXEC_ARGMIN : B2ND_EXEC_ARGMAX)};
            an[0].dtype = ME_FLOAT64; an[0].itemsize = 8;
            an[1].nargs = 1; an[1].args[0] = 0;
            b2nd_exec_descriptor ad = {sizeof(ad), an, 2, 1, 1};
            b2nd_exec_plan *ap = NULL;
            b2nd_exec_schedule *as = NULL;
            assert(b2nd_exec_prepare(&ad, NULL, &ap, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_specialize(ap, &fm[0], 1, 6, 64, &as, &ferr) == ME_SUCCESS);
            int64_t idx_res = -1, idx_b2 = -1;
            b2nd_exec_view vfx = {sizeof(vfx), fx, sizeof(fx), 0, {56, 8}, 0};
            b2nd_exec_report vr;
            assert(b2nd_exec_execute_views(as, &vfx, 1, (me_mut_buffer){&idx_res, sizeof(idx_res)},
                0, 64, &fj, &vr, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_execute_arrays_ex(as, &p_acf, NULL, 1, (me_mut_buffer){&idx_b2, sizeof(idx_b2)},
                0, fdec, 64, &fj, &fsr, &ferr) == ME_SUCCESS);
            assert(idx_res == idx_b2 && idx_b2 == (is_min ? 0 : 11));
            b2nd_exec_schedule_free(as); b2nd_exec_plan_free(ap);

            an[1].has_axes = true; an[1].naxes = 1; an[1].axes[0] = 0;
            assert(b2nd_exec_prepare(&ad, NULL, &ap, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_specialize(ap, &fm[0], 1, 2, 64, &as, &ferr) == ME_SUCCESS);
            int64_t pidx_res[7], pidx_b2[7];
            assert(b2nd_exec_execute_views(as, &vfx, 1, (me_mut_buffer){pidx_res, sizeof(pidx_res)},
                0, 64, &fj, &vr, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_execute_arrays_ex(as, &p_acf, NULL, 1, (me_mut_buffer){pidx_b2, sizeof(pidx_b2)},
                0, fdec, 64, &fj, &fsr, &ferr) == ME_SUCCESS);
            assert(!memcmp(pidx_res, pidx_b2, sizeof(pidx_res)));
            b2nd_exec_schedule_free(as); b2nd_exec_plan_free(ap);
        }

        /* Moments: var/std with ddof=0 & ddof=1, parallel whole-domain & partial-axis. */
        for (int is_std = 0; is_std <= 1; is_std++) {
            b2nd_exec_node mn2[2] = {
                make_node(0, B2ND_EXEC_INPUT),
                make_node(1, is_std ? B2ND_EXEC_STD : B2ND_EXEC_VAR)};
            mn2[0].dtype = ME_FLOAT64; mn2[0].itemsize = 8;
            mn2[1].nargs = 1; mn2[1].args[0] = 0;
            mn2[1].ddof = is_std ? 1.0 : 0.0;
            b2nd_exec_descriptor md2 = {sizeof(md2), mn2, 2, 1, 1};
            b2nd_exec_plan *mp2 = NULL;
            b2nd_exec_schedule *ms2 = NULL;
            assert(b2nd_exec_prepare(&md2, NULL, &mp2, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_specialize(mp2, &fm[0], 1, 6, 64, &ms2, &ferr) == ME_SUCCESS);
            double mv_res = 0.0, mv_b2 = 0.0;
            b2nd_exec_view vfx = {sizeof(vfx), fx, sizeof(fx), 0, {56, 8}, 0};
            b2nd_exec_report vr;
            assert(b2nd_exec_execute_views(ms2, &vfx, 1, (me_mut_buffer){&mv_res, sizeof(mv_res)},
                0, 64, &fj, &vr, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_execute_arrays_ex(ms2, &p_acf, NULL, 1, (me_mut_buffer){&mv_b2, sizeof(mv_b2)},
                0, fdec, 64, &fj, &fsr, &ferr) == ME_SUCCESS);
            assert(mv_res == mv_b2);
            b2nd_exec_schedule_free(ms2); b2nd_exec_plan_free(mp2);

            mn2[1].has_axes = true; mn2[1].naxes = 1; mn2[1].axes[0] = 1; mn2[1].keepdims = true;
            assert(b2nd_exec_prepare(&md2, NULL, &mp2, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_specialize(mp2, &fm[0], 1, 3, 64, &ms2, &ferr) == ME_SUCCESS);
            double pmv_res[5], pmv_b2[5];
            assert(b2nd_exec_execute_views(ms2, &vfx, 1, (me_mut_buffer){pmv_res, sizeof(pmv_res)},
                0, 64, &fj, &vr, &ferr) == ME_SUCCESS);
            assert(b2nd_exec_execute_arrays_ex(ms2, &p_acf, NULL, 1, (me_mut_buffer){pmv_b2, sizeof(pmv_b2)},
                0, fdec, 64, &fj, &fsr, &ferr) == ME_SUCCESS);
            assert(!memcmp(pmv_res, pmv_b2, sizeof(pmv_res)));
            b2nd_exec_schedule_free(ms2); b2nd_exec_plan_free(mp2);
        }

        b2nd_free(acf); b2nd_free(adin); b2nd_free(ocf); b2nd_free(adout);
        remove(p_in); remove(p_out);
    }

    {
        /* M6 Part 1: Metadata-only generic source/sink callbacks + bounded buffers. */
        b2nd_exec_node sn[3] = {
            make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_INPUT), make_node(2, B2ND_EXEC_ADD)};
        sn[0].dtype = ME_FLOAT64; sn[0].itemsize = 8;
        sn[1].slot = 1; sn[1].dtype = ME_FLOAT64; sn[1].itemsize = 8;
        sn[2].nargs = 2; sn[2].args[1] = 1;
        b2nd_exec_descriptor sd = {sizeof(sd), sn, 3, 2, 2};
        b2nd_exec_plan *sp = NULL;
        me_error serr;
        assert(b2nd_exec_prepare(&sd, NULL, &sp, &serr) == ME_SUCCESS);

        /* Large 200,000-item streaming domain with tile_items=128 stays strictly bounded. */
        const size_t nbig = 200000;
        double *res_in = malloc(nbig * sizeof(double));
        assert(res_in);
        double expected_sum = 0.0;
        for (size_t i = 0; i < nbig; i++) {
            res_in[i] = (double)(i % 19) * 0.25;
            expected_sum += (double)(i % 97) * 1.5 + res_in[i];
        }
        b2nd_exec_input_meta bm[2] = {
            {ME_FLOAT64, 8, 1, {(int64_t)nbig}}, {ME_FLOAT64, 8, 1, {(int64_t)nbig}}};
        b2nd_exec_schedule *bs = NULL;
        assert(b2nd_exec_specialize(sp, bm, 2, 128, 128 * 8, &bs, &serr) == ME_SUCCESS);

        test_cb_source_ctx src_ctx = {0, 0, 0, true, 1.5};
        test_cb_sink_ctx sink_ctx = {0, 0, 0, 0.0};
        b2nd_exec_source srcs[2] = {
            {sizeof(srcs[0]), ME_FLOAT64, 8, 1, {(int64_t)nbig}, &src_ctx, test_cb_read},
            {sizeof(srcs[1]), ME_FLOAT64, 8, 1, {(int64_t)nbig}, NULL, NULL},
        };
        b2nd_exec_view rviews[2] = {
            {0},
            {sizeof(rviews[1]), res_in, nbig * sizeof(double), 0, {8}, 0},
        };
        b2nd_exec_sink snk = {sizeof(snk), ME_FLOAT64, 8, 1, {(int64_t)nbig}, &sink_ctx, test_cb_write};
        b2nd_exec_callback_report crep;
        assert(b2nd_exec_execute_sources(bs, srcs, rviews, 2, (me_mut_buffer){0}, &snk,
                                         0, 128 * 8, 128 * 8, &crep, &serr) == ME_SUCCESS);
        assert(src_ctx.expected_next == nbig);
        assert(sink_ctx.expected_offset == nbig);
        assert(crep.index_buffer_bytes == 128 * sizeof(size_t));
        assert(crep.execution.gather_bytes == 128 * 8);
        assert(crep.output_buffer_bytes == 128 * 8);
        assert(crep.read_items == nbig && crep.max_read_items == 128);
        assert(crep.write_items == nbig && crep.max_write_items == 128);
        assert(fabs(sink_ctx.accum - expected_sum) < 1e-9);

        /* Preflight failures never fire any callback. */
        test_cb_source_ctx p_src = {0, 0, 0, false, 1.0};
        test_cb_sink_ctx p_sink = {0, 0, 0, 0.0};
        b2nd_exec_source psrcs[2] = {
            {sizeof(psrcs[0]), ME_FLOAT64, 8, 1, {(int64_t)nbig}, &p_src, test_cb_read},
            srcs[1],
        };
        b2nd_exec_sink psnk = {sizeof(psnk), ME_FLOAT64, 8, 1, {(int64_t)nbig}, &p_sink, test_cb_write};
        assert(b2nd_exec_execute_sources(bs, psrcs, rviews, 2, (me_mut_buffer){0}, &psnk,
                                         0, 128 * 8, 64, &crep, &serr) == ME_ERR_INVALID_ARG);
        assert(p_src.calls == 0 && p_sink.calls == 0);
        psrcs[0].shape[0] = 10;
        assert(b2nd_exec_execute_sources(bs, psrcs, rviews, 2, (me_mut_buffer){0}, &psnk,
                                         0, 128 * 8, 128 * 8, &crep, &serr) == ME_ERR_INVALID_ARG);
        assert(p_src.calls == 0 && p_sink.calls == 0);
#ifdef STORAGE_FAULT_TEST
        psrcs[0].shape[0] = (int64_t)nbig;
        for (size_t failure = 1; failure <= 3; failure++) {
            allocations = 0;
            allocation_fail_at = failure;
            assert(b2nd_exec_execute_sources(bs, psrcs, rviews, 2, (me_mut_buffer){0}, &psnk,
                                             0, 128 * 8, 128 * 8, &crep, &serr) == ME_ERR_OOM);
            assert(p_src.calls == 0 && p_sink.calls == 0);
        }
        allocation_fail_at = 0;
#endif

        /* Mid-stream source failure and sink failure stop immediately with zero replay. */
        test_cb_source_ctx f_src = {0, 3, 0, false, 1.0};
        test_cb_sink_ctx f_sink = {0, 0, 0, 0.0};
        psrcs[0].shape[0] = (int64_t)nbig;
        psrcs[0].context = &f_src;
        psnk.context = &f_sink;
        assert(b2nd_exec_execute_sources(bs, psrcs, rviews, 2, (me_mut_buffer){0}, &psnk,
                                         0, 128 * 8, 128 * 8, &crep, &serr) == ME_ERR_EVAL);
        assert(f_src.calls == 3 && f_sink.calls == 2);

        test_cb_source_ctx s_src = {0, 0, 0, false, 1.0};
        test_cb_sink_ctx s_sink = {0, 2, 0, 0.0};
        psrcs[0].context = &s_src;
        psnk.context = &s_sink;
        assert(b2nd_exec_execute_sources(bs, psrcs, rviews, 2, (me_mut_buffer){0}, &psnk,
                                         0, 128 * 8, 128 * 8, &crep, &serr) == ME_ERR_EVAL);
        assert(s_src.calls == 2 && s_sink.calls == 2);
        free(res_in);
        b2nd_exec_schedule_free(bs);

        /* Mixed N-D broadcast + partial-axis reduction + empty domain zero callbacks. */
        b2nd_exec_node rn[4] = {
            make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_INPUT),
            make_node(2, B2ND_EXEC_ADD), make_node(3, B2ND_EXEC_SUM)};
        rn[0].dtype = ME_FLOAT64; rn[0].itemsize = 8;
        rn[1].slot = 1; rn[1].dtype = ME_FLOAT64; rn[1].itemsize = 8;
        rn[2].nargs = 2; rn[2].args[1] = 1;
        rn[3].nargs = 1; rn[3].args[0] = 2;
        rn[3].has_axes = true; rn[3].naxes = 1; rn[3].axes[0] = 0;
        b2nd_exec_descriptor rd = {sizeof(rd), rn, 4, 2, 3};
        b2nd_exec_plan *rp = NULL;
        assert(b2nd_exec_prepare(&rd, NULL, &rp, &serr) == ME_SUCCESS);
        b2nd_exec_input_meta rm[2] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 1, {7}}};
        b2nd_exec_schedule *rs = NULL;
        assert(b2nd_exec_specialize(rs ? NULL : rp, rm, 2, 3, 64, &rs, &serr) == ME_SUCCESS);
        double row_b[7] = {1, 2, 3, 4, 5, 6, 7};
        double r_out[7] = {0};
        test_cb_source_ctx nd_ctx = {0, 0, 0, false, 1.0};
        b2nd_exec_source nsrcs[2] = {
            {sizeof(nsrcs[0]), ME_FLOAT64, 8, 2, {5, 7}, &nd_ctx, test_cb_read},
            {sizeof(nsrcs[1]), ME_FLOAT64, 8, 1, {7}, NULL, NULL},
        };
        b2nd_exec_view nviews[2] = {{0}, {sizeof(nviews[1]), row_b, sizeof(row_b), 0, {8}, 0}};
        assert(b2nd_exec_execute_sources(rs, nsrcs, nviews, 2, (me_mut_buffer){r_out, sizeof(r_out)},
                                         NULL, 0, 64, 64, &crep, &serr) == ME_SUCCESS);
        for (size_t c = 0; c < 7; c++) {
            double want = 0.0;
            for (size_t r = 0; r < 5; r++) want += (double)((r * 7 + c) % 97) + row_b[c];
            assert(fabs(r_out[c] - want) < 1e-12);
        }
        b2nd_exec_schedule_free(rs);
        b2nd_exec_plan_free(rp);

        /* Empty domain executes zero source/sink callbacks. */
        b2nd_exec_input_meta em[2] = {{ME_FLOAT64, 8, 1, {0}}, {ME_FLOAT64, 8, 1, {0}}};
        assert(b2nd_exec_specialize(sp, em, 2, 4, 64, &rs, &serr) == ME_SUCCESS);
        test_cb_source_ctx e_src = {0, 0, 0, false, 1.0};
        test_cb_sink_ctx e_sink = {0, 0, 0, 0.0};
        b2nd_exec_source esrcs[2] = {
            {sizeof(esrcs[0]), ME_FLOAT64, 8, 1, {0}, &e_src, test_cb_read},
            {sizeof(esrcs[1]), ME_FLOAT64, 8, 1, {0}, &e_src, test_cb_read},
        };
        b2nd_exec_sink esnk = {sizeof(esnk), ME_FLOAT64, 8, 1, {0}, &e_sink, test_cb_write};
        assert(b2nd_exec_execute_sources(rs, esrcs, NULL, 2, (me_mut_buffer){0}, &esnk,
                                         0, 64, 64, &crep, &serr) == ME_SUCCESS);
        assert(e_src.calls == 0 && e_sink.calls == 0);
        b2nd_exec_schedule_free(rs);
        b2nd_exec_plan_free(sp);
    }

    {
        /* M6 Part 2: Logical partitions, block_scalar, 2-D context, 0-D, generator, rechunk invariance. */
        double pdata[35];
        for (size_t i = 0; i < 35; i++) pdata[i] = (double)(i + 1);
        const int32_t c1[2] = {3, 4}, b1[2] = {2, 3};
        const int32_t c2[2] = {1, 2}, b2[2] = {1, 1};
        blosc2_cparams pcp = BLOSC2_CPARAMS_DEFAULTS;
        pcp.typesize = sizeof(double);
        blosc2_storage pst1 = {.cparams = &pcp};
        b2nd_context_t *pc1 = b2nd_create_ctx(&pst1, 2, shape, c1, b1, "=f8", 0, NULL, 0);
        b2nd_context_t *pc2 = b2nd_create_ctx(&pst1, 2, shape, c2, b2, "=f8", 0, NULL, 0);
        b2nd_array_t *pa1 = NULL, *pa2 = NULL;
        assert(b2nd_from_cbuffer(pc1, &pa1, pdata, sizeof(pdata)) == 0);
        assert(b2nd_from_cbuffer(pc2, &pa2, pdata, sizeof(pdata)) == 0);
        b2nd_free_ctx(pc1); b2nd_free_ctx(pc2);

        me_input_spec pspec = {"x", ME_FLOAT64, sizeof(double)};
        me_signature bsig = ME_SIGNATURE_INITIALIZER;
        bsig.inputs = &pspec; bsig.ninputs = 1;
        bsig.output_dtype = ME_FLOAT64;
        bsig.cardinality = ME_BLOCK_SCALAR;
        me_prepare_options ppol = ME_PREPARE_OPTIONS_INITIALIZER;
        me_kernel *bk = NULL;
        me_error perr = {0};
        assert(me_prepare_menudet("0.1", "def k(x):\n    return block_sum(x)\n",
                                  &bsig, &ppol, &bk, &perr) == ME_SUCCESS);

        /* Block-scalar kernel cannot be a whole-array root reducer or resident schedule. */
        b2nd_exec_plan *bad_bp = NULL;
        assert(b2nd_exec_prepare_borrowed_kernel(bk, ME_REDUCTION_SUM, NULL, &bad_bp, &perr) == ME_ERR_UNSUPPORTED);
        b2nd_exec_plan *bp = NULL;
        assert(b2nd_exec_prepare_borrowed_kernel(bk, ME_REDUCTION_NONE, NULL, &bp, &perr) == ME_SUCCESS);
        b2nd_exec_input_meta pm = {ME_FLOAT64, sizeof(double), 2, {5, 7}};
        b2nd_exec_schedule *bad_rs = NULL;
        assert(b2nd_exec_specialize(bp, &pm, 1, 8, 64, &bad_rs, &perr) == ME_ERR_UNSUPPORTED);

        b2nd_exec_partition_spec ppart = {sizeof(ppart), 2, {5, 7}, {2, 3}, false, {0}, {0}};
        b2nd_exec_schedule *ps = NULL;
        assert(b2nd_exec_specialize_partitions(bp, &pm, 1, &ppart, 64, &ps, &perr) == ME_SUCCESS);
        b2nd_exec_schedule_info ps_info = {0};
        assert(b2nd_exec_describe_schedule(ps, &ps_info) == ME_SUCCESS);
        assert(ps_info.output_ndim == 2 && ps_info.output_shape[0] == 3 && ps_info.output_shape[1] == 3);

        double pout1[9] = {0}, pout2[9] = {0};
        const b2nd_array_t *arr1[1] = {pa1}, *arr2[1] = {pa2};
        b2nd_exec_storage_report prep1 = {0}, prep2 = {0};
        assert(b2nd_exec_execute_partitions(ps, arr1, NULL, NULL, 1, (me_mut_buffer){pout1, sizeof(pout1)},
                                            0, 1 << 20, 64, &prep1, &perr) == ME_SUCCESS);
        assert(b2nd_exec_execute_partitions(ps, arr2, NULL, NULL, 1, (me_mut_buffer){pout2, sizeof(pout2)},
                                            0, 1 << 20, 64, &prep2, &perr) == ME_SUCCESS);
        assert(prep1.evaluated_groups == 9 && prep1.max_group_items == 6);
        assert(!memcmp(pout1, pout2, sizeof(pout1)));
        for (size_t gr = 0; gr < 3; gr++) {
            for (size_t gc = 0; gc < 3; gc++) {
                double want = 0.0;
                for (size_t r = gr * 2; r < (gr * 2 + 2 < 5 ? gr * 2 + 2 : 5); r++) {
                    for (size_t c = gc * 3; c < (gc * 3 + 3 < 7 ? gc * 3 + 3 : 7); c++) {
                        want += pdata[r * 7 + c];
                    }
                }
                assert(pout1[gr * 3 + gc] == want);
            }
        }
        b2nd_exec_schedule_free(ps);

        /* Elementwise 2-D context partial selection reads only intersecting groups. */
        me_signature csig = ME_SIGNATURE_INITIALIZER;
        csig.inputs = &pspec; csig.ninputs = 1;
        csig.output_dtype = ME_FLOAT64;
        csig.cardinality = ME_ELEMENTWISE;
        csig.context_ndim = 2;
        me_kernel *ck = NULL;
        assert(me_prepare_menudet("0.1", "def k(x):\n    return x + block_sum(x) + _flat_idx\n",
                                  &csig, &ppol, &ck, &perr) == ME_SUCCESS);
        b2nd_exec_plan *cp2 = NULL;
        assert(b2nd_exec_prepare_borrowed_kernel(ck, ME_REDUCTION_NONE, NULL, &cp2, &perr) == ME_SUCCESS);
        b2nd_exec_partition_spec bad_ctx_part = {sizeof(bad_ctx_part), 1, {35}, {7}, false, {0}, {0}};
        assert(b2nd_exec_specialize_partitions(cp2, &pm, 1, &bad_ctx_part, 64, &ps, &perr) == ME_ERR_INVALID_ARG);
        b2nd_exec_partition_spec sub_part = {sizeof(sub_part), 2, {5, 7}, {2, 3}, true, {2, 3}, {5, 6}};
        assert(b2nd_exec_specialize_partitions(cp2, &pm, 1, &sub_part, 64, &ps, &perr) == ME_SUCCESS);
        double sub_out1[9] = {0}, sub_out2[9] = {0};
        assert(b2nd_exec_execute_partitions(ps, arr1, NULL, NULL, 1, (me_mut_buffer){sub_out1, sizeof(sub_out1)},
                                            0, 1 << 20, 64, &prep1, &perr) == ME_SUCCESS);
        assert(b2nd_exec_execute_partitions(ps, arr2, NULL, NULL, 1, (me_mut_buffer){sub_out2, sizeof(sub_out2)},
                                            0, 1 << 20, 64, &prep2, &perr) == ME_SUCCESS);
        assert(prep1.evaluated_groups == 2);
        assert(!memcmp(sub_out1, sub_out2, sizeof(sub_out1)));
#ifdef STORAGE_FAULT_TEST
        for (size_t failure = 1; failure <= 3; failure++) {
            allocations = 0;
            allocation_fail_at = failure;
            assert(b2nd_exec_execute_partitions(ps, arr1, NULL, NULL, 1,
                                                (me_mut_buffer){sub_out1, sizeof(sub_out1)},
                                                0, 1 << 20, 64, &prep1, &perr) == ME_ERR_OOM);
        }
        allocation_fail_at = 0;
#endif
        for (size_t r = 2; r < 5; r++) {
            size_t gr = (r / 2) * 2;
            size_t gr_end = gr + 2 < 5 ? gr + 2 : 5;
            double bsum = 0.0;
            for (size_t rr = gr; rr < gr_end; rr++) {
                for (size_t cc = 3; cc < 6; cc++) bsum += pdata[rr * 7 + cc];
            }
            for (size_t c = 3; c < 6; c++) {
                double want = pdata[r * 7 + c] + bsum + (double)(r * 7 + c);
                assert(sub_out1[(r - 2) * 3 + (c - 3)] == want);
            }
        }
        b2nd_exec_schedule_free(ps);
        b2nd_exec_plan_free(cp2);
        me_kernel_free(ck);

        /* 0-D scalar domain and 0-input generator kernel. */
        b2nd_exec_partition_spec p0d = {sizeof(p0d), 0, {0}, {0}, false, {0}, {0}};
        b2nd_exec_input_meta pm0d = {ME_FLOAT64, sizeof(double), 0, {0}};
        assert(b2nd_exec_specialize_partitions(bp, &pm0d, 1, &p0d, 64, &ps, &perr) == ME_SUCCESS);
        double v0 = 42.5, out0 = 0.0;
        b2nd_exec_view view0 = {sizeof(view0), &v0, sizeof(v0), 0, {0}, 0};
        assert(b2nd_exec_execute_partitions(ps, NULL, &view0, NULL, 1, (me_mut_buffer){&out0, sizeof(out0)},
                                            0, 0, 64, &prep1, &perr) == ME_SUCCESS);
        assert(out0 == 42.5 && prep1.evaluated_groups == 1);
        b2nd_exec_schedule_free(ps);
        b2nd_exec_plan_free(bp);
        me_kernel_free(bk);

        me_signature gsig = ME_SIGNATURE_INITIALIZER;
        gsig.output_dtype = ME_INT64;
        gsig.cardinality = ME_ELEMENTWISE;
        gsig.context_ndim = 1;
        me_kernel *gk = NULL;
        assert(me_prepare_menudet("0.1", "def k():\n    return _flat_idx + 10\n",
                                  &gsig, &ppol, &gk, &perr) == ME_SUCCESS);
        b2nd_exec_plan *gp = NULL;
        assert(b2nd_exec_prepare_borrowed_kernel(gk, ME_REDUCTION_NONE, NULL, &gp, &perr) == ME_SUCCESS);
        b2nd_exec_partition_spec gpart = {sizeof(gpart), 1, {5}, {2}, false, {0}, {0}};
        assert(b2nd_exec_specialize_partitions(gp, NULL, 0, &gpart, 64, &ps, &perr) == ME_SUCCESS);
        int64_t gout[5] = {0};
        assert(b2nd_exec_execute_partitions(ps, NULL, NULL, NULL, 0, (me_mut_buffer){gout, sizeof(gout)},
                                            0, 0, 64, &prep1, &perr) == ME_SUCCESS);
        for (int64_t i = 0; i < 5; i++) assert(gout[i] == i + 10);
        b2nd_exec_schedule_free(ps);
        b2nd_exec_plan_free(gp);
        me_kernel_free(gk);

        b2nd_free(pa1); b2nd_free(pa2);
    }

    {
        /* 1-D logical partition block_sum preserves exact boundaries and budget rejections. */
        double d1[7] = {1, 2, 3, 4, 5, 6, 7};
        int64_t sh1[] = {7};
        int32_t c1d[] = {5}, b1d[] = {2};
        blosc2_cparams cp1 = BLOSC2_CPARAMS_DEFAULTS;
        cp1.typesize = sizeof(double);
        blosc2_storage st1 = {.cparams = &cp1};
        b2nd_context_t *ctx1 = b2nd_create_ctx(&st1, 1, sh1, c1d, b1d, "=f8", 0, NULL, 0);
        b2nd_array_t *arr1d = NULL;
        assert(ctx1 && b2nd_from_cbuffer(ctx1, &arr1d, d1, sizeof(d1)) == 0);
        b2nd_free_ctx(ctx1);
        poison_padding(arr1d);
        me_input_spec pspec1 = {"x", ME_FLOAT64, sizeof(double)};
        me_signature bsig1 = ME_SIGNATURE_INITIALIZER;
        bsig1.inputs = &pspec1;
        bsig1.ninputs = 1;
        bsig1.output_dtype = ME_FLOAT64;
        bsig1.cardinality = ME_BLOCK_SCALAR;
        me_kernel *bk1 = NULL;
        me_error perr1 = {0};
        assert(me_prepare_menudet("0.2", "def k(x):\n    return block_sum(x)\n",
                                  &bsig1, NULL, &bk1, &perr1) == ME_SUCCESS);
        b2nd_exec_plan *bp1 = NULL;
        assert(b2nd_exec_prepare_borrowed_kernel(bk1, ME_REDUCTION_NONE, NULL, &bp1, &perr1) == ME_SUCCESS);
        b2nd_exec_input_meta pm1 = {ME_FLOAT64, sizeof(double), 1, {7}};
        b2nd_exec_partition_spec part1 = {sizeof(part1), 1, {7}, {3}, false, {0}, {0}};
        b2nd_exec_schedule *ps1 = NULL;
        assert(b2nd_exec_specialize_partitions(bp1, &pm1, 1, &part1, sizeof(double) - 1,
                                               &ps1, &perr1) == ME_ERR_INVALID_ARG);
        assert(b2nd_exec_specialize_partitions(bp1, &pm1, 1, &part1, sizeof(double),
                                               &ps1, &perr1) == ME_SUCCESS);
        double out1d[3] = {-1, -1, -1};
        const b2nd_array_t *in1d[1] = {arr1d};
        b2nd_exec_storage_report rep1d = {0};
        assert(b2nd_exec_execute_partitions(ps1, in1d, NULL, NULL, 1, (me_mut_buffer){out1d, sizeof(out1d)},
                                            0, 1 << 20, 3 * sizeof(double) - 1, &rep1d, &perr1) == ME_ERR_INVALID_ARG);
        assert(!rep1d.decodes && !rep1d.evaluated_groups && out1d[0] == -1);
        assert(b2nd_exec_execute_partitions(ps1, in1d, NULL, NULL, 1, (me_mut_buffer){out1d, sizeof(out1d)},
                                            0, 1 << 20, 3 * sizeof(double), &rep1d, &perr1) == ME_SUCCESS);
        assert(rep1d.evaluated_groups == 3 && out1d[0] == 6.0 && out1d[1] == 15.0 && out1d[2] == 7.0);
        b2nd_exec_schedule_free(ps1);
        b2nd_exec_plan_free(bp1);
        me_kernel_free(bk1);
        b2nd_free(arr1d);
    }

    {
        /* Bounded LRU preserves strict reduction order, including cancellation
         * and sNaN-poisoned physical padding, while reusing a full chunk row. */
        double values[17 * 19];
        for (size_t i = 0; i < 17 * 19; i++) values[i] = (double)(i % 113 + 1) / 8;
        values[0] = 1e16; values[1] = 1; values[2] = -1e16;
        int64_t lshape[] = {17, 19};
        int32_t lchunks[] = {5, 6}, lblocks[] = {2, 4};
        blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
        cp.typesize = 8;
        blosc2_storage storage = {.cparams = &cp};
        b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, lshape, lchunks, lblocks, "=f8", 0, NULL, 0);
        b2nd_array_t *array = NULL;
        assert(ctx && b2nd_from_cbuffer(ctx, &array, values, sizeof(values)) == 0);
        b2nd_free_ctx(ctx);
        poison_padding(array);
        b2nd_exec_node ln[] = {make_node(0, B2ND_EXEC_INPUT), make_node(1, B2ND_EXEC_SUM)};
        ln[0].dtype = ME_FLOAT64; ln[0].itemsize = 8; ln[1].nargs = 1;
        b2nd_exec_descriptor ld = {sizeof(ld), ln, 2, 1, 1};
        b2nd_exec_plan *lp = NULL;
        b2nd_exec_schedule *ls = NULL;
        me_error le;
        assert(b2nd_exec_prepare(&ld, NULL, &lp, &le) == ME_SUCCESS);
        b2nd_exec_input_meta lm = {ME_FLOAT64, 8, 2, {17, 19}};
        assert(b2nd_exec_specialize(lp, &lm, 1, 7, 64, &ls, &le) == ME_SUCCESS);
        b2nd_exec_view view = {sizeof(view), values, sizeof(values), 0, {19 * 8, 8}, 0};
        double expected, actual;
        b2nd_exec_report resident;
        assert(b2nd_exec_execute_views(ls, &view, 1, (me_mut_buffer){&expected, 8},
            0, 64, NULL, &resident, &le) == ME_SUCCESS);
        size_t chunk_bytes = (size_t)array->extchunknitems * 8;
        b2nd_exec_storage_report lr;
        const b2nd_array_t *inputs[] = {array};
        for (size_t slots = 1; slots <= 8; slots *= 2) {
            assert(b2nd_exec_execute_arrays(ls, inputs, 1, (me_mut_buffer){&actual, 8},
                0, slots * chunk_bytes, 64, &lr, &le) == ME_SUCCESS);
            assert(!memcmp(&actual, &expected, 8) && lr.execution.fp.flags == resident.fp.flags);
            assert(lr.decode_bytes == slots * chunk_bytes);
            assert(lr.decodes == (slots < 4 ? 68 : 16));
            assert(lr.decoded_bytes == lr.decodes * chunk_bytes);
        }
#ifdef STORAGE_FAULT_TEST
        for (size_t failure = 1; failure <= 2; failure++) {
            allocations = 0;
            allocation_fail_at = failure;
            assert(b2nd_exec_execute_arrays(ls, inputs, 1, (me_mut_buffer){&actual, 8},
                0, 8 * chunk_bytes, 64, &lr, &le) == ME_ERR_OOM);
            assert(!lr.decodes && !lr.read_spans);
        }
        allocation_fail_at = 0;
#endif
        b2nd_exec_schedule_free(ls);
        b2nd_exec_plan_free(lp);
        b2nd_free(array);
    }
    blosc2_destroy();
    return 0;
}

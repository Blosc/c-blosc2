/* Differential storage/resident execution through exactly the same schedule. */
#undef NDEBUG
#include "b2nd-exec-storage.h"
#include <assert.h>
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
            assert(b2nd_exec_execute_arrays(schedule, inputs, 2, (me_mut_buffer){actual, bytes},
                ME_FP_INVALID, decode_budget, gather_budget, &report, &error) == ME_SUCCESS);
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
        assert(sr.decode_bytes == sr.execution.tasks * match_dec_b);

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
        assert(b2nd_exec_execute_to_array(ss, in_nan, NULL, 2, out_match, ME_FP_INVALID,
                                          match_dec_b, 64, NULL, &sr, &serr) == ME_ERR_EVAL);
        assert(sr.execution.fp.flags & ME_FP_INVALID);
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

    blosc2_destroy();
    return 0;
}

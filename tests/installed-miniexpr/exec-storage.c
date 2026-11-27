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
    blosc2_destroy();
    return 0;
}

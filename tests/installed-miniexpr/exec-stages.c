#include "b2nd-exec-stages.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef STAGE_ADAPTER_TEST
#include "miniexpr_graph.h"
static void compare_adapter(const double *x, const double *y, const double *actual,
                            size_t tile, const b2nd_exec_staged_report *typed,
                            unsigned raise_mask) {
    const char *expression = "sum(x, axis=0) + y";
    me_graph_input_metadata metadata[] = {
        {"x", ME_FLOAT64, 2, {5, 7}}, {"y", ME_FLOAT64, 1, {7}}};
    me_graph_plan *plan = NULL;
    me_graph_schedule *schedule = NULL;
    me_graph_error error;
    assert(me_graph_prepare_expression(expression, strlen(expression), metadata, 2,
        NULL, &plan, &error) == ME_GRAPH_SUCCESS);
    me_graph_specialize_options options = {sizeof(options), ME_GRAPH_VERSION, tile, 56, false};
    assert(me_graph_specialize(plan, metadata, 2, &options, &schedule, &error) == ME_GRAPH_SUCCESS);
    me_array_view inputs[] = {
        {"x", ME_FLOAT64, x, 35 * 8, 0, 2, {5, 7}, {56, 8}, 0},
        {"y", ME_FLOAT64, y, 7 * 8, 0, 1, {7}, {8}, 0}};
    double result[7];
    memset(result, 0x55, sizeof(result));
    me_graph_report report;
    me_graph_execute_options execution = {sizeof(execution), ME_GRAPH_VERSION, raise_mask};
    me_graph_status status = me_graph_execute(schedule, inputs, 2, result, sizeof(result),
        &execution, &report, &error);
    assert(status == ((typed->fp.flags & raise_mask) ? ME_GRAPH_ERR_EXECUTION : ME_GRAPH_SUCCESS));
    assert(!memcmp(actual, result, sizeof(result)));
    assert(me_graph_output_dtype(schedule) == ME_FLOAT64);
    assert(me_graph_output_rank(schedule) == 1);
    assert(report.stages == 2);
    if (status != ME_GRAPH_SUCCESS) assert(error.stage == (int)typed->failed_stage);
    assert(report.array.fp_flags == typed->fp.flags);
    assert(report.array.fp_supported == typed->fp.supported);
    me_graph_schedule_free(schedule);
    me_graph_plan_free(plan);
}
#endif

typedef struct {
    size_t calls, items, fail_at;
    const double *values;
} stage_source;

static me_status stage_read(void *context, const size_t *indices, size_t count,
                            me_mut_buffer output, me_error *error) {
    stage_source *state = context;
    state->calls++;
    if (state->calls == state->fail_at) {
        error->status = ME_ERR_EVAL;
        snprintf(error->message, sizeof(error->message), "stage reader failure");
        return ME_ERR_EVAL;
    }
    assert(output.capacity == count * sizeof(double));
    double *dst = output.data;
    for (size_t i = 0; i < count; i++) dst[i] = state->values[indices[i]];
    state->items += count;
    return ME_SUCCESS;
}

static b2nd_exec_node stage_node(uint32_t id, b2nd_exec_op op) {
    b2nd_exec_node node = {0};
    node.struct_size = sizeof(node);
    node.id = id;
    node.op = op;
    return node;
}

int main(void) {
    /* sum(x, axis=0) + y: producer width/shape differ from input domain.
     * y deliberately has no role in the producer, exercising slot remapping. */
    b2nd_exec_node nodes[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_INPUT),
        stage_node(2, B2ND_EXEC_SUM), stage_node(3, B2ND_EXEC_ADD)};
    nodes[0].dtype = nodes[1].dtype = ME_FLOAT64;
    nodes[0].itemsize = nodes[1].itemsize = 8;
    nodes[1].slot = 1;
    nodes[2].nargs = 1;
    nodes[2].has_axes = true;
    nodes[2].naxes = 1;
    nodes[2].axes[0] = 0;
    nodes[3].nargs = 2;
    nodes[3].args[0] = 2;
    nodes[3].args[1] = 1;
    b2nd_exec_descriptor descriptor = {sizeof(descriptor), nodes, 4, 2, 3};
    b2nd_exec_staged_plan *plan = NULL;
    me_error error;
    assert(b2nd_exec_staged_prepare(&descriptor, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta meta[] = {{ME_FLOAT64, 8, 2, {5, 7}}, {ME_FLOAT64, 8, 1, {7}}};
    double x[35], y[7], expected[7] = {0}, actual[7];
    for (size_t i = 0; i < 35; i++) x[i] = (double)i - 12.5;
    for (size_t j = 0; j < 7; j++) {
        y[j] = (double)j * 0.25;
        for (size_t i = 0; i < 5; i++) expected[j] += x[i * 7 + j];
        expected[j] += y[j];
    }
    stage_source states[] = {{0, 0, 0, x}, {0, 0, 0, y}};
    b2nd_exec_source sources[] = {
        {sizeof(b2nd_exec_source), ME_FLOAT64, 8, 2, {5, 7}, &states[0], stage_read},
        {sizeof(b2nd_exec_source), ME_FLOAT64, 8, 1, {7}, &states[1], stage_read}};
    b2nd_exec_view views[] = {
        {sizeof(b2nd_exec_view), x, sizeof(x), 0, {56, 8}, 0},
        {sizeof(b2nd_exec_view), y, sizeof(y), 0, {8}, 0}};

    for (size_t tile = 1; tile <= 8; tile *= 2) {
        b2nd_exec_staged_schedule *schedule = NULL;
        assert(b2nd_exec_staged_specialize(plan, meta, 2, tile, 64, sizeof(expected) - 1,
            &schedule, &error) == ME_ERR_INVALID_ARG);
        assert(!schedule && !states[0].calls && !states[1].calls);
        assert(strstr(error.message, "intermediate budget"));
        assert(b2nd_exec_staged_specialize(plan, meta, 2, tile, 64, sizeof(expected),
            &schedule, &error) == ME_SUCCESS);
        b2nd_exec_stage_info producer, consumer;
        assert(b2nd_exec_staged_describe(schedule, 0, &producer) == ME_SUCCESS);
        assert(b2nd_exec_staged_describe(schedule, 1, &consumer) == ME_SUCCESS);
        assert(producer.node == 2 && producer.last_consumer == 1);
        assert(producer.plan.dtype == ME_FLOAT64 && producer.schedule.output_ndim == 1);
        assert(producer.schedule.output_shape[0] == 7 && producer.output_bytes == sizeof(expected));
        assert(consumer.node == 3 && consumer.schedule.output_shape[0] == 7);
        assert(consumer.last_consumer == SIZE_MAX);
        b2nd_exec_staged_report report;
        /* Invalid final output and stage-1 metadata/gather budget must not read x. */
        memset(actual, 0x55, sizeof(actual));
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual) - 1}, 0, 128,
            &report, &error) == ME_ERR_INVALID_ARG);
        assert(!states[0].calls && !states[1].calls && !report.entered_stages);
        assert(report.intermediate_bytes == 0);
        sources[1].shape[0]--;
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128,
            &report, &error) == ME_ERR_INVALID_ARG);
        sources[1].shape[0]++;
        assert(!states[0].calls && !states[1].calls);
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 0,
            &report, &error) == ME_ERR_INVALID_ARG);
        assert(!states[0].calls && !states[1].calls);

        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
        assert(!memcmp(actual, expected, sizeof(expected)));
        assert(states[0].items == 35 && states[1].items == 7);
        assert(report.entered_stages == 2 && report.intermediate_bytes == sizeof(expected));
        assert(report.live_intermediate_bytes == 0);
        assert(report.failed_stage == SIZE_MAX && report.failed_node == UINT32_MAX);
        assert(report.stages[0].read_items == 35 && report.stages[1].read_items == 7);
        assert(report.stages[0].execution.direct_tiles > 0);

        /* Late producer failure never enters the consumer or publishes output. */
        states[0] = (stage_source){0, 0, 2, x};
        states[1] = (stage_source){0, 0, 0, y};
        memset(actual, 0x55, sizeof(actual));
        double untouched[7];
        memcpy(untouched, actual, sizeof(actual));
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_ERR_EVAL);
        assert(states[0].calls == 2 && states[1].calls == 0);
        assert(!memcmp(actual, untouched, sizeof(actual)));
        assert(report.entered_stages == 1 && report.live_intermediate_bytes == 0);
        assert(report.failed_stage == 0 && report.failed_node == 2);
        assert(strstr(error.message, "stage reader failure"));

        /* Final map failure stops immediately; no retry of either source. */
        states[0] = (stage_source){0, 0, 0, x};
        states[1] = (stage_source){0, 0, 1, y};
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_ERR_EVAL);
        assert(states[0].items == 35 && states[1].calls == 1);
        assert(report.entered_stages == 2 && report.live_intermediate_bytes == 0);
        assert(report.failed_stage == 1 && report.failed_node == 3);
        states[0] = (stage_source){0, 0, 0, x};
        states[1] = (stage_source){0, 0, 0, y};

        /* Same immutable schedule, resident and mixed bindings; reject aliases. */
        sources[0].read = sources[1].read = NULL;
        assert(b2nd_exec_staged_execute_sources(schedule, sources, views, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
        assert(!memcmp(actual, expected, sizeof(expected)));
        assert(b2nd_exec_staged_execute_sources(schedule, sources, views, 2,
            (me_mut_buffer){x, sizeof(actual)}, 0, 128, &report, &error) == ME_ERR_UNSUPPORTED);
        assert(!report.entered_stages);
        sources[0].read = stage_read;
        assert(b2nd_exec_staged_execute_sources(schedule, sources, views, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
        assert(!memcmp(actual, expected, sizeof(expected)));
        sources[1].read = stage_read;

#ifdef STAGE_ADAPTER_TEST
        compare_adapter(x, y, actual, tile, &report, 0);
#endif
        /* Union late floating-point flags across stages, then enforce raise_mask.
         * Both masks follow a full traversal; a final map can already be written. */
        uint64_t snan = UINT64_C(0x7ff0000000000001);
        double saved = y[6];
        memcpy(&y[6], &snan, sizeof(snan));
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
        assert(isnan(actual[6]));
#ifdef STAGE_ADAPTER_TEST
        compare_adapter(x, y, actual, tile, &report, 0);
#endif
        if (report.fp.supported) {
            assert(report.fp.flags & ME_FP_INVALID);
            states[0].items = states[1].items = 0;
            assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
                (me_mut_buffer){actual, sizeof(actual)}, ME_FP_INVALID, 128,
                &report, &error) == ME_ERR_EVAL);
            assert(states[0].items == 35 && states[1].items == 7);
            assert(report.entered_stages == 2 && report.live_intermediate_bytes == 0);
#ifdef STAGE_ADAPTER_TEST
            compare_adapter(x, y, actual, tile, &report, ME_FP_INVALID);
#endif
        }
        y[6] = saved;
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
        assert(!report.fp.flags && !memcmp(actual, expected, sizeof(expected)));

        /* A flagged producer stops before reading any consumer-only source,
         * preserving the adapter's per-stage raise-mask boundary. */
        double saved_x = x[34];
        memcpy(&x[34], &snan, sizeof(snan));
        if (report.fp.supported) {
            states[0].items = states[1].items = 0;
            memset(actual, 0x55, sizeof(actual));
            assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
                (me_mut_buffer){actual, sizeof(actual)}, ME_FP_INVALID, 128,
                &report, &error) == ME_ERR_EVAL);
            assert(states[0].items == 35 && states[1].items == 0);
            assert(report.entered_stages == 1 && report.failed_stage == 0);
            assert(report.live_intermediate_bytes == 0);
#ifdef STAGE_ADAPTER_TEST
            compare_adapter(x, y, actual, tile, &report, ME_FP_INVALID);
#endif
        }
        x[34] = saved_x;

#ifdef STAGE_FAULT_TEST
        /* Metadata rejection must happen before the producer allocation. Only
         * the two bounded binding arrays may be allocated on this path. */
        allocations = 0;
        allocation_fail_at = 3;
        sources[1].shape[0]--;
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_ERR_INVALID_ARG);
        sources[1].shape[0]++;
        assert(allocations == 2 && report.intermediate_bytes == 0);
        allocation_fail_at = 0;
        /* Explicit FP observations must merge with AND, whereas an absent
         * observation must not downgrade the other stage's observed support. */
        for (size_t stage = 0; stage < 2; stage++) {
            execution_calls = 0;
            unsupported_fp_stage = stage;
            assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
                (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
            assert(!report.fp.supported);
        }
        unsupported_fp_stage = SIZE_MAX;
        execution_calls = 0;
        unobserved_fp_stage = 0;
        assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
            (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_SUCCESS);
        assert(report.fp.supported == report.stages[1].execution.fp.supported);
        unobserved_fp_stage = SIZE_MAX;
        allocations = 0;
        allocation_fail_at = 1;
        b2nd_exec_staged_schedule *failed = NULL;
        assert(b2nd_exec_staged_specialize(plan, meta, 2, tile, 64, sizeof(expected),
            &failed, &error) == ME_ERR_OOM);
        assert(!failed);
        for (size_t failure = 1; failure <= 3; failure++) {
            allocations = 0;
            allocation_fail_at = failure;
            states[0].calls = states[1].calls = 0;
            assert(b2nd_exec_staged_execute_sources(schedule, sources, NULL, 2,
                (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &report, &error) == ME_ERR_OOM);
            assert(!states[0].calls && !states[1].calls && !report.entered_stages);
            assert(report.live_intermediate_bytes == 0 && report.intermediate_bytes == 0);
        }
        allocation_fail_at = 0;
#endif
        b2nd_exec_staged_schedule_free(schedule);
        states[0] = (stage_source){0, 0, 0, x};
        states[1] = (stage_source){0, 0, 0, y};
    }
    /* Empty producer result is a zero-byte intermediate, with no callbacks. */
    b2nd_exec_input_meta empty_meta[] = {
        {ME_FLOAT64, 8, 2, {5, 0}}, {ME_FLOAT64, 8, 1, {0}}};
    b2nd_exec_staged_schedule *empty_schedule = NULL;
    assert(b2nd_exec_staged_specialize(plan, empty_meta, 2, 4, 64, 0,
        &empty_schedule, &error) == ME_SUCCESS);
    sources[0].shape[1] = 0;
    sources[1].shape[0] = 0;
    b2nd_exec_staged_report empty_report;
    assert(b2nd_exec_staged_execute_sources(empty_schedule, sources, NULL, 2,
        (me_mut_buffer){NULL, 0}, 0, 0, &empty_report, &error) == ME_SUCCESS);
    assert(!states[0].calls && !states[1].calls);
    assert(empty_report.intermediate_bytes == 0 && empty_report.live_intermediate_bytes == 0);
    /* Non-NULL origins of empty intervals do not alias enclosing allocations. */
    sources[0].read = sources[1].read = NULL;
    views[0] = (b2nd_exec_view){sizeof(b2nd_exec_view), actual + 1, 0, 0, {0}, 0};
    views[1] = (b2nd_exec_view){sizeof(b2nd_exec_view), actual + 2, 0, 0, {0}, 0};
    assert(b2nd_exec_staged_execute_sources(empty_schedule, sources, views, 2,
        (me_mut_buffer){actual, sizeof(actual)}, 0, 0, &empty_report, &error) == ME_SUCCESS);
    views[0] = (b2nd_exec_view){sizeof(b2nd_exec_view), actual, sizeof(actual), 0, {0}, 0};
    assert(b2nd_exec_staged_execute_sources(empty_schedule, sources, views, 2,
        (me_mut_buffer){actual + 1, 0}, 0, 0, &empty_report, &error) == ME_SUCCESS);
    b2nd_exec_staged_schedule_free(empty_schedule);
    b2nd_exec_staged_plan_free(plan);

    /* Canonical shared input: x - sum(x), including repeated source reads across
     * the two regions, late consumer failure, resident parity and reuse. */
    b2nd_exec_node centered[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_SUM),
        stage_node(2, B2ND_EXEC_SUBTRACT)};
    centered[0].dtype = ME_FLOAT64; centered[0].itemsize = 8;
    centered[1].nargs = 1;
    centered[2].nargs = 2; centered[2].args[1] = 1;
    b2nd_exec_descriptor centered_desc = {sizeof(centered_desc), centered, 3, 1, 2};
    assert(b2nd_exec_staged_prepare(&centered_desc, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta centered_meta = {ME_FLOAT64, 8, 1, {7}};
    b2nd_exec_staged_schedule *centered_schedule = NULL;
    assert(b2nd_exec_staged_specialize(plan, &centered_meta, 1, 2, 64, 8,
        &centered_schedule, &error) == ME_SUCCESS);
    states[0] = (stage_source){0, 0, 0, y};
    sources[0] = (b2nd_exec_source){sizeof(b2nd_exec_source), ME_FLOAT64, 8, 1, {7}, &states[0], stage_read};
    b2nd_exec_staged_report centered_report;
    assert(b2nd_exec_staged_execute_sources(centered_schedule, sources, NULL, 1,
        (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &centered_report, &error) == ME_SUCCESS);
    assert(states[0].items == 14 && states[0].calls == 8);
    double sum_y = 0;
    for (size_t i = 0; i < 7; i++) sum_y += y[i];
    for (size_t i = 0; i < 7; i++) assert(actual[i] == y[i] - sum_y);
    states[0] = (stage_source){0, 0, 6, y};
    assert(b2nd_exec_staged_execute_sources(centered_schedule, sources, NULL, 1,
        (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &centered_report, &error) == ME_ERR_EVAL);
    assert(states[0].calls == 6 && centered_report.failed_stage == 1);
    sources[0].read = NULL;
    views[0] = (b2nd_exec_view){sizeof(b2nd_exec_view), y, sizeof(y), 0, {8}, 0};
    assert(b2nd_exec_staged_execute_sources(centered_schedule, sources, views, 1,
        (me_mut_buffer){actual, sizeof(actual)}, 0, 128, &centered_report, &error) == ME_SUCCESS);
    for (size_t i = 0; i < 7; i++) assert(actual[i] == y[i] - sum_y);
    b2nd_exec_staged_schedule_free(centered_schedule);
    b2nd_exec_staged_plan_free(plan);

#ifdef STAGE_FAULT_TEST
    for (size_t failure = 1; failure <= 3; failure++) {
        allocations = 0;
        allocation_fail_at = failure;
        assert(b2nd_exec_staged_prepare(&descriptor, NULL, &plan, &error) == ME_ERR_OOM);
        assert(!plan);
    }
    allocation_fail_at = 0;
#endif

    /* Inferred narrow-integer sum promotion feeds the consumer as int64, not int8. */
    nodes[0].dtype = ME_INT8; nodes[0].itemsize = 1;
    nodes[1].dtype = ME_INT64;
    assert(b2nd_exec_staged_prepare(&descriptor, NULL, &plan, &error) == ME_SUCCESS);
    meta[0].dtype = ME_INT8; meta[0].itemsize = 1;
    meta[0].ndim = 1; meta[0].shape[0] = 4;
    meta[1].dtype = ME_INT64; meta[1].ndim = 0;
    b2nd_exec_staged_schedule *schedule = NULL;
    assert(b2nd_exec_staged_specialize(plan, meta, 2, 2, 64, 8, &schedule, &error) == ME_SUCCESS);
    b2nd_exec_stage_info info;
    assert(b2nd_exec_staged_describe(schedule, 0, &info) == ME_SUCCESS);
    assert(info.plan.dtype == ME_INT64 && info.schedule.output_ndim == 0);
    int8_t narrow[] = {100, 100, 100, 100};
    int64_t scalar = 7, total = 0;
    sources[0] = (b2nd_exec_source){sizeof(b2nd_exec_source), ME_INT8, 1, 1, {4}, NULL, NULL};
    sources[1] = (b2nd_exec_source){sizeof(b2nd_exec_source), ME_INT64, 8, 0, {0}, NULL, NULL};
    views[0] = (b2nd_exec_view){sizeof(b2nd_exec_view), narrow, sizeof(narrow), 0, {1}, 0};
    views[1] = (b2nd_exec_view){sizeof(b2nd_exec_view), &scalar, sizeof(scalar), 0, {0}, 0};
    b2nd_exec_staged_report report;
    assert(b2nd_exec_staged_execute_sources(schedule, sources, views, 2,
        (me_mut_buffer){&total, sizeof(total)}, 0, 128, &report, &error) == ME_SUCCESS);
    assert(total == 407);
    assert(!report.stages[0].execution.fp_observed);
    assert(report.fp.supported == report.stages[1].execution.fp.supported);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);

    /* Shared producer references are one stage result, not repeated reductions.
     * Fuse the producer map and preserve exact typed captures beyond double. */
    int64_t capture = INT64_C(9007199254740993);
    b2nd_exec_node shared[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_CAPTURE),
        stage_node(2, B2ND_EXEC_MULTIPLY), stage_node(3, B2ND_EXEC_SUM),
        stage_node(4, B2ND_EXEC_ADD)};
    shared[0].dtype = shared[1].dtype = ME_INT64;
    shared[0].itemsize = shared[1].itemsize = 8;
    shared[1].scalar_size = 8;
    shared[1].category = ME_CAPTURE_TYPED;
    memcpy(shared[1].scalar, &capture, sizeof(capture));
    shared[2].nargs = 2; shared[2].args[1] = 1;
    shared[3].nargs = 1; shared[3].args[0] = 2;
    shared[3].has_initial = true; shared[3].initial_dtype = ME_INT64;
    shared[3].initial_category = ME_CAPTURE_TYPED; shared[3].initial_size = 8;
    int64_t initial = 3;
    memcpy(shared[3].initial_scalar, &initial, sizeof(initial));
    shared[3].keepdims = true;
    shared[4].nargs = 2; shared[4].args[0] = shared[4].args[1] = 3;
    b2nd_exec_descriptor shared_desc = {sizeof(shared_desc), shared, 5, 1, 4};
    assert(b2nd_exec_staged_prepare(&shared_desc, NULL, &plan, &error) == ME_SUCCESS);
    /* Preparation copies all child nodes/captures; caller storage can change. */
    memset(shared, 0, sizeof(shared));
    b2nd_exec_input_meta shared_meta = {ME_INT64, 8, 1, {2}};
    assert(b2nd_exec_staged_specialize(plan, &shared_meta, 1, 1, 64, 8,
        &schedule, &error) == ME_SUCCESS);
    assert(b2nd_exec_staged_describe(schedule, 0, &info) == ME_SUCCESS);
    assert(info.schedule.output_ndim == 1 && info.schedule.output_shape[0] == 1);
    int64_t ones[] = {1, 1};
    sources[0] = (b2nd_exec_source){sizeof(b2nd_exec_source), ME_INT64, 8, 1, {2}, NULL, NULL};
    views[0] = (b2nd_exec_view){sizeof(b2nd_exec_view), ones, sizeof(ones), 0, {8}, 0};
    assert(b2nd_exec_staged_execute_sources(schedule, sources, views, 1,
        (me_mut_buffer){&total, sizeof(total)}, 0, 128, &report, &error) == ME_SUCCESS);
    assert(total == 4 * capture + 2 * initial);
    assert(report.entered_stages == 2 && report.stages[0].execution.map_calls == 2);
    assert(report.stages[1].execution.map_calls == 1);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);

    /* Unsupported topology and malformed node references reject during prepare. */
    b2nd_exec_node nested[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_SUM),
        stage_node(2, B2ND_EXEC_SUM)};
    nested[0].dtype = ME_FLOAT64; nested[0].itemsize = 8;
    nested[1].nargs = nested[2].nargs = 1; nested[2].args[0] = 1;
    b2nd_exec_descriptor bad = {sizeof(bad), nested, 3, 1, 2};
    assert(b2nd_exec_staged_prepare(&bad, NULL, &plan, &error) == ME_ERR_UNSUPPORTED);
    assert(!plan);
    nodes[3].op = B2ND_EXEC_ADD; nodes[3].nargs = 2; nodes[3].args[0] = 3;
    assert(b2nd_exec_staged_prepare(&descriptor, NULL, &plan, &error) == ME_ERR_INVALID_ARG);
    assert(!plan);
    puts("Typed reduction-to-map stages passed");
    return 0;
}

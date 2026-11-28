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

static void compare_adapter_nested(const int8_t *narrow, size_t tile, int64_t actual,
                                   const b2nd_exec_staged_report *typed) {
    const char *expression = "sum(sum(x, axis=0))";
    me_graph_input_metadata metadata = {"x", ME_INT8, 2, {3, 4}};
    me_graph_plan *plan = NULL;
    me_graph_schedule *schedule = NULL;
    me_graph_error error;
    assert(me_graph_prepare_expression(expression, strlen(expression), &metadata, 1,
        NULL, &plan, &error) == ME_GRAPH_SUCCESS);
    me_graph_specialize_options options = {sizeof(options), ME_GRAPH_VERSION, tile, 32, false};
    assert(me_graph_specialize(plan, &metadata, 1, &options, &schedule, &error) == ME_GRAPH_SUCCESS);
    me_array_view input = {"x", ME_INT8, narrow, 12, 0, 2, {3, 4}, {4, 1}, 0};
    int64_t result = 0;
    me_graph_report report;
    assert(me_graph_execute(schedule, &input, 1, &result, sizeof(result), NULL, &report, &error) == ME_GRAPH_SUCCESS);
    assert(result == actual);
    assert(me_graph_output_dtype(schedule) == ME_INT64);
    assert(me_graph_output_rank(schedule) == 0);
    assert(report.stages == 2);
    assert(report.array.fp_flags == typed->fp.flags);
    if (typed->stages[0].execution.fp_observed || typed->stages[1].execution.fp_observed) {
        assert(report.array.fp_supported == typed->fp.supported);
    }
    me_graph_schedule_free(schedule);
    me_graph_plan_free(plan);
}

static void compare_adapter_masked_staged(const double *x, const uint8_t *m, double actual,
                                          const b2nd_exec_staged_report *typed) {
    const char *doc =
        "{\"format\":\"menudet-staged-graph-1\",\"semantics\":\"menudet-numpy-0.1\","
        "\"requires\":[\"numeric\",\"staged\"],"
        "\"inputs\":[{\"name\":\"x\",\"dtype\":\"float64\"},{\"name\":\"m\",\"dtype\":\"bool\"}],"
        "\"root\":1,\"stages\":["
        "{\"id\":0,\"kind\":\"graph\",\"inputs\":{\"x\":{\"input\":\"x\"}},\"graph\":{"
        "\"format\":\"menudet-graph-1\",\"semantics\":\"menudet-numpy-0.1\",\"requires\":[\"numeric\"],"
        "\"nodes\":[{\"id\":0,\"op\":\"input\",\"name\":\"x\",\"dtype\":\"auto\"},"
        "{\"id\":1,\"op\":\"constant\",\"dtype\":\"int64\",\"encoding\":\"decimal\",\"value\":\"2\",\"category\":\"weak\"},"
        "{\"id\":2,\"op\":\"mul\",\"args\":[0,1]}],"
        "\"root\":2,\"output\":{\"dtype\":\"auto\",\"casting\":\"unsafe\"}}},"
        "{\"id\":1,\"kind\":\"graph\",\"inputs\":{\"v\":{\"stage\":0},\"m\":{\"input\":\"m\"}},\"graph\":{"
        "\"format\":\"menudet-graph-1\",\"semantics\":\"menudet-numpy-0.1\",\"requires\":[\"numeric\"],"
        "\"nodes\":[{\"id\":0,\"op\":\"input\",\"name\":\"v\",\"dtype\":\"auto\"},"
        "{\"id\":1,\"op\":\"input\",\"name\":\"m\",\"dtype\":\"auto\"},"
        "{\"id\":2,\"op\":\"function\",\"name\":\"sqrt\",\"args\":[0]},"
        "{\"id\":3,\"op\":\"sum\",\"args\":[2],\"axes\":null,\"keepdims\":false,\"dtype\":\"auto\",\"initial\":null,\"where\":1}],"
        "\"root\":3,\"output\":{\"dtype\":\"auto\",\"casting\":\"unsafe\"}}}]}";
    me_graph_plan *plan = NULL;
    me_graph_schedule *schedule = NULL;
    me_graph_error error;
    assert(me_graph_prepare_json(doc, strlen(doc), NULL, &plan, &error) == ME_GRAPH_SUCCESS);
    me_graph_input_metadata metadata[] = {
        {"x", ME_FLOAT64, 1, {4}}, {"m", ME_BOOL, 1, {4}}};
    me_graph_specialize_options options = {sizeof(options), ME_GRAPH_VERSION, 3, 32, false};
    assert(me_graph_specialize(plan, metadata, 2, &options, &schedule, &error) == ME_GRAPH_SUCCESS);
    me_array_view inputs[] = {
        {"x", ME_FLOAT64, x, 32, 0, 1, {4}, {8}, 0},
        {"m", ME_BOOL, m, 4, 0, 1, {4}, {1}, 0}};
    double result = -1.0;
    me_graph_report report;
    me_graph_execute_options exec = {sizeof(exec), ME_GRAPH_VERSION, 0};
    assert(me_graph_execute(schedule, inputs, 2, &result, sizeof(result), &exec, &report, &error) == ME_GRAPH_SUCCESS);
    assert(!memcmp(&result, &actual, sizeof(actual)));
    assert(report.stages == 2);
    assert(report.array.fp_flags == typed->fp.flags);
    if (typed->stages[0].execution.fp_observed || typed->stages[1].execution.fp_observed) {
        assert(report.array.fp_supported == typed->fp.supported);
    }
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

static void test_general_stages(void) {
    b2nd_exec_schedule *schedule_s = NULL;
    double values[7] = {1, 4, 9, 16, 25, 36, 49}, output = -77;
    b2nd_exec_node n[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_FUNCTION),
        stage_node(2, B2ND_EXEC_SUM), stage_node(3, B2ND_EXEC_MAX),
        stage_node(4, B2ND_EXEC_ADD)};
    n[0].dtype = ME_FLOAT64; n[0].itemsize = 8;
    n[1].nargs = 1; strcpy(n[1].function, "sqrt");
    n[2].nargs = n[3].nargs = 1; n[2].args[0] = n[3].args[0] = 1;
    n[4].nargs = 2; n[4].args[0] = 2; n[4].args[1] = 3;
    b2nd_exec_descriptor d = {sizeof(d), n, 5, 1, 4};
    b2nd_exec_staged_plan *plan = NULL;
    me_error error;
    assert(b2nd_exec_staged_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_staged_count(plan) == 4);
    b2nd_exec_input_meta meta = {ME_FLOAT64, 8, 1, {7}};
    b2nd_exec_staged_schedule *schedule = NULL;
    b2nd_exec_stage_diagnostic diag = {0, 0};
    assert(b2nd_exec_staged_specialize_ex(plan, &meta, 1, 2, 64, 71, &schedule, &diag, &error) == ME_ERR_INVALID_ARG);
    assert(!schedule && diag.stage == 2 && diag.node == 3);
    assert(b2nd_exec_staged_specialize_ex(plan, &meta, 1, 2, 64, 72, &schedule, &diag, &error) == ME_SUCCESS);
    assert(diag.stage == SIZE_MAX && diag.node == UINT32_MAX);
    b2nd_exec_stage_info info;
    assert(b2nd_exec_staged_describe(schedule, 0, &info) == ME_SUCCESS);
    assert(info.node == 1 && info.last_consumer == 2 && info.output_bytes == 56);
    stage_source state = {0, 0, 0, values};
    b2nd_exec_source source = {sizeof(source), ME_FLOAT64, 8, 1, {7}, &state, stage_read};
    b2nd_exec_staged_report report;
    assert(b2nd_exec_staged_execute_sources(schedule, &source, NULL, 1,
        (me_mut_buffer){&output, sizeof(output)}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(output == 35 && state.items == 7 && state.calls == 4);
    assert(report.entered_stages == 4 && report.intermediate_bytes == 72 && !report.live_intermediate_bytes);
#ifdef STAGE_FAULT_TEST
    for (size_t failure = 1; failure <= 5; failure++) {
        allocations = 0; allocation_fail_at = failure;
        state.calls = 0; state.items = 0; output = -77;
        assert(b2nd_exec_staged_execute_sources(schedule, &source, NULL, 1,
            (me_mut_buffer){&output, sizeof(output)}, 0, 64, &report, &error) == ME_ERR_OOM);
        assert(output == -77 && !state.calls && !report.entered_stages && !report.live_intermediate_bytes);
        assert(report.intermediate_bytes == (failure < 4 ? 0 : failure == 4 ? 56 : 64));
    }
    allocation_fail_at = 0;
#endif
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);

    /* Nested reductions infer each producer's promoted dtype before its consumer. */
    b2nd_exec_node nested[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_SUM),
        stage_node(2, B2ND_EXEC_SUM)};
    nested[0].dtype = ME_INT8; nested[0].itemsize = 1;
    nested[1].nargs = nested[2].nargs = 1; nested[2].args[0] = 1;
    nested[1].has_axes = true; nested[1].naxes = 1;
    b2nd_exec_descriptor nd = {sizeof(nd), nested, 3, 1, 2};
    assert(b2nd_exec_staged_prepare(&nd, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta nm = {ME_INT8, 1, 2, {3, 4}};
    assert(b2nd_exec_staged_specialize(plan, &nm, 1, 2, 64, 32, &schedule, &error) == ME_SUCCESS);
    int8_t narrow[12];
    for (size_t i = 0; i < 12; i++) narrow[i] = (int8_t)(i + 1);
    b2nd_exec_view view = {sizeof(view), narrow, sizeof(narrow), 0, {4, 1}, 0};
    source = (b2nd_exec_source){sizeof(source), ME_INT8, 1, 2, {3, 4}, NULL, NULL};
    int64_t sum = 0;
    assert(b2nd_exec_staged_execute_sources(schedule, &source, &view, 1,
        (me_mut_buffer){&sum, sizeof(sum)}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(sum == 78 && report.entered_stages == 2);
    assert(b2nd_exec_staged_describe(schedule, 0, &info) == ME_SUCCESS && info.plan.dtype == ME_INT64);
#ifdef STAGE_ADAPTER_TEST
    compare_adapter_nested(narrow, 2, sum, &report);
#endif
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);

    nested[2].op = B2ND_EXEC_SUBTRACT;
    nested[2].nargs = 2; nested[2].args[0] = 0; nested[2].args[1] = 1;
    assert(b2nd_exec_staged_prepare(&nd, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_staged_specialize(plan, &nm, 1, 3, 64, 32, &schedule, &error) == ME_SUCCESS);
    view.offset = 3; view.strides[1] = -1;
    int64_t centered[12];
    assert(b2nd_exec_staged_execute_sources(schedule, &source, &view, 1,
        (me_mut_buffer){centered, sizeof(centered)}, 0, 64, &report, &error) == ME_SUCCESS);
    for (size_t row = 0; row < 3; row++) {
        for (size_t col = 0; col < 4; col++) {
            int64_t total_column = narrow[3 - col] + narrow[7 - col] + narrow[11 - col];
            assert(centered[row * 4 + col] == narrow[row * 4 + 3 - col] - total_column);
        }
    }
    assert(report.stages[1].execution.gather_bytes == 32); /* Includes mixed-width alignment. */
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);

    /* Explicit imported bindings borrow prepared plans; no child serialization,
     * recompilation, accidental transfer or dependence on a Python owner. */
    b2nd_exec_plan *children[3] = {NULL};
    b2nd_exec_node map[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_ADD)};
    map[0].dtype = ME_FLOAT64; map[0].itemsize = 8;
    map[1].nargs = 2;
    b2nd_exec_descriptor md = {sizeof(md), map, 2, 1, 1};
    assert(b2nd_exec_prepare(&md, NULL, &children[0], &error) == ME_SUCCESS);
    map[1].op = B2ND_EXEC_SUM; map[1].nargs = 1;
    assert(b2nd_exec_prepare(&md, NULL, &children[1], &error) == ME_SUCCESS);
    b2nd_exec_node add[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_INPUT),
        stage_node(2, B2ND_EXEC_ADD)};
    add[0].dtype = add[1].dtype = ME_FLOAT64; add[0].itemsize = add[1].itemsize = 8;
    add[1].slot = 1; add[2].nargs = 2; add[2].args[1] = 1;
    b2nd_exec_descriptor ad = {sizeof(ad), add, 3, 2, 2};
    assert(b2nd_exec_prepare(&ad, NULL, &children[2], &error) == ME_SUCCESS);
    int bindings[][2] = {{-1, 0}, {0, 0}, {0, 1}};
    b2nd_exec_stage_spec specs[] = {
        {children[0], bindings[0], 700}, {children[1], bindings[1], 701},
        {children[2], bindings[2], 702}};
    assert(b2nd_exec_staged_assemble(specs, 3, 1, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_staged_specialize(plan, &meta, 1, 2, 64, 64, &schedule, &error) == ME_SUCCESS);
    source = (b2nd_exec_source){sizeof(source), ME_FLOAT64, 8, 1, {7}, NULL, NULL};
    view = (b2nd_exec_view){sizeof(view), values, sizeof(values), 0, {8}, 0};
    double result[7], total = 0;
    for (size_t i = 0; i < 7; i++) total += values[i] * 2;
    assert(b2nd_exec_staged_execute_sources(schedule, &source, &view, 1,
        (me_mut_buffer){result, sizeof(result)}, 0, 64, &report, &error) == ME_SUCCESS);
    for (size_t i = 0; i < 7; i++) assert(result[i] == values[i] * 2 + total);
    assert(b2nd_exec_staged_describe(schedule, 2, &info) == ME_SUCCESS && info.node == 702);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);
    bindings[2][1] = 2;
    assert(b2nd_exec_staged_assemble_ex(specs, 3, 1, &plan, &diag, &error) == ME_ERR_INVALID_ARG && !plan);
    assert(diag.stage == 2 && diag.node == 702);
    b2nd_exec_plan_info child_info;
    for (size_t i = 0; i < 3; i++) {
        assert(b2nd_exec_describe(children[i], &child_info) == ME_SUCCESS);
        b2nd_exec_plan_free(children[i]);
    }

    /* has_jit AND-semantics: an interpreter child disqualifies whole-plan has_jit. */
    map[1].op = B2ND_EXEC_ADD; map[1].nargs = 2;
    b2nd_exec_options interp_opts = B2ND_EXEC_OPTIONS_INITIALIZER;
    interp_opts.execution = ME_EXEC_INTERPRET;
    interp_opts.fp_mode = ME_FP_FAST;
    interp_opts.jit_compiler = ME_COMPILER_CC; /* TCC remains strict-only. */
    assert(b2nd_exec_prepare(&md, &interp_opts, &children[0], &error) == ME_SUCCESS);
    b2nd_exec_options jit_opts = B2ND_EXEC_OPTIONS_INITIALIZER;
    jit_opts.execution = ME_EXEC_PREFER_JIT;
    map[1].op = B2ND_EXEC_SUM; map[1].nargs = 1;
    assert(b2nd_exec_prepare(&md, &jit_opts, &children[1], &error) == ME_SUCCESS);
    int jit_bindings[][1] = {{-1}, {0}};
    b2nd_exec_stage_spec jit_specs[] = {
        {children[0], jit_bindings[0], 10}, {children[1], jit_bindings[1], 11}};
    assert(b2nd_exec_staged_assemble(jit_specs, 2, 1, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_staged_plan_describe(plan, &child_info) == ME_SUCCESS);
    assert(!child_info.has_jit);
    /* Aggregate policy is root-stage policy; children retain their own policy. */
    assert(strcmp(child_info.fp_mode, "strict") == 0);
    assert(b2nd_exec_describe(children[0], &child_info) == ME_SUCCESS);
    assert(strcmp(child_info.fp_mode, "fast") == 0);
    b2nd_exec_staged_plan_free(plan);
    b2nd_exec_plan_free(children[0]);
    b2nd_exec_plan_free(children[1]);

    /* Authored numeric Menudet and masked graph regions share the same native
     * stage scheduler in the artifact-off consumer. Inactive sqrt lanes must
     * not raise FP; reader-backed masks reject before even the first stage. */
    me_input_spec input = {"x", ME_FLOAT64, 8};
    me_signature signature = ME_SIGNATURE_INITIALIZER;
    signature.inputs = &input; signature.ninputs = 1;
    signature.output_dtype = ME_FLOAT64; signature.cardinality = ME_ELEMENTWISE;
    me_prepare_options options = ME_PREPARE_OPTIONS_INITIALIZER;
    me_kernel *kernel = NULL;
    assert(me_prepare_menudet("0.1", "def k(x):\n    return x * 2\n", &signature,
        &options, &kernel, &error) == ME_SUCCESS);
    assert(b2nd_exec_prepare_kernel(&kernel, ME_REDUCTION_NONE, &children[0], &error) == ME_SUCCESS);
    assert(!kernel);
    b2nd_exec_node mask_nodes[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_INPUT),
        stage_node(2, B2ND_EXEC_FUNCTION), stage_node(3, B2ND_EXEC_SUM)};
    mask_nodes[0].dtype = ME_FLOAT64; mask_nodes[0].itemsize = 8;
    mask_nodes[1].dtype = ME_BOOL; mask_nodes[1].itemsize = 1; mask_nodes[1].slot = 1;
    mask_nodes[2].nargs = 1; strcpy(mask_nodes[2].function, "sqrt");
    mask_nodes[3].nargs = 1; mask_nodes[3].args[0] = 2;
    mask_nodes[3].has_where = true; mask_nodes[3].where = 1;
    b2nd_exec_descriptor masked = {sizeof(masked), mask_nodes, 4, 2, 3};
    assert(b2nd_exec_prepare(&masked, NULL, &children[1], &error) == ME_SUCCESS);
    int mask_bindings[][2] = {{-1, 0}, {0, -2}};
    b2nd_exec_stage_spec mask_specs[] = {
        {children[0], mask_bindings[0], 800}, {children[1], mask_bindings[1], 801}};
    assert(b2nd_exec_staged_assemble(mask_specs, 2, 2, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta mask_meta[] = {{ME_FLOAT64, 8, 1, {4}}, {ME_BOOL, 1, 1, {4}}};
    assert(b2nd_exec_staged_specialize(plan, mask_meta, 2, 3, 64, 32, &schedule, &error) == ME_SUCCESS);
    double masked_values[] = {-9, 4, -1, 16};
    uint8_t mask[] = {0, 1, 0, 1};
    b2nd_exec_source mask_sources[] = {
        {sizeof(b2nd_exec_source), ME_FLOAT64, 8, 1, {4}, NULL, NULL},
        {sizeof(b2nd_exec_source), ME_BOOL, 1, 1, {4}, NULL, NULL}};
    b2nd_exec_view mask_views[] = {
        {sizeof(b2nd_exec_view), masked_values, sizeof(masked_values), 0, {8}, 0},
        {sizeof(b2nd_exec_view), mask, sizeof(mask), 0, {1}, 0}};
    assert(b2nd_exec_staged_execute_sources(schedule, mask_sources, mask_views, 2,
        (me_mut_buffer){&output, sizeof(output)}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(output == sqrt(8) + sqrt(32) && !report.fp.flags);
#ifdef STAGE_ADAPTER_TEST
    compare_adapter_masked_staged(masked_values, mask, output, &report);
#endif
    uint8_t empty_mask[4] = {0, 0, 0, 0};
    mask_views[1].base = empty_mask;
    assert(b2nd_exec_staged_execute_sources(schedule, mask_sources, mask_views, 2,
        (me_mut_buffer){&output, sizeof(output)}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(output == 0.0 && !report.fp.flags);
    mask_views[1].base = mask;
    stage_source rejected = {0, 0, 0, masked_values};
    mask_sources[0].context = &rejected; mask_sources[0].read = stage_read;
    assert(b2nd_exec_staged_execute_sources(schedule, mask_sources, mask_views, 2,
        (me_mut_buffer){&output, sizeof(output)}, 0, 64, &report, &error) == ME_ERR_UNSUPPORTED);
    assert(!rejected.calls && !report.entered_stages);
    b2nd_exec_staged_schedule_free(schedule); b2nd_exec_staged_plan_free(plan);
    b2nd_exec_plan_free(children[0]); b2nd_exec_plan_free(children[1]);

    /* All-false mask on single-stage prod and max(initial=5.0), without inactive FP traps. */
    mask_nodes[3].op = B2ND_EXEC_PROD;
    assert(b2nd_exec_prepare(&masked, NULL, &children[0], &error) == ME_SUCCESS);
    assert(b2nd_exec_specialize(children[0], mask_meta, 2, 2, 64, &schedule_s, &error) == ME_SUCCESS);
    b2nd_exec_view fviews[] = {
        {sizeof(b2nd_exec_view), masked_values, sizeof(masked_values), 0, {8}, 0},
        {sizeof(b2nd_exec_view), empty_mask, sizeof(empty_mask), 0, {1}, 0}};
    b2nd_exec_report srep;
    output = -1.0;
    assert(b2nd_exec_execute_views(schedule_s, fviews, 2, (me_mut_buffer){&output, sizeof(output)}, 0, 64, NULL, &srep, &error) == ME_SUCCESS);
    assert(output == 1.0 && !srep.fp.flags);
    b2nd_exec_schedule_free(schedule_s);
    b2nd_exec_plan_free(children[0]);
    mask_nodes[3].op = B2ND_EXEC_MAX;
    mask_nodes[3].has_initial = true;
    mask_nodes[3].initial_dtype = ME_FLOAT64;
    mask_nodes[3].initial_category = ME_CAPTURE_WEAK;
    mask_nodes[3].initial_size = 8;
    double five = 5.0;
    memcpy(mask_nodes[3].initial_scalar, &five, sizeof(five));
    assert(b2nd_exec_prepare(&masked, NULL, &children[0], &error) == ME_SUCCESS);
    assert(b2nd_exec_specialize(children[0], mask_meta, 2, 2, 64, &schedule_s, &error) == ME_SUCCESS);
    output = -1.0;
    assert(b2nd_exec_execute_views(schedule_s, fviews, 2, (me_mut_buffer){&output, sizeof(output)}, 0, 64, NULL, &srep, &error) == ME_SUCCESS);
    assert(output == 5.0 && !srep.fp.flags);
    b2nd_exec_schedule_free(schedule_s);
    b2nd_exec_plan_free(children[0]);

    /* Empty (0, 4), (2, 0) and rank-0 () staged execution on sum(x) + x. */
    b2nd_exec_node empty_nodes[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_SUM),
        stage_node(2, B2ND_EXEC_ADD)};
    empty_nodes[0].dtype = ME_FLOAT64; empty_nodes[0].itemsize = 8;
    empty_nodes[1].nargs = 1;
    empty_nodes[2].nargs = 2; empty_nodes[2].args[0] = 1; empty_nodes[2].args[1] = 0;
    b2nd_exec_descriptor ed = {sizeof(ed), empty_nodes, 3, 1, 2};
    assert(b2nd_exec_staged_prepare(&ed, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta em0 = {ME_FLOAT64, 8, 2, {0, 4}};
    assert(b2nd_exec_staged_specialize(plan, &em0, 1, 2, 64, 8, &schedule, &error) == ME_SUCCESS);
    source = (b2nd_exec_source){sizeof(source), ME_FLOAT64, 8, 2, {0, 4}, &state, stage_read};
    state.calls = 0;
    assert(b2nd_exec_staged_execute_sources(schedule, &source, NULL, 1,
        (me_mut_buffer){NULL, 0}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(!state.calls && report.intermediate_bytes == 8);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_input_meta em1 = {ME_FLOAT64, 8, 2, {2, 0}};
    assert(b2nd_exec_staged_specialize(plan, &em1, 1, 2, 64, 8, &schedule, &error) == ME_SUCCESS);
    source.shape[0] = 2; source.shape[1] = 0;
    assert(b2nd_exec_staged_execute_sources(schedule, &source, NULL, 1,
        (me_mut_buffer){NULL, 0}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(!state.calls);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_input_meta r0 = {ME_FLOAT64, 8, 0, {0}};
    assert(b2nd_exec_staged_specialize(plan, &r0, 1, 2, 64, 8, &schedule, &error) == ME_SUCCESS);
    double scalar_val = 4.5, scalar_out = 0.0;
    source = (b2nd_exec_source){sizeof(source), ME_FLOAT64, 8, 0, {0}, NULL, NULL};
    b2nd_exec_view r0_view = {sizeof(r0_view), &scalar_val, sizeof(scalar_val), 0, {0}, 0};
    assert(b2nd_exec_staged_execute_sources(schedule, &source, &r0_view, 1,
        (me_mut_buffer){&scalar_out, sizeof(scalar_out)}, 0, 64, &report, &error) == ME_SUCCESS);
    assert(scalar_out == 9.0);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);

    /* Reject shared scalar-only subexpression and automatic conditional/masked staged graphs. */
    b2nd_exec_node scalar_shared[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_CAPTURE),
        stage_node(2, B2ND_EXEC_CAPTURE), stage_node(3, B2ND_EXEC_ADD),
        stage_node(4, B2ND_EXEC_ADD), stage_node(5, B2ND_EXEC_ADD)};
    int64_t one = 1, two = 2;
    scalar_shared[0].dtype = ME_FLOAT64; scalar_shared[0].itemsize = 8;
    for (size_t k = 1; k <= 2; k++) {
        scalar_shared[k].dtype = ME_INT64; scalar_shared[k].itemsize = 8;
        scalar_shared[k].scalar_size = 8; scalar_shared[k].category = ME_CAPTURE_WEAK;
    }
    memcpy(scalar_shared[1].scalar, &one, 8);
    memcpy(scalar_shared[2].scalar, &two, 8);
    scalar_shared[3].nargs = 2; scalar_shared[3].args[0] = 1; scalar_shared[3].args[1] = 2;
    scalar_shared[4].nargs = 2; scalar_shared[4].args[0] = 3; scalar_shared[4].args[1] = 3;
    scalar_shared[5].nargs = 2; scalar_shared[5].args[0] = 0; scalar_shared[5].args[1] = 4;
    b2nd_exec_descriptor ssd = {sizeof(ssd), scalar_shared, 6, 1, 5};
    assert(b2nd_exec_staged_prepare_ex(&ssd, NULL, &plan, &diag, &error) == ME_ERR_UNSUPPORTED);
    assert(!plan && diag.stage == SIZE_MAX && diag.node == 3);

    b2nd_exec_node cond_nodes[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_INPUT),
        stage_node(2, B2ND_EXEC_GT), stage_node(3, B2ND_EXEC_SELECT),
        stage_node(4, B2ND_EXEC_SUM), stage_node(5, B2ND_EXEC_ADD)};
    cond_nodes[0].dtype = ME_FLOAT64; cond_nodes[0].itemsize = 8;
    cond_nodes[1].dtype = ME_FLOAT64; cond_nodes[1].itemsize = 8; cond_nodes[1].slot = 1;
    cond_nodes[2].nargs = 2; cond_nodes[2].args[0] = 0; cond_nodes[2].args[1] = 1;
    cond_nodes[3].nargs = 3; cond_nodes[3].args[0] = 2; cond_nodes[3].args[1] = 0; cond_nodes[3].args[2] = 1;
    cond_nodes[4].nargs = 1; cond_nodes[4].args[0] = 0;
    cond_nodes[5].nargs = 2; cond_nodes[5].args[0] = 3; cond_nodes[5].args[1] = 4;
    b2nd_exec_descriptor cd = {sizeof(cd), cond_nodes, 6, 2, 5};
    assert(b2nd_exec_staged_prepare_ex(&cd, NULL, &plan, &diag, &error) == ME_ERR_UNSUPPORTED);
    assert(!plan && diag.node == 3);

    b2nd_exec_node auto_masked[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_INPUT),
        stage_node(2, B2ND_EXEC_SUM), stage_node(3, B2ND_EXEC_SUBTRACT),
        stage_node(4, B2ND_EXEC_SUM)};
    auto_masked[0].dtype = ME_FLOAT64; auto_masked[0].itemsize = 8;
    auto_masked[1].dtype = ME_BOOL; auto_masked[1].itemsize = 1; auto_masked[1].slot = 1;
    auto_masked[2].nargs = 1;
    auto_masked[3].nargs = 2; auto_masked[3].args[0] = 0; auto_masked[3].args[1] = 2;
    auto_masked[4].nargs = 1; auto_masked[4].args[0] = 3;
    auto_masked[4].has_where = true; auto_masked[4].where = 1;
    b2nd_exec_descriptor amd = {sizeof(amd), auto_masked, 5, 2, 4};
    assert(b2nd_exec_staged_prepare_ex(&amd, NULL, &plan, &diag, &error) == ME_ERR_UNSUPPORTED);
    assert(!plan && diag.node == 4);

    /* Consumer stage failure during prepare sets stage/node diagnostics. */
    b2nd_exec_node bad_fn[] = {
        stage_node(0, B2ND_EXEC_INPUT), stage_node(1, B2ND_EXEC_SUM),
        stage_node(2, B2ND_EXEC_FUNCTION), stage_node(3, B2ND_EXEC_ADD)};
    bad_fn[0].dtype = ME_FLOAT64; bad_fn[0].itemsize = 8;
    bad_fn[1].nargs = 1;
    bad_fn[2].nargs = 1; strcpy(bad_fn[2].function, "no_such_fn");
    bad_fn[3].nargs = 2; bad_fn[3].args[0] = 1; bad_fn[3].args[1] = 2;
    b2nd_exec_descriptor bfd = {sizeof(bfd), bad_fn, 4, 1, 3};
    assert(b2nd_exec_staged_prepare_ex(&bfd, NULL, &plan, &diag, &error) != ME_SUCCESS);
    assert(!plan && diag.stage == 1 && diag.node == 3);
}

int main(void) {
    test_general_stages();
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
    for (size_t failure = 1; failure <= 2; failure++) {
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
    assert(b2nd_exec_staged_prepare(&bad, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_staged_count(plan) == 2);
    b2nd_exec_staged_plan_free(plan);
    nodes[3].op = B2ND_EXEC_ADD; nodes[3].nargs = 2; nodes[3].args[0] = 3;
    assert(b2nd_exec_staged_prepare(&descriptor, NULL, &plan, &error) == ME_ERR_INVALID_ARG);
    assert(!plan);

    /* Non-contiguous / F-order staged output view & non-injective rejection before reads. */
    b2nd_exec_node fnodes[3] = {
        stage_node(0, B2ND_EXEC_INPUT),
        stage_node(1, B2ND_EXEC_SUM),
        stage_node(2, B2ND_EXEC_SUBTRACT),
    };
    fnodes[0].dtype = ME_FLOAT64; fnodes[0].itemsize = 8;
    fnodes[1].nargs = 1; fnodes[1].axes[0] = 0; fnodes[1].has_axes = true; fnodes[1].naxes = 1;
    fnodes[2].nargs = 2; fnodes[2].args[1] = 1;
    b2nd_exec_descriptor fdesc = {sizeof(fdesc), fnodes, 3, 1, 2};
    assert(b2nd_exec_staged_prepare(&fdesc, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta fmeta = {ME_FLOAT64, 8, 2, {2, 3}};
    assert(b2nd_exec_staged_specialize(plan, &fmeta, 1, 2, 128, 128, &schedule, &error) == ME_SUCCESS);
    double fin[6] = {1, 2, 3, 4, 5, 6};
    double fout[6] = {0};
    sources[0] = (b2nd_exec_source){sizeof(b2nd_exec_source), ME_FLOAT64, 8, 2, {2, 3}, NULL, NULL};
    views[0] = (b2nd_exec_view){sizeof(views[0]), fin, sizeof(fin), 0, {24, 8}, 0};
    b2nd_exec_output_view f_out = {sizeof(f_out), fout, sizeof(fout), 0, {8, 16}};
    assert(b2nd_exec_staged_execute_sources_into(schedule, sources, views, 1, &f_out, 0, 128, &report, &error) == ME_SUCCESS);
    double expected_f[6] = {-4, -1, -5, -2, -6, -3};
    for (int i = 0; i < 6; i++) assert(fout[i] == expected_f[i]);
    b2nd_exec_output_view bad_out = {sizeof(bad_out), fout, sizeof(fout), 0, {0, 8}};
    assert(b2nd_exec_staged_execute_sources_into(schedule, sources, views, 1, &bad_out, 0, 128, &report, &error) == ME_ERR_INVALID_ARG);
    b2nd_exec_staged_schedule_free(schedule);
    b2nd_exec_staged_plan_free(plan);
    puts("Typed reduction-to-map stages passed");
    return 0;
}

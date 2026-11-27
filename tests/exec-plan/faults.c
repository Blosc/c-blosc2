/* Fail each allocation owned by the new planner, independently of core tests. */
#include <assert.h>
#include <stdlib.h>

static size_t allocations, fail_at;
static void *fault_malloc(size_t size) {
    return ++allocations == fail_at ? NULL : malloc(size);
}
static void *fault_calloc(size_t count, size_t size) {
    return ++allocations == fail_at ? NULL : calloc(count, size);
}
#define malloc fault_malloc
#define calloc fault_calloc
#include "../../blosc/b2nd-exec.c"
#undef malloc
#undef calloc

int main(void) {
    b2nd_exec_node node = {0};
    node.struct_size = sizeof(node);
    node.op = B2ND_EXEC_INPUT;
    node.dtype = ME_INT64;
    node.itemsize = 8;
    b2nd_exec_descriptor d = {sizeof(d), &node, 1, 1, 0};
    me_error error;
    for (size_t failure = 1; failure <= 3; failure++) {
        allocations = 0;
        fail_at = failure;
        b2nd_exec_plan *plan = (void *)(uintptr_t)1;
        assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_ERR_OOM);
        assert(!plan);
    }
    fail_at = 0;
    b2nd_exec_plan *plan = NULL;
    assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
    b2nd_exec_input_meta meta = {ME_INT64, 8, 1, {17}};
    b2nd_exec_schedule *schedule = (void *)(uintptr_t)1;
    allocations = 0;
    fail_at = 1;
    assert(b2nd_exec_specialize(plan, &meta, 1, 8, 0, &schedule, &error) == ME_ERR_OOM);
    assert(!schedule);
    b2nd_exec_plan_free(plan);

    b2nd_exec_node nodes[2] = {node, {0}};
    nodes[1].struct_size = sizeof(node);
    nodes[1].id = 1;
    nodes[1].op = B2ND_EXEC_NEGATE;
    nodes[1].nargs = 1;
    b2nd_exec_node minimum = {0};
    minimum.struct_size = sizeof(minimum);
    minimum.id = 2;
    minimum.op = B2ND_EXEC_MIN;
    minimum.nargs = 1;
    minimum.args[0] = 1;
    b2nd_exec_node reduction[3] = {nodes[0], nodes[1], minimum};
    d.nodes = reduction;
    d.nnodes = 3;
    d.root = 2;
    fail_at = 0;
    assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
    assert(b2nd_exec_specialize(plan, &meta, 1, 8, 64, &schedule, &error) == ME_SUCCESS);
    int64_t values[17] = {0}, output[17];
    memset(output, 0x55, sizeof(output));
    me_buffer input = {values, sizeof(values)};
    b2nd_exec_report report;
    allocations = 0;
    fail_at = 1;
    assert(b2nd_exec_execute(schedule, &input, 1, (me_mut_buffer){output, sizeof(output)},
        0, NULL, &report, &error) == ME_ERR_OOM);
    assert(!report.tiles && ((unsigned char *)output)[0] == 0x55);
    /* Inject OOM across mapped, gather and job allocations for strided views. */
    b2nd_exec_view strided = {sizeof(strided), values, sizeof(values), 0, {8}, 0};
    b2nd_exec_schedule *strided_schedule = NULL;
    b2nd_exec_input_meta half_meta = {ME_INT64, 8, 1, {8}};
    fail_at = 0;
    assert(b2nd_exec_specialize(plan, &half_meta, 1, 4, 32, &strided_schedule, &error) == ME_SUCCESS);
    strided.strides[0] = 16;
    for (size_t failure = 1; failure <= 3; failure++) {
        allocations = 0;
        fail_at = failure;
        assert(b2nd_exec_execute_views(strided_schedule, &strided, 1,
            (me_mut_buffer){output, sizeof(output)}, 0, 32, NULL, &report, &error) == ME_ERR_OOM);
        assert(!report.tiles && ((unsigned char *)output)[0] == 0x55);
    }
    b2nd_exec_schedule_free(strided_schedule);
    b2nd_exec_schedule_free(schedule);
    b2nd_exec_plan_free(plan);
    return 0;
}

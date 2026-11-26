/* Wire-free descriptor and borrowed-schedule contract. No artifact headers. */
#include "b2nd-exec.h"
#include <assert.h>
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
    for (int invalid = 0; invalid < 12; invalid++) {
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
        }
        plan = (b2nd_exec_plan *)(uintptr_t)1;
        assert(b2nd_exec_prepare(&bad, &opts, &plan, &error) != ME_SUCCESS);
        assert(!plan && error.status != ME_SUCCESS);
        memcpy(nodes, saved, sizeof(saved));
    }

    /* The descriptor is temporary: poison it before metadata queries. */
    assert(b2nd_exec_prepare(&d, NULL, &plan, &error) == ME_SUCCESS);
    memset(nodes, 0xff, sizeof(nodes));
    assert(b2nd_exec_describe(plan, &info) == ME_SUCCESS);
    assert(info.dtype == ME_INT64 && info.ninputs == 2);
    b2nd_exec_plan_free(plan);
    b2nd_exec_plan_free(NULL);
    b2nd_exec_schedule_free(NULL);
    return 0;
}

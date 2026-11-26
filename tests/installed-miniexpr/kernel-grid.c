#include "b2nd-kernel.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

typedef struct {
    const b2nd_array_t *array;
    int reads, writes;
    bool scalar, fail_read, logical;
} fixture;

static int read_block(void *context, size_t input, int64_t chunk, int64_t block,
                      const me_grid_block_info *info, me_mut_buffer out) {
    (void)chunk;
    (void)block;
    fixture *state = context;
    if (input != 0 || info->ndim != 1 || !info->valid_nitems ||
        info->padded_nitems != (state->logical ? 3 : 2)) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    state->reads++;
    if (state->fail_read) {
        return BLOSC2_ERROR_READ_BUFFER;
    }
    int64_t stop = info->origin[0] + info->extent[0];
    int64_t padded = (int64_t)info->padded_nitems;
    return b2nd_get_slice_cbuffer(state->array, info->origin, &stop,
                                  out.data, &padded, (int64_t)out.capacity);
}

static int write_block(void *context, int64_t chunk, int64_t block,
                       const me_grid_block_info *info, me_buffer result) {
    fixture *state = context;
    const double *values = result.data;
    if (state->logical) {
        const double sums[] = {3, 12, 6};
        if (block != 0 || chunk < 0 || chunk >= 3 || chunk != state->writes++ ||
            result.capacity != sizeof(double)) {
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        return values[0] == sums[chunk] ? 0 : BLOSC2_ERROR_FAILURE;
    }
    if (chunk * 3 + block != state->writes++ ||
        result.capacity != (state->scalar ? 1 : 2) * sizeof(double)) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (state->scalar) {
        const double sums[] = {1, 5, 4, 11, 0, 0};
        return values[0] == sums[chunk * 3 + block] ? 0 : BLOSC2_ERROR_FAILURE;
    }
    for (size_t i = 0; i < info->padded_nitems; i++) {
        double expected = i < info->valid_nitems ? (double)info->origin[0] + (double)i + 1 : 0;
        if (values[i] != expected) {
            return BLOSC2_ERROR_FAILURE;
        }
    }
    return 0;
}

int main(void) {
    const int64_t shape[] = {7};
    const int32_t chunks[] = {5}, blocks[] = {2};
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    blosc2_init();
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunks, blocks, "=f8", 0, NULL, 0);
    b2nd_array_t *array = NULL;
    CHECK(ctx && b2nd_arange(ctx, &array, 0, 7, 1) == 0);
    me_input_spec input = {"x", ME_FLOAT64, 0};
    me_signature signature = ME_SIGNATURE_INITIALIZER;
    signature.inputs = &input;
    signature.ninputs = 1;
    const char *sources[] = {
        "def k(x):\n    return x + 1\n",
        "def k(x):\n    return block_sum(x)\n",
    };
    size_t width = sizeof(double);
    for (int scalar = 0; scalar <= 1; scalar++) {
        fixture state = {.array = array, .scalar = scalar != 0};
        size_t budget = (scalar ? 3 : 4) * width;
        me_kernel *kernel = NULL;
        CHECK(me_prepare_menudet("0.2", sources[scalar], &signature, NULL, &kernel, NULL) == ME_SUCCESS);
        CHECK(b2nd_eval_kernel_grid(array, kernel, 1, &width, budget - 1,
            read_block, &state, write_block, &state, NULL) == BLOSC2_ERROR_INVALID_PARAM);
        CHECK(state.reads == 0 && state.writes == 0);
        CHECK(b2nd_eval_kernel_grid(array, kernel, 1, &width, budget,
            read_block, &state, write_block, &state, NULL) == 0);
        CHECK(state.reads == 4 && state.writes == 6);
        state = (fixture){.array = array, .scalar = scalar != 0, .fail_read = true};
        CHECK(b2nd_eval_kernel_grid(array, kernel, 1, &width, budget,
            read_block, &state, write_block, &state, NULL) == BLOSC2_ERROR_READ_BUFFER);
        CHECK(state.reads == 1 && state.writes == 0);
        if (scalar) {
            state = (fixture){.array = array, .scalar = true, .logical = true};
            CHECK(b2nd_eval_kernel_partitions_1d(7, 3, kernel, 1, &width, 4 * width - 1,
                read_block, &state, write_block, &state, NULL, NULL) == BLOSC2_ERROR_INVALID_PARAM);
            CHECK(state.reads == 0 && state.writes == 0);
            me_fp_status fp;
            CHECK(b2nd_eval_kernel_partitions_1d(7, 3, kernel, 1, &width, 4 * width,
                read_block, &state, write_block, &state, &fp, NULL) == 0);
            CHECK(state.reads == 3 && state.writes == 3);
            CHECK(fp.supported && fp.flags == 0);
        }
        me_kernel_free(kernel);
    }
    b2nd_free(array);
    b2nd_free_ctx(ctx);
    blosc2_destroy();
    return 0;
}

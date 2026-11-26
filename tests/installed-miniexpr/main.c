#include "b2nd.h"

int main(void) {
    int64_t shape[] = {5};
    int32_t chunks[] = {3}, blocks[] = {2};
    double values[5];
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    b2nd_array_t *array = NULL;
    int rc = 1;

    blosc2_init();
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunks, blocks, "=f8", 0, NULL, 0);
    if (ctx != NULL && b2nd_linspace(ctx, &array, 1.0, 5.0, true) == 0 &&
        b2nd_to_cbuffer(array, values, sizeof(values)) == 0) {
        rc = 0;
        for (int i = 0; i < 5; i++) {
            if (values[i] != i + 1.0) {
                rc = 1;
            }
        }
    }
    if (array != NULL) {
        b2nd_free(array);
    }
    if (ctx != NULL) {
        b2nd_free_ctx(ctx);
    }
    blosc2_destroy();
    return rc;
}

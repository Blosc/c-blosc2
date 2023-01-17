/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <caterva.h>

int main() {

    int8_t ndim = 2;
    int64_t shape[] = {10, 10};
    int32_t chunkshape[] = {4, 4};
    int32_t blockshape[] = {2, 2};
    int8_t typesize = 8;

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = typesize;
    blosc2_context *ctx = blosc2_create_cctx(cparams);

    caterva_params_t params = {0};
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    blosc2_storage b_storage = {.cparams=&cparams, .dparams=&dparams};
    caterva_storage_t storage = {.b_storage=&b_storage};
    int32_t blocknitems = 1;
    for (int i = 0; i < ndim; ++i) {
      storage.chunkshape[i] = chunkshape[i];
      storage.blockshape[i] = blockshape[i];
      blocknitems *= storage.blockshape[i];
    }
    storage.b_storage->cparams->blocksize = blocknitems * storage.b_storage->cparams->typesize;

    int64_t dataitems = 1;
    for (int i = 0; i < ndim; ++i) {
        dataitems *= shape[i];
    }
    int64_t datasize = dataitems * typesize;
    double *data = malloc(datasize);
    for (int i = 0; i < dataitems; ++i) {
        data[i] = (double) i;
    }
    caterva_array_t *arr;
    CATERVA_ERROR(caterva_from_buffer(data, datasize, &params, &storage, &arr));
    free(data);

    int64_t sel0[] = {3, 1, 2};
    int64_t sel1[] = {2, 5};
    int64_t sel2[] = {3, 3, 3, 9,3, 1, 0};
    int64_t *selection[] = {sel0, sel1, sel2};
    int64_t selection_size[] = {sizeof(sel0)/sizeof(int64_t), sizeof(sel1)/(sizeof(int64_t)), sizeof(sel2)/(sizeof(int64_t))};
    int64_t *buffershape = selection_size;
    int64_t nitems = 1;
    for (int i = 0; i < ndim; ++i) {
        nitems *= buffershape[i];
    }
    int64_t buffersize = nitems * arr->sc->typesize;
    double *buffer = calloc(nitems, arr->sc->typesize);
    CATERVA_ERROR(caterva_set_orthogonal_selection(ctx, arr, selection, selection_size, buffer, buffershape, buffersize));
    CATERVA_ERROR(caterva_get_orthogonal_selection(ctx, arr, selection, selection_size, buffer, buffershape, buffersize));

    printf("Results: \n");
    for (int i = 0; i < nitems; ++i) {
        if (i % buffershape[1] == 0) {
            printf("\n");
        }
        printf(" %f ", buffer[i]);
    }
    printf("\n");
    free(buffer);
    CATERVA_ERROR(caterva_free(&arr));
    blosc2_free_ctx(ctx);

    return 0;
}

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

# include <caterva.h>

int main() {

    int8_t ndim = 2;
    int64_t shape[] = {10, 10};
    int32_t chunkshape[] = {4, 4};
    int32_t blockshape[] = {2, 2};
    int8_t typesize = 8;

    int64_t slice_start[] = {2, 5};
    int64_t slice_stop[] = {2, 6};
    int32_t slice_chunkshape[] = {0, 1};
    int32_t slice_blockshape[] = {0, 1};

    int64_t nelem = 1;
    for (int i = 0; i < ndim; ++i) {
        nelem *= shape[i];
    }
    int64_t size = nelem * typesize;
    int8_t *data = malloc(size);

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

    caterva_array_t *arr;
    CATERVA_ERROR(caterva_from_buffer(data, size, &params, &storage, &arr));


    blosc2_storage slice_b_storage = {.cparams=&cparams, .dparams=&dparams};
    caterva_storage_t slice_storage = {.b_storage=&slice_b_storage};
    slice_storage.b_storage->urlpath = "example_hola.b2frame";
    blosc2_remove_urlpath(slice_storage.b_storage->urlpath);
    blocknitems = 1;
    for (int i = 0; i < ndim; ++i) {
        slice_storage.chunkshape[i] = slice_chunkshape[i];
        slice_storage.blockshape[i] = slice_blockshape[i];
        blocknitems *= slice_storage.blockshape[i];
    }
    slice_storage.b_storage->cparams->blocksize = blocknitems * slice_storage.b_storage->cparams->typesize;

    caterva_array_t *slice;
    CATERVA_ERROR(caterva_get_slice(arr, slice_start, slice_stop, &slice_storage,
                                    &slice));


    uint8_t *buffer;
    uint64_t buffer_size = 1;
    for (int i = 0; i < slice->ndim; ++i) {
        buffer_size *= slice->shape[i];
    }
    buffer_size *= slice->sc->typesize;
    buffer = malloc(buffer_size);

    CATERVA_ERROR(caterva_to_buffer(ctx, slice, buffer, buffer_size));

    // printf("Elapsed seconds: %.5f\n", blosc_elapsed_secs(t0, t1));
    blosc2_free_ctx(ctx);

    return 0;
}

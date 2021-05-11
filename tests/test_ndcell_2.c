/*
    Copyright (C) 2014  Francesc Alted
    http://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    Example program demonstrating use of the Blosc filter from C code.

    To compile this program:

    $ gcc -O many_compressors.c -o many_compressors -lblosc2

    To run:

    $ ./test_ndcell
    Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
    Using 4 threads (previously using 1)
    Using blosclz compressor
    Compression: 4000000 -> 57577 (69.5x)
    Succesful roundtrip!
    Using lz4 compressor
    Compression: 4000000 -> 97276 (41.1x)
    Succesful roundtrip!
    Using lz4hc compressor
    Compression: 4000000 -> 38314 (104.4x)
    Succesful roundtrip!
    Using zlib compressor
    Compression: 4000000 -> 21486 (186.2x)
    Succesful roundtrip!
    Using zstd compressor
    Compression: 4000000 -> 10692 (374.1x)
    Succesful roundtrip!

 */

#include <stdio.h>
#include <caterva.h>


static int test_ndcell(void *data, int64_t nbytes, int typesize, int ndim, caterva_params_t params, caterva_storage_t storage) {

    bool ndcell = false;
    if (storage.properties.blosc.cellshape[0] == -1) {
        ndcell = true;
    }
    uint8_t *data2 = (uint8_t*) data;
    caterva_array_t *array;
    caterva_ctx_t *ctx;
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 1;
    cfg.compcodec = BLOSC_ZLIB;
 //   cfg.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_NDCELL;
    cfg.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
 //   cfg.filtersmeta[BLOSC2_MAX_FILTERS - 2] = BLOSC2_NDCELL_4;
    if (ndcell) {
        for (int i = 0; i < CATERVA_MAX_DIM; ++i) {
            storage.properties.blosc.cellshape[i] = 4;
        }
        cfg.filters[4] = BLOSC_UDFILTER;
        cfg.filtersmeta[4] = 128;
    }
    caterva_ctx_new(&cfg, &ctx);
    CATERVA_ERROR(caterva_from_buffer(ctx, data2, nbytes, &params, &storage, &array));

    int64_t nchunks = array->nchunks;
    int32_t chunksize = (int32_t) (array->extchunknitems * typesize);
//    int64_t isize = nchunks * chunksize;
    uint8_t *data_in = malloc(chunksize);
    CATERVA_ERROR_NULL(data_in);
    int decompressed;
    int csize, dsize;
    int csize_f = 0;
    uint8_t *data_out = malloc(chunksize);
    uint8_t *data_dest = malloc(chunksize);

    blosc_timestamp_t start, comp, end;
    blosc_set_timestamp(&start);
    double ctime, dtime;
    double ctime_f = 0;
    double dtime_f = 0;

    for (int ci = 0; ci < nchunks; ci++) {
        decompressed = blosc2_schunk_decompress_chunk(array->sc, ci, data_in, chunksize);
        if (decompressed < 0) {
            printf("Error decompressing chunk \n");
            return -1;
        }
        /* Compress with clevel=5 and shuffle active  */
        csize = blosc2_compress_ctx(array->sc->cctx, data_in, chunksize, data_out, chunksize);
        if (csize == 0) {
            printf("Buffer is uncompressible.  Giving up.\n");
            return 0;
        }
        else if (csize < 0) {
            printf("Compression error.  Error code: %d\n", csize);
            return csize;
        }
        csize_f += csize;
        blosc_set_timestamp(&comp);
//        printf("Compression: %d -> %d (%.1fx)\n", chunksize, csize, (1. * chunksize) / csize);
        /* Decompress  */
        dsize = blosc2_decompress_ctx(array->sc->dctx, data_out, chunksize + BLOSC_MAX_OVERHEAD, data_dest, chunksize );
        if (dsize <= 0) {
            printf("Decompression error.  Error code: %d\n", dsize);
            return dsize;
        }

        blosc_set_timestamp(&end);
        ctime = blosc_elapsed_nsecs(start, comp);
        dtime = blosc_elapsed_nsecs(comp, end);
        ctime_f += ctime;
        dtime_f += dtime;

        for (int i = 0; i < chunksize; i++) {
            if (data_in[i] != data_dest[i]) {
                printf("i: %d, data %u, dest %u", i, data_in[i], data_dest[i]);
                printf("\n Decompressed data differs from original!\n");
                return -1;
            }
        }
    }
    csize_f = csize_f / nchunks;


/*
    printf("\n data \n");
    for (int i = 0; i < nbytes; i++) {
    printf("%u, ", data2[i]);
    }

    printf("\n ----------------------------------------------------------------------------- TEST NDLZ ----------"
           "----------------------------------------------------------------------- \n");
*/
/*
    printf("\n data_in \n");
    for (int i = 0; i < isize; i++) {
      printf("%u, ", data_in[i]);
    }
    printf("\n output \n");
    for (int i = 0; i < osize; i++) {
      printf("%u, ", data_out[i]);
    }


    printf("\n dest \n");
    for (int i = 0; i < dsize; i++) {
        printf("%u, ", data_dest[i]);
    }
*/

    caterva_free(ctx, &array);
    caterva_ctx_free(&ctx);
    free(data_in);
    free(data_out);
    free(data_dest);

    printf("Succesful roundtrip!\n");
    printf("Compression: %d -> %d (%.1fx)\n", chunksize, csize_f, (1. * chunksize) / csize_f);
    printf("\n Test time: \n Compression: %f secs \n Decompression: %f secs \n", ctime_f / 1000000000, dtime_f / 1000000000);
    return chunksize - csize_f;
}


int rand_() {
    int ndim = 3;
    int typesize = 1;
    int32_t shape[8] = {278, 264, 243};
    int32_t chunkshape[8] = {32, 64, 32};
    int32_t blockshape[8] = {8, 16, 8};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
        isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i++) {
        data[i] = rand() % 120;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int no_matches() {
    int ndim = 3;
    int typesize = 1;
    int32_t shape[8] = {32, 32, 32};
    int32_t chunkshape[8] = {32, 32, 32};
    int32_t blockshape[8] = {16, 16, 16};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i++) {
        data[i] = i;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int no_matches_pad() {
    int ndim = 7;
    int typesize = 4;
    int32_t shape[8] = {5, 8, 8, 9, 11, 11, 16};
    int32_t chunkshape[8] = {4, 5, 6, 5, 6, 8, 8};
    int32_t blockshape[8] = {4, 4, 4, 5, 6, 7, 8};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
        isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i++) {
        data[i] = (-i^2) * 111111 - (-i^2) * 11111 + i * 1111 - i * 110 + i;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int all_elem_eq() {
    int ndim = 5;
    int typesize = 4;
    int32_t shape[8] = {12, 32, 10, 11, 12};
    int32_t chunkshape[8] = {7, 19, 8, 8, 10};
    int32_t blockshape[8] = {5, 16, 4, 4, 8};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i++) {
        data[i] = 1;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int all_elem_pad() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {29, 31};
    int32_t chunkshape[8] = {24, 21};
    int32_t blockshape[8] = {12, 14};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i++) {
        data[i] = 1;
    }
    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int same_cells() {
    int ndim = 3;
    int typesize = 4;
    int32_t shape[8] = {31, 39, 32};
    int32_t chunkshape[8] = {22, 19, 23};
    int32_t blockshape[8] = {7, 13, 14};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i += 4) {
        data[i] = 0;
        data[i + 1] = 1111111;
        data[i + 2] = 2;
        data[i + 3] = 1111111;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int same_cells_pad() {
    int ndim = 4;
    int typesize = 4;
    int32_t shape[8] = {34, 47, 43, 44};
    int32_t chunkshape[8] = {28, 28, 28, 22};
    int32_t blockshape[8] = {17, 17, 23, 22};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < (isize / 4); i++) {
        data[i * 4] = (uint32_t) 11111111;
        data[i * 4 + 1] = (uint32_t) 99999999;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int same_cells_pad_tam1() {
    int ndim = 6;
    int typesize = 1;
    int32_t shape[8] = {30, 24, 8, 11, 9, 16};
    int32_t chunkshape[8] = {26, 22, 5, 8, 8, 11};
    int32_t blockshape[8] = {13, 11, 4, 5, 6, 8};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int64_t i = 0; i < (isize / 4); i++) {
        data[i * 4] = (uint32_t) 111;
        data[i * 4 + 1] = (uint32_t) 99;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int matches_2_rows() {
    int ndim = 4;
    int typesize = 4;
    int32_t shape[8] = {43, 63, 57, 52};
    int32_t chunkshape[8] = {42, 43, 33, 26};
    int32_t blockshape[8] = {23, 31, 13, 16};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize; i += 4) {
        if ((i <= 20) || ((i >= 48) && (i <= 68)) || ((i >= 96) && (i <= 116))) {
            data[i] = 0;
            data[i + 1] = 1;
            data[i + 2] = 2;
            data[i + 3] = 3;
        } else if (((i >= 24) && (i <= 44)) || ((i >= 72) && (i <= 92)) || ((i >= 120) && (i <= 140))){
            data[i] = i;
            data[i + 1] = i + 1;
            data[i + 2] = i + 2;
            data[i + 3] = i + 3;
        } else {
            data[i] = i;
        }
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int matches_3_rows() {
    int ndim = 4;
    int typesize = 4;
    int32_t shape[8] = {51, 45, 63, 50};
    int32_t chunkshape[8] = {50, 38, 42, 25};
    int32_t blockshape[8] = {25, 24, 16, 18};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize - 4; i += 4) {
        if ((i % 12 == 0) && (i != 0)) {
            data[i] = 1111111;
            data[i + 1] = 3;
            data[i + 2] = 11111;
            data[i + 3] = 4;
        } else {
            data[i] = 0;
            data[i + 1] = 1111111;
            data[i + 2] = 2;
            data[i + 3] = 1111;
        }
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int matches_2_couples() {
    int ndim = 4;
    int typesize = 1;
    int32_t shape[8] = {42, 55, 62, 88};
    int32_t chunkshape[8] = {42, 53, 41, 33};
    int32_t blockshape[8] = {13, 39, 28, 11};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    for (int64_t i = 0; i < isize / 4; i++) {
        if (i % 4 == 0) {
            data[i * 4] = 0;
            data[i * 4 + 1] = 1;
            data[i * 4 + 2] = 2;
            data[i * 4 + 3] = 3;
        } else if (i % 4 == 1){
            data[i * 4] = 10;
            data[i * 4 + 1] = 11;
            data[i * 4 + 2] = 12;
            data[i * 4 + 3] = 13;
        } else if (i % 4 == 2){
            data[i * 4] = 20;
            data[i * 4 + 1] = 21;
            data[i * 4 + 2] = 22;
            data[i * 4 + 3] = 23;
        } else {
            data[i * 4] = 30;
            data[i * 4 + 1] = 31;
            data[i * 4 + 2] = 32;
            data[i * 4 + 3] = 33;
        }
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int some_matches() {
    int ndim = 4;
    int typesize = 4;
    int32_t shape[8] = {56, 46, 55, 66};
    int32_t chunkshape[8] = {48, 32, 42, 33};
    int32_t blockshape[8] = {14, 18, 26, 33};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < (isize / 2); i++) {
        data[i] = i;
    }
    for (int64_t i = (isize / 2); i < isize; i++) {
        data[i] = 1;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int padding_some() {
    int ndim = 4;
    int typesize = 4;
    int32_t shape[8] = {45, 53, 52, 38};
    int32_t chunkshape[8] = {32, 38, 48, 33};
    int32_t blockshape[8] = {16, 26, 17, 11};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < 2 * isize / 3; i++) {
        data[i] = 0;
    }
    for (int64_t i = 2 * isize / 3; i < isize; i++) {
        data[i] = i;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int pad_some_32() {
    int ndim = 6;
    int typesize = 4;
    int32_t shape[8] = {16, 8, 11, 12, 9, 16};
    int32_t chunkshape[8] = {5, 6, 5, 6, 8, 8};
    int32_t blockshape[8] = {4, 4, 5, 6, 7, 8};
    int64_t isize = 1;
    for (int i = 0; i < ndim; ++i) {
      isize *= (int)(shape[i]);
    }
    int64_t nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    for (int64_t i = 0; i < 2 * isize / 3; i++) {
        data[i] = 0;
    }
    for (int64_t i = 2 * isize / 3; i < isize; i++) {
        data[i] = i;
    }

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image1() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {300, 450};
    int32_t chunkshape[8] = {150, 150};
    int32_t blockshape[8] = {50, 50};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image2() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {800, 1200};
    int32_t chunkshape[8] = {400, 400};
    int32_t blockshape[8] = {40, 40};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res2.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image3() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {256, 256};
    int32_t chunkshape[8] = {64, 128};
    int32_t blockshape[8] = {32, 32};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res3.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image4() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {64, 64};
    int32_t chunkshape[8] = {32, 32};
    int32_t blockshape[8] = {16, 16};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res4.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image5() {
    int ndim = 2;
    int typesize = 4;
    int32_t shape[8] = {641, 1140};
    int32_t chunkshape[8] = {256, 512};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res5.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image6() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {256, 256};
    int32_t chunkshape[8] = {128, 128};
    int32_t blockshape[8] = {64, 64};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res6.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image7() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {2506, 5000};
    int32_t chunkshape[8] = {512, 1024};
    int32_t blockshape[8] = {128, 512};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res7.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image8() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {1575, 2400};
    int32_t chunkshape[8] = {1575, 2400};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res8.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image9() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {675, 1200};
    int32_t chunkshape[8] = {675, 1200};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint32_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res9.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}

int image10() {
    int ndim = 2;
    int typesize = 3;
    int32_t shape[8] = {2045, 3000};
    int32_t chunkshape[8] = {2045, 3000};
    int32_t blockshape[8] = {256, 256};
    int isize = (int)(shape[0] * shape[1]);
    int nbytes = typesize * isize;
    uint8_t *data = malloc(nbytes);
    FILE *f = fopen("/mnt/c/Users/sosca/CLionProjects/Caterva/files/res10.bin", "rb");
    int err = (int) fread(data, 1, nbytes, f);
    if (err != nbytes) {
      printf("\n read error");
      return -1;
    }
    fclose(f);

    caterva_params_t params;
    params.itemsize = typesize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    storage.backend = CATERVA_STORAGE_BLOSC;
    for (int i = 0; i < ndim; ++i) {
        storage.properties.blosc.chunkshape[i] = chunkshape[i];
        storage.properties.blosc.blockshape[i] = blockshape[i];
    }

    /* Run the test. */
    printf("\n Sin filtro \n");
    int result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    storage.properties.blosc.cellshape[0] = -1;
    printf("\n Con filtro \n");
    result = test_ndcell(data, nbytes, typesize, ndim, params, storage);
    free(data);
    return result;
}


int main(void) {

    int result;

    result = rand_();
    printf("rand: %d obtained \n \n", result);
    result = no_matches();
    printf("no_matches: %d obtained \n \n", result);
    result = no_matches_pad();
    printf("no_matches_pad: %d obtained \n \n", result);
    result = all_elem_eq();
    printf("all_elem_eq: %d obtained \n \n", result);
    result = all_elem_pad();
    printf("all_elem_pad: %d obtained \n \n", result);
    result = same_cells();
    printf("same_cells: %d obtained \n \n", result);
    result = same_cells_pad();
    printf("same_cells_pad: %d obtained \n \n", result);
    result = same_cells_pad_tam1();
    printf("same_cells_pad_tam1: %d obtained \n \n", result);
    result = matches_2_rows();
    printf("matches_2_rows: %d obtained \n \n", result);
    result = matches_3_rows();
    printf("matches_3_rows: %d obtained \n \n", result);
    result = matches_2_couples();
    printf("matches_2_couples: %d obtained \n \n", result);
    result = some_matches();
    printf("some_matches: %d obtained \n \n", result);
    result = padding_some();
    printf("pad_some: %d obtained \n \n", result);
    result = pad_some_32();
    printf("pad_some_32: %d obtained \n \n", result);
/*
    printf("TEST BLOSCLZ \n");
    result = image1();
    printf("image1 with padding: %d obtained \n \n", result);
    result = image2();
    printf("image2 with  padding: %d obtained \n \n", result);
    result = image3();
    printf("image3 with NO padding: %d obtained \n \n", result);
    result = image4();
    printf("image4 with NO padding: %d obtained \n \n", result);
    result = image5();
    printf("image5 with padding: %d obtained \n \n", result);
    result = image6();
    printf("image6 with NO padding: %d obtained \n \n", result);
    result = image7();
    printf("image7 with NO padding: %d obtained \n \n", result);
    result = image8();
    printf("image8 with NO padding: %d obtained \n \n", result);
    result = image9();
    printf("image9 with NO padding: %d obtained \n \n", result);
    result = image10();
    printf("image10 with NO padding: %d obtained \n \n", result);
*/
}
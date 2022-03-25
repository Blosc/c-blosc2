/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc codec from C code.
    To compile this program:

    $ gcc -O test_zfp_rate_getitem.c -o test_zfp_rate_getitem -lblosc2


**********************************************************************/

#include <stdio.h>
#include "blosc2.h"
#include "blosc2/codecs-registry.h"
#include <inttypes.h>

static int test_zfp_rate_getitem_float(blosc2_schunk* schunk) {

    if (schunk->typesize != 4) {
        printf("Error: This test is only for doubles.\n");
        return 0;
    }
    int nchunks = schunk->nchunks;
    int32_t chunksize = (int32_t) (schunk->chunksize);
    float *data_in = malloc(chunksize);
    int decompressed;
    int64_t csize;
    uint8_t *data_out = malloc(chunksize + BLOSC_MAX_OVERHEAD);
    float *data_dest = malloc(chunksize);

    /* Create a context for compression */
    int8_t zfp_rate = 37;
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.splitmode = BLOSC_NEVER_SPLIT;
    cparams.typesize = schunk->typesize;
    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
    cparams.compcode_meta = zfp_rate;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
    cparams.clevel = 5;
    cparams.nthreads = 1;
    cparams.blocksize = schunk->blocksize;
    cparams.schunk = schunk;
    blosc2_context *cctx;
    cctx = blosc2_create_cctx(cparams);

    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = 1;
    dparams.schunk = schunk;
    blosc2_context *dctx;
    dctx = blosc2_create_dctx(dparams);

    for (int ci = 0; ci < nchunks; ci++) {

        decompressed = blosc2_schunk_decompress_chunk(schunk, ci, data_in, chunksize);
        if (decompressed < 0) {
            printf("Error decompressing chunk \n");
            return -1;
        }
/*
        printf("\n chunk \n");
        for (int i = 0; i < (chunksize / cparams.typesize); i++) {
            printf("%f, ", data_in[i]);
        }
*/
        /* Compress with clevel=5 and shuffle active  */
        csize = blosc2_compress_ctx(cctx, data_in, chunksize, data_out, chunksize + BLOSC_MAX_OVERHEAD);
        if (csize == 0) {
            printf("Buffer is uncompressible.  Giving up.\n");
            return 0;
        } else if (csize < 0) {
            printf("Compression error.  Error code: %" PRId64 "\n", csize);
            return (int) csize;
        }
        int32_t chunk_nbytes, chunk_cbytes;
        blosc2_cbuffer_sizes(data_out, &chunk_nbytes, &chunk_cbytes, NULL);

        /* Get item  */
        int index, dsize_zfp, dsize_blosc;
        float item_zfp, item_blosc;
        blosc_timestamp_t t0, t1;
        double zfp_time = 0;
        double blosc_time = 0;
        int nelems = schunk->chunksize / schunk->typesize;
        for (int i = 0; i < 100; ++i) {
            srand(i);
            index = rand() % nelems;
            blosc_set_timestamp(&t0);
            dsize_blosc = blosc2_getitem_ctx(schunk->dctx, data_out, chunk_cbytes,
                                             index, 1, &item_zfp, sizeof(item_zfp));
            blosc_set_timestamp(&t1);
            blosc_time += blosc_elapsed_secs(t0, t1);
            blosc_set_timestamp(&t0);
            dsize_zfp = blosc2_getitem_ctx(dctx, data_out, chunk_cbytes,
                                           index, 1, &item_blosc, sizeof(item_blosc));
            blosc_set_timestamp(&t1);
            zfp_time += blosc_elapsed_secs(t0, t1);
            if (dsize_blosc != dsize_zfp) {
                printf("Different amount of items gotten");
                return -1;
            }
            if (item_blosc != item_zfp) {
                printf("\nIn index %d different items extracted zfp %f blosc %f", index, item_zfp, item_blosc);
                return -1;
            }

        }
        printf("ZFP_FIXED_RATE time: %.10f s\n", zfp_time);
        printf("Blosc2 time: %.10f s\n", blosc_time);

    }

    free(data_in);
    free(data_out);
    free(data_dest);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);

    printf("Succesful roundtrip!\n");
    return (int) (BLOSC2_ERROR_SUCCESS);
}

static int test_zfp_rate_getitem_double(blosc2_schunk* schunk) {

    if (schunk->typesize != 8) {
        printf("Error: This test is only for doubles.\n");
        return 0;
    }
    int nchunks = schunk->nchunks;
    int32_t chunksize = (int32_t) (schunk->chunksize);
    double *data_in = malloc(chunksize);
    int decompressed;
    int64_t csize;
    uint8_t *data_out = malloc(chunksize + BLOSC_MAX_OVERHEAD);
    double *data_dest = malloc(chunksize);

    /* Create a context for compression */
    int zfp_rate = 37;
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.splitmode = BLOSC_NEVER_SPLIT;
    cparams.typesize = schunk->typesize;
    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
    cparams.compcode_meta = zfp_rate;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
    cparams.clevel = 5;
    cparams.nthreads = 1;
    cparams.blocksize = schunk->blocksize;
    cparams.schunk = schunk;
    blosc2_context *cctx;
    cctx = blosc2_create_cctx(cparams);

    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = 1;
    dparams.schunk = schunk;
    blosc2_context *dctx;
    dctx = blosc2_create_dctx(dparams);

    for (int ci = 0; ci < nchunks; ci++) {

        decompressed = blosc2_schunk_decompress_chunk(schunk, ci, data_in, chunksize);
        if (decompressed < 0) {
            printf("Error decompressing chunk \n");
            return -1;
        }
/*
        printf("\n chunk \n");
        for (int i = 0; i < (chunksize / cparams.typesize); i++) {
            printf("%f, ", data_in[i]);
        }
*/
        /* Compress with clevel=5 and shuffle active  */
        csize = blosc2_compress_ctx(cctx, data_in, chunksize, data_out, chunksize + BLOSC_MAX_OVERHEAD);
        if (csize == 0) {
            printf("Buffer is uncompressible.  Giving up.\n");
            return 0;
        } else if (csize < 0) {
            printf("Compression error.  Error code: %" PRId64 "\n", csize);
            return (int) csize;
        }
        int32_t chunk_nbytes, chunk_cbytes;
        blosc2_cbuffer_sizes(data_out, &chunk_nbytes, &chunk_cbytes, NULL);

        /* Get item  */
        int index, dsize_zfp, dsize_blosc;
        double item_zfp, item_blosc;
        blosc_timestamp_t t0, t1;
        double zfp_time = 0;
        double blosc_time = 0;
        int nelems = schunk->chunksize / schunk->typesize;
        for (int i = 0; i < 100; ++i) {
            srand(i);
            index = rand() % nelems;
            blosc_set_timestamp(&t0);
            dsize_blosc = blosc2_getitem_ctx(schunk->dctx, data_out, chunk_cbytes,
                                             index, 1, &item_zfp, sizeof(item_zfp));
            blosc_set_timestamp(&t1);
            blosc_time += blosc_elapsed_secs(t0, t1);
            blosc_set_timestamp(&t0);
            dsize_zfp = blosc2_getitem_ctx(dctx, data_out, chunk_cbytes,
                                           index, 1, &item_blosc, sizeof(item_blosc));
            blosc_set_timestamp(&t1);
            zfp_time += blosc_elapsed_secs(t0, t1);
            if (dsize_blosc != dsize_zfp) {
                printf("Different amount of items gotten");
                return -1;
            }
            if (item_blosc != item_zfp) {
                printf("\nIn index %d different items extracted zfp %.10f blosc %.10f", index, item_zfp, item_blosc);
                return -1;
            }

        }
        printf("ZFP_FIXED_RATE time: %.10f s\n", zfp_time);
        printf("Blosc2 time: %.10f s\n", blosc_time);

    }

    free(data_in);
    free(data_out);
    free(data_dest);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);

    printf("Succesful roundtrip!\n");
    return (int) (BLOSC2_ERROR_SUCCESS);
}


int float_cyclic() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_float_cyclic.caterva");

    /* Run the test. */
    int result = test_zfp_rate_getitem_float(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int double_same_cells() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_double_same_cells.caterva");

    /* Run the test. */
    int result = test_zfp_rate_getitem_double(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int day_month_temp() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_day_month_temp.caterva");

    /* Run the test. */
    int result = test_zfp_rate_getitem_float(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int item_prices() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_item_prices.caterva");

    /* Run the test. */
    int result = test_zfp_rate_getitem_float(schunk);
    blosc2_schunk_free(schunk);
    return result;
}


int main(void) {

    int result;
    blosc_init();   // this is mandatory for initiallizing the plugin mechanism
    result = float_cyclic();
    printf("float_cyclic: %d obtained \n \n", result);
    result = double_same_cells();
    printf("double_same_cells: %d obtained \n \n", result);
    result = day_month_temp();
    printf("day_month_temp: %d obtained \n \n", result);
    result = item_prices();
    printf("item_prices: %d obtained \n \n", result);
    blosc_destroy();

    return BLOSC2_ERROR_SUCCESS;
}

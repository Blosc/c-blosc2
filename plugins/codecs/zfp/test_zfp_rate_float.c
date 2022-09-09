/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc codec from C code.
    To compile this program:

    $ gcc -O test_zfp_rate_float.c -o test_zfp_rate_float -lblosc2


**********************************************************************/

#include <stdio.h>
#include "blosc2.h"
#include "blosc2/codecs-registry.h"
#include "blosc-private.h"
#include <inttypes.h>

static int test_zfp_rate_float(blosc2_schunk* schunk) {

    if (schunk->typesize != 4) {
        printf("Error: This test is only for doubles.\n");
        return 0;
    }
    int64_t nchunks = schunk->nchunks;
    int32_t chunksize = (int32_t) (schunk->chunksize);
    float *data_in = malloc(chunksize);
    int decompressed;
    int64_t csize;
    int64_t dsize;
    int64_t csize_f = 0;
    uint8_t *data_out = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
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

        /* Compress with clevel=5 and shuffle active  */
        csize = blosc2_compress_ctx(cctx, data_in, chunksize, data_out, chunksize + BLOSC2_MAX_OVERHEAD);
        if (csize == 0) {
            printf("Buffer is incompressible.  Giving up.\n");
            return 0;
        } else if (csize < 0) {
            printf("Compression error.  Error code: %" PRId64 "\n", csize);
            return (int) csize;
        }
        csize_f += csize;

        /* Decompress  */
        dsize = blosc2_decompress_ctx(dctx, data_out, chunksize + BLOSC2_MAX_OVERHEAD, data_dest, chunksize);
        if (dsize <= 0) {
            printf("Decompression error.  Error code: %" PRId64 "\n", dsize);
            return (int) dsize;
        }
    }
    csize_f = csize_f / nchunks;

    free(data_in);
    free(data_out);
    free(data_dest);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);

    printf("Successful roundtrip!\n");
    printf("Compression: %d -> %" PRId64 " (%.1fx)\n", chunksize, csize_f, (1. * chunksize) / (double) csize_f);
    return (int) (chunksize - csize_f);
}

static int test_zfp_rate_double(blosc2_schunk* schunk) {

    if (schunk->typesize != 8) {
        printf("Error: This test is only for doubles.\n");
        return 0;
    }
    int64_t nchunks = schunk->nchunks;
    int32_t chunksize = (int32_t) (schunk->chunksize);
    double *data_in = malloc(chunksize);
    int decompressed;
    int64_t csize;
    int64_t dsize;
    int64_t csize_f = 0;
    uint8_t *data_out = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
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

        /* Compress with clevel=5 and shuffle active  */
        csize = blosc2_compress_ctx(cctx, data_in, chunksize, data_out, chunksize + BLOSC2_MAX_OVERHEAD);
        if (csize == 0) {
            printf("Buffer is incompressible.  Giving up.\n");
            return 0;
        } else if (csize < 0) {
            printf("Compression error.  Error code: %" PRId64 "\n", csize);
            return (int) csize;
        }
        csize_f += csize;

        /* Decompress  */
        dsize = blosc2_decompress_ctx(dctx, data_out, chunksize + BLOSC2_MAX_OVERHEAD, data_dest, chunksize);
        if (dsize <= 0) {
            printf("Decompression error.  Error code: %" PRId64 "\n", dsize);
            return (int) dsize;
        }
    }

    csize_f = csize_f / nchunks;

    free(data_in);
    free(data_out);
    free(data_dest);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);

    printf("Successful roundtrip!\n");
    printf("Compression: %d -> %" PRId64 " (%.1fx)\n", chunksize, csize_f, (1. * chunksize) / (double) csize_f);
    return (int) (chunksize - csize_f);
}


int float_cyclic() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_float_cyclic.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp_rate_float(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int double_same_cells() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_double_same_cells.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp_rate_double(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int day_month_temp() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_day_month_temp.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp_rate_float(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int item_prices() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_item_prices.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp_rate_float(schunk);
    blosc2_schunk_free(schunk);
    return result;
}


int main(void) {

    int result;
  blosc2_init();   // this is mandatory for initiallizing the plugin mechanism
    result = float_cyclic();
    printf("float_cyclic: %d obtained \n \n", result);
    result = double_same_cells();
    printf("double_same_cells: %d obtained \n \n", result);
    result = day_month_temp();
    printf("day_month_temp: %d obtained \n \n", result);
    result = item_prices();
    printf("item_prices: %d obtained \n \n", result);
  blosc2_destroy();

    return BLOSC2_ERROR_SUCCESS;
}

/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc codec from C code.
    To compile this program:

    $ gcc -O test_zfp_acc_int.c -o test_zfp_acc_int -lblosc2


**********************************************************************/

#include <stdio.h>
#include "blosc2.h"
#include "blosc2/codecs-registry.h"
#include "blosc-private.h"
#include <inttypes.h>

static int test_zfp(blosc2_schunk* schunk) {

    int64_t nchunks = schunk->nchunks;
    int32_t chunksize = (int32_t) (schunk->chunksize);
    uint8_t *data_in = malloc(chunksize);
    int decompressed;
    int64_t csize;
    int64_t dsize;
    int64_t csize_f = 0;
    uint8_t *data_out = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
    uint8_t *data_dest = malloc(chunksize);

    /* Create a context for compression */
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.splitmode = BLOSC_NEVER_SPLIT;
    cparams.typesize = schunk->typesize;
    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_ACCURACY;
    cparams.compcode_meta = schunk->typesize;
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

        for (int i = 0; i < chunksize; i++) {
            if ((data_in[i] - data_dest[i]) > 1) {
                printf("i: %d, data %u, dest %u", i, data_in[i], data_dest[i]);
                printf("\n Decompressed data differs from original!\n");
                return -1;
            }
        }
    }
    csize_f = csize_f / nchunks;

    free(data_in);
    free(data_out);
    free(data_dest);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);

    printf("Successful roundtrip!\n");
    printf("Compression: %d -> %" PRId64 " (%.1fx)\n", chunksize, csize_f, (1. * chunksize) / (double)csize_f);
    return (int) (chunksize - csize_f);
}


int rand_() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_ndmean_repart_rand.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int same_cells() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_ndmean_repart_same_cells.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int some_matches() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_ndmean_repart_some_matches.caterva");
    BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

    /* Run the test. */
    int result = test_zfp(schunk);
    blosc2_schunk_free(schunk);
    return result;
}


int main(void) {

    int result;
  blosc2_init();   // this is mandatory for initiallizing the plugin mechanism
    result = rand_();
    printf("rand: %d obtained \n \n", result);
    result = same_cells();
    printf("same_cells: %d obtained \n \n", result);
    result = some_matches();
    printf("some_matches: %d obtained \n \n", result);
  blosc2_destroy();

    return BLOSC2_ERROR_SUCCESS;
}

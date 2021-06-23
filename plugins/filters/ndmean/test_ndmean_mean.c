/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc filter from C code.
    To compile this program:

    $ gcc -O test_ndmean_mean.c -o test_ndmean_mean -lblosc2

    To run:

    $ ./test_ndmean_mean
    Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
    Using 1 thread
    Using ZSTD compressor
    Succesful roundtrip!
    Compression: 256 -> 137 (1.9x)
    2_rows_matches: 119 obtained

    Succesful roundtrip!
    Compression: 128 -> 110 (1.2x)
    same_cells: 18 obtained

    Succesful roundtrip!
    Compression: 448 -> 259 (1.7x)
    some_matches: 189 obtained


**********************************************************************/

#include <stdio.h>
#include "ndmean.h"
#include <math.h>
#include <inttypes.h>
#include "blosc2/filters-registry.h"

#define EPSILON (float) (1)

static bool is_close(double d1, double d2) {

    double aux = 1;
    if (fabs(d1) < fabs(d2)) {
        if (fabs(d1) > 0) {
            aux = fabs(d2);
        }
    } else {
        if (fabs(d2) > 0) {
            aux = fabs(d1);
        }
    }

    return fabs(d1 - d2) < aux * EPSILON;
}

static void swap_store(void *dest, const void *pa, int size) {
    uint8_t *pa_ = (uint8_t *) pa;
    uint8_t *pa2_ = malloc((size_t) size);
    int i = 1; /* for big/little endian detection */
    char *p = (char *) &i;

    if (p[0] == 1) {
        /* little endian */
        switch (size) {
            case 8:
                pa2_[0] = pa_[7];
                pa2_[1] = pa_[6];
                pa2_[2] = pa_[5];
                pa2_[3] = pa_[4];
                pa2_[4] = pa_[3];
                pa2_[5] = pa_[2];
                pa2_[6] = pa_[1];
                pa2_[7] = pa_[0];
                break;
            case 4:
                pa2_[0] = pa_[3];
                pa2_[1] = pa_[2];
                pa2_[2] = pa_[1];
                pa2_[3] = pa_[0];
                break;
            case 2:
                pa2_[0] = pa_[1];
                pa2_[1] = pa_[0];
                break;
            case 1:
                pa2_[0] = pa_[0];
                break;
            default:
                fprintf(stderr, "Unhandled nitems: %d\n", size);
        }
    }
    memcpy(dest, pa2_, size);
    free(pa2_);
}

static int32_t deserialize_meta(uint8_t *smeta, uint32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape) {
    uint8_t *pmeta = smeta;

    // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
    pmeta += 1;

    // version entry
    int8_t version = pmeta[0];  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // ndim entry
    *ndim = pmeta[0];
    int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // shape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) shape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(shape + i, pmeta, sizeof(int64_t));
        pmeta += sizeof(int64_t);
    }

    // chunkshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) chunkshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(chunkshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }

    // blockshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) blockshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(blockshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }
    uint32_t slen = (uint32_t)(pmeta - smeta);
    return 0;
}


static int test_ndmean(blosc2_schunk* schunk) {

    int8_t ndim;
    int64_t* shape = malloc(8 * sizeof(int64_t));
    int32_t* chunkshape = malloc(8 * sizeof(int32_t));
    int32_t* blockshape = malloc(8 * sizeof(int32_t));
    uint8_t cellshape = 4;
    uint8_t* smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        return 0;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    if (ndim != 1) {
        fprintf(stderr, "This test only works for ndim = 1");
        return -1;
    }

    int32_t typesize = schunk->typesize;
    int nchunks = schunk->nchunks;
    int32_t chunksize = (int32_t) (schunk->chunksize);
    //   int isize = (int) array->extchunknitems * typesize;
    uint8_t *data_in = malloc(chunksize);
    int decompressed;
    int64_t csize;
    int64_t dsize;
    int64_t csize_f = 0;
    uint8_t *data_out = malloc(chunksize + BLOSC_MAX_OVERHEAD);
    uint8_t *data_dest = malloc(chunksize);

    /* Create a context for compression */
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.splitmode = BLOSC_ALWAYS_SPLIT;
    cparams.typesize = typesize;
    cparams.compcode = BLOSC_ZSTD;
    cparams.filters[4] = BLOSC_FILTER_NDMEAN;
    cparams.filters_meta[4] = cellshape;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
    cparams.clevel = 9;
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

    double cell_mean;
    for (int ci = 0; ci < nchunks; ci++) {

        decompressed = blosc2_schunk_decompress_chunk(schunk, ci, data_in, chunksize);
        if (decompressed < 0) {
            printf("Error decompressing chunk \n");
            return -1;
        }

        /*
        printf("\n data \n");
        for (int i = 0; i < nbytes; i++) {
        printf("%u, ", data2[i]);
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
        csize_f += csize;

        /* Decompress  */
        dsize = blosc2_decompress_ctx(dctx, data_out, chunksize + BLOSC_MAX_OVERHEAD, data_dest, chunksize);
        if (dsize <= 0) {
            printf("Decompression error.  Error code: %" PRId64 "\n", dsize);
            return (int) dsize;
        }
/*
        printf("\n data_dest: \n");
        for (int i = 0; i < chunksize / typesize; i++) {
            if (typesize == 4) {
                printf("%f, ", ((float *) data_dest)[i]);
            } else if (typesize == 8) {
                printf("%f, ", ((double *) data_dest)[i]);
            }
        }
*/
        int chunk_shape = chunkshape[0];
        if (ci == nchunks - 1) {
            chunk_shape = (int) (shape[0] % chunkshape[0]);
        }
        int nblocks = (chunk_shape + blockshape[0] - 1) / blockshape[0];

        for (int bi = 0; bi < nblocks; bi++) {
            int block_shape = blockshape[0];
            if (bi == nblocks - 1) {
                block_shape = chunk_shape % blockshape[0];
            }
            int ncells = (block_shape + cellshape - 1) / cellshape;

            for (int cei = 0; cei < ncells; cei++) {
                int ind = bi * blockshape[0] + cei * cellshape;
                cell_mean = 0;
                int cell_shape = cellshape;
                if (cei == ncells - 1) {
                    cell_shape = block_shape % cellshape;
                }
                switch (typesize) {
                    case 4:
                        for (int i = 0; i < cell_shape; i++) {
                            cell_mean += ((float *) data_in)[ind + i];
                        }
                        cell_mean /= cell_shape;
                        for (int i = 0; i < cell_shape; i++) {
                            if (!is_close(cell_mean, ((float *) data_dest)[ind + i])) {
                                printf("i: %d, cell_mean %.9f, dest %.9f", ind + i, cell_mean, ((float *) data_dest)[ind + i]);
                                printf("\n Decompressed data differs from original!\n");
                                return -1;
                            }
                        }
                        break;
                    case 8:
                        for (int i = 0; i < cell_shape; i++) {
                            cell_mean += ((double *) data_in)[ind + i];
                        }
                        cell_mean /= cell_shape;
                        for (int i = 0; i < cell_shape; i++) {
                            if (!is_close(cell_mean, ((double *) data_dest)[ind + i])) {
                                printf("i: %d, cell_mean %.9f, dest %.9f", ind + i, cell_mean, ((double *) data_dest)[ind + i]);
                                printf("\n Decompressed data differs from original!\n");
                                return -1;
                            }
                        }
                        break;
                    default :
                        break;
                }
            }
        }
    }
    csize_f = csize_f / nchunks;

    free(data_in);
    free(data_out);
    free(data_dest);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);
    free(shape);
    free(chunkshape);
    free(blockshape);

    printf("Succesful roundtrip!\n");
    printf("Compression: %d -> %" PRId64 " (%.1fx)\n", chunksize, csize_f, (1. * chunksize) / csize_f);
    return (int) (chunksize - csize_f);
}


int rows_matches() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_ndmean_1dim_2rows.caterva");

    /* Run the test. */
    int result = test_ndmean(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int same_cells() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_ndmean_1dim_same_cells.caterva");

    /* Run the test. */
    int result = test_ndmean(schunk);
    blosc2_schunk_free(schunk);
    return result;
}

int some_matches() {
    blosc2_schunk *schunk = blosc2_schunk_open("example_ndmean_1dim_some_matches.caterva");

    /* Run the test. */
    int result = test_ndmean(schunk);
    blosc2_schunk_free(schunk);
    return result;
}


int main(void) {

    int result;
    blosc_init();
    result = rows_matches();
    printf("2_rows_matches: %d obtained \n \n", result);
    result = same_cells();
    printf("same_cells: %d obtained \n \n", result);
    result = some_matches();
    printf("some_matches: %d obtained \n \n", result);
    blosc_destroy();
}

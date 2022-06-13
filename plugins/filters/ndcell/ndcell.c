/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/


#include <blosc2.h>
#include "ndcell.h"
#include <math.h>
#include <stdio.h>
#include "../plugins/plugin_utils.h"


int ndcell_encoder(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta, blosc2_cparams* cparams) {

    int8_t ndim;
    int64_t* shape = malloc(8 * sizeof(int64_t));
    int32_t* chunkshape = malloc(8 * sizeof(int32_t));
    int32_t* blockshape = malloc(8 * sizeof(int32_t));
    uint8_t* smeta;
    int32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Blosc error");
        return 0;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);
    int typesize = cparams->typesize;

    int8_t cell_shape = (int8_t)meta;
    const int cell_size = (int) pow(cell_shape, ndim);

    int32_t blocksize = (int32_t) typesize;
    for (int i = 0; i < ndim; i++){
        blocksize *= blockshape[i];
    }

    if (length != blocksize) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Length not equal to blocksize %d %d \n", length, blocksize);
        return -1;
    }

    uint8_t* ip = (uint8_t *) input;
    uint8_t* op = (uint8_t *) output;
    uint8_t* op_limit = op + length;

    /* input and output buffer cannot be less than cell size */
    if (length < cell_size * typesize) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Incorrect length");
        return 0;
    }

    uint8_t* obase = op;

    int64_t i_shape[NDCELL_MAX_DIM];
    for (int i = 0; i < ndim; ++i) {
        i_shape[i] = (blockshape[i] + cell_shape - 1) / cell_shape;
    }

    int64_t ncells = 1;
    for (int i = 0; i < ndim; ++i) {
        ncells *= i_shape[i];
    }

    /* main loop */
    int64_t pad_shape[NDCELL_MAX_DIM];
    int64_t ii[NDCELL_MAX_DIM];
    for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell
        blosc2_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = (int64_t) cell_shape;
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cell_shape != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cell_shape;
            } else {
                pad_shape[dim_ind] = (int64_t) cell_shape;
            }
        }
        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[NDCELL_MAX_DIM];
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            blosc2_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
            nd_aux = blockshape[ndim - 1];
            int64_t ind = orig;
            for (int i = ndim - 2; i >= 0; i--) {
                ind += kk[i] * nd_aux;
                nd_aux *= blockshape[i];
            }
            memcpy(op, &ip[ind * typesize], pad_shape[ndim - 1] * typesize);
            op += pad_shape[ndim - 1] * typesize;
        }

        if (op > op_limit) {
            free(shape);
            free(chunkshape);
            free(blockshape);
            printf("Output too big");
            return 0;
        }
    }

    if((op - obase) != length) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Output size must be equal to input size \n");
        return 0;
    }

    free(shape);
    free(chunkshape);
    free(blockshape);

    return BLOSC2_ERROR_SUCCESS;
}


int ndcell_decoder(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta, blosc2_dparams* dparams) {

    blosc2_schunk *schunk = dparams->schunk;
    int8_t ndim;
    int64_t* shape = malloc(8 * sizeof(int64_t));
    int32_t* chunkshape = malloc(8 * sizeof(int32_t));
    int32_t* blockshape = malloc(8 * sizeof(int32_t));
    uint8_t* smeta;
    int32_t smeta_len;
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Blosc error");
        return 0;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    int8_t cell_shape = (int8_t)meta;
    int cell_size = (int) pow(cell_shape, ndim);
    int32_t typesize = schunk->typesize;
    uint8_t* ip = (uint8_t*)input;
    uint8_t* ip_limit = ip + length;
    uint8_t* op = (uint8_t*)output;
    int32_t blocksize = (int32_t) typesize;
    for (int i = 0; i < ndim; i++){
        blocksize *= blockshape[i];
    }

    if (length != blocksize) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Length not equal to blocksize \n");
        return -1;
    }

    /* input and output buffer cannot be less than cell size */
    if (length < cell_size * typesize) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Incorrect length");
        return 0;
    }

    int64_t i_shape[NDCELL_MAX_DIM];
    for (int i = 0; i < ndim; ++i) {
        i_shape[i] = (blockshape[i] + cell_shape - 1) / cell_shape;
    }

    int64_t ncells = 1;
    for (int i = 0; i < ndim; ++i) {
        ncells *= i_shape[i];
    }

    /* main loop */
    int64_t pad_shape[NDCELL_MAX_DIM] = {0};
    int64_t ii[NDCELL_MAX_DIM];
    int32_t ind = 0;
    for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell

        if (ip > ip_limit) {
            free(shape);
            free(chunkshape);
            free(blockshape);
            printf("Literal copy \n");
            return 0;
        }
        blosc2_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = (int64_t) cell_shape;
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cell_shape != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cell_shape;
            } else {
                pad_shape[dim_ind] = (int64_t) cell_shape;
            }
        }

        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[NDCELL_MAX_DIM];
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            blosc2_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
            nd_aux = blockshape[ndim - 1];
            ind = (int32_t) orig;
            for (int i = ndim - 2; i >= 0; i--) {
                ind += (int32_t) (kk[i] * nd_aux);
                nd_aux *= blockshape[i];
            }
            memcpy(&op[ind * typesize], ip, pad_shape[ndim - 1] * typesize);
            ip += pad_shape[ndim - 1] * typesize;
        }
    }
    ind += (int32_t) pad_shape[ndim - 1];


    if (ind != (int32_t) (blocksize / typesize)) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Output size is not compatible with embedded blockshape ind %d %d \n", ind, (blocksize / typesize));
        return 0;
    }

    free(shape);
    free(chunkshape);
    free(blockshape);

    return BLOSC2_ERROR_SUCCESS;
}

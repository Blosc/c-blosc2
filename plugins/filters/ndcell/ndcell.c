/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/


#include <blosc2.h>
#include "ndcell.h"
#include <math.h>
#include <stdio.h>


static void index_unidim_to_multidim(int8_t ndim, int64_t *shape, int64_t i, int64_t *index) {
    int64_t strides[8];
    strides[ndim - 1] = 1;
    for (int j = ndim - 2; j >= 0; --j) {
        strides[j] = shape[j + 1] * strides[j + 1];
    }

    index[0] = i / strides[0];
    for (int j = 1; j < ndim; ++j) {
        index[j] = (i % strides[j - 1]) / strides[j];
    }
}

static void swap_store(void *dest, const void *pa, int size) {
    uint8_t *pa_ = (uint8_t *) pa;
    uint8_t pa2_[8];
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

int ndcell_encoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_cparams* cparams) {

    int8_t ndim;
    int64_t* shape = malloc(8 * sizeof(int64_t));
    int32_t* chunkshape = malloc(8 * sizeof(int32_t));
    int32_t* blockshape = malloc(8 * sizeof(int32_t));
    uint8_t* smeta;
    uint32_t smeta_len;
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

    int8_t cell_shape = meta;
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
        index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = cell_shape;
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cell_shape != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cell_shape;
            } else {
                pad_shape[dim_ind] = cell_shape;
            }
        }
        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[NDCELL_MAX_DIM];
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            index_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
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


int ndcell_decoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_dparams* dparams) {

    blosc2_schunk *schunk = dparams->schunk;
    int8_t ndim;
    int64_t* shape = malloc(8 * sizeof(int64_t));
    int32_t* chunkshape = malloc(8 * sizeof(int32_t));
    int32_t* blockshape = malloc(8 * sizeof(int32_t));
    uint8_t* smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Blosc error");
        return 0;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    int8_t cell_shape = meta;
    int cell_size = (int) pow(cell_shape, ndim);
    int8_t typesize = schunk->typesize;
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
    int64_t pad_shape[NDCELL_MAX_DIM];
    int64_t ii[NDCELL_MAX_DIM];
    int32_t ind;
    for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell

        if (ip > ip_limit) {
            free(shape);
            free(chunkshape);
            free(blockshape);
            printf("Literal copy \n");
            return 0;
        }
        index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = cell_shape;
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cell_shape != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cell_shape;
            } else {
                pad_shape[dim_ind] = cell_shape;
            }
        }

        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[NDCELL_MAX_DIM];
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            index_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
            nd_aux = blockshape[ndim - 1];
            ind = orig;
            for (int i = ndim - 2; i >= 0; i--) {
                ind += kk[i] * nd_aux;
                nd_aux *= blockshape[i];
            }
            memcpy(&op[ind * typesize], ip, pad_shape[ndim - 1] * typesize);
            ip += pad_shape[ndim - 1] * typesize;
        }
    }
    ind += pad_shape[ndim - 1];


    if (ind != (int32_t) (blocksize / typesize)) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Output size is not compatible with embeded blockshape ind %d %d \n", ind, (blocksize / typesize));
        return 0;
    }

    free(shape);
    free(chunkshape);
    free(blockshape);

    return BLOSC2_ERROR_SUCCESS;
}

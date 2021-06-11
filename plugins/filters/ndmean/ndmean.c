/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/


#include "ndmean.h"
#include <stdio.h>
#include <blosc2/blosc2-common.h>


static void index_unidim_to_multidim(int8_t ndim, int64_t *shape, int64_t i, int64_t *index) {
    int64_t strides[NDMEAN_MAX_DIM];
    strides[0] = 1;
    if (ndim > 1) {
        strides[ndim - 1] = 1;
        for (int j = ndim - 2; j >= 0; --j) {
            strides[j] = shape[j + 1] * strides[j + 1];
        }
    }

    index[0] = i / strides[0];
    if (ndim > 1) {
        for (int j = 1; j < ndim; ++j) {
            index[j] = (i % strides[j - 1]) / strides[j];
        }
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

int ndmean_encoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_cparams* cparams) {

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

    if ((typesize != 4) && (typesize != 8)) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        fprintf(stderr, "This filter only works for float or double buffers");
        return -1;
    }

    int8_t cellshape[8];
    int cell_size = 1;
    for (int i = 0; i < 8; ++i) {
        if (i < ndim) {
            cellshape[i] = meta;
            if (cellshape[i] > blockshape[i]) {
                cellshape[i] = (int8_t) blockshape[i];
            }
            cell_size *= cellshape[i];
        } else {
            cellshape[i] = 1;
        }
    }
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
    float* ip_float = (float *) ip;
    double * ip_double = (double *) ip;
    uint8_t* op = (uint8_t *) output;
    uint8_t* op_limit = op + length;
    int64_t cell_length = 0;
    float mean_float = 0;
    double mean_double = 0;


    /* input and output buffer cannot be less than cell size */
    if (length < cell_size * typesize) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        printf("Incorrect length");
        return 0;
    }

    uint8_t* obase = op;

    int64_t i_shape[NDMEAN_MAX_DIM];
    for (int i = 0; i < ndim; ++i) {
        i_shape[i] = (blockshape[i] + cellshape[i] - 1) / cellshape[i];
    }

    int64_t ncells = 1;
    for (int i = 0; i < ndim; ++i) {
        ncells *= i_shape[i];
    }

    /* main loop */
    int64_t pad_shape[NDMEAN_MAX_DIM];
    int64_t ii[NDMEAN_MAX_DIM];
    for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell
        index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = cellshape[0];
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cellshape[dim_ind] != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cellshape[dim_ind];
            } else {
                pad_shape[dim_ind] = (int32_t) cellshape[dim_ind];
            }
        }
        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[8];
        mean_float = 0;
        mean_double = 0;
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            index_unidim_to_multidim((int8_t) (ndim - 1), pad_shape, copy_ind, kk);
            nd_aux = blockshape[ndim - 1];
            int64_t ind = orig;
            for (int i = ndim - 2; i >= 0; i--) {
                ind += kk[i] * nd_aux;
                nd_aux *= blockshape[i];
            }

            switch (typesize) {
                case 4:
                    for (int i = 0; i < pad_shape[ndim - 1]; i++) {
                        mean_float += ip_float[ind + i];
                    }
                    break;
                case 8:
                    for (int i = 0; i < pad_shape[ndim - 1]; i++) {
                        mean_double += ip_double[ind + i];
                    }
                    break;
                default :
                    break;
            }

        }
        cell_length = ncopies * pad_shape[ndim - 1];

        switch (typesize) {
            case 4:
                mean_float /= (float) cell_length;
                for (int i = 0; i < cell_length; i++) {
                    memcpy(op, &mean_float, typesize);
                    op += typesize;
                }
                break;
            case 8:
                mean_double /= (double) cell_length;
                for (int i = 0; i < cell_length; i++) {
                    memcpy(op, &mean_double, typesize);
                    op += typesize;
                }
                break;
            default :
                break;
        }

        if (op > op_limit) {
            free(shape);
            free(chunkshape);
            free(blockshape);
            printf("Output too big");
            return 0;
        }
    }

    free(shape);
    free(chunkshape);
    free(blockshape);
    if((op - obase) != length) {
        printf("Output size must be equal to input size \n");
        return 0;
    }

    return BLOSC2_ERROR_SUCCESS;
}


int ndmean_decoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_dparams* dparams) {

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

    int8_t cellshape[8];
    int cell_size = 1;
    for (int i = 0; i < 8; ++i) {
        if (i < ndim) {
            cellshape[i] = meta;
            if (cellshape[i] > blockshape[i]) {
                cellshape[i] = (int8_t) blockshape[i];
            }
            cell_size *= cellshape[i];
        } else {
            cellshape[i] = 1;
        }
    }

    int8_t typesize = (int8_t) schunk->typesize;
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

    int64_t i_shape[NDMEAN_MAX_DIM];
    for (int i = 0; i < ndim; ++i) {
        i_shape[i] = (blockshape[i] + cellshape[i] - 1) / cellshape[i];
    }

    int64_t ncells = 1;
    for (int i = 0; i < ndim; ++i) {
        ncells *= i_shape[i];
    }

    /* main loop */
    int64_t pad_shape[NDMEAN_MAX_DIM];
    int64_t ii[NDMEAN_MAX_DIM];
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
        int64_t nd_aux = cellshape[0];
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cellshape[dim_ind] != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cellshape[dim_ind];
            } else {
                pad_shape[dim_ind] = (int64_t) cellshape[dim_ind];
            }
        }

        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[NDMEAN_MAX_DIM];
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            index_unidim_to_multidim((int8_t) (ndim - 1), pad_shape, copy_ind, kk);
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

    free(shape);
    free(chunkshape);
    free(blockshape);

    if (ind != (int32_t) (blocksize / typesize)) {
        printf("Output size is not compatible with embeded blockshape ind %d %d \n", ind, (blocksize / typesize));
        return 0;
    }

    return BLOSC2_ERROR_SUCCESS;
}

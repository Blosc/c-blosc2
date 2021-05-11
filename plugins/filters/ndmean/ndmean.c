/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/


#include <caterva.h>
#include <ndmean.h>
#include <caterva_utilities.h>


int ndmean_encoder(const uint8_t* input, uint8_t* output, int32_t length, void* params) {
    ndmean_params *fparams = (ndmean_params *) params;
    int8_t typesize = fparams->array->itemsize;

    if ((typesize != 4) && (typesize != 8)) {
        fprintf(stderr, "This filter only works for float or double buffers");
        return -1;
    }
/*
    printf("\n input \n");
    for (int i = 0; i < 50; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim = fparams->array->ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t cellshape[CATERVA_MAX_DIM];
    int cell_size = 1;
    for (int i = 0; i < CATERVA_MAX_DIM; ++i) {
        shape[i] = fparams->array->shape[i];
        chunkshape[i] = fparams->array->chunkshape[i];
        blockshape[i] = fparams->array->blockshape[i];
        cellshape[i] = fparams->cellshape[i];
        if (cellshape[i] > blockshape[i]) {
            cellshape[i] = blockshape[i];
        }
        cell_size *= cellshape[i];
    }
    int32_t blocksize = (int32_t) typesize;
    for (int i = 0; i < ndim; i++){
        blocksize *= blockshape[i];
    }

    if (length != blocksize) {
//    if (NDCELL_UNEXPECT_CONDITIONAL(length != blocksize)) {
        printf("Length not equal to blocksize %d %d \n", length, blocksize);
        return -1;
    }

    uint8_t* ip = (uint8_t *) input;
    float* ip_float = (float *) ip;
    double * ip_double = (double *) ip;
    uint8_t* op = (uint8_t *) output;
    uint8_t* op_limit = op + length;
 //   uint8_t* cell = malloc(cell_size);
 //   double* cell_double = malloc(cell_size / typesize);
    int64_t cell_length;
    float mean_float = 0;
    double mean_double = 0;


    /* input and output buffer cannot be less than cell size */
    if (length < cell_size * typesize) {
        printf("Incorrect length");
        return 0;
    }

    uint8_t* obase = op;

    int64_t i_shape[ndim];
    for (int i = 0; i < ndim; ++i) {
        i_shape[i] = (blockshape[i] + cellshape[i] - 1) / cellshape[i];
    }

    int64_t ncells = 1;
    for (int i = 0; i < ndim; ++i) {
        ncells *= i_shape[i];
    }

    /* main loop */
    int64_t pad_shape[ndim];
    int64_t ii[ndim];
    for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell
        index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = 1;
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * cellshape[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cellshape[dim_ind] != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cellshape[dim_ind];
            } else {
                pad_shape[dim_ind] = cellshape[dim_ind];
            }
        }
        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[CATERVA_MAX_DIM];
        mean_float = 0;
        mean_double = 0;
        for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
            index_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
            nd_aux = blockshape[ndim - 1];
            int64_t ind = orig;
            for (int i = ndim - 2; i >= 0; i--) {
                ind += kk[i] * nd_aux;
                nd_aux *= blockshape[i];
            }

/*
   //         memcpy(cell, &ip[ind * typesize], pad_shape[ndim - 1] * typesize);
     //       cell += pad_shape[ndim - 1] * typesize;
            if (typesize == 8) {
                for (int i = 0; i < pad_shape[ndim - 1]; i++) {
           //         cell_double[i] = ((double*) ip)[ind + i];
                    cell_double[i] = ip_double[ind + i];
                }
                cell_double += pad_shape[ndim - 1];
            }
*/
    //        printf("\n mean: ");

            switch (typesize) {
                case 4:
                    for (int i = 0; i < pad_shape[ndim - 1]; i++) {
                        mean_float += ip_float[ind + i];
      //                  printf("%f, ", mean_float);
                    }
                    break;
                case 8:
                    for (int i = 0; i < pad_shape[ndim - 1]; i++) {
                        mean_double += ip_double[ind + i];
        //                printf("%f, ", mean_double);
                    }
                    break;
                default :
                    break;
            }

        }
        cell_length = ncopies * pad_shape[ndim - 1];
  /*
        printf("\n ip_float \n");
        for (int i = 0; i < cell_length; i++) {
            printf("%f, ", ip_float[i]);
        }
/*
        cell -= cell_length * typesize;
        printf("\n cell \n");
        for (int i = 0; i < cell_length * typesize; i++) {
            printf("%u, ", cell[i]);
        }
*
        cell_double -= cell_length;
        printf("\n cell_double \n");
        for (int i = 0; i < cell_length; i++) {
            printf("%f, ", cell_double[i]);
        }
*/
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
   //     printf("\n MEAN: %f, %f", mean_float, mean_double);


    //    printf("\n op \n");


        if (op > op_limit) {
//        if (NDCELL_UNEXPECT_CONDITIONAL(op > op_limit)) {
            printf("Output too big");
            return 0;
        }
    }

 //   free(cell);
//    free(cell_double);
    if((op - obase) != length) {
        printf("Output size must be equal to input size \n");
        return 0;
    }
/*
   // free(content);
    printf("\n op \n");
    for (int i = 0; i < length; i++) {
        printf("%u, ", output[i]);
    }
*/
    return BLOSC2_ERROR_SUCCESS;
}


int ndmean_decoder(const uint8_t* input, uint8_t* output, int32_t length, void* params) {
    ndmean_params *fparams = (ndmean_params *) params;

 /*   uint8_t* content;
    uint32_t content_len;
    int nmetalayer = blosc2_meta_get(fparams->array->sc, "caterva", &content, &content_len);
    if (nmetalayer < 0) {
        BLOSC_TRACE_ERROR("Metalayer \"caterva\" not found.");
        return nmetalayer;
    }
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    deserialize_meta(content, content_len, &ndim, shape, chunkshape, blockshape);
*/
    int8_t ndim = fparams->array->ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t cellshape[CATERVA_MAX_DIM];
    int cell_size = 1;
    for (int i = 0; i < CATERVA_MAX_DIM; ++i) {
        shape[i] = fparams->array->shape[i];
        chunkshape[i] = fparams->array->chunkshape[i];
        blockshape[i] = fparams->array->blockshape[i];
        cellshape[i] = fparams->cellshape[i];
        if (cellshape[i] > blockshape[i]) {
            cellshape[i] = blockshape[i];
        }
        cell_size *= cellshape[i];
    }

    int8_t typesize = fparams->array->itemsize;
    uint8_t* ip = (uint8_t*)input;
    uint8_t* ip_limit = ip + length;
    uint8_t* op = (uint8_t*)output;
    int32_t blocksize = (int32_t) typesize;
    for (int i = 0; i < ndim; i++){
        blocksize *= blockshape[i];
    }

    if (length != blocksize) {
 //   if (NDCELL_UNEXPECT_CONDITIONAL(length != blocksize)) {
        printf("Length not equal to blocksize \n");
        return -1;
    }
    /* input and output buffer cannot be less than cell size */
    if (length < cell_size * typesize) {
//    if (NDCELL_UNEXPECT_CONDITIONAL(length < cell_size * typesize)) {
        printf("Incorrect length");
        return 0;
    }

    int64_t i_shape[ndim];
    for (int i = 0; i < ndim; ++i) {
        i_shape[i] = (blockshape[i] + cellshape[i] - 1) / cellshape[i];
    }

    int64_t ncells = 1;
    for (int i = 0; i < ndim; ++i) {
        ncells *= i_shape[i];
    }

    /* main loop */
    int64_t pad_shape[ndim];
    int64_t ii[ndim];
    int32_t ind;
    for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell

        if (ip > ip_limit) {
//        if (NDCELL_UNEXPECT_CONDITIONAL(ip > ip_limit)) {
            printf("Literal copy \n");
            return 0;
        }
        index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
        uint32_t orig = 0;
        int64_t nd_aux = 1;
        for (int i = ndim - 1; i >= 0; i--) {
            orig += ii[i] * cellshape[i] * nd_aux;
            nd_aux *= blockshape[i];
        }

        for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
            if ((blockshape[dim_ind] % cellshape[dim_ind] != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
                pad_shape[dim_ind] = blockshape[dim_ind] % cellshape[dim_ind];
            } else {
                pad_shape[dim_ind] = cellshape[dim_ind];
            }
        }

        int64_t ncopies = 1;
        for (int i = 0; i < ndim - 1; ++i) {
            ncopies *= pad_shape[i];
        }
        int64_t kk[ndim];
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
        printf("Output size is not compatible with embeded blockshape ind %d %d \n", ind, (blocksize / typesize));
        return 0;
    }

 //   free(content);

    return BLOSC2_ERROR_SUCCESS;
}

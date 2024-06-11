/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/


#include "ndmean.h"
#include "blosc2/blosc2-common.h"
#include "b2nd.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

int ndmean_forward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta, blosc2_cparams *cparams,
                   uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);
  int typesize = cparams->typesize;

  if ((typesize != 4) && (typesize != 8)) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("This filter only works for float or double");
    return BLOSC2_ERROR_FAILURE;
  }

  int8_t cellshape[8];
  int cell_size = 1;
  for (int i = 0; i < 8; ++i) {
    if (i < ndim) {
      cellshape[i] = (int8_t) meta;
      if (cellshape[i] > blockshape[i]) {
        cellshape[i] = (int8_t) blockshape[i];
      }
      cell_size *= cellshape[i];
    } else {
      cellshape[i] = 1;
    }
  }
  int32_t blocksize = (int32_t) typesize;
  for (int i = 0; i < ndim; i++) {
    blocksize *= blockshape[i];
  }

  if (length != blocksize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Length not equal to blocksize %d %d \n", length, blocksize);
    return BLOSC2_ERROR_FAILURE;
  }

  uint8_t *ip = (uint8_t *) input;
  float *ip_float = (float *) ip;
  double *ip_double = (double *) ip;
  uint8_t *op = (uint8_t *) output;
  uint8_t *op_limit = op + length;
  int64_t cell_length;
  float mean_float = 0;
  double mean_double = 0;


  if (length < cell_size * typesize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("input and output buffer cannot be smaller than cell size");
    return BLOSC2_ERROR_FAILURE;
  }

  uint8_t *obase = op;

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
    blosc2_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
    uint32_t orig = 0;
    int64_t nd_aux = (int64_t) (cellshape[0]);
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
      blosc2_unidim_to_multidim((int8_t) (ndim - 1), pad_shape, copy_ind, kk);
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
      BLOSC_TRACE_ERROR("Exceeding output buffer limits!");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  free(shape);
  free(chunkshape);
  free(blockshape);
  if ((op - obase) != length) {
    BLOSC_TRACE_ERROR("Output size must be equal to input size");
    return BLOSC2_ERROR_FAILURE;
  }

  return BLOSC2_ERROR_SUCCESS;
}


int ndmean_backward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta, blosc2_dparams *dparams,
                   uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  blosc2_schunk *schunk = dparams->schunk;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  int8_t cellshape[8];
  int cell_size = 1;
  for (int i = 0; i < 8; ++i) {
    if (i < ndim) {
      cellshape[i] = (int8_t) meta;
      if (cellshape[i] > blockshape[i]) {
        cellshape[i] = (int8_t) blockshape[i];
      }
      cell_size *= cellshape[i];
    } else {
      cellshape[i] = 1;
    }
  }

  int8_t typesize = (int8_t) schunk->typesize;
  uint8_t *ip = (uint8_t *) input;
  uint8_t *ip_limit = ip + length;
  uint8_t *op = (uint8_t *) output;
  int32_t blocksize = (int32_t) typesize;
  for (int i = 0; i < ndim; i++) {
    blocksize *= blockshape[i];
  }

  if (length != blocksize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Length not equal to blocksize");
    return BLOSC2_ERROR_FAILURE;
  }

  if (length < cell_size * typesize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("input and output buffer cannot be smaller than cell size");
    return BLOSC2_ERROR_FAILURE;
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
  int64_t pad_shape[NDMEAN_MAX_DIM] = {0};
  int64_t ii[NDMEAN_MAX_DIM];
  int32_t ind = 0;
  for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell

    if (ip > ip_limit) {
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("Exceeding input length!");
      return BLOSC2_ERROR_FAILURE;
    }
    blosc2_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
    uint32_t orig = 0;
    int64_t nd_aux = (int64_t) (cellshape[0]);
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
      blosc2_unidim_to_multidim((int8_t) (ndim - 1), pad_shape, copy_ind, kk);
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
    BLOSC_TRACE_ERROR("Output size is not compatible with embedded blockshape ind %d %d \n",
                      ind, (blocksize / typesize));
    return BLOSC2_ERROR_FAILURE;
  }

  return BLOSC2_ERROR_SUCCESS;
}

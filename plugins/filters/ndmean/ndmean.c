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
  // b2nd_deserialize_meta() writes up to B2ND_MAX_DIM entries (it only validates
  // ndim <= B2ND_MAX_DIM), so the destination buffers must be sized accordingly,
  // not for NDMEAN_MAX_DIM.  Otherwise a crafted b2nd metalayer with
  // NDMEAN_MAX_DIM < ndim <= B2ND_MAX_DIM overflows these allocations.
  int64_t *shape = malloc(B2ND_MAX_DIM * sizeof(int64_t));
  int32_t *chunkshape = malloc(B2ND_MAX_DIM * sizeof(int32_t));
  int32_t *blockshape = malloc(B2ND_MAX_DIM * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  if (b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL) < 0) {
    free(smeta);
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Cannot deserialize b2nd metalayer");
    return BLOSC2_ERROR_FAILURE;
  }
  free(smeta);
  // Guard the fixed-size (NDMEAN_MAX_DIM) stack arrays used below.
  if (ndim <= 0 || ndim > NDMEAN_MAX_DIM) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("ndim %d is out of range", ndim);
    return BLOSC2_ERROR_FAILURE;
  }
  int typesize = cparams->typesize;

  if ((typesize != 4) && (typesize != 8)) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("This filter only works for float or double");
    return BLOSC2_ERROR_FAILURE;
  }

  // `meta` carries the NDMEAN cell shape and is attacker-controlled (it is the
  // chunk-header filters_meta byte).  It is cast to int8_t and used as the
  // divisor when building i_shape[] below, so a value of 0 (divide-by-zero) or
  // one that becomes negative after the cast (meta > INT8_MAX) would crash or
  // produce bogus, out-of-range geometry.  Require a positive cell shape.
  if ((int8_t) meta <= 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Invalid cell shape (meta) %d", (int) meta);
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
  // See ndmean_backward(): validate the b2nd block geometry in 64-bit so a
  // crafted blockshape cannot wrap a 32-bit product back onto `length` and make
  // the index arithmetic below stray outside the `length`-sized buffers.
  int64_t blocksize = (int64_t) typesize;
  for (int i = 0; i < ndim; i++) {
    if (blockshape[i] <= 0) {
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("Invalid blockshape[%d] = %d", i, blockshape[i]);
      return BLOSC2_ERROR_FAILURE;
    }
    blocksize *= blockshape[i];
    if (blocksize > (int64_t) length) {
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("blockshape too large for block of %d bytes", length);
      return BLOSC2_ERROR_FAILURE;
    }
  }

  if ((int64_t) length != blocksize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Length not equal to blocksize %d %lld \n", length, (long long) blocksize);
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
  // b2nd_deserialize_meta() writes up to B2ND_MAX_DIM entries (it only validates
  // ndim <= B2ND_MAX_DIM), so the destination buffers must be sized accordingly,
  // not for NDMEAN_MAX_DIM.  Otherwise a crafted b2nd metalayer with
  // NDMEAN_MAX_DIM < ndim <= B2ND_MAX_DIM overflows these allocations.
  int64_t *shape = malloc(B2ND_MAX_DIM * sizeof(int64_t));
  int32_t *chunkshape = malloc(B2ND_MAX_DIM * sizeof(int32_t));
  int32_t *blockshape = malloc(B2ND_MAX_DIM * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  if (b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL) < 0) {
    free(smeta);
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Cannot deserialize b2nd metalayer");
    return BLOSC2_ERROR_FAILURE;
  }
  free(smeta);
  // Guard the fixed-size (NDMEAN_MAX_DIM) stack arrays used below.
  if (ndim <= 0 || ndim > NDMEAN_MAX_DIM) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("ndim %d is out of range", ndim);
    return BLOSC2_ERROR_FAILURE;
  }

  // `meta` carries the NDMEAN cell shape and is attacker-controlled (it is the
  // chunk-header filters_meta byte).  It is cast to int8_t and used as the
  // divisor when building i_shape[] below, so a value of 0 (divide-by-zero) or
  // one that becomes negative after the cast (meta > INT8_MAX) would crash or
  // produce bogus, out-of-range geometry (e.g. a negative pad_shape feeding a
  // huge memcpy).  Require a positive cell shape.
  if ((int8_t) meta <= 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Invalid cell shape (meta) %d", (int) meta);
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

  int8_t typesize = (int8_t) schunk->typesize;
  uint8_t *ip = (uint8_t *) input;
  uint8_t *ip_limit = ip + length;
  uint8_t *op = (uint8_t *) output;
  if (typesize <= 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Invalid typesize %d", typesize);
    return BLOSC2_ERROR_FAILURE;
  }
  // The block geometry (blockshape) comes from the attacker-controlled "b2nd"
  // metalayer and is NOT validated by b2nd_deserialize_meta().  Compute the
  // block size in 64-bit and reject non-positive dimensions.  Otherwise a
  // crafted blockshape whose 32-bit product wraps back onto `length` would pass
  // the check below, while the scatter-write index arithmetic further down uses
  // the true (huge) dimensions, writing past the `length`-sized output buffer
  // (heap overflow).  Bailing as soon as the running product exceeds `length`
  // also keeps the 64-bit multiply from overflowing.
  int64_t blocksize = (int64_t) typesize;
  for (int i = 0; i < ndim; i++) {
    if (blockshape[i] <= 0) {
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("Invalid blockshape[%d] = %d", i, blockshape[i]);
      return BLOSC2_ERROR_FAILURE;
    }
    blocksize *= blockshape[i];
    if (blocksize > (int64_t) length) {
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("blockshape too large for block of %d bytes", length);
      return BLOSC2_ERROR_FAILURE;
    }
  }

  if ((int64_t) length != blocksize) {
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
    BLOSC_TRACE_ERROR("Output size is not compatible with embedded blockshape ind %d %lld \n",
                      ind, (long long) (blocksize / typesize));
    return BLOSC2_ERROR_FAILURE;
  }

  return BLOSC2_ERROR_SUCCESS;
}

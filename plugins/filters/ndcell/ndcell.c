/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "ndcell.h"
#include "blosc2.h"
#include "b2nd.h"

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>


int ndcell_forward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta, blosc2_cparams *cparams,
                   uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  int8_t ndim;
  int64_t *shape = malloc(B2ND_MAX_DIM * sizeof(int64_t));
  int32_t *chunkshape = malloc(B2ND_MAX_DIM * sizeof(int32_t));
  int32_t *blockshape = malloc(B2ND_MAX_DIM * sizeof(int32_t));
  if (b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL) < 0) {
    free(smeta);
    free(shape);
    free(chunkshape);
    free(blockshape);
    return BLOSC2_ERROR_FAILURE;
  }
  free(smeta);
  if (ndim <= 0 || ndim > NDCELL_MAX_DIM) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("ndim %d is out of range", ndim);
    return BLOSC2_ERROR_FAILURE;
  }
  int typesize = cparams->typesize;

  // `meta` is the cell side length and comes from the chunk header (uint8_t,
  // 0..255).  Interpret it as unsigned and reject 0: an (int8_t) cast would turn
  // 128..255 into negative values and 0 would later divide by zero in
  // `(blockshape[i] + cell_shape - 1) / cell_shape`.
  int cell_shape = (int) meta;
  if (cell_shape <= 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Invalid cell_shape %d", cell_shape);
    return BLOSC2_ERROR_FAILURE;
  }
  const int cell_size = (int) pow(cell_shape, ndim);

  if (typesize <= 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Invalid typesize %d", typesize);
    return BLOSC2_ERROR_FAILURE;
  }
  // See ndcell_backward(): validate the b2nd block geometry in 64-bit so a
  // crafted blockshape cannot wrap a 32-bit product back onto `length` and make
  // the index arithmetic below read past the `length`-sized input buffer.
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
  uint8_t *op = (uint8_t *) output;
  uint8_t *op_limit = op + length;

  if (length < cell_size * typesize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("input or output buffer cannot be smaller than cell size");
    return BLOSC2_ERROR_FAILURE;
  }

  uint8_t *obase = op;

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
      BLOSC_TRACE_ERROR("Exceeding output buffer limits!");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  if ((op - obase) != length) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Output size must be equal to input size");
    return BLOSC2_ERROR_FAILURE;
  }

  free(shape);
  free(chunkshape);
  free(blockshape);

  return BLOSC2_ERROR_SUCCESS;
}


int ndcell_backward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta, blosc2_dparams *dparams,
                   uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  blosc2_schunk *schunk = dparams->schunk;
  int8_t ndim;
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
    return BLOSC2_ERROR_FAILURE;
  }
  free(smeta);
  if (ndim <= 0 || ndim > NDCELL_MAX_DIM) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("ndim %d is out of range", ndim);
    return BLOSC2_ERROR_FAILURE;
  }

  // `meta` is attacker-controlled in a crafted frame.  Interpret it as unsigned
  // and reject 0: an (int8_t) cast would turn 128..255 into negative values and
  // 0 would later divide by zero in `(blockshape[i] + cell_shape - 1) / cell_shape`.
  int cell_shape = (int) meta;
  if (cell_shape <= 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Invalid cell_shape %d", cell_shape);
    return BLOSC2_ERROR_FAILURE;
  }
  int cell_size = (int) pow(cell_shape, ndim);
  int32_t typesize = schunk->typesize;
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
  // also keeps the 64-bit multiply from overflowing (it stays
  // <= length * INT32_MAX < INT64_MAX).
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
    BLOSC_TRACE_ERROR("Length not equal to blocksize %d %lld", length, (long long) blocksize);
    return BLOSC2_ERROR_FAILURE;
  }

  if (length < cell_size * typesize) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("input and output buffer cannot be smaller than cell size");
    return BLOSC2_ERROR_FAILURE;
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
      BLOSC_TRACE_ERROR("Exceeding input length!");
      return BLOSC2_ERROR_FAILURE;
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
    BLOSC_TRACE_ERROR("Output size is not compatible with embedded blockshape ind %d %lld \n",
                      ind, (long long) (blocksize / typesize));
    return BLOSC2_ERROR_FAILURE;
  }

  free(shape);
  free(chunkshape);
  free(blockshape);

  return BLOSC2_ERROR_SUCCESS;
}

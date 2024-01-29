/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc2.h"
#include "../plugins/plugin_utils.h"
#include "transpose.h"

#include <stdio.h>
#include <stdlib.h>


int transpose_int16(int z, int y, int x, int16_t *input, int16_t *output) {

  for(int i = 0; i < x; i++)
  {
    for(int j = 0; j < y; j++)
    {
      for(int k = 0; k < z; k++)
      {
        *(output + i*y*z + j*z + k) = *(input + k*y*x + j*x + i);
      }
    }
  }
  return 0;
}


int transpose_forward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                      blosc2_cparams* cparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_UNUSED_PARAM(length);
  int32_t typesize = cparams->typesize;

  // Get dims from meta
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
  deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
  free(smeta);
  if (ndim != 3) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("Transpose filter only works for 3D arrays currently");
    return BLOSC2_ERROR_FAILURE;
  }
  int z = (int) blockshape[0];
  int y = (int) blockshape[1];
  int x = (int) blockshape[2];

  printf("transpose_forward\n");

  switch (typesize) {
    case 2:
      return transpose_int16(z, y, x, (int16_t *)input, (int16_t *)output);
    default:
      BLOSC_TRACE_ERROR("Error in BLOSC_FILTER_TRANSPOSE filter: "
                        "Precision for typesize %d not handled yet",
                        (int)typesize);
      return -1;
  }
  return 0;
}

int transpose_backward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
                       blosc2_dparams *dparams, uint8_t id) {
  // transpose is its own inverse
  blosc2_schunk *schunk = (blosc2_schunk*)dparams->schunk;
  blosc2_cparams *cparams;
  blosc2_schunk_get_cparams(schunk, &cparams);
  printf("transpose_backward\n");
  return transpose_forward(input, output, length, meta, cparams, id);
}

/*
  Copyright (C) 2021 The Blosc Developers
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "blosc2.h"
#include "blosc-private.h"
#include "plugin_utils.h"

#define BLOSC_PLUGINS_MAX_DIM 8


int32_t deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape) {
  BLOSC_UNUSED_PARAM(smeta_len);
  uint8_t *pmeta = smeta;

  // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
  pmeta += 1;

  // version entry
  int8_t version = (int8_t) pmeta[0];  // positive fixnum (7-bit positive integer)
  BLOSC_UNUSED_PARAM(version);
  pmeta += 1;

  // ndim entry
  *ndim = (int8_t) pmeta[0];
  int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
  pmeta += 1;

  // shape entry
  // Initialize to ones, as required by Blosc2 NDim
  for (int i = 0; i < BLOSC_PLUGINS_MAX_DIM; i++) shape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(shape + i, pmeta, sizeof(int64_t));
    pmeta += sizeof(int64_t);
  }

  // chunkshape entry
  // Initialize to ones, as required by Blosc2 NDim
  for (int i = 0; i < BLOSC_PLUGINS_MAX_DIM; i++) chunkshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(chunkshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // blockshape entry
  // Initialize to ones, as required by Blosc2 NDim
  for (int i = 0; i < BLOSC_PLUGINS_MAX_DIM; i++) blockshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(blockshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }
  int32_t slen = (int32_t) (pmeta - smeta);
  return slen;
}

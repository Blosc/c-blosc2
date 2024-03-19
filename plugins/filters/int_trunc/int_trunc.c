/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc2.h"


#include <stdio.h>
#include <stdlib.h>


int truncate_uint8(int8_t prec_bits, int32_t nelems,
                   const uint8_t *src, uint8_t *dest) {
  uint8_t max_prec_bits = (uint8_t) sizeof(uint8_t) * 8;
  uint8_t zeroed_bits = (prec_bits >= 0) ? max_prec_bits - prec_bits : -prec_bits;
  if (zeroed_bits >= max_prec_bits) {
    BLOSC_TRACE_ERROR("The reduction in precision cannot be larger or equal than %d bits"
                      " (asking for %d bits)",  max_prec_bits, prec_bits);
    return -1;
  }
  uint8_t mask = ~((1 << zeroed_bits) - 1);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
  return BLOSC2_ERROR_SUCCESS;
}


int truncate_uint16(int8_t prec_bits, int32_t nelems,
                    const uint16_t *src, uint16_t *dest) {
  uint8_t max_prec_bits = (uint8_t) sizeof(uint16_t) * 8;
  uint8_t zeroed_bits = (prec_bits >= 0) ? max_prec_bits - prec_bits : -prec_bits;
  if (zeroed_bits >= max_prec_bits) {
    BLOSC_TRACE_ERROR("The reduction in precision cannot be larger or equal than %d bits"
                      " (asking for %d bits)",  max_prec_bits, prec_bits);
    return -1;
  }
  uint16_t mask = ~((1 << zeroed_bits) - 1);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
  return BLOSC2_ERROR_SUCCESS;
}


int truncate_uint32(int8_t prec_bits, int32_t nelems,
                    const uint32_t *src, uint32_t *dest) {
  uint8_t max_prec_bits = (uint8_t) sizeof(uint32_t) * 8;
  uint8_t zeroed_bits = (prec_bits >= 0) ? max_prec_bits - prec_bits : -prec_bits;
  if (zeroed_bits >= max_prec_bits) {
    BLOSC_TRACE_ERROR("The reduction in precision cannot be larger or equal than %d bits"
                      " (asking for %d bits)",  max_prec_bits, prec_bits);
    return -1;
  }
  uint32_t mask = ~((1 << zeroed_bits) - 1);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
  return BLOSC2_ERROR_SUCCESS;
}


int truncate_uint64(int8_t prec_bits, int32_t nelems,
                    const uint64_t *src, uint64_t *dest) {
  uint8_t max_prec_bits = (uint8_t) sizeof(uint64_t) * 8;
  uint8_t zeroed_bits = (prec_bits >= 0) ? max_prec_bits - prec_bits : -prec_bits;
  if (zeroed_bits >= max_prec_bits) {
    BLOSC_TRACE_ERROR("The reduction in precision cannot be larger or equal than %d bits"
                      " (asking for %d bits)",  max_prec_bits, prec_bits);
    return -1;
  }
  uint64_t mask = ~((1ULL << zeroed_bits) - 1ULL);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
  return BLOSC2_ERROR_SUCCESS;
}


int int_trunc_forward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                      blosc2_cparams* cparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  int32_t typesize = cparams->typesize;
  int32_t nelems = length / typesize;
  int8_t prec_bits = (int8_t)meta;

  // Positive values of prec_bits will set absolute precision bits, whereas negative
  // values will reduce the precision bits (similar to Python slicing convention).
  switch (typesize) {
    case 1:
      return truncate_uint8(prec_bits, nelems,
                            (uint8_t *)input, (uint8_t *)output);
    case 2:
      return truncate_uint16(prec_bits, nelems,
                             (uint16_t *)input, (uint16_t *)output);
    case 4:
      return truncate_uint32(prec_bits, nelems,
                             (uint32_t *)input, (uint32_t *)output);
    case 8:
      return truncate_uint64(prec_bits, nelems,
                             (uint64_t *)input, (uint64_t *)output);
    default:
      BLOSC_TRACE_ERROR("Error in BLOSC_FILTER_INT_TRUNC filter: "
                        "Precision for typesize %d not handled",
                        (int)typesize);
      return -1;
  }
}

int int_trunc_backward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
                       blosc2_dparams *dparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(id);
  BLOSC_UNUSED_PARAM(dparams);
  BLOSC_UNUSED_PARAM(meta);

  // Do a copy of the input buffer (truncation is lossy and cannot be reversed)
  memcpy(output, input, length);
  return BLOSC2_ERROR_SUCCESS;
}

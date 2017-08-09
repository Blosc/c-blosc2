/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-12-18

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>
#include "blosc.h"
#include "delta.h"


/* Apply the delta filters to src.  This can never fail. */
void delta_encoder8(uint8_t* dref, int32_t offset, int32_t nbytes,
                    uint8_t* src, uint8_t* dest) {

  if (offset == 0) {
    /* This is the reference block, copy it literally */
    for (int i = 0; i < nbytes; i++) {
      dest[i] = dref[i];
    }
  } else {
    /* Encode delta for the other blocks */
    for (int i = 0; i < nbytes; i++) {
      dest[i] = src[i] - dref[i];
    }
  }
}


/* Undo the delta filter in dest.  This can never fail. */
void delta_decoder8(uint8_t* dref, int32_t offset, int32_t nbytes,
                    uint8_t* dest) {

  if (offset != 0) {
    /* Decode delta for the non-ref blocks */
    for (int i = 0; i < nbytes; i++) {
      dest[i] += dref[i];
    }
  }
}

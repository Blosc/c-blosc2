/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-12-18

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include "blosc.h"
#include "delta.h"


/* Apply the delta filters to src.  This can never fail. */
void delta_encoder8(uint8_t* dref, int32_t offset, int32_t nbytes,
                    int32_t typesize, uint8_t* src, uint8_t* dest) {

  if (offset == 0) {
    /* This is the reference block, use delta coding in elements */
    switch (typesize) {
      case 2:
        ((uint16_t *)dest)[0] = ((uint16_t *)dref)[0];
        for (int i = 1; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] = ((uint16_t *)src)[i] -
                                  ((uint16_t *)dref)[i-1];
        }
        break;
      case 4:
        ((uint32_t *)dest)[0] = ((uint32_t *)dref)[0];
        for (int i = 1; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] = ((uint32_t *)src)[i] -
                                  ((uint32_t *)dref)[i-1];
        }
        break;
      case 8:
        ((uint64_t *)dest)[0] = ((uint64_t *)dref)[0];
        for (int i = 1; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] = ((uint64_t *)src)[i] -
                                  ((uint64_t *)dref)[i-1];
        }
        break;
      default:
        dest[0] = dref[0];
        for (int i = 1; i < nbytes; i++) {
          dest[i] = src[i] - dref[i-1];
        }
    }
  } else {
    /* Use delta coding wrt reference block */
    switch (typesize) {
      case 2:
        for (int i = 0; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] = ((uint16_t *)src)[i] - ((uint16_t *)dref)[i];
        }
        break;
      case 4:
        for (int i = 0; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] = ((uint32_t *)src)[i] - ((uint32_t *)dref)[i];
        }
        break;
      case 8:
        for (int i = 0; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] = ((uint64_t *)src)[i] - ((uint64_t *)dref)[i];
        }
        break;
      default:
        for (int i = 0; i < nbytes; i++) {
          dest[i] = src[i] - dref[i];
        }
    }
  }
}


/* Undo the delta filter in dest.  This can never fail. */
void delta_decoder8(uint8_t* dref, int32_t offset, int32_t nbytes,
                    int32_t typesize, uint8_t* dest) {

  if (offset == 0) {
    /* Decode delta for the reference block */
    switch (typesize) {
      case 2:
        for (int i = 1; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] += ((uint16_t *)dref)[i-1];
        }
        break;
      case 4:
        for (int i = 1; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] += ((uint32_t *)dref)[i-1];
        }
        break;
      case 8:
        for (int i = 1; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] += ((uint64_t *)dref)[i-1];
        }
        break;
      default:
        for (int i = 1; i < nbytes; i++) {
          dest[i] += dref[i-1];
        }
    }
  } else {
    /* Decode delta for the non-reference blocks */
    switch (typesize) {
      case 2:
        for (int i = 0; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] += ((uint16_t *)dref)[i];
        }
        break;
      case 4:
        for (int i = 0; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] += ((uint32_t *)dref)[i];
        }
        break;
      case 8:
        for (int i = 0; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] += ((uint64_t *)dref)[i];
        }
        break;
      default:
        for (int i = 0; i < nbytes; i++) {
          dest[i] += dref[i];
        }
    }
  }
}

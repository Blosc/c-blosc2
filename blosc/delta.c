/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "delta.h"

#include <stdio.h>
#include <stdint.h>


/* Apply the delta filters to src.  This can never fail. */
void delta_encoder(const uint8_t* dref, int32_t offset, int32_t nbytes, int32_t typesize,
                   const uint8_t* src, uint8_t* dest) {
  int32_t i;
  if (offset == 0) {
    /* This is the reference block, use delta coding in elements */
    switch (typesize) {
      case 1:
        dest[0] = dref[0];
        for (i = 1; i < nbytes; i++) {
          dest[i] = src[i] ^ dref[i-1];
        }
        break;
      case 2:
        ((uint16_t *)dest)[0] = ((uint16_t *)dref)[0];
        for (i = 1; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] =
                  ((uint16_t *)src)[i] ^ ((uint16_t *)dref)[i-1];
        }
        break;
      case 4:
        ((uint32_t *)dest)[0] = ((uint32_t *)dref)[0];
        for (i = 1; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] =
                  ((uint32_t *)src)[i] ^ ((uint32_t *)dref)[i-1];
        }
        break;
      case 8:
        ((uint64_t *)dest)[0] = ((uint64_t *)dref)[0];
        for (i = 1; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] =
                  ((uint64_t *)src)[i] ^ ((uint64_t *)dref)[i-1];
        }
        break;
      default:
        if ((typesize % 8) == 0) {
          delta_encoder(dref, offset, nbytes, 8, src, dest);
        } else {
          delta_encoder(dref, offset, nbytes, 1, src, dest);
        }
    }
  } else {
    /* Use delta coding wrt reference block */
    switch (typesize) {
      case 1:
        for (i = 0; i < nbytes; i++) {
          dest[i] = src[i] ^ dref[i];
        }
        break;
      case 2:
        for (i = 0; i < nbytes / 2; i++) {
          ((uint16_t *) dest)[i] =
                  ((uint16_t *) src)[i] ^ ((uint16_t *) dref)[i];
        }
        break;
      case 4:
        for (i = 0; i < nbytes / 4; i++) {
          ((uint32_t *) dest)[i] =
                  ((uint32_t *) src)[i] ^ ((uint32_t *) dref)[i];
        }
        break;
      case 8:
        for (i = 0; i < nbytes / 8; i++) {
          ((uint64_t *) dest)[i] =
                  ((uint64_t *) src)[i] ^ ((uint64_t *) dref)[i];
        }
        break;
      default:
        if ((typesize % 8) == 0) {
          delta_encoder(dref, offset, nbytes, 8, src, dest);
        } else {
          delta_encoder(dref, offset, nbytes, 1, src, dest);
        }
    }
  }
}


/* Undo the delta filter in dest.  This can never fail. */
void delta_decoder(const uint8_t* dref, int32_t offset, int32_t nbytes,
                   int32_t typesize, uint8_t* dest) {
  int32_t i;

  if (offset == 0) {
    /* Decode delta for the reference block */
    switch (typesize) {
      case 1:
        for (i = 1; i < nbytes; i++) {
          dest[i] ^= dref[i-1];
        }
        break;
      case 2:
        for (i = 1; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] ^= ((uint16_t *)dref)[i-1];
        }
        break;
      case 4:
        for (i = 1; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] ^= ((uint32_t *)dref)[i-1];
        }
        break;
      case 8:
        for (i = 1; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] ^= ((uint64_t *)dref)[i-1];
        }
        break;
      default:
        if ((typesize % 8) == 0) {
          delta_decoder(dref, offset, nbytes, 8, dest);
        } else {
          delta_decoder(dref, offset, nbytes, 1, dest);
        }
    }
  } else {
    /* Decode delta for the non-reference blocks */
    switch (typesize) {
      case 1:
        for (i = 0; i < nbytes; i++) {
          dest[i] ^= dref[i];
        }
        break;
      case 2:
        for (i = 0; i < nbytes / 2; i++) {
          ((uint16_t *)dest)[i] ^= ((uint16_t *)dref)[i];
        }
        break;
      case 4:
        for (i = 0; i < nbytes / 4; i++) {
          ((uint32_t *)dest)[i] ^= ((uint32_t *)dref)[i];
        }
        break;
      case 8:
        for (i = 0; i < nbytes / 8; i++) {
          ((uint64_t *)dest)[i] ^= ((uint64_t *)dref)[i];
        }
        break;
      default:
        if ((typesize % 8) == 0) {
          delta_decoder(dref, offset, nbytes, 8, dest);
        } else {
          delta_decoder(dref, offset, nbytes, 1, dest);
        }
    }
  }
}

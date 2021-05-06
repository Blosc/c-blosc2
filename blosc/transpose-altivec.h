/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc developers <blosc@blosc.org> and Jerome Kieffer <jerome.kieffer@esrf.fr>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_TRANSPOSE_ALTIVEC_H
#define BLOSC_TRANSPOSE_ALTIVEC_H

#ifdef __cplusplus
extern "C" {
#endif

static const __vector uint8_t even = (const __vector uint8_t) {
  0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
  0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e};

static const __vector uint8_t odd = (const __vector uint8_t) {
  0x01, 0x03, 0x05, 0x07, 0x09, 0x0b, 0x0d, 0x0f,
  0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f};


/* Transpose inplace 2 vectors of 16 bytes in src into dst. */
static void transpose2x16(__vector uint8_t *xmm0) {
  __vector uint8_t xmm1[2];
  xmm1[0] = vec_perm(xmm0[0], xmm0[1], even);
  xmm1[1] = vec_perm(xmm0[0], xmm0[1], odd);

  for (int i = 0; i < 2; i++) {
    xmm0[i] = xmm1[i];
  }
}


/* Transpose inplace 4 vectors of 16 bytes in src into dst.
 * Total cost: 8 calls to vec_perm. */
static void transpose4x16(__vector uint8_t *xmm0) {
  __vector uint8_t xmm1[4];

  /* Transpose vectors 0-1*/
  xmm1[0] = vec_perm(xmm0[0], xmm0[1], even);
  xmm1[1] = vec_perm(xmm0[0], xmm0[1], odd);
  xmm1[2] = vec_perm(xmm0[2], xmm0[3], even);
  xmm1[3] = vec_perm(xmm0[2], xmm0[3], odd);
  /* Transpose vectors 0-2*/
  xmm0[0] = vec_perm(xmm1[0], xmm1[2], even);
  xmm0[1] = vec_perm(xmm1[1], xmm1[3], even);
  xmm0[2] = vec_perm(xmm1[0], xmm1[2], odd);
  xmm0[3] = vec_perm(xmm1[1], xmm1[3], odd);
}


/* Transpose inplace 8 vectors of 16 bytes in src into dst.
 * Total cost: 24 calls to vec_perm. */
static void transpose8x16(__vector uint8_t *xmm0) {
  __vector uint8_t xmm1[8];

  /* Transpose vectors 0-1*/
  for (int i = 0; i < 8; i += 2){
    xmm1[i] = vec_perm(xmm0[i], xmm0[i+1], even);
    xmm1[i+1] = vec_perm(xmm0[i], xmm0[i+1], odd);
  }
  /* Transpose vectors 0-2*/
  for (int i = 0; i < 8; i += 4){
    for (int k = 0; k < 2; k++){
      xmm0[i+k] = vec_perm(xmm1[i+k], xmm1[i+k+2], even);
      xmm0[i+k+2] = vec_perm(xmm1[i+k], xmm1[i+k+2], odd);
    }
  }
  /* Transpose vectors 0-4*/
  for (int k = 0; k < 4; k++){
    xmm1[k] = vec_perm(xmm0[k], xmm0[k+4], even);
    xmm1[k+4] = vec_perm(xmm0[k], xmm0[k+4], odd);
  }

  for (int i = 0; i < 8; i++) {
    xmm0[i] = xmm1[i];
  }
}


/* Transpose inplace 16 vectors of 16 bytes in src into dst.
 * Total cost: 64 calls to vec_perm. */
static void transpose16x16(__vector uint8_t * xmm0){
  __vector uint8_t xmm1[16];
  /* Transpose vectors 0-1*/
  for (int i = 0; i < 16; i += 2){
    xmm1[i] = vec_perm(xmm0[i], xmm0[i+1], even);
    xmm1[i+1] = vec_perm(xmm0[i], xmm0[i+1], odd);
  }
  /* Transpose vectors 0-2*/
  for (int i = 0; i < 16; i += 4){
    for (int k = 0; k < 2; k++){
      xmm0[i+k] = vec_perm(xmm1[i+k], xmm1[i+k+2], even);
      xmm0[i+k+2] = vec_perm(xmm1[i+k], xmm1[i+k+2], odd);
    }
  }
  /* Transpose vectors 0-4*/
  for (int i = 0; i < 16; i += 8){
    for (int k = 0; k < 4; k++){
      xmm1[i+k] = vec_perm(xmm0[i+k], xmm0[i+k+4], even);
      xmm1[i+k+4] = vec_perm(xmm0[i+k], xmm0[i+k+4], odd);
    }
  }
  /* Transpose vectors 0-8*/
  for (int k = 0; k < 8; k++){
    xmm0[k] = vec_perm(xmm1[k], xmm1[k+8], even);
    xmm0[k+8] = vec_perm(xmm1[k], xmm1[k+8], odd);
  }
}


#ifdef __cplusplus
}
#endif

#endif //BLOSC_TRANSPOSE_ALTIVEC_H

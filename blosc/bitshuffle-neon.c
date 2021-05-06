/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "bitshuffle-generic.h"
#include "bitshuffle-neon.h"

/* Make sure NEON is available for the compilation target and compiler. */
#if !defined(__ARM_NEON)
  #error NEON is not supported by the target architecture/platform and/or this compiler.
#endif

#include <arm_neon.h>

/* The next is useful for debugging purposes */
#if 0
#include <stdio.h>
#include <string.h>

static void printmem(uint8_t* buf)
{
  printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\n",
          buf[0], buf[1], buf[2], buf[3],
          buf[4], buf[5], buf[6], buf[7],
          buf[8], buf[9], buf[10], buf[11],
          buf[12], buf[13], buf[14], buf[15]);
}
#endif

/* Routine optimized for bit-shuffling a buffer for a type size of 1 byte. */
static void
bitshuffle1_neon(void* src, void* dest, const size_t size, const size_t elem_size) {

  uint16x8_t x0;
  size_t i, j, k;
  uint8x8_t lo_x, hi_x, lo, hi;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 16, k++) {
    /* Load 16-byte groups */
    x0 = vld1q_u8(src + k * 16);
    /* Split in 8-bytes grops */
    lo_x = vget_low_u8(x0);
    hi_x = vget_high_u8(x0);
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      lo = vand_u8(lo_x, mask_and);
      lo = vshl_u8(lo, mask_shift);
      hi = vand_u8(hi_x, mask_and);
      hi = vshl_u8(hi, mask_shift);
      lo = vpadd_u8(lo, lo);
      lo = vpadd_u8(lo, lo);
      lo = vpadd_u8(lo, lo);
      hi = vpadd_u8(hi, hi);
      hi = vpadd_u8(hi, hi);
      hi = vpadd_u8(hi, hi);
      /* Shift packed 8-bit */
      lo_x = vshr_n_u8(lo_x, 1);
      hi_x = vshr_n_u8(hi_x, 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size), lo, 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size), hi, 0);
    }
  }
}

/* Routine optimized for bit-shuffling a buffer for a type size of 2 bytes. */
static void
bitshuffle2_neon(void* src, void* dest, const size_t size, const size_t elem_size) {

  uint8x16x2_t x0;
  size_t i, j, k;
  uint8x8_t lo_x[2], hi_x[2], lo[2], hi[2];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 32, k++) {
    /* Load 32-byte groups */
    x0 = vld2q_u8(src + i);
    /* Split in 8-bytes grops */
    lo_x[0] = vget_low_u8(x0.val[0]);
    hi_x[0] = vget_high_u8(x0.val[0]);
    lo_x[1] = vget_low_u8(x0.val[1]);
    hi_x[1] = vget_high_u8(x0.val[1]);
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      lo[0] = vand_u8(lo_x[0], mask_and);
      lo[0] = vshl_u8(lo[0], mask_shift);
      lo[1] = vand_u8(lo_x[1], mask_and);
      lo[1] = vshl_u8(lo[1], mask_shift);

      hi[0] = vand_u8(hi_x[0], mask_and);
      hi[0] = vshl_u8(hi[0], mask_shift);
      hi[1] = vand_u8(hi_x[1], mask_and);
      hi[1] = vshl_u8(hi[1], mask_shift);

      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);

      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      /* Shift packed 8-bit */
      lo_x[0] = vshr_n_u8(lo_x[0], 1);
      hi_x[0] = vshr_n_u8(hi_x[0], 1);
      lo_x[1] = vshr_n_u8(lo_x[1], 1);
      hi_x[1] = vshr_n_u8(hi_x[1], 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size), lo[0], 0);
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size) + size * elem_size / 2, lo[1], 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size), hi[0], 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size) + size * elem_size / 2, hi[1], 0);
    }
  }
}

/* Routine optimized for bit-shuffling a buffer for a type size of 4 bytes. */
static void
bitshuffle4_neon(void* src, void* dest, const size_t size, const size_t elem_size) {
  uint8x16x4_t x0;
  size_t i, j, k;
  uint8x8_t lo_x[4], hi_x[4], lo[4], hi[4];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 64, k++) {
    /* Load 64-byte groups */
    x0 = vld4q_u8(src + i);
    /* Split in 8-bytes grops */
    lo_x[0] = vget_low_u8(x0.val[0]);
    hi_x[0] = vget_high_u8(x0.val[0]);
    lo_x[1] = vget_low_u8(x0.val[1]);
    hi_x[1] = vget_high_u8(x0.val[1]);
    lo_x[2] = vget_low_u8(x0.val[2]);
    hi_x[2] = vget_high_u8(x0.val[2]);
    lo_x[3] = vget_low_u8(x0.val[3]);
    hi_x[3] = vget_high_u8(x0.val[3]);
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      lo[0] = vand_u8(lo_x[0], mask_and);
      lo[0] = vshl_u8(lo[0], mask_shift);
      lo[1] = vand_u8(lo_x[1], mask_and);
      lo[1] = vshl_u8(lo[1], mask_shift);
      lo[2] = vand_u8(lo_x[2], mask_and);
      lo[2] = vshl_u8(lo[2], mask_shift);
      lo[3] = vand_u8(lo_x[3], mask_and);
      lo[3] = vshl_u8(lo[3], mask_shift);

      hi[0] = vand_u8(hi_x[0], mask_and);
      hi[0] = vshl_u8(hi[0], mask_shift);
      hi[1] = vand_u8(hi_x[1], mask_and);
      hi[1] = vshl_u8(hi[1], mask_shift);
      hi[2] = vand_u8(hi_x[2], mask_and);
      hi[2] = vshl_u8(hi[2], mask_shift);
      hi[3] = vand_u8(hi_x[3], mask_and);
      hi[3] = vshl_u8(hi[3], mask_shift);

      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[2] = vpadd_u8(lo[2], lo[2]);
      lo[2] = vpadd_u8(lo[2], lo[2]);
      lo[2] = vpadd_u8(lo[2], lo[2]);
      lo[3] = vpadd_u8(lo[3], lo[3]);
      lo[3] = vpadd_u8(lo[3], lo[3]);
      lo[3] = vpadd_u8(lo[3], lo[3]);

      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[2] = vpadd_u8(hi[2], hi[2]);
      hi[2] = vpadd_u8(hi[2], hi[2]);
      hi[2] = vpadd_u8(hi[2], hi[2]);
      hi[3] = vpadd_u8(hi[3], hi[3]);
      hi[3] = vpadd_u8(hi[3], hi[3]);
      hi[3] = vpadd_u8(hi[3], hi[3]);
      /* Shift packed 8-bit */
      lo_x[0] = vshr_n_u8(lo_x[0], 1);
      hi_x[0] = vshr_n_u8(hi_x[0], 1);
      lo_x[1] = vshr_n_u8(lo_x[1], 1);
      hi_x[1] = vshr_n_u8(hi_x[1], 1);
      lo_x[2] = vshr_n_u8(lo_x[2], 1);
      hi_x[2] = vshr_n_u8(hi_x[2], 1);
      lo_x[3] = vshr_n_u8(lo_x[3], 1);
      hi_x[3] = vshr_n_u8(hi_x[3], 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 4, lo[0], 0);
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 4, lo[1], 0);
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 4, lo[2], 0);
      vst1_lane_u8(dest + 2 * k + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 4, lo[3], 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 4, hi[0], 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 4, hi[1], 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 4, hi[2], 0);
      vst1_lane_u8(dest + 2 * k + 1 + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 4, hi[3], 0);
    }
  }
}

/* Routine optimized for bit-shuffling a buffer for a type size of 8 bytes. */
static void
bitshuffle8_neon(void* src, void* dest, const size_t size, const size_t elem_size) {

  size_t i, j, k;
  uint8x8x2_t r0[4];
  uint16x4x2_t r1[4];
  uint32x2x2_t r2[4];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 64, k++) {
    /* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
    r0[0] = vzip_u8(vld1_u8(src + i + 0 * 8), vld1_u8(src + i + 1 * 8));
    r0[1] = vzip_u8(vld1_u8(src + i + 2 * 8), vld1_u8(src + i + 3 * 8));
    r0[2] = vzip_u8(vld1_u8(src + i + 4 * 8), vld1_u8(src + i + 5 * 8));
    r0[3] = vzip_u8(vld1_u8(src + i + 6 * 8), vld1_u8(src + i + 7 * 8));
    /* Interleave 16 bytes */
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    /* Interleave 32 bytes */
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      r0[0].val[0] = vand_u8(vreinterpret_u8_u32(r2[0].val[0]), mask_and);
      r0[0].val[0] = vshl_u8(r0[0].val[0], mask_shift);
      r0[0].val[1] = vand_u8(vreinterpret_u8_u32(r2[0].val[1]), mask_and);
      r0[0].val[1] = vshl_u8(r0[0].val[1], mask_shift);
      r0[1].val[0] = vand_u8(vreinterpret_u8_u32(r2[1].val[0]), mask_and);
      r0[1].val[0] = vshl_u8(r0[1].val[0], mask_shift);
      r0[1].val[1] = vand_u8(vreinterpret_u8_u32(r2[1].val[1]), mask_and);
      r0[1].val[1] = vshl_u8(r0[1].val[1], mask_shift);
      r0[2].val[0] = vand_u8(vreinterpret_u8_u32(r2[2].val[0]), mask_and);
      r0[2].val[0] = vshl_u8(r0[2].val[0], mask_shift);
      r0[2].val[1] = vand_u8(vreinterpret_u8_u32(r2[2].val[1]), mask_and);
      r0[2].val[1] = vshl_u8(r0[2].val[1], mask_shift);
      r0[3].val[0] = vand_u8(vreinterpret_u8_u32(r2[3].val[0]), mask_and);
      r0[3].val[0] = vshl_u8(r0[3].val[0], mask_shift);
      r0[3].val[1] = vand_u8(vreinterpret_u8_u32(r2[3].val[1]), mask_and);
      r0[3].val[1] = vshl_u8(r0[3].val[1], mask_shift);

      r0[0].val[0] = vpadd_u8(r0[0].val[0], r0[0].val[0]);
      r0[0].val[0] = vpadd_u8(r0[0].val[0], r0[0].val[0]);
      r0[0].val[0] = vpadd_u8(r0[0].val[0], r0[0].val[0]);
      r0[0].val[1] = vpadd_u8(r0[0].val[1], r0[0].val[1]);
      r0[0].val[1] = vpadd_u8(r0[0].val[1], r0[0].val[1]);
      r0[0].val[1] = vpadd_u8(r0[0].val[1], r0[0].val[1]);
      r0[1].val[0] = vpadd_u8(r0[1].val[0], r0[1].val[0]);
      r0[1].val[0] = vpadd_u8(r0[1].val[0], r0[1].val[0]);
      r0[1].val[0] = vpadd_u8(r0[1].val[0], r0[1].val[0]);
      r0[1].val[1] = vpadd_u8(r0[1].val[1], r0[1].val[1]);
      r0[1].val[1] = vpadd_u8(r0[1].val[1], r0[1].val[1]);
      r0[1].val[1] = vpadd_u8(r0[1].val[1], r0[1].val[1]);
      r0[2].val[0] = vpadd_u8(r0[2].val[0], r0[2].val[0]);
      r0[2].val[0] = vpadd_u8(r0[2].val[0], r0[2].val[0]);
      r0[2].val[0] = vpadd_u8(r0[2].val[0], r0[2].val[0]);
      r0[2].val[1] = vpadd_u8(r0[2].val[1], r0[2].val[1]);
      r0[2].val[1] = vpadd_u8(r0[2].val[1], r0[2].val[1]);
      r0[2].val[1] = vpadd_u8(r0[2].val[1], r0[2].val[1]);
      r0[3].val[0] = vpadd_u8(r0[3].val[0], r0[3].val[0]);
      r0[3].val[0] = vpadd_u8(r0[3].val[0], r0[3].val[0]);
      r0[3].val[0] = vpadd_u8(r0[3].val[0], r0[3].val[0]);
      r0[3].val[1] = vpadd_u8(r0[3].val[1], r0[3].val[1]);
      r0[3].val[1] = vpadd_u8(r0[3].val[1], r0[3].val[1]);
      r0[3].val[1] = vpadd_u8(r0[3].val[1], r0[3].val[1]);
      /* Shift packed 8-bit */
      r2[0].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[0].val[0]), 1));
      r2[0].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[0].val[1]), 1));
      r2[1].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[1].val[0]), 1));
      r2[1].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[1].val[1]), 1));
      r2[2].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[2].val[0]), 1));
      r2[2].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[2].val[1]), 1));
      r2[3].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[3].val[0]), 1));
      r2[3].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[3].val[1]), 1));
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 8, r0[0].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 8, r0[0].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 8, r0[1].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 8, r0[1].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 4 * size * elem_size / 8, r0[2].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 5 * size * elem_size / 8, r0[2].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 6 * size * elem_size / 8, r0[3].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 7 * size * elem_size / 8, r0[3].val[1], 0);
    }
  }
}

/* Routine optimized for bit-shuffling a buffer for a type size of 16 bytes. */
static void
bitshuffle16_neon(void* src, void* dest, const size_t size, const size_t elem_size) {

  size_t i, j, k;
  uint8x8x2_t r0[8];
  uint16x4x2_t r1[8];
  uint32x2x2_t r2[8];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 128, k++) {
    /* Load and interleave groups of 16 bytes (128 bytes) to the structure r0 */
    r0[0] = vzip_u8(vld1_u8(src + i + 0 * 8), vld1_u8(src + i + 2 * 8));
    r0[1] = vzip_u8(vld1_u8(src + i + 1 * 8), vld1_u8(src + i + 3 * 8));
    r0[2] = vzip_u8(vld1_u8(src + i + 4 * 8), vld1_u8(src + i + 6 * 8));
    r0[3] = vzip_u8(vld1_u8(src + i + 5 * 8), vld1_u8(src + i + 7 * 8));
    r0[4] = vzip_u8(vld1_u8(src + i + 8 * 8), vld1_u8(src + i + 10 * 8));
    r0[5] = vzip_u8(vld1_u8(src + i + 9 * 8), vld1_u8(src + i + 11 * 8));
    r0[6] = vzip_u8(vld1_u8(src + i + 12 * 8), vld1_u8(src + i + 14 * 8));
    r0[7] = vzip_u8(vld1_u8(src + i + 13 * 8), vld1_u8(src + i + 15 * 8));
    /* Interleave 16 bytes */
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[2].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[2].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[1].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[1].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    r1[4] = vzip_u16(vreinterpret_u16_u8(r0[4].val[0]), vreinterpret_u16_u8(r0[6].val[0]));
    r1[5] = vzip_u16(vreinterpret_u16_u8(r0[4].val[1]), vreinterpret_u16_u8(r0[6].val[1]));
    r1[6] = vzip_u16(vreinterpret_u16_u8(r0[5].val[0]), vreinterpret_u16_u8(r0[7].val[0]));
    r1[7] = vzip_u16(vreinterpret_u16_u8(r0[5].val[1]), vreinterpret_u16_u8(r0[7].val[1]));
    /* Interleave 32 bytes */
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[4].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[4].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[5].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[5].val[1]));
    r2[4] = vzip_u32(vreinterpret_u32_u16(r1[2].val[0]), vreinterpret_u32_u16(r1[6].val[0]));
    r2[5] = vzip_u32(vreinterpret_u32_u16(r1[2].val[1]), vreinterpret_u32_u16(r1[6].val[1]));
    r2[6] = vzip_u32(vreinterpret_u32_u16(r1[3].val[0]), vreinterpret_u32_u16(r1[7].val[0]));
    r2[7] = vzip_u32(vreinterpret_u32_u16(r1[3].val[1]), vreinterpret_u32_u16(r1[7].val[1]));
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      r0[0].val[0] = vand_u8(vreinterpret_u8_u32(r2[0].val[0]), mask_and);
      r0[0].val[0] = vshl_u8(r0[0].val[0], mask_shift);
      r0[0].val[1] = vand_u8(vreinterpret_u8_u32(r2[0].val[1]), mask_and);
      r0[0].val[1] = vshl_u8(r0[0].val[1], mask_shift);
      r0[1].val[0] = vand_u8(vreinterpret_u8_u32(r2[1].val[0]), mask_and);
      r0[1].val[0] = vshl_u8(r0[1].val[0], mask_shift);
      r0[1].val[1] = vand_u8(vreinterpret_u8_u32(r2[1].val[1]), mask_and);
      r0[1].val[1] = vshl_u8(r0[1].val[1], mask_shift);
      r0[2].val[0] = vand_u8(vreinterpret_u8_u32(r2[2].val[0]), mask_and);
      r0[2].val[0] = vshl_u8(r0[2].val[0], mask_shift);
      r0[2].val[1] = vand_u8(vreinterpret_u8_u32(r2[2].val[1]), mask_and);
      r0[2].val[1] = vshl_u8(r0[2].val[1], mask_shift);
      r0[3].val[0] = vand_u8(vreinterpret_u8_u32(r2[3].val[0]), mask_and);
      r0[3].val[0] = vshl_u8(r0[3].val[0], mask_shift);
      r0[3].val[1] = vand_u8(vreinterpret_u8_u32(r2[3].val[1]), mask_and);
      r0[3].val[1] = vshl_u8(r0[3].val[1], mask_shift);
      r0[4].val[0] = vand_u8(vreinterpret_u8_u32(r2[4].val[0]), mask_and);
      r0[4].val[0] = vshl_u8(r0[4].val[0], mask_shift);
      r0[4].val[1] = vand_u8(vreinterpret_u8_u32(r2[4].val[1]), mask_and);
      r0[4].val[1] = vshl_u8(r0[4].val[1], mask_shift);
      r0[5].val[0] = vand_u8(vreinterpret_u8_u32(r2[5].val[0]), mask_and);
      r0[5].val[0] = vshl_u8(r0[5].val[0], mask_shift);
      r0[5].val[1] = vand_u8(vreinterpret_u8_u32(r2[5].val[1]), mask_and);
      r0[5].val[1] = vshl_u8(r0[5].val[1], mask_shift);
      r0[6].val[0] = vand_u8(vreinterpret_u8_u32(r2[6].val[0]), mask_and);
      r0[6].val[0] = vshl_u8(r0[6].val[0], mask_shift);
      r0[6].val[1] = vand_u8(vreinterpret_u8_u32(r2[6].val[1]), mask_and);
      r0[6].val[1] = vshl_u8(r0[6].val[1], mask_shift);
      r0[7].val[0] = vand_u8(vreinterpret_u8_u32(r2[7].val[0]), mask_and);
      r0[7].val[0] = vshl_u8(r0[7].val[0], mask_shift);
      r0[7].val[1] = vand_u8(vreinterpret_u8_u32(r2[7].val[1]), mask_and);
      r0[7].val[1] = vshl_u8(r0[7].val[1], mask_shift);

      r0[0].val[0] = vpadd_u8(r0[0].val[0], r0[0].val[0]);
      r0[0].val[0] = vpadd_u8(r0[0].val[0], r0[0].val[0]);
      r0[0].val[0] = vpadd_u8(r0[0].val[0], r0[0].val[0]);
      r0[0].val[1] = vpadd_u8(r0[0].val[1], r0[0].val[1]);
      r0[0].val[1] = vpadd_u8(r0[0].val[1], r0[0].val[1]);
      r0[0].val[1] = vpadd_u8(r0[0].val[1], r0[0].val[1]);
      r0[1].val[0] = vpadd_u8(r0[1].val[0], r0[1].val[0]);
      r0[1].val[0] = vpadd_u8(r0[1].val[0], r0[1].val[0]);
      r0[1].val[0] = vpadd_u8(r0[1].val[0], r0[1].val[0]);
      r0[1].val[1] = vpadd_u8(r0[1].val[1], r0[1].val[1]);
      r0[1].val[1] = vpadd_u8(r0[1].val[1], r0[1].val[1]);
      r0[1].val[1] = vpadd_u8(r0[1].val[1], r0[1].val[1]);
      r0[2].val[0] = vpadd_u8(r0[2].val[0], r0[2].val[0]);
      r0[2].val[0] = vpadd_u8(r0[2].val[0], r0[2].val[0]);
      r0[2].val[0] = vpadd_u8(r0[2].val[0], r0[2].val[0]);
      r0[2].val[1] = vpadd_u8(r0[2].val[1], r0[2].val[1]);
      r0[2].val[1] = vpadd_u8(r0[2].val[1], r0[2].val[1]);
      r0[2].val[1] = vpadd_u8(r0[2].val[1], r0[2].val[1]);
      r0[3].val[0] = vpadd_u8(r0[3].val[0], r0[3].val[0]);
      r0[3].val[0] = vpadd_u8(r0[3].val[0], r0[3].val[0]);
      r0[3].val[0] = vpadd_u8(r0[3].val[0], r0[3].val[0]);
      r0[3].val[1] = vpadd_u8(r0[3].val[1], r0[3].val[1]);
      r0[3].val[1] = vpadd_u8(r0[3].val[1], r0[3].val[1]);
      r0[3].val[1] = vpadd_u8(r0[3].val[1], r0[3].val[1]);
      r0[4].val[0] = vpadd_u8(r0[4].val[0], r0[4].val[0]);
      r0[4].val[0] = vpadd_u8(r0[4].val[0], r0[4].val[0]);
      r0[4].val[0] = vpadd_u8(r0[4].val[0], r0[4].val[0]);
      r0[4].val[1] = vpadd_u8(r0[4].val[1], r0[4].val[1]);
      r0[4].val[1] = vpadd_u8(r0[4].val[1], r0[4].val[1]);
      r0[4].val[1] = vpadd_u8(r0[4].val[1], r0[4].val[1]);
      r0[5].val[0] = vpadd_u8(r0[5].val[0], r0[5].val[0]);
      r0[5].val[0] = vpadd_u8(r0[5].val[0], r0[5].val[0]);
      r0[5].val[0] = vpadd_u8(r0[5].val[0], r0[5].val[0]);
      r0[5].val[1] = vpadd_u8(r0[5].val[1], r0[5].val[1]);
      r0[5].val[1] = vpadd_u8(r0[5].val[1], r0[5].val[1]);
      r0[5].val[1] = vpadd_u8(r0[5].val[1], r0[5].val[1]);
      r0[6].val[0] = vpadd_u8(r0[6].val[0], r0[6].val[0]);
      r0[6].val[0] = vpadd_u8(r0[6].val[0], r0[6].val[0]);
      r0[6].val[0] = vpadd_u8(r0[6].val[0], r0[6].val[0]);
      r0[6].val[1] = vpadd_u8(r0[6].val[1], r0[6].val[1]);
      r0[6].val[1] = vpadd_u8(r0[6].val[1], r0[6].val[1]);
      r0[6].val[1] = vpadd_u8(r0[6].val[1], r0[6].val[1]);
      r0[7].val[0] = vpadd_u8(r0[7].val[0], r0[7].val[0]);
      r0[7].val[0] = vpadd_u8(r0[7].val[0], r0[7].val[0]);
      r0[7].val[0] = vpadd_u8(r0[7].val[0], r0[7].val[0]);
      r0[7].val[1] = vpadd_u8(r0[7].val[1], r0[7].val[1]);
      r0[7].val[1] = vpadd_u8(r0[7].val[1], r0[7].val[1]);
      r0[7].val[1] = vpadd_u8(r0[7].val[1], r0[7].val[1]);
      /* Shift packed 8-bit */
      r2[0].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[0].val[0]), 1));
      r2[0].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[0].val[1]), 1));
      r2[1].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[1].val[0]), 1));
      r2[1].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[1].val[1]), 1));
      r2[2].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[2].val[0]), 1));
      r2[2].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[2].val[1]), 1));
      r2[3].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[3].val[0]), 1));
      r2[3].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[3].val[1]), 1));
      r2[4].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[4].val[0]), 1));
      r2[4].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[4].val[1]), 1));
      r2[5].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[5].val[0]), 1));
      r2[5].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[5].val[1]), 1));
      r2[6].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[6].val[0]), 1));
      r2[6].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[6].val[1]), 1));
      r2[7].val[0] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[7].val[0]), 1));
      r2[7].val[1] = vreinterpret_u8_u32(vshr_n_u8(vreinterpret_u8_u32(r2[7].val[1]), 1));
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 16, r0[0].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 16, r0[0].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 16, r0[1].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 16, r0[1].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 4 * size * elem_size / 16, r0[2].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 5 * size * elem_size / 16, r0[2].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 6 * size * elem_size / 16, r0[3].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 7 * size * elem_size / 16, r0[3].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 8 * size * elem_size / 16, r0[4].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 9 * size * elem_size / 16, r0[4].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 10 * size * elem_size / 16, r0[5].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 11 * size * elem_size / 16, r0[5].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 12 * size * elem_size / 16, r0[6].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 13 * size * elem_size / 16, r0[6].val[1], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 14 * size * elem_size / 16, r0[7].val[0], 0);
      vst1_lane_u8(dest + k + j * size * elem_size / (8 * elem_size) + 15 * size * elem_size / 16, r0[7].val[1], 0);
    }
  }
}

/* Routine optimized for bit-unshuffling a buffer for a type size of 1 byte. */
static void
bitunshuffle1_neon(void* _src, void* dest, const size_t size, const size_t elem_size) {

  uint8x8_t lo_x, hi_x, lo, hi;
  size_t i, j, k;
  uint8_t* src = _src;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 16, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      lo_x[j] = src[2 * k + 0 + j * size * elem_size / (8 * elem_size)];
      hi_x[j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size)];
    }
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      lo = vand_u8(lo_x, mask_and);
      lo = vshl_u8(lo, mask_shift);
      hi = vand_u8(hi_x, mask_and);
      hi = vshl_u8(hi, mask_shift);
      lo = vpadd_u8(lo, lo);
      lo = vpadd_u8(lo, lo);
      lo = vpadd_u8(lo, lo);
      hi = vpadd_u8(hi, hi);
      hi = vpadd_u8(hi, hi);
      hi = vpadd_u8(hi, hi);
      /* Shift packed 8-bit */
      lo_x = vshr_n_u8(lo_x, 1);
      hi_x = vshr_n_u8(hi_x, 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + j + i, lo, 0);
      vst1_lane_u8(dest + j + i + 8, hi, 0);
    }
  }
}

/* Routine optimized for bit-unshuffling a buffer for a type size of 2 byte. */
static void
bitunshuffle2_neon(void* _src, void* dest, const size_t size, const size_t elem_size) {

  size_t i, j, k;
  uint8x8_t lo_x[2], hi_x[2], lo[2], hi[2];
  uint8_t* src = _src;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 32, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      lo_x[0][j] = src[2 * k + j * size * elem_size / (8 * elem_size)];
      lo_x[1][j] = src[2 * k + j * size * elem_size / (8 * elem_size) + size * elem_size / 2];
      hi_x[0][j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size)];
      hi_x[1][j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size) + size * elem_size / 2];
    }
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      lo[0] = vand_u8(lo_x[0], mask_and);
      lo[0] = vshl_u8(lo[0], mask_shift);
      lo[1] = vand_u8(lo_x[1], mask_and);
      lo[1] = vshl_u8(lo[1], mask_shift);

      hi[0] = vand_u8(hi_x[0], mask_and);
      hi[0] = vshl_u8(hi[0], mask_shift);
      hi[1] = vand_u8(hi_x[1], mask_and);
      hi[1] = vshl_u8(hi[1], mask_shift);

      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);

      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      /* Shift packed 8-bit */
      lo_x[0] = vshr_n_u8(lo_x[0], 1);
      hi_x[0] = vshr_n_u8(hi_x[0], 1);
      lo_x[1] = vshr_n_u8(lo_x[1], 1);
      hi_x[1] = vshr_n_u8(hi_x[1], 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 2 * j + i, lo[0], 0);
      vst1_lane_u8(dest + 2 * j + 1 + i, lo[1], 0);
      vst1_lane_u8(dest + 2 * j + i + 16, hi[0], 0);
      vst1_lane_u8(dest + 2 * j + 1 + i + 16, hi[1], 0);
    }
  }
}

/* Routine optimized for bit-unshuffling a buffer for a type size of 4 byte. */
static void
bitunshuffle4_neon(void* _src, void* dest, const size_t size, const size_t elem_size) {
  size_t i, j, k;
  uint8x8_t lo_x[4], hi_x[4], lo[4], hi[4];
  uint8_t* src = _src;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 64, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      lo_x[0][j] = src[2 * k + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 4];
      hi_x[0][j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 4];
      lo_x[1][j] = src[2 * k + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 4];
      hi_x[1][j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 4];
      lo_x[2][j] = src[2 * k + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 4];
      hi_x[2][j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 4];
      lo_x[3][j] = src[2 * k + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 4];
      hi_x[3][j] = src[2 * k + 1 + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 4];
    }

    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      lo[0] = vand_u8(lo_x[0], mask_and);
      lo[0] = vshl_u8(lo[0], mask_shift);
      lo[1] = vand_u8(lo_x[1], mask_and);
      lo[1] = vshl_u8(lo[1], mask_shift);
      lo[2] = vand_u8(lo_x[2], mask_and);
      lo[2] = vshl_u8(lo[2], mask_shift);
      lo[3] = vand_u8(lo_x[3], mask_and);
      lo[3] = vshl_u8(lo[3], mask_shift);

      hi[0] = vand_u8(hi_x[0], mask_and);
      hi[0] = vshl_u8(hi[0], mask_shift);
      hi[1] = vand_u8(hi_x[1], mask_and);
      hi[1] = vshl_u8(hi[1], mask_shift);
      hi[2] = vand_u8(hi_x[2], mask_and);
      hi[2] = vshl_u8(hi[2], mask_shift);
      hi[3] = vand_u8(hi_x[3], mask_and);
      hi[3] = vshl_u8(hi[3], mask_shift);

      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[0] = vpadd_u8(lo[0], lo[0]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[1] = vpadd_u8(lo[1], lo[1]);
      lo[2] = vpadd_u8(lo[2], lo[2]);
      lo[2] = vpadd_u8(lo[2], lo[2]);
      lo[2] = vpadd_u8(lo[2], lo[2]);
      lo[3] = vpadd_u8(lo[3], lo[3]);
      lo[3] = vpadd_u8(lo[3], lo[3]);
      lo[3] = vpadd_u8(lo[3], lo[3]);

      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[2] = vpadd_u8(hi[2], hi[2]);
      hi[2] = vpadd_u8(hi[2], hi[2]);
      hi[2] = vpadd_u8(hi[2], hi[2]);
      hi[3] = vpadd_u8(hi[3], hi[3]);
      hi[3] = vpadd_u8(hi[3], hi[3]);
      hi[3] = vpadd_u8(hi[3], hi[3]);
      /* Shift packed 8-bit */
      lo_x[0] = vshr_n_u8(lo_x[0], 1);
      hi_x[0] = vshr_n_u8(hi_x[0], 1);
      lo_x[1] = vshr_n_u8(lo_x[1], 1);
      hi_x[1] = vshr_n_u8(hi_x[1], 1);
      lo_x[2] = vshr_n_u8(lo_x[2], 1);
      hi_x[2] = vshr_n_u8(hi_x[2], 1);
      lo_x[3] = vshr_n_u8(lo_x[3], 1);
      hi_x[3] = vshr_n_u8(hi_x[3], 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 4 * j + i, lo[0], 0);
      vst1_lane_u8(dest + 4 * j + 1 + i, lo[1], 0);
      vst1_lane_u8(dest + 4 * j + 2 + i, lo[2], 0);
      vst1_lane_u8(dest + 4 * j + 3 + i, lo[3], 0);
      vst1_lane_u8(dest + 4 * j + i + 32, hi[0], 0);
      vst1_lane_u8(dest + 4 * j + 1 + i + 32, hi[1], 0);
      vst1_lane_u8(dest + 4 * j + 2 + i + 32, hi[2], 0);
      vst1_lane_u8(dest + 4 * j + 3 + i + 32, hi[3], 0);
    }
  }
}

/* Routine optimized for bit-unshuffling a buffer for a type size of 8 byte. */
static void
bitunshuffle8_neon(void* _src, void* dest, const size_t size, const size_t elem_size) {

  size_t i, j, k;
  uint8x8x2_t r0[4], r1[4];
  uint8_t* src = _src;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 64, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      r0[0].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 8];
      r0[0].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 8];
      r0[1].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 8];
      r0[1].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 8];
      r0[2].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 4 * size * elem_size / 8];
      r0[2].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 5 * size * elem_size / 8];
      r0[3].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 6 * size * elem_size / 8];
      r0[3].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 7 * size * elem_size / 8];
    }
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      r1[0].val[0] = vand_u8(r0[0].val[0], mask_and);
      r1[0].val[0] = vshl_u8(r1[0].val[0], mask_shift);
      r1[0].val[1] = vand_u8(r0[0].val[1], mask_and);
      r1[0].val[1] = vshl_u8(r1[0].val[1], mask_shift);
      r1[1].val[0] = vand_u8(r0[1].val[0], mask_and);
      r1[1].val[0] = vshl_u8(r1[1].val[0], mask_shift);
      r1[1].val[1] = vand_u8(r0[1].val[1], mask_and);
      r1[1].val[1] = vshl_u8(r1[1].val[1], mask_shift);
      r1[2].val[0] = vand_u8(r0[2].val[0], mask_and);
      r1[2].val[0] = vshl_u8(r1[2].val[0], mask_shift);
      r1[2].val[1] = vand_u8(r0[2].val[1], mask_and);
      r1[2].val[1] = vshl_u8(r1[2].val[1], mask_shift);
      r1[3].val[0] = vand_u8(r0[3].val[0], mask_and);
      r1[3].val[0] = vshl_u8(r1[3].val[0], mask_shift);
      r1[3].val[1] = vand_u8(r0[3].val[1], mask_and);
      r1[3].val[1] = vshl_u8(r1[3].val[1], mask_shift);

      r1[0].val[0] = vpadd_u8(r1[0].val[0], r1[0].val[0]);
      r1[0].val[0] = vpadd_u8(r1[0].val[0], r1[0].val[0]);
      r1[0].val[0] = vpadd_u8(r1[0].val[0], r1[0].val[0]);
      r1[0].val[1] = vpadd_u8(r1[0].val[1], r1[0].val[1]);
      r1[0].val[1] = vpadd_u8(r1[0].val[1], r1[0].val[1]);
      r1[0].val[1] = vpadd_u8(r1[0].val[1], r1[0].val[1]);
      r1[1].val[0] = vpadd_u8(r1[1].val[0], r1[1].val[0]);
      r1[1].val[0] = vpadd_u8(r1[1].val[0], r1[1].val[0]);
      r1[1].val[0] = vpadd_u8(r1[1].val[0], r1[1].val[0]);
      r1[1].val[1] = vpadd_u8(r1[1].val[1], r1[1].val[1]);
      r1[1].val[1] = vpadd_u8(r1[1].val[1], r1[1].val[1]);
      r1[1].val[1] = vpadd_u8(r1[1].val[1], r1[1].val[1]);
      r1[2].val[0] = vpadd_u8(r1[2].val[0], r1[2].val[0]);
      r1[2].val[0] = vpadd_u8(r1[2].val[0], r1[2].val[0]);
      r1[2].val[0] = vpadd_u8(r1[2].val[0], r1[2].val[0]);
      r1[2].val[1] = vpadd_u8(r1[2].val[1], r1[2].val[1]);
      r1[2].val[1] = vpadd_u8(r1[2].val[1], r1[2].val[1]);
      r1[2].val[1] = vpadd_u8(r1[2].val[1], r1[2].val[1]);
      r1[3].val[0] = vpadd_u8(r1[3].val[0], r1[3].val[0]);
      r1[3].val[0] = vpadd_u8(r1[3].val[0], r1[3].val[0]);
      r1[3].val[0] = vpadd_u8(r1[3].val[0], r1[3].val[0]);
      r1[3].val[1] = vpadd_u8(r1[3].val[1], r1[3].val[1]);
      r1[3].val[1] = vpadd_u8(r1[3].val[1], r1[3].val[1]);
      r1[3].val[1] = vpadd_u8(r1[3].val[1], r1[3].val[1]);
      /* Shift packed 8-bit */
      r0[0].val[0] = vshr_n_u8(r0[0].val[0], 1);
      r0[0].val[1] = vshr_n_u8(r0[0].val[1], 1);
      r0[1].val[0] = vshr_n_u8(r0[1].val[0], 1);
      r0[1].val[1] = vshr_n_u8(r0[1].val[1], 1);
      r0[2].val[0] = vshr_n_u8(r0[2].val[0], 1);
      r0[2].val[1] = vshr_n_u8(r0[2].val[1], 1);
      r0[3].val[0] = vshr_n_u8(r0[3].val[0], 1);
      r0[3].val[1] = vshr_n_u8(r0[3].val[1], 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 8 * j + 0 + i, r1[0].val[0], 0);
      vst1_lane_u8(dest + 8 * j + 1 + i, r1[0].val[1], 0);
      vst1_lane_u8(dest + 8 * j + 2 + i, r1[1].val[0], 0);
      vst1_lane_u8(dest + 8 * j + 3 + i, r1[1].val[1], 0);
      vst1_lane_u8(dest + 8 * j + 4 + i, r1[2].val[0], 0);
      vst1_lane_u8(dest + 8 * j + 5 + i, r1[2].val[1], 0);
      vst1_lane_u8(dest + 8 * j + 6 + i, r1[3].val[0], 0);
      vst1_lane_u8(dest + 8 * j + 7 + i, r1[3].val[1], 0);
    }
  }
}

/* Routine optimized for bit-unshuffling a buffer for a type size of 16 byte. */
static void
bitunshuffle16_neon(void* _src, void* dest, const size_t size, const size_t elem_size) {

  size_t i, j, k;
  uint8x8x2_t r0[8], r1[8];
  uint8_t* src = _src;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  for (i = 0, k = 0; i < size * elem_size; i += 128, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      r0[0].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 0 * size * elem_size / 16];
      r0[0].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 1 * size * elem_size / 16];
      r0[1].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 2 * size * elem_size / 16];
      r0[1].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 3 * size * elem_size / 16];
      r0[2].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 4 * size * elem_size / 16];
      r0[2].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 5 * size * elem_size / 16];
      r0[3].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 6 * size * elem_size / 16];
      r0[3].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 7 * size * elem_size / 16];
      r0[4].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 8 * size * elem_size / 16];
      r0[4].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 9 * size * elem_size / 16];
      r0[5].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 10 * size * elem_size / 16];
      r0[5].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 11 * size * elem_size / 16];
      r0[6].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 12 * size * elem_size / 16];
      r0[6].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 13 * size * elem_size / 16];
      r0[7].val[0][j] = src[k + j * size * elem_size / (8 * elem_size) + 14 * size * elem_size / 16];
      r0[7].val[1][j] = src[k + j * size * elem_size / (8 * elem_size) + 15 * size * elem_size / 16];
    }
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
      r1[0].val[0] = vand_u8(r0[0].val[0], mask_and);
      r1[0].val[0] = vshl_u8(r1[0].val[0], mask_shift);
      r1[0].val[1] = vand_u8(r0[0].val[1], mask_and);
      r1[0].val[1] = vshl_u8(r1[0].val[1], mask_shift);
      r1[1].val[0] = vand_u8(r0[1].val[0], mask_and);
      r1[1].val[0] = vshl_u8(r1[1].val[0], mask_shift);
      r1[1].val[1] = vand_u8(r0[1].val[1], mask_and);
      r1[1].val[1] = vshl_u8(r1[1].val[1], mask_shift);
      r1[2].val[0] = vand_u8(r0[2].val[0], mask_and);
      r1[2].val[0] = vshl_u8(r1[2].val[0], mask_shift);
      r1[2].val[1] = vand_u8(r0[2].val[1], mask_and);
      r1[2].val[1] = vshl_u8(r1[2].val[1], mask_shift);
      r1[3].val[0] = vand_u8(r0[3].val[0], mask_and);
      r1[3].val[0] = vshl_u8(r1[3].val[0], mask_shift);
      r1[3].val[1] = vand_u8(r0[3].val[1], mask_and);
      r1[3].val[1] = vshl_u8(r1[3].val[1], mask_shift);
      r1[4].val[0] = vand_u8(r0[4].val[0], mask_and);
      r1[4].val[0] = vshl_u8(r1[4].val[0], mask_shift);
      r1[4].val[1] = vand_u8(r0[4].val[1], mask_and);
      r1[4].val[1] = vshl_u8(r1[4].val[1], mask_shift);
      r1[5].val[0] = vand_u8(r0[5].val[0], mask_and);
      r1[5].val[0] = vshl_u8(r1[5].val[0], mask_shift);
      r1[5].val[1] = vand_u8(r0[5].val[1], mask_and);
      r1[5].val[1] = vshl_u8(r1[5].val[1], mask_shift);
      r1[6].val[0] = vand_u8(r0[6].val[0], mask_and);
      r1[6].val[0] = vshl_u8(r1[6].val[0], mask_shift);
      r1[6].val[1] = vand_u8(r0[6].val[1], mask_and);
      r1[6].val[1] = vshl_u8(r1[6].val[1], mask_shift);
      r1[7].val[0] = vand_u8(r0[7].val[0], mask_and);
      r1[7].val[0] = vshl_u8(r1[7].val[0], mask_shift);
      r1[7].val[1] = vand_u8(r0[7].val[1], mask_and);
      r1[7].val[1] = vshl_u8(r1[7].val[1], mask_shift);

      r1[0].val[0] = vpadd_u8(r1[0].val[0], r1[0].val[0]);
      r1[0].val[0] = vpadd_u8(r1[0].val[0], r1[0].val[0]);
      r1[0].val[0] = vpadd_u8(r1[0].val[0], r1[0].val[0]);
      r1[0].val[1] = vpadd_u8(r1[0].val[1], r1[0].val[1]);
      r1[0].val[1] = vpadd_u8(r1[0].val[1], r1[0].val[1]);
      r1[0].val[1] = vpadd_u8(r1[0].val[1], r1[0].val[1]);
      r1[1].val[0] = vpadd_u8(r1[1].val[0], r1[1].val[0]);
      r1[1].val[0] = vpadd_u8(r1[1].val[0], r1[1].val[0]);
      r1[1].val[0] = vpadd_u8(r1[1].val[0], r1[1].val[0]);
      r1[1].val[1] = vpadd_u8(r1[1].val[1], r1[1].val[1]);
      r1[1].val[1] = vpadd_u8(r1[1].val[1], r1[1].val[1]);
      r1[1].val[1] = vpadd_u8(r1[1].val[1], r1[1].val[1]);
      r1[2].val[0] = vpadd_u8(r1[2].val[0], r1[2].val[0]);
      r1[2].val[0] = vpadd_u8(r1[2].val[0], r1[2].val[0]);
      r1[2].val[0] = vpadd_u8(r1[2].val[0], r1[2].val[0]);
      r1[2].val[1] = vpadd_u8(r1[2].val[1], r1[2].val[1]);
      r1[2].val[1] = vpadd_u8(r1[2].val[1], r1[2].val[1]);
      r1[2].val[1] = vpadd_u8(r1[2].val[1], r1[2].val[1]);
      r1[3].val[0] = vpadd_u8(r1[3].val[0], r1[3].val[0]);
      r1[3].val[0] = vpadd_u8(r1[3].val[0], r1[3].val[0]);
      r1[3].val[0] = vpadd_u8(r1[3].val[0], r1[3].val[0]);
      r1[3].val[1] = vpadd_u8(r1[3].val[1], r1[3].val[1]);
      r1[3].val[1] = vpadd_u8(r1[3].val[1], r1[3].val[1]);
      r1[3].val[1] = vpadd_u8(r1[3].val[1], r1[3].val[1]);
      r1[4].val[0] = vpadd_u8(r1[4].val[0], r1[4].val[0]);
      r1[4].val[0] = vpadd_u8(r1[4].val[0], r1[4].val[0]);
      r1[4].val[0] = vpadd_u8(r1[4].val[0], r1[4].val[0]);
      r1[4].val[1] = vpadd_u8(r1[4].val[1], r1[4].val[1]);
      r1[4].val[1] = vpadd_u8(r1[4].val[1], r1[4].val[1]);
      r1[4].val[1] = vpadd_u8(r1[4].val[1], r1[4].val[1]);
      r1[5].val[0] = vpadd_u8(r1[5].val[0], r1[5].val[0]);
      r1[5].val[0] = vpadd_u8(r1[5].val[0], r1[5].val[0]);
      r1[5].val[0] = vpadd_u8(r1[5].val[0], r1[5].val[0]);
      r1[5].val[1] = vpadd_u8(r1[5].val[1], r1[5].val[1]);
      r1[5].val[1] = vpadd_u8(r1[5].val[1], r1[5].val[1]);
      r1[5].val[1] = vpadd_u8(r1[5].val[1], r1[5].val[1]);
      r1[6].val[0] = vpadd_u8(r1[6].val[0], r1[6].val[0]);
      r1[6].val[0] = vpadd_u8(r1[6].val[0], r1[6].val[0]);
      r1[6].val[0] = vpadd_u8(r1[6].val[0], r1[6].val[0]);
      r1[6].val[1] = vpadd_u8(r1[6].val[1], r1[6].val[1]);
      r1[6].val[1] = vpadd_u8(r1[6].val[1], r1[6].val[1]);
      r1[6].val[1] = vpadd_u8(r1[6].val[1], r1[6].val[1]);
      r1[7].val[0] = vpadd_u8(r1[7].val[0], r1[7].val[0]);
      r1[7].val[0] = vpadd_u8(r1[7].val[0], r1[7].val[0]);
      r1[7].val[0] = vpadd_u8(r1[7].val[0], r1[7].val[0]);
      r1[7].val[1] = vpadd_u8(r1[7].val[1], r1[7].val[1]);
      r1[7].val[1] = vpadd_u8(r1[7].val[1], r1[7].val[1]);
      r1[7].val[1] = vpadd_u8(r1[7].val[1], r1[7].val[1]);
      /* Shift packed 8-bit */
      r0[0].val[0] = vshr_n_u8(r0[0].val[0], 1);
      r0[0].val[1] = vshr_n_u8(r0[0].val[1], 1);
      r0[1].val[0] = vshr_n_u8(r0[1].val[0], 1);
      r0[1].val[1] = vshr_n_u8(r0[1].val[1], 1);
      r0[2].val[0] = vshr_n_u8(r0[2].val[0], 1);
      r0[2].val[1] = vshr_n_u8(r0[2].val[1], 1);
      r0[3].val[0] = vshr_n_u8(r0[3].val[0], 1);
      r0[3].val[1] = vshr_n_u8(r0[3].val[1], 1);
      r0[4].val[0] = vshr_n_u8(r0[4].val[0], 1);
      r0[4].val[1] = vshr_n_u8(r0[4].val[1], 1);
      r0[5].val[0] = vshr_n_u8(r0[5].val[0], 1);
      r0[5].val[1] = vshr_n_u8(r0[5].val[1], 1);
      r0[6].val[0] = vshr_n_u8(r0[6].val[0], 1);
      r0[6].val[1] = vshr_n_u8(r0[6].val[1], 1);
      r0[7].val[0] = vshr_n_u8(r0[7].val[0], 1);
      r0[7].val[1] = vshr_n_u8(r0[7].val[1], 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 16 * j + 0 + i, r1[0].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 1 + i, r1[0].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 2 + i, r1[1].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 3 + i, r1[1].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 4 + i, r1[2].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 5 + i, r1[2].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 6 + i, r1[3].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 7 + i, r1[3].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 8 + i, r1[4].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 9 + i, r1[4].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 10 + i, r1[5].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 11 + i, r1[5].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 12 + i, r1[6].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 13 + i, r1[6].val[1], 0);
      vst1_lane_u8(dest + 16 * j + 14 + i, r1[7].val[0], 0);
      vst1_lane_u8(dest + 16 * j + 15 + i, r1[7].val[1], 0);
    }
  }
}

/* Shuffle a block.  This can never fail. */
int64_t
bitshuffle_neon(void* _src, void* _dest, const size_t size,
                const size_t elem_size, void* tmp_buf) {
  size_t vectorized_chunk_size = 0;
  int64_t count;
  if (elem_size == 1 || elem_size == 2 || elem_size == 4) {
    vectorized_chunk_size = elem_size * 16;
  } else if (elem_size == 8 || elem_size == 16) {
    vectorized_chunk_size = elem_size * 8;
  }

  /* If the block size is too small to be vectorized,
     use the generic implementation. */
  if (size * elem_size < vectorized_chunk_size) {
    count = bshuf_trans_bit_elem_scal((void*)_src, (void*)_dest, size, elem_size, tmp_buf);
    return count;
  }

  /* Optimized bitshuffle implementations */
  switch (elem_size) {
    case 1:
      bitshuffle1_neon(_src, _dest, size, elem_size);
      break;
    case 2:
      bitshuffle2_neon(_src, _dest, size, elem_size);
      break;
    case 4:
      bitshuffle4_neon(_src, _dest, size, elem_size);
      break;
    case 8:
      bitshuffle8_neon(_src, _dest, size, elem_size);
      break;
    case 16:
      bitshuffle16_neon(_src, _dest, size, elem_size);
      break;
    default:
      /* Non-optimized bitshuffle */
      count = bshuf_trans_bit_elem_scal((void*)_src, (void*)_dest, size, elem_size, tmp_buf);
      /* The non-optimized function covers the whole buffer,
         so we're done processing here. */
      return count;
  }

  return (int64_t)size * (int64_t)elem_size;
}

/* Bitunshuffle a block.  This can never fail. */
int64_t
bitunshuffle_neon(void* _src, void* _dest, const size_t size,
                  const size_t elem_size, void* tmp_buf) {
  size_t vectorized_chunk_size = 0;
  int64_t count;
  if (size * elem_size == 1 || size * elem_size == 2 || size * elem_size == 4) {
    vectorized_chunk_size = size * elem_size * 16;
  } else if (size * elem_size == 8 || size * elem_size == 16) {
    vectorized_chunk_size = size * elem_size * 8;
  }

  /* If the block size is too small to be vectorized,
     use the generic implementation. */
  if (size * elem_size < vectorized_chunk_size) {
    count = bshuf_untrans_bit_elem_scal((void*)_src, (void*)_dest, size, elem_size, tmp_buf);
    return count;
  }

  /* Optimized bitunshuffle implementations */
  switch (elem_size) {
    case 1:
      bitunshuffle1_neon(_src, _dest, size, elem_size);
      break;
    case 2:
      bitunshuffle2_neon(_src, _dest, size, elem_size);
      break;
    case 4:
      bitunshuffle4_neon(_src, _dest, size, elem_size);
      break;
    case 8:
      bitunshuffle8_neon(_src, _dest, size, elem_size);
      break;
    case 16:
      bitunshuffle16_neon(_src, _dest, size, elem_size);
      break;
    default:
      /* Non-optimized bitunshuffle */
      count = bshuf_untrans_bit_elem_scal((void*)_src, (void*)_dest, size, elem_size, tmp_buf);
      /* The non-optimized function covers the whole buffer,
         so we're done processing here. */
      return count;
  }
  
  return (int64_t)size * (int64_t)elem_size;
}

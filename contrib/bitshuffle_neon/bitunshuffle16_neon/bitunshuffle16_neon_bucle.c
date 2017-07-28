#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <string.h>

#define CHECK_MULT_EIGHT(n) if (n % 8) exit(0);

static void printmem8_l(uint8x8_t buf)
{
  printf("%x,%x,%x,%x,%x,%x,%x,%x\n",
          buf[0], buf[1], buf[2], buf[3],
          buf[4], buf[5], buf[6], buf[7]);
}

static void printmem8x16(uint8x16_t buf)
{
  printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
          buf[0], buf[1], buf[2], buf[3],
          buf[4], buf[5], buf[6], buf[7],
          buf[8], buf[9], buf[10], buf[11],
          buf[12], buf[13], buf[14], buf[15]);
}

static void printmem16(uint8_t* buf)
{
printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
          buf[0], buf[1], buf[2], buf[3],
          buf[4], buf[5], buf[6], buf[7],
          buf[8], buf[9], buf[10], buf[11],
          buf[12], buf[13], buf[14], buf[15]);
}

int32_t _mm_movemask_epi8_neon(uint8x16_t input)
{
    const int8_t __attribute__ ((aligned (16))) xr[8] = {-7,-6,-5,-4,-3,-2,-1,0};
    uint8x8_t mask_and = vdup_n_u8(0x80);
    int8x8_t mask_shift = vld1_s8(xr);

    uint8x8_t lo = vget_low_u8(input);
    uint8x8_t hi = vget_high_u8(input);

    lo = vand_u8(lo, mask_and);
    lo = vshl_u8(lo, mask_shift);

    hi = vand_u8(hi, mask_and);
    hi = vshl_u8(hi, mask_shift);

    lo = vpadd_u8(lo,lo);
    lo = vpadd_u8(lo,lo);
    lo = vpadd_u8(lo,lo);

    hi = vpadd_u8(hi,hi);
    hi = vpadd_u8(hi,hi);
    hi = vpadd_u8(hi,hi);

    return ((hi[0] << 8) | (lo[0] & 0xFF));
}
/* Routine optimized for bit-shuffling a buffer for a type size of 16 bytes. */
static void
bitshuffle16_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 16;
  size_t i, j, k;
  uint8x8x2_t r0[8];
  uint16x4x2_t r1[8];
  uint32x2x2_t r2[8];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 128, k++) {
    /* Load and interleave groups of 16 bytes (128 bytes) to the structure r0 */
    r0[0] = vzip_u8(vld1_u8(src + i + 0*8), vld1_u8(src + i + 2*8));
    r0[1] = vzip_u8(vld1_u8(src + i + 1*8), vld1_u8(src + i + 3*8));
    r0[2] = vzip_u8(vld1_u8(src + i + 4*8), vld1_u8(src + i + 6*8));
    r0[3] = vzip_u8(vld1_u8(src + i + 5*8), vld1_u8(src + i + 7*8));
    r0[4] = vzip_u8(vld1_u8(src + i + 8*8), vld1_u8(src + i + 10*8));
    r0[5] = vzip_u8(vld1_u8(src + i + 9*8), vld1_u8(src + i + 11*8));
    r0[6] = vzip_u8(vld1_u8(src + i + 12*8), vld1_u8(src + i + 14*8));
    r0[7] = vzip_u8(vld1_u8(src + i + 13*8), vld1_u8(src + i + 15*8));
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
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 0*nbyte/16, r0[0].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 1*nbyte/16, r0[0].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 2*nbyte/16, r0[1].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 3*nbyte/16, r0[1].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 4*nbyte/16, r0[2].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 5*nbyte/16, r0[2].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 6*nbyte/16, r0[3].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 7*nbyte/16, r0[3].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 8*nbyte/16, r0[4].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 9*nbyte/16, r0[4].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 10*nbyte/16, r0[5].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 11*nbyte/16, r0[5].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 12*nbyte/16, r0[6].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 13*nbyte/16, r0[6].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 14*nbyte/16, r0[7].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 15*nbyte/16, r0[7].val[1], 0);
    }
  }
}

static void
bitunshuffle16_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 16;
  size_t i, j, k;
  uint8x8x2_t r0[8], r1[8];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 128, k++) {
    for (j = 0; j < 8; j++) {
      r0[0].val[0][j] = src[k + j*nbyte/(8*elem_size) + 0*nbyte/16];
      r0[0].val[1][j] = src[k + j*nbyte/(8*elem_size) + 1*nbyte/16];
      r0[1].val[0][j] = src[k + j*nbyte/(8*elem_size) + 2*nbyte/16];
      r0[1].val[1][j] = src[k + j*nbyte/(8*elem_size) + 3*nbyte/16];
      r0[2].val[0][j] = src[k + j*nbyte/(8*elem_size) + 4*nbyte/16];
      r0[2].val[1][j] = src[k + j*nbyte/(8*elem_size) + 5*nbyte/16];
      r0[3].val[0][j] = src[k + j*nbyte/(8*elem_size) + 6*nbyte/16];
      r0[3].val[1][j] = src[k + j*nbyte/(8*elem_size) + 7*nbyte/16];
      r0[4].val[0][j] = src[k + j*nbyte/(8*elem_size) + 8*nbyte/16];
      r0[4].val[1][j] = src[k + j*nbyte/(8*elem_size) + 9*nbyte/16];
      r0[5].val[0][j] = src[k + j*nbyte/(8*elem_size) + 10*nbyte/16];
      r0[5].val[1][j] = src[k + j*nbyte/(8*elem_size) + 11*nbyte/16];
      r0[6].val[0][j] = src[k + j*nbyte/(8*elem_size) + 12*nbyte/16];
      r0[6].val[1][j] = src[k + j*nbyte/(8*elem_size) + 13*nbyte/16];
      r0[7].val[0][j] = src[k + j*nbyte/(8*elem_size) + 14*nbyte/16];
      r0[7].val[1][j] = src[k + j*nbyte/(8*elem_size) + 15*nbyte/16];
    }
    printf("\n");
    printmem8_l(r0[0].val[0]);
    printmem8_l(r0[0].val[1]);
    printmem8_l(r0[1].val[0]);
    printmem8_l(r0[1].val[1]);
    printmem8_l(r0[2].val[0]);
    printmem8_l(r0[2].val[1]);
    printmem8_l(r0[3].val[0]);
    printmem8_l(r0[3].val[1]);
    printmem8_l(r0[4].val[0]);
    printmem8_l(r0[4].val[1]);
    printmem8_l(r0[5].val[0]);
    printmem8_l(r0[5].val[1]);
    printmem8_l(r0[6].val[0]);
    printmem8_l(r0[6].val[1]);
    printmem8_l(r0[7].val[0]);
    printmem8_l(r0[7].val[1]);
    printf("\n");

    for (j = 0; j < 8; j++) {
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

      int s, t;
      for(s = 0; s < 8; s++) {
        for(t = 0; t < 2; t++) {
          printf("r1[%d].val[%d] = ", s, t);
          printmem8_l(r1[s].val[t]);
        }
      }
      printf("\n");
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

      vst1_lane_u8(dest + 16*j+0 + i, r1[0].val[0], 0);
      vst1_lane_u8(dest + 16*j+1 + i, r1[0].val[1], 0);
      vst1_lane_u8(dest + 16*j+2 + i, r1[1].val[0], 0);
      vst1_lane_u8(dest + 16*j+3 + i, r1[1].val[1], 0);
      vst1_lane_u8(dest + 16*j+4 + i, r1[2].val[0], 0);
      vst1_lane_u8(dest + 16*j+5 + i, r1[2].val[1], 0);
      vst1_lane_u8(dest + 16*j+6 + i, r1[3].val[0], 0);
      vst1_lane_u8(dest + 16*j+7 + i, r1[3].val[1], 0);
      vst1_lane_u8(dest + 16*j+8 + i, r1[4].val[0], 0);
      vst1_lane_u8(dest + 16*j+9 + i, r1[4].val[1], 0);
      vst1_lane_u8(dest + 16*j+10 + i, r1[5].val[0], 0);
      vst1_lane_u8(dest + 16*j+11 + i, r1[5].val[1], 0);
      vst1_lane_u8(dest + 16*j+12 + i, r1[6].val[0], 0);
      vst1_lane_u8(dest + 16*j+13 + i, r1[6].val[1], 0);
      vst1_lane_u8(dest + 16*j+14 + i, r1[7].val[0], 0);
      vst1_lane_u8(dest + 16*j+15 + i, r1[7].val[1], 0);
    }
  }
}

main()
{
  const uint8_t *aux = "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
                       "\x45\x3e\x5f\xdf\xa2\x43\x41\x25\x77\xae\xfd\x22\x19\x1a\x38\x2b"
                       "\x56\x93\xab\xc3\x61\xa8\x7d\xfc\xbb\x98\xf6\xd1\x29\xce\xe7\x58"
                       "\x73\x4c\xd3\x12\x3f\xcf\x46\x94\xba\xfa\x49\x83\x71\x1e\x35\x5f"
                       "\xbc\x2d\x3f\x7c\xf8\xb4\xb9\xa8\xc9\x9f\x8d\x9d\x11\xc4\xc3\x23"
                       "\x44\x3a\x11\x4f\xf2\x41\x31\xb8\x19\xbe\xad\x72\xdc\x3a\xbc\x34"
                       "\x53\xa7\xc6\xb3\x71\xc8\x83\x27\xb3\x45\x82\xd8\x95\x9e\x71\x92"
                       "\x88\x4f\xdd\x66\xbf\xc5\xd6\x42\x33\x18\x33\xf7\xaf\xab\x42\x47"
                       "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
                       "\x45\x3e\x5f\xdf\xa2\x43\x41\x25\x77\xae\xfd\x22\x19\x1a\x38\x2b"
                       "\x56\x93\xab\xc3\x61\xa8\x7d\xfc\xbb\x98\xf6\xd1\x29\xce\xe7\x58"
                       "\x73\x4c\xd3\x12\x3f\xcf\x46\x94\xba\xfa\x49\x83\x71\x1e\x35\x5f"
                       "\xbc\x2d\x3f\x7c\xf8\xb4\xb9\xa8\xc9\x9f\x8d\x9d\x11\xc4\xc3\x23"
                       "\x44\x3a\x11\x4f\xf2\x41\x31\xb8\x19\xbe\xad\x72\xdc\x3a\xbc\x34"
                       "\x53\xa7\xc6\xb3\x71\xc8\x83\x27\xb3\x45\x82\xd8\x95\x9e\x71\x92"
                       "\x88\x4f\xdd\x66\xbf\xc5\xd6\x42\x33\x18\x33\xf7\xaf\xab\x42\x47"
                       "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
                       "\x45\x3e\x5f\xdf\xa2\x43\x41\x25\x77\xae\xfd\x22\x19\x1a\x38\x2b"
                       "\x56\x93\xab\xc3\x61\xa8\x7d\xfc\xbb\x98\xf6\xd1\x29\xce\xe7\x58"
                       "\x73\x4c\xd3\x12\x3f\xcf\x46\x94\xba\xfa\x49\x83\x71\x1e\x35\x5f"
                       "\xbc\x2d\x3f\x7c\xf8\xb4\xb9\xa8\xc9\x9f\x8d\x9d\x11\xc4\xc3\x23"
                       "\x44\x3a\x11\x4f\xf2\x41\x31\xb8\x19\xbe\xad\x72\xdc\x3a\xbc\x34"
                       "\x53\xa7\xc6\xb3\x71\xc8\x83\x27\xb3\x45\x82\xd8\x95\x9e\x71\x92"
                       "\x88\x4f\xdd\x66\xbf\xc5\xd6\x42\x33\x18\x33\xf7\xaf\xab\x42\x47"
                       "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
                       "\x45\x3e\x5f\xdf\xa2\x43\x41\x25\x77\xae\xfd\x22\x19\x1a\x38\x2b"
                       "\x56\x93\xab\xc3\x61\xa8\x7d\xfc\xbb\x98\xf6\xd1\x29\xce\xe7\x58"
                       "\x73\x4c\xd3\x12\x3f\xcf\x46\x94\xba\xfa\x49\x83\x71\x1e\x35\x5f"
                       "\xbc\x2d\x3f\x7c\xf8\xb4\xb9\xa8\xc9\x9f\x8d\x9d\x11\xc4\xc3\x23"
                       "\x44\x3a\x11\x4f\xf2\x41\x31\xb8\x19\xbe\xad\x72\xdc\x3a\xbc\x34"
                       "\x53\xa7\xc6\xb3\x71\xc8\x83\x27\xb3\x45\x82\xd8\x95\x9e\x71\x92"
                       "\x88\x4f\xdd\x66\xbf\xc5\xd6\x42\x33\x18\x33\xf7\xaf\xab\x42\x47";


  uint8_t *dest = calloc(512,1);
  uint8_t *src = calloc(512,1);
  size_t size = 512;
  bitshuffle16_neon(aux, src, size);
  bitunshuffle16_neon(src, dest, size);
  printmem16(dest);
  printmem16(dest + 16);
  printmem16(dest + 32);
  printmem16(dest + 48);
  printmem16(dest + 64);
  printmem16(dest + 80);
  printmem16(dest + 96);
  printmem16(dest + 112);
  printmem16(dest + 128);
  printmem16(dest + 144);
  printmem16(dest + 160);
  printmem16(dest + 176);
  printmem16(dest + 192);
  printmem16(dest + 208);
  printmem16(dest + 224);
  printmem16(dest + 240);
  printmem16(dest + 256);
  printmem16(dest + 272);
  printmem16(dest + 288);
  printmem16(dest + 304);
  printmem16(dest + 320);
  printmem16(dest + 336);
  printmem16(dest + 352);
  printmem16(dest + 368);
  printmem16(dest + 384);
  printmem16(dest + 400);
  printmem16(dest + 416);
  printmem16(dest + 432);
  printmem16(dest + 448);
  printmem16(dest + 464);
  printmem16(dest + 480);
  printmem16(dest + 496);
  free(dest);
}


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


/* Routine optimized for bit-unshuffling a buffer for a type size of 16 byte. */
static void
bitunshuffle16_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 16;
  size_t i, j, k;
  uint8x8x2_t r0[8], r1[8];


  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 128, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
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


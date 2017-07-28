/* Routine optimized for bit-shuffling a buffer for a type size of 2 bytes. */
static void
bitshuffle2_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 2;
  uint8x16x2_t x0;
  size_t i, j, k;
  uint8x8_t lo_x[2], hi_x[2], lo[2], hi[2];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 32, k++) {
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
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size), lo[0], 0);
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size) + nbyte/2, lo[1], 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size), hi[0], 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size) + nbyte/2, hi[1], 0);
    }
  }
}


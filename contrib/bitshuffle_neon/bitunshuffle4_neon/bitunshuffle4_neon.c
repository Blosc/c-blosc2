/* Routine optimized for bit-unshuffling a buffer for a type size of 4 byte. */
static void
bitunshuffle4_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 4;
  size_t i, j, k;
  uint8x8_t lo_x[4], hi_x[4], lo[4], hi[4];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 64, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      lo_x[0][j] = src[2*k + j*nbyte/(8*elem_size)];
      hi_x[0][j] = src[2*k+1 + j*nbyte/(8*elem_size)];
      lo_x[1][j] = src[2*k + j*nbyte/(8*elem_size) + nbyte/4];
      hi_x[1][j] = src[2*k+1 + j*nbyte/(8*elem_size) + nbyte/4];
      lo_x[2][j] = src[2*k + j*nbyte/(8*elem_size) + nbyte/2];
      hi_x[2][j] = src[2*k+1 + j*nbyte/(8*elem_size) + nbyte/2];
      lo_x[3][j] = src[2*k + j*nbyte/(8*elem_size) + 3*nbyte/4];
      hi_x[3][j] = src[2*k+1 + j*nbyte/(8*elem_size) + 3*nbyte/4];
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
      vst1_lane_u8(dest + 4*j + i, lo[0], 0);
      vst1_lane_u8(dest + 4*j+1 + i, lo[1], 0);
      vst1_lane_u8(dest + 4*j+2 + i, lo[2], 0);
      vst1_lane_u8(dest + 4*j+3 + i, lo[3], 0);
      vst1_lane_u8(dest + 4*j + i + 32, hi[0], 0);
      vst1_lane_u8(dest + 4*j+1 + i + 32, hi[1], 0);
      vst1_lane_u8(dest + 4*j+2 + i + 32, hi[2], 0);
      vst1_lane_u8(dest + 4*j+3 + i + 32, hi[3], 0);
    }
  }
}


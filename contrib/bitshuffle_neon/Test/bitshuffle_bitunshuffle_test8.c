#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <assert.h>

#define CHECK_MULT_EIGHT(n) if (n % 8) exit(0);

static void printmem8(uint8x8_t buf)
{
  printf("%x,%x,%x,%x,%x,%x,%x,%x\n",
          buf[7], buf[6], buf[5], buf[4],
          buf[3], buf[2], buf[1], buf[0]);
}

static void printmem16(uint8x16_t r0)
{
  uint8_t buf[16];
  ((uint8x16_t *)buf)[0] = r0;
  printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
          buf[15], buf[14], buf[13], buf[12],
          buf[11], buf[10], buf[9], buf[8],
          buf[7], buf[6], buf[5], buf[4],
          buf[3], buf[2], buf[1], buf[0]);
}

static void printmem(uint8_t* buf)
{
  printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,"
         "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
	          buf[31], buf[30], buf[29], buf[28],
            buf[27], buf[26], buf[25], buf[24],
            buf[23], buf[22], buf[21], buf[20],
            buf[19], buf[18], buf[17], buf[16],
            buf[15], buf[14], buf[13], buf[12],
            buf[11], buf[10], buf[9], buf[8],
            buf[7], buf[6], buf[5], buf[4],
            buf[3], buf[2], buf[1], buf[0]);
}
/* Routine optimized for bit-shuffling a buffer for a type size of 8 bytes. */
static void
bitshuffle8_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 8;
  size_t i, j, k;
  uint8x8x2_t r0[4];
  uint16x4x2_t r1[4];
  uint32x2x2_t r2[4];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 64, k++) {
    /* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
    r0[0] = vzip_u8(vld1_u8(src + i + 0*8), vld1_u8(src + i +1*8));
    r0[1] = vzip_u8(vld1_u8(src + i + 2*8), vld1_u8(src + i +3*8));
    r0[2] = vzip_u8(vld1_u8(src + i + 4*8), vld1_u8(src + i +5*8));
    r0[3] = vzip_u8(vld1_u8(src + i + 6*8), vld1_u8(src + i +7*8));
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
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 0*nbyte/8, r0[0].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 1*nbyte/8, r0[0].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 2*nbyte/8, r0[1].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 3*nbyte/8, r0[1].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 4*nbyte/8, r0[2].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 5*nbyte/8, r0[2].val[1], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 6*nbyte/8, r0[3].val[0], 0);
      vst1_lane_u8(dest + k + j*nbyte/(8*elem_size) + 7*nbyte/8, r0[3].val[1], 0);
    }
  }
}
/* Routine optimized for bit-unshuffling a buffer for a type size of 8 byte. */
static void
bitunshuffle8_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 8;
  size_t i, j, k;
  uint8x8x2_t r0[4], r1[4];


  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 64, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      r0[0].val[0][j] = src[k + j*nbyte/(8*elem_size) + 0*nbyte/8];
      r0[0].val[1][j] = src[k + j*nbyte/(8*elem_size) + 1*nbyte/8];
      r0[1].val[0][j] = src[k + j*nbyte/(8*elem_size) + 2*nbyte/8];
      r0[1].val[1][j] = src[k + j*nbyte/(8*elem_size) + 3*nbyte/8];
      r0[2].val[0][j] = src[k + j*nbyte/(8*elem_size) + 4*nbyte/8];
      r0[2].val[1][j] = src[k + j*nbyte/(8*elem_size) + 5*nbyte/8];
      r0[3].val[0][j] = src[k + j*nbyte/(8*elem_size) + 6*nbyte/8];
      r0[3].val[1][j] = src[k + j*nbyte/(8*elem_size) + 7*nbyte/8];
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
      vst1_lane_u8(dest + 8*j+0 + i, r1[0].val[0], 0);
      vst1_lane_u8(dest + 8*j+1 + i, r1[0].val[1], 0);
      vst1_lane_u8(dest + 8*j+2 + i, r1[1].val[0], 0);
      vst1_lane_u8(dest + 8*j+3 + i, r1[1].val[1], 0);
      vst1_lane_u8(dest + 8*j+4 + i, r1[2].val[0], 0);
      vst1_lane_u8(dest + 8*j+5 + i, r1[2].val[1], 0);
      vst1_lane_u8(dest + 8*j+6 + i, r1[3].val[0], 0);
      vst1_lane_u8(dest + 8*j+7 + i, r1[3].val[1], 0);
    }
  }
}

main()
{
  uint8_t *src = "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
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
  size_t i;

  uint8_t *dest1 = calloc(512,1);
  uint8_t *dest2 = calloc(512,1);
  size_t size = 512;
  bitshuffle8_neon(src, dest1, size);
  bitunshuffle8_neon(dest1, dest2, size);

  for (i = 0; i < 256; i++) {
    assert(dest2[i] == src[i]);
  }

  free(dest1);
  free(dest2);
}

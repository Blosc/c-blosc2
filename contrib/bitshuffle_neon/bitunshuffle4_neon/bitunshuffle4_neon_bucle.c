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
/* Routine optimized for bit-shuffling a buffer for a type size of 4 bytes. */
static void
bitshuffle4_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 4;
  uint8x16x4_t x0;
  size_t i, j, k;
  uint8x8_t lo_x[4], hi_x[4], lo[4], hi[4];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 64, k++) {
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
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size), lo[0], 0);
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size) + nbyte/4, lo[1], 0);
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size) + nbyte/2, lo[2], 0);
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size) + 3*nbyte/4, lo[3], 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size), hi[0], 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size) + nbyte/4, hi[1], 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size) + nbyte/2, hi[2], 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size) + 3*nbyte/4, hi[3], 0);
    }
  }
}

static void
bitunshuffle4_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 4;
  size_t i, j, k;
  uint8x8_t lo_x[4], hi_x[4], lo[4], hi[4];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 64, k++) {
    for (j = 0; j < 8; j++) {
      lo_x[0][j] = src[2*k + j*nbyte/(8*elem_size)];
      hi_x[0][j] = src[2*k+1 + j*nbyte/(8*elem_size)];
      lo_x[1][j] = src[2*k + j*nbyte/(8*elem_size) + nbyte/4];
      hi_x[1][j] = src[2*k+1 + j*nbyte/(8*elem_size) + nbyte/4];
      lo_x[2][j] = src[2*k + j*nbyte/(8*elem_size) + nbyte/2];
      hi_x[2][j] = src[2*k+1 + j*nbyte/(8*elem_size) + nbyte/2];
      lo_x[3][j] = src[2*k + j*nbyte/(8*elem_size) + 3*nbyte/4];
      hi_x[3][j] = src[2*k+1 + j*nbyte/(8*elem_size) + 3*nbyte/4];
    }
    printf("\n");
    printmem8_l(lo_x[0]);
    printmem8_l(lo_x[1]);
    printmem8_l(lo_x[2]);
    printmem8_l(lo_x[3]);
    printmem8_l(hi_x[0]);
    printmem8_l(hi_x[1]);
    printmem8_l(hi_x[2]);
    printmem8_l(hi_x[3]);
    printf("\n");

    for (j = 0; j < 8; j++) {
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

      printf("lo[0] = ");
      printmem8_l(lo[0]);
      printf("lo[1] = ");
      printmem8_l(lo[1]);
      printf("lo[2] = ");
      printmem8_l(lo[2]);
      printf("lo[3] = ");
      printmem8_l(lo[3]);

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

      printf("hi[0] = ");
      printmem8_l(hi[0]);
      printf("hi[1] = ");
      printmem8_l(hi[1]);
      printf("hi[2] = ");
      printmem8_l(hi[2]);
      printf("hi[3] = ");
      printmem8_l(hi[3]);

      lo_x[0] = vshr_n_u8(lo_x[0], 1);
      hi_x[0] = vshr_n_u8(hi_x[0], 1);
      lo_x[1] = vshr_n_u8(lo_x[1], 1);
      hi_x[1] = vshr_n_u8(hi_x[1], 1);
      lo_x[2] = vshr_n_u8(lo_x[2], 1);
      hi_x[2] = vshr_n_u8(hi_x[2], 1);
      lo_x[3] = vshr_n_u8(lo_x[3], 1);
      hi_x[3] = vshr_n_u8(hi_x[3], 1);

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

  uint8_t *src = calloc(512,1);
  uint8_t *dest = calloc(512,1);
  size_t size = 512;
  bitshuffle4_neon(aux, src, size);
  bitunshuffle4_neon(src, dest, size);
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


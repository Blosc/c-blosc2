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
static void
bitshuffle2_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 2;
  size_t i, j, k;
  uint8x8_t lo_x[2], hi_x[2], lo[2], hi[2];

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 32, k++) {
    for (j = 0; j < 8; j++) {
        lo_x[0][j] = src[2*k + j*nbyte/(8*elem_size)];
        lo_x[1][j] = src[2*k + j*nbyte/(8*elem_size) + nbyte/2];
        hi_x[0][j] = src[2*k+1 + j*nbyte/(8*elem_size)];
        hi_x[1][j] = src[2*k+1 + j*nbyte/(8*elem_size) + nbyte/2];
        }

    printmem8_l(lo_x[0]);
    printmem8_l(lo_x[1]);
    printmem8_l(hi_x[0]);
    printmem8_l(hi_x[1]);
    printf("\n");

    for (j = 0; j < 8; j++) {
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

      printf("lo[0] = ");
      printmem8_l(lo[0]);
      printf("lo[1] = ");
      printmem8_l(lo[1]);

      hi[0] = vpadd_u8(hi[0], hi[0]);

      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[0] = vpadd_u8(hi[0], hi[0]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);
      hi[1] = vpadd_u8(hi[1], hi[1]);

      printf("hi[0] = ");
      printmem8_l(hi[0]);
      printf("hi[1] = ");
      printmem8_l(hi[1]);
      lo_x[0] = vshr_n_u8(lo_x[0], 1);
      hi_x[0] = vshr_n_u8(hi_x[0], 1);
      lo_x[1] = vshr_n_u8(lo_x[1], 1);
      hi_x[1] = vshr_n_u8(hi_x[1], 1);

      vst1_lane_u8(dest + 2*j + i, lo[0], 0);
      vst1_lane_u8(dest + 2*j+1 + i, lo[1], 0);
      vst1_lane_u8(dest + 2*j + i + 16, hi[0], 0);
      vst1_lane_u8(dest + 2*j+1 + i + 16, hi[1], 0);
    }
  }
}

void main()
{
  const uint8_t *src = "\x5b\x7b\xde\xe7\xfa\x3a\xdd\x76\x5b\x7b\xde\xe7\xfa\x3a\xdd\x76"
                       "\x5b\x7b\xde\xe7\xfa\x3a\xdd\x76\x5b\x7b\xde\xe7\xfa\x3a\xdd\x76"
                       "\xa1\x16\xb3\x1f\x82\x04\x3b\xfc\xa1\x16\xb3\x1f\x82\x04\x3b\xfc"
                       "\xa1\x16\xb3\x1f\x82\x04\x3b\xfc\xa1\x16\xb3\x1f\x82\x04\x3b\xfc"
                       "\x24\x33\xa9\x8c\x23\xe1\x42\x4e\x24\x33\xa9\x8c\x23\xe1\x42\x4e"
                       "\x24\x33\xa9\x8c\x23\xe1\x42\x4e\x24\x33\xa9\x8c\x23\xe1\x42\x4e"
                       "\xf1\xe2\x5a\x34\x3f\xf0\x00\x47\xf1\xe2\x5a\x34\x3f\xf0\x00\x47"
                       "\xf1\xe2\x5a\x34\x3f\xf0\x00\x47\xf1\xe2\x5a\x34\x3f\xf0\x00\x47"
                       "\xca\xf2\x39\xd7\x4f\xde\xd5\x3e\xca\xf2\x39\xd7\x4f\xde\xd5\x3e"
                       "\xca\xf2\x39\xd7\x4f\xde\xd5\x3e\xca\xf2\x39\xd7\x4f\xde\xd5\x3e"
                       "\xbe\xb4\xfe\xd5\x0f\xac\x94\x74\xbe\xb4\xfe\xd5\x0f\xac\x94\x74"
                       "\xbe\xb4\xfe\xd5\x0f\xac\x94\x74\xbe\xb4\xfe\xd5\x0f\xac\x94\x74"
                       "\xb3\x3b\xad\x6b\x94\x45\x87\x8a\xb3\x3b\xad\x6b\x94\x45\x87\x8a"
                       "\xb3\x3b\xad\x6b\x94\x45\x87\x8a\xb3\x3b\xad\x6b\x94\x45\x87\x8a"
                       "\x6b\x24\xb2\x12\xbd\xe4\x7a\x4f\x6b\x24\xb2\x12\xbd\xe4\x7a\x4f"
                       "\x6b\x24\xb2\x12\xbd\xe4\x7a\x4f\x6b\x24\xb2\x12\xbd\xe4\x7a\x4f"
                       "\x23\x8e\x23\xa4\xb1\x06\x1b\xe5\x23\x8e\x23\xa4\xb1\x06\x1b\xe5"
                       "\x23\x8e\x23\xa4\xb1\x06\x1b\xe5\x23\x8e\x23\xa4\xb1\x06\x1b\xe5"
                       "\xd1\xf7\x43\xf6\x90\x73\xcb\xeb\xd1\xf7\x43\xf6\x90\x73\xcb\xeb"
                       "\xd1\xf7\x43\xf6\x90\x73\xcb\xeb\xd1\xf7\x43\xf6\x90\x73\xcb\xeb"
                       "\xa5\x1b\x48\xcd\x77\x92\x59\xa7\xa5\x1b\x48\xcd\x77\x92\x59\xa7"
                       "\xa5\x1b\x48\xcd\x77\x92\x59\xa7\xa5\x1b\x48\xcd\x77\x92\x59\xa7"
                       "\x6f\xd3\xdc\xd5\x3b\x5b\x64\x51\x6f\xd3\xdc\xd5\x3b\x5b\x64\x51"
                       "\x6f\xd3\xdc\xd5\x3b\x5b\x64\x51\x6f\xd3\xdc\xd5\x3b\x5b\x64\x51"
                       "\x7f\x43\xb9\xda\x36\xf9\xe2\x30\x7f\x43\xb9\xda\x36\xf9\xe2\x30"
                       "\x7f\x43\xb9\xda\x36\xf9\xe2\x30\x7f\x43\xb9\xda\x36\xf9\xe2\x30"
                       "\x07\xb9\x0c\x10\x8f\xf9\x0b\x62\x07\xb9\x0c\x10\x8f\xf9\x0b\x62"
                       "\x07\xb9\x0c\x10\x8f\xf9\x0b\x62\x07\xb9\x0c\x10\x8f\xf9\x0b\x62"
                       "\x3f\x06\xea\x95\x42\x26\x34\xaf\x3f\x06\xea\x95\x42\x26\x34\xaf"
                       "\x3f\x06\xea\x95\x42\x26\x34\xaf\x3f\x06\xea\x95\x42\x26\x34\xaf"
                       "\xf1\x12\x7f\x3c\x7c\x18\xe7\x64\xf1\x12\x7f\x3c\x7c\x18\xe7\x64"
                       "\xf1\x12\x7f\x3c\x7c\x18\xe7\x64\xf1\x12\x7f\x3c\x7c\x18\xe7\x64";

  uint8_t *dest = calloc(512,1);
  size_t size = 512;
  bitshuffle2_neon(src, dest, size);
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


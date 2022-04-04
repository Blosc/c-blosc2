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
bitshuffle1_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 1;
  uint16x8_t x0;
  size_t i, j, k, ind;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  uint8x8_t lo_x, hi_x, lo, hi;

  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 16, k++) {

    x0 = vld1q_u8(src + k*16);

    lo_x = vget_low_u8(x0);
    hi_x = vget_high_u8(x0);

    printmem8_l(lo_x);
    printmem8_l(hi_x);
    printf("\n");

    for (j = 0; j < 8; j++) {
      lo = vand_u8(lo_x, mask_and);
      lo = vshl_u8(lo, mask_shift);
      hi = vand_u8(hi_x, mask_and);
      hi = vshl_u8(hi, mask_shift);

      lo = vpadd_u8(lo,lo);
      lo = vpadd_u8(lo,lo);
      lo = vpadd_u8(lo,lo);

      printf("lo = ");
      printmem8_l(lo);

      hi = vpadd_u8(hi,hi);
      hi = vpadd_u8(hi,hi);
      hi = vpadd_u8(hi,hi);

      printf("hi = ");
      printmem8_l(hi);

      lo_x = vshr_n_u8(lo_x, 1);
      hi_x = vshr_n_u8(hi_x, 1);

      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size), lo, 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size), hi, 0);

      //ind = k + j*nbyte/(8*elem_size);
      //dest[ind] = x1[0];
      //printf("dest[%d] = %d\n", ind, dest[ind]);
    //printf("\n");
    }
  }
}

void main()
{
  const uint8_t *src = "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
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

  uint8_t *dest = calloc(256,1);
  size_t size = 256;
  bitshuffle1_neon(src, dest, size);
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
  free(dest);
}


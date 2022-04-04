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

static void
bitunshuffle1_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 1;
  uint8x8_t lo_x, hi_x, lo, hi;
  size_t i, j, k;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 16, k++) {
    for (j = 0; j < 8; j++) {
      lo_x[j] = src[2*k + j*nbyte/(8*elem_size)];
      hi_x[j] = src[2*k+1 + j*nbyte/(8*elem_size)];
    }
    printf("lo_x = ");
    printmem8_l(lo_x);
    printf("hi_x = ");
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

      hi = vpadd_u8(hi,hi);
      hi = vpadd_u8(hi,hi);
      hi = vpadd_u8(hi,hi);

      printf("\tlo = ");
      printmem8_l(lo);
      printf("\thi = ");
      printmem8_l(hi);
      /* Shift packed 8-bit */
      lo_x = vshr_n_u8(lo_x, 1);
      hi_x = vshr_n_u8(hi_x, 1);
      vst1_lane_u8(dest + j + i, lo, 0);
      vst1_lane_u8(dest + 8*elem_size + j + i, hi, 0);
      //ind2 = i + j;
      //dest[ind2] = x1[0];
    }
    printf("\n");
  }
}
void main()
{
  const uint8_t *src = "\x4f\x19\xed\x95\x5e\x59\x35\xdc\x46\xdf\x6c\x05\xdb\x53\x36\xbd"
                       "\x4f\x19\xed\x95\x5e\x59\x35\xdc\x46\xdf\x6c\x05\xdb\x53\x36\xbd"
                       "\x03\xe6\x3e\xab\x0f\x65\x7d\xab\x04\xc2\x1a\x2a\xcf\xa5\xda\xfd"
                       "\x03\xe6\x3e\xab\x0f\x65\x7d\xab\x04\xc2\x1a\x2a\xcf\xa5\xda\xfd"
                       "\x32\x8c\x8f\x07\xc1\x64\xf2\xe0\x2f\x2e\x09\xd6\x86\x32\x7e\x98"
                       "\x32\x8c\x8f\x07\xc1\x64\xf2\xe0\x2f\x2e\x09\xd6\x86\x32\x7e\x98"
                       "\xab\x7d\x0e\xf6\xe4\xb3\x32\xa7\xdf\x0f\x8a\x77\x20\x28\x17\x32"
                       "\xab\x7d\x0e\xf6\xe4\xb3\x32\xa7\xdf\x0f\x8a\x77\x20\x28\x17\x32"
                       "\xee\x7a\x0e\x75\xc3\x8f\x9d\xf3\x7d\x1a\xd6\xfb\x19\xf9\x54\x0f"
                       "\xee\x7a\x0e\x75\xc3\x8f\x9d\xf3\x7d\x1a\xd6\xfb\x19\xf9\x54\x0f"
                       "\x7e\x45\x92\xcf\xf4\x55\x11\x53\xff\x80\xd2\xee\x9a\x41\x18\x3d"
                       "\x7e\x45\x92\xcf\xf4\x55\x11\x53\xff\x80\xd2\xee\x9a\x41\x18\x3d"
                       "\xaf\x4f\x6d\x05\xd9\xec\x67\x96\x18\x61\x39\x18\x35\x4a\xee\xc8"
                       "\xaf\x4f\x6d\x05\xd9\xec\x67\x96\x18\x61\x39\x18\x35\x4a\xee\xc8"
                       "\x47\xbe\x18\x06\xae\x6f\xa4\x0b\xf1\x6f\x90\x56\x6e\xbd\x75\x38"
                       "\x47\xbe\x18\x06\xae\x6f\xa4\x0b\xf1\x6f\x90\x56\x6e\xbd\x75\x38";

  uint8_t *dest = calloc(256,1);
  size_t size = 256;
  bitunshuffle1_neon(src, dest, size);
  printf("dest\n");
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

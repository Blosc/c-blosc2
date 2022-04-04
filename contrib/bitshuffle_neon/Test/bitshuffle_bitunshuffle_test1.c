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

/* Routine optimized for bit-shuffling a buffer for a type size of 1 byte. */
static void
bitshuffle1_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 1;
  uint16x8_t x0;
  size_t i, j, k;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  uint8x8_t lo_x, hi_x, lo, hi;

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 16, k++) {
    /* Load 16-byte groups */
    x0 = vld1q_u8(src + k*16);
    /* Split in 8-bytes grops */
    lo_x = vget_low_u8(x0);
    hi_x = vget_high_u8(x0);
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
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
      /* Shift packed 8-bit */
      lo_x = vshr_n_u8(lo_x, 1);
      hi_x = vshr_n_u8(hi_x, 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + 2*k + j*nbyte/(8*elem_size), lo, 0);
      vst1_lane_u8(dest + 2*k+1 + j*nbyte/(8*elem_size), hi, 0);
    }
  }
}


/* Routine optimized for bit-unshuffling a buffer for a type size of 1 byte. */
static void
bitunshuffle1_neon(const uint8_t* const src, uint8_t* dest, const size_t nbyte) {

  const size_t elem_size = 1;
  uint8x8_t lo_x, hi_x, lo, hi;
  size_t i, j, k;

  const int8_t __attribute__ ((aligned (16))) xr[8] = {0,1,2,3,4,5,6,7};
  uint8x8_t mask_and = vdup_n_u8(0x01);
  int8x8_t mask_shift = vld1_s8(xr);

  /* #define CHECK_MULT_EIGHT(n) if (n % 8) exit(0); */
  CHECK_MULT_EIGHT(nbyte);

  for (i = 0, k = 0; i < nbyte; i += 16, k++) {
    for (j = 0; j < 8; j++) {
      /* Load lanes */
      lo_x[j] = src[2*k + j*nbyte/(8*elem_size)];
      hi_x[j] = src[2*k+1 + j*nbyte/(8*elem_size)];
    }
    for (j = 0; j < 8; j++) {
      /* Create mask from the most significant bit of each 8-bit element */
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
      /* Shift packed 8-bit */
      lo_x = vshr_n_u8(lo_x, 1);
      hi_x = vshr_n_u8(hi_x, 1);
      /* Store the created mask to the destination vector */
      vst1_lane_u8(dest + j + i, lo, 0);
      vst1_lane_u8(dest + 8*elem_size + j + i, hi, 0);
    }
  }
}

void main()
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
  bitshuffle1_neon(src, dest1, size);
  bitunshuffle1_neon(dest1, dest2, size);

  for (i = 0; i < 256; i++) {
    assert(dest2[i] == src[i]);
  }

  free(dest1);
  free(dest2);
}

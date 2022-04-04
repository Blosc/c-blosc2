#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <assert.h>

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

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
void shuffle16_neon(uint8_t* const dest, const uint8_t* const src,
               const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 16;
  uint8x8x2_t r0[8];
  uint16x4x2_t r1[8];
  uint32x2x2_t r2[8];

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 128, k++) {
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
    /* Store the results to the destination vector */
    vst1_u8(dest + k*8 + 0*total_elements, vreinterpret_u8_u32(r2[0].val[0]));
    vst1_u8(dest + k*8 + 1*total_elements, vreinterpret_u8_u32(r2[0].val[1]));
    vst1_u8(dest + k*8 + 2*total_elements, vreinterpret_u8_u32(r2[1].val[0]));
    vst1_u8(dest + k*8 + 3*total_elements, vreinterpret_u8_u32(r2[1].val[1]));
    vst1_u8(dest + k*8 + 4*total_elements, vreinterpret_u8_u32(r2[2].val[0]));
    vst1_u8(dest + k*8 + 5*total_elements, vreinterpret_u8_u32(r2[2].val[1]));
    vst1_u8(dest + k*8 + 6*total_elements, vreinterpret_u8_u32(r2[3].val[0]));
    vst1_u8(dest + k*8 + 7*total_elements, vreinterpret_u8_u32(r2[3].val[1]));
    vst1_u8(dest + k*8 + 8*total_elements, vreinterpret_u8_u32(r2[4].val[0]));
    vst1_u8(dest + k*8 + 9*total_elements, vreinterpret_u8_u32(r2[4].val[1]));
    vst1_u8(dest + k*8 + 10*total_elements, vreinterpret_u8_u32(r2[5].val[0]));
    vst1_u8(dest + k*8 + 11*total_elements, vreinterpret_u8_u32(r2[5].val[1]));
    vst1_u8(dest + k*8 + 12*total_elements, vreinterpret_u8_u32(r2[6].val[0]));
    vst1_u8(dest + k*8 + 13*total_elements, vreinterpret_u8_u32(r2[6].val[1]));
    vst1_u8(dest + k*8 + 14*total_elements, vreinterpret_u8_u32(r2[7].val[0]));
    vst1_u8(dest + k*8 + 15*total_elements, vreinterpret_u8_u32(r2[7].val[1]));
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
void unshuffle16_neon(uint8_t* const dest, const uint8_t* const src,
               const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 16;
  uint8x8x2_t r0[8];
  uint16x4x2_t r1[8];
  uint32x2x2_t r2[8];

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 128, k++) {
    /* Load and interleave groups of 16 bytes (128 bytes) to the structure r0*/
    r0[0] = vzip_u8(vld1_u8(src + k*8 + 0*total_elements), vld1_u8(src + k*8 + 1*total_elements));
    r0[1] = vzip_u8(vld1_u8(src + k*8 + 2*total_elements), vld1_u8(src + k*8 + 3*total_elements));
    r0[2] = vzip_u8(vld1_u8(src + k*8 + 4*total_elements), vld1_u8(src + k*8 + 5*total_elements));
    r0[3] = vzip_u8(vld1_u8(src + k*8 + 6*total_elements), vld1_u8(src + k*8 + 7*total_elements));
    r0[4] = vzip_u8(vld1_u8(src + k*8 + 8*total_elements), vld1_u8(src + k*8 + 9*total_elements));
    r0[5] = vzip_u8(vld1_u8(src + k*8 + 10*total_elements), vld1_u8(src + k*8 + 11*total_elements));
    r0[6] = vzip_u8(vld1_u8(src + k*8 + 12*total_elements), vld1_u8(src + k*8 + 13*total_elements));
    r0[7] = vzip_u8(vld1_u8(src + k*8 + 14*total_elements), vld1_u8(src + k*8 + 15*total_elements));
    /* Interleave 16 bytes */
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    r1[4] = vzip_u16(vreinterpret_u16_u8(r0[4].val[0]), vreinterpret_u16_u8(r0[5].val[0]));
    r1[5] = vzip_u16(vreinterpret_u16_u8(r0[4].val[1]), vreinterpret_u16_u8(r0[5].val[1]));
    r1[6] = vzip_u16(vreinterpret_u16_u8(r0[6].val[0]), vreinterpret_u16_u8(r0[7].val[0]));
    r1[7] = vzip_u16(vreinterpret_u16_u8(r0[6].val[1]), vreinterpret_u16_u8(r0[7].val[1]));
    /* Interleave 32 bytes */
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
    r2[4] = vzip_u32(vreinterpret_u32_u16(r1[4].val[0]), vreinterpret_u32_u16(r1[6].val[0]));
    r2[5] = vzip_u32(vreinterpret_u32_u16(r1[4].val[1]), vreinterpret_u32_u16(r1[6].val[1]));
    r2[6] = vzip_u32(vreinterpret_u32_u16(r1[5].val[0]), vreinterpret_u32_u16(r1[7].val[0]));
    r2[7] = vzip_u32(vreinterpret_u32_u16(r1[5].val[1]), vreinterpret_u32_u16(r1[7].val[1]));
    /* Store */
    vst1_u8(dest + i + 0*8, vreinterpret_u8_u32(r2[0].val[0]));
    vst1_u8(dest + i + 1*8, vreinterpret_u8_u32(r2[4].val[0]));
    vst1_u8(dest + i + 2*8, vreinterpret_u8_u32(r2[0].val[1]));
    vst1_u8(dest + i + 3*8, vreinterpret_u8_u32(r2[4].val[1]));
    vst1_u8(dest + i + 4*8, vreinterpret_u8_u32(r2[1].val[0]));
    vst1_u8(dest + i + 5*8, vreinterpret_u8_u32(r2[5].val[0]));
    vst1_u8(dest + i + 6*8, vreinterpret_u8_u32(r2[1].val[1]));
    vst1_u8(dest + i + 7*8, vreinterpret_u8_u32(r2[5].val[1]));
    vst1_u8(dest + i + 8*8, vreinterpret_u8_u32(r2[2].val[0]));
    vst1_u8(dest + i + 9*8, vreinterpret_u8_u32(r2[6].val[0]));
    vst1_u8(dest + i + 10*8, vreinterpret_u8_u32(r2[2].val[1]));
    vst1_u8(dest + i + 11*8, vreinterpret_u8_u32(r2[6].val[1]));
    vst1_u8(dest + i + 12*8, vreinterpret_u8_u32(r2[3].val[0]));
    vst1_u8(dest + i + 13*8, vreinterpret_u8_u32(r2[7].val[0]));
    vst1_u8(dest + i + 14*8, vreinterpret_u8_u32(r2[3].val[1]));
    vst1_u8(dest + i + 15*8, vreinterpret_u8_u32(r2[7].val[1]));
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
                 "\x13\x21\x17\xc8\xc9\x34\x25\x11\x67\x74\x4e\xe8\x67\x74\x4e\xe8"
                 "\x13\x21\x17\xc8\xc9\x34\x25\x11\x67\x74\x4e\xe8\x67\x74\x4e\xe8";

  size_t vectorizable_elements = 16;
  size_t total_elements = 18;
  size_t i;

  uint8_t *dest1 = calloc(288,2);
  uint8_t *dest2 = calloc(288,2);

  shuffle16_neon(dest1, src, vectorizable_elements, total_elements);
  unshuffle16_neon(dest2, dest1, vectorizable_elements, total_elements);

  for (i = 0; i < 256; i++) {
    assert(dest2[i] == src[i]);
  }

  free(dest1);
  free(dest2);
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <string.h>

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

static void printmem16x4(uint16x4_t buf)
{
  printf("%x,%x,%x,%x\n",
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
    //printf("i = %d\n", i);
    /* Load and interleave groups of 16 bytes (128 bytes) to the structure r0*/
    r0[0] = vzip_u8(vld1_u8(src + k*8 + 0*total_elements), vld1_u8(src + k*8 + 1*total_elements));
    r0[1] = vzip_u8(vld1_u8(src + k*8 + 2*total_elements), vld1_u8(src + k*8 + 3*total_elements));
    r0[2] = vzip_u8(vld1_u8(src + k*8 + 4*total_elements), vld1_u8(src + k*8 + 5*total_elements));
    r0[3] = vzip_u8(vld1_u8(src + k*8 + 6*total_elements), vld1_u8(src + k*8 + 7*total_elements));
    r0[4] = vzip_u8(vld1_u8(src + k*8 + 8*total_elements), vld1_u8(src + k*8 + 9*total_elements));
    r0[5] = vzip_u8(vld1_u8(src + k*8 + 10*total_elements), vld1_u8(src + k*8 + 11*total_elements));
    r0[6] = vzip_u8(vld1_u8(src + k*8 + 12*total_elements), vld1_u8(src + k*8 + 13*total_elements));
    r0[7] = vzip_u8(vld1_u8(src + k*8 + 14*total_elements), vld1_u8(src + k*8 + 15*total_elements));
    /*printf("vzip 8\n");
    for (j = 0; j < 8; j++) {
      for(l = 0; l < 2; l++) {
        printf("r0[%d].val[%d] = ", j, l);
        printmem8(r0[j].val[l]);
      }
    }*/
    /* Interleave 16 bytes */
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    r1[4] = vzip_u16(vreinterpret_u16_u8(r0[4].val[0]), vreinterpret_u16_u8(r0[5].val[0]));
    r1[5] = vzip_u16(vreinterpret_u16_u8(r0[4].val[1]), vreinterpret_u16_u8(r0[5].val[1]));
    r1[6] = vzip_u16(vreinterpret_u16_u8(r0[6].val[0]), vreinterpret_u16_u8(r0[7].val[0]));
    r1[7] = vzip_u16(vreinterpret_u16_u8(r0[6].val[1]), vreinterpret_u16_u8(r0[7].val[1]));
    /*printf("vzip 16\n");
    for (j = 0; j < 8; j++) {
      for(l = 0; l < 2; l++) {
        printf("r1[%d].val[%d] = ", j, l);
        printmem8(vreinterpret_u8_u16(r1[j].val[l]));
      }
    }*/
    /* Interleave 32 bytes */
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
    r2[4] = vzip_u32(vreinterpret_u32_u16(r1[4].val[0]), vreinterpret_u32_u16(r1[6].val[0]));
    r2[5] = vzip_u32(vreinterpret_u32_u16(r1[4].val[1]), vreinterpret_u32_u16(r1[6].val[1]));
    r2[6] = vzip_u32(vreinterpret_u32_u16(r1[5].val[0]), vreinterpret_u32_u16(r1[7].val[0]));
    r2[7] = vzip_u32(vreinterpret_u32_u16(r1[5].val[1]), vreinterpret_u32_u16(r1[7].val[1]));
    /*printf("vzip 32\n");
    for (j = 0; j < 8; j++) {
      for(l = 0; l < 2; l++) {
        printf("r2[%d].val[%d] = ", j, l);
        printmem8(vreinterpret_u8_u32(r2[j].val[l]));
      }
    }*/
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
  uint8_t *src = "\xcb\x45\x56\x73\xbc\x44\x53\x88\xcb\x45\x56\x73\xbc\x44\x53\x88\x0\x0"
                 "\xff\x3e\x93\x4c\x2d\x3a\xa7\x4f\xff\x3e\x93\x4c\x2d\x3a\xa7\x4f\x0\x0"
                 "\xf1\x5f\xab\xd3\x3f\x11\xc6\xdd\xf1\x5f\xab\xd3\x3f\x11\xc6\xdd\x0\x0"
                 "\x79\xdf\xc3\x12\x7c\x4f\xb3\x66\x79\xdf\xc3\x12\x7c\x4f\xb3\x66\x0\x0"
                 "\x24\xa2\x61\x3f\xf8\xf2\x71\xbf\x24\xa2\x61\x3f\xf8\xf2\x71\xbf\x0\x0"
                 "\x7c\x43\xa8\xcf\xb4\x41\xc8\xc5\x7c\x43\xa8\xcf\xb4\x41\xc8\xc5\x0\x0"
                 "\xb1\x41\x7d\x46\xb9\x31\x83\xd6\xb1\x41\x7d\x46\xb9\x31\x83\xd6\x0\x0"
                 "\x58\x25\xfc\x94\xa8\xb8\x27\x42\x58\x25\xfc\x94\xa8\xb8\x27\x42\x0\x0"
                 "\x69\x77\xbb\xba\xc9\x19\xb3\x33\x69\x77\xbb\xba\xc9\x19\xb3\x33\x0\x0"
                 "\xd2\xae\x98\xfa\x9f\xbe\x45\x18\xd2\xae\x98\xfa\x9f\xbe\x45\x18\x0\x0"
                 "\xee\xfd\xf6\x49\x8d\xad\x82\x33\xee\xfd\xf6\x49\x8d\xad\x82\x33\x0\x0"
                 "\xdd\x22\xd1\x83\x9d\x72\xd8\xf7\xdd\x22\xd1\x83\x9d\x72\xd8\xf7\x0\x0"
                 "\x99\x19\x29\x71\x11\xdc\x95\xaf\x99\x19\x29\x71\x11\xdc\x95\xaf\x0\x0"
                 "\x9a\x1a\xce\x1e\xc4\x3a\x9e\xab\x9a\x1a\xce\x1e\xc4\x3a\x9e\xab\x0\x0"
                 "\x7a\x38\xe7\x35\xc3\xbc\x71\x42\x7a\x38\xe7\x35\xc3\xbc\x71\x42\x0\x0"
                 "\x86\x2b\x58\x5f\x23\x34\x92\x47\x86\x2b\x58\x5f\x23\x34\x92\x47\x0\x0";

  uint8_t *dest = calloc(288,2);
  size_t vectorizable_elements = 16;
  size_t total_elements = 18;

  unshuffle16_neon(dest, src, vectorizable_elements, total_elements);
      printf("vst1q_u8 \n");
      printmem(dest);
      printmem(dest+32);
      printmem(dest+64);
      printmem(dest+96);
      printmem(dest+128);
      printmem(dest+160);
      printmem(dest+192);
      printmem(dest+224);
      printmem(dest+256);

  free(dest);
}


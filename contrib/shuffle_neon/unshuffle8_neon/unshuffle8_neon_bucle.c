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
/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
unshuffle8_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 8;
  uint8x8x2_t r0[4];
  uint16x4x2_t r1[4];
  uint32x2x2_t r2[4];

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* Load 8 groups of 8 bytes (64 bytes) to the structure r2 */
    printf("i = %d\n", i);
    for (j = 0; j < 4; j++) {
      r0[j] = vzip_u8(vld1_u8(src + (j*2)*total_elements + k*8), vld1_u8(src + (j*2+1)*total_elements + k*8));
    }
    /* VZIP_U8 */
    /*r0[0] = vzip_u8(vld1_u8(src + 0*total_elements + k*8), vld1_u8(src + 1*total_elements + k*8));
    r0[1] = vzip_u8(vld1_u8(src + 2*total_elements + k*8), vld1_u8(src + 3*total_elements + k*8));
    r0[2] = vzip_u8(vld1_u8(src + 4*total_elements + k*8), vld1_u8(src + 5*total_elements + k*8));
    r0[3] = vzip_u8(vld1_u8(src + 6*total_elements + k*8), vld1_u8(src + 7*total_elements + k*8));*/
    /*for (j = 0; j < 4; j++) {
      for(l = 0; l < 2; l++) {
        printf("r0[%d].val[%d] = ", j, l);
        printmem8(r0[j].val[l]);
      }
    }*/
    /* VZIP_U16 */
    printf("vzip 16\n");
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    /*for (j = 0; j < 4; j++) {
      for(l = 0; l < 2; l++) {
        printf("r1[%d].val[%d] = ", j, l);
        printmem8(vreinterpret_u8_u16(r1[j].val[l]));
      }
    }*/
    /* VZIP_U32 */
    printf("vzip 8\n");
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
    /*for (j = 0; j < 4; j++) {
      for(l = 0; l < 2; l++) {
        printf("r2[%d].val[%d] = ", j, l);
        printmem8(vreinterpret_u8_u32(r2[j].val[l]));
      }
    }*/
    /* Store */
    for(j = 0; j < 4; j++) {
      r0[j] = vzip_u8(vld1_u8(src + i + (2*j)*8), vld1_u8(src + i +(2*j+1)*8));
    }
    for(j = 0; j < 4; j++) {
      vst1_u8(dest + i + (j*2)*8, vreinterpret_u8_u32(r2[j].val[0]));
      vst1_u8(dest + i + (j*2+1)*8, vreinterpret_u8_u32(r2[j].val[1]));

      /*vst1_u8(dest + i + 0*8, vreinterpret_u8_u32(r2[0].val[0]));
      vst1_u8(dest + i + 1*8, vreinterpret_u8_u32(r2[0].val[1]));
      vst1_u8(dest + i + 2*8, vreinterpret_u8_u32(r2[1].val[0]));
      vst1_u8(dest + i + 3*8, vreinterpret_u8_u32(r2[1].val[1]));
      vst1_u8(dest + i + 4*8, vreinterpret_u8_u32(r2[2].val[0]));
      vst1_u8(dest + i + 5*8, vreinterpret_u8_u32(r2[2].val[1]));
      vst1_u8(dest + i + 6*8, vreinterpret_u8_u32(r2[3].val[0]));
      vst1_u8(dest + i + 7*8, vreinterpret_u8_u32(r2[3].val[1]));*/
    }
  }
}

main()
{
  uint8_t *src = "\xcb\x69\x45\x77\x56\xbb\x73\xba\xbc\xc9\x44\x19\x53\xb3\x88\x33\x0\x0"
                 "\xff\xd2\x3e\xae\x93\x98\x4c\xfa\x2d\x9f\x3a\xbe\xa7\x45\x4f\x18\x0\x0"
                 "\xf1\xee\x5f\xfd\xab\xf6\xd3\x49\x3f\x8d\x11\xad\xc6\x82\xdd\x33\x0\x0"
                 "\x79\xdd\xdf\x22\xc3\xd1\x12\x83\x7c\x9d\x4f\x72\xb3\xd8\x66\xf7\x0\x0"
                 "\x24\x99\xa2\x19\x61\x29\x3f\x71\xf8\x11\xf2\xdc\x71\x95\xbf\xaf\x0\x0"
                 "\x7c\x9a\x43\x1a\xa8\xce\xcf\x1e\xb4\xc4\x41\x3a\xc8\x9e\xc5\xab\x0\x0"
                 "\xb1\x7a\x41\x38\x7d\xe7\x46\x35\xb9\xc3\x31\xbc\x83\x71\xd6\x42\x0\x0"
                 "\x58\x86\x25\x2b\xfc\x58\x94\x5f\xa8\x23\xb8\x34\x27\x92\x42\x47\x0\x0";


  uint8_t *dest = calloc(144,2);
  size_t vectorizable_elements = 16;
  size_t total_elements = 18;

  unshuffle8_neon(dest, src, vectorizable_elements, total_elements);
  printf("vst1q_u8 \n");
  printmem(dest);
  printmem(dest+32);
  printmem(dest+64);
  printmem(dest+96);
  printmem(dest+128);

  free(dest);
}


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
shuffle8_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 8;
  uint8x8x4_t r0[2], r1[2];

  uint8_t *index = "\x00\x02\x04\x06\xff\xff\xff\xff"
                   "\x08\x0a\x0c\x0e\xff\xff\xff\xff"
                   "\x10\x12\x14\x16\xff\xff\xff\xff"
                   "\x18\x1a\x1c\x1e\xff\xff\xff\xff"
                   "\xff\xff\xff\xff\x00\x02\x04\x06"
                   "\xff\xff\xff\xff\x08\x0a\x0c\x0e"
                   "\xff\xff\xff\xff\x10\x12\x14\x16"
                   "\xff\xff\xff\xff\x18\x1a\x1c\x1e"
                   "\x01\x03\x05\x07\xff\xff\xff\xff"
                   "\x09\x0b\x0d\x0f\xff\xff\xff\xff"
                   "\x11\x13\x15\x17\xff\xff\xff\xff"
                   "\x19\x1b\x1d\x1f\xff\xff\xff\xff"
                   "\xff\xff\xff\xff\x01\x03\x05\x07"
                   "\xff\xff\xff\xff\x09\x0b\x0d\x0f"
                   "\xff\xff\xff\xff\x11\x13\x15\x17"
                   "\xff\xff\xff\xff\x19\x1b\x1d\x1f";

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* Load (and permute) 64 bytes to the structure r0 */
    for (j = 0; j < 2; j++) {
      r0[j] = vld4_u8(src + i + j*32);
    }
    /* Extended table look up to to rearrange bytes */
    for(j = 0; j < 4; j++) {
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[0], vld1_u8(index + j*8));
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[1], vld1_u8(index + 32 + j*8));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[0], vld1_u8(index + 64 + j*8));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[1], vld1_u8(index + 96 + j*8));
    }

    /*r1[0].val[0] = vtbx4_u8(r1[0].val[0], r0[0], vld1_u8(index + 0*8));
    r1[0].val[1] = vtbx4_u8(r1[0].val[1], r0[0], vld1_u8(index + 1*8));
    r1[0].val[2] = vtbx4_u8(r1[0].val[2], r0[0], vld1_u8(index + 2*8));
    r1[0].val[3] = vtbx4_u8(r1[0].val[3], r0[0], vld1_u8(index + 3*8));

    r1[0].val[0] = vtbx4_u8(r1[0].val[0], r0[1], vld1_u8(index + 32 + 0*8));
    r1[0].val[1] = vtbx4_u8(r1[0].val[1], r0[1], vld1_u8(index + 32 + 1*8));
    r1[0].val[2] = vtbx4_u8(r1[0].val[2], r0[1], vld1_u8(index + 32 + 2*8));
    r1[0].val[3] = vtbx4_u8(r1[0].val[3], r0[1], vld1_u8(index + 32 + 3*8));

    r1[1].val[0] = vtbx4_u8(r1[1].val[0], r0[0], vld1_u8(index + 64 + 0*8));
    r1[1].val[1] = vtbx4_u8(r1[1].val[1], r0[0], vld1_u8(index + 64 + 1*8));
    r1[1].val[2] = vtbx4_u8(r1[1].val[2], r0[0], vld1_u8(index + 64 + 2*8));
    r1[1].val[3] = vtbx4_u8(r1[1].val[3], r0[0], vld1_u8(index + 64 + 3*8));

    r1[1].val[0] = vtbx4_u8(r1[1].val[0], r0[1], vld1_u8(index + 96 + 0*8));
    r1[1].val[1] = vtbx4_u8(r1[1].val[1], r0[1], vld1_u8(index + 96 + 1*8));
    r1[1].val[2] = vtbx4_u8(r1[1].val[2], r0[1], vld1_u8(index + 96 + 2*8));
    r1[1].val[3] = vtbx4_u8(r1[1].val[3], r0[1], vld1_u8(index + 96 + 3*8));*/

    /*printf("vtbl_u8 i = %d\n", i);
    printf("r1[0].val[0] = ");
    printmem8(r1[0].val[0]);
    printf("r1[0].val[1] = ");
    printmem8(r1[0].val[1]);
    printf("r1[0].val[2] = ");
    printmem8(r1[0].val[2]);
    printf("r1[0].val[3] = ");
    printmem8(r1[0].val[3]);
    printf("r1[1].val[0] = ");
    printmem8(r1[1].val[0]);
    printf("r1[1].val[1] = ");
    printmem8(r1[1].val[1]);
    printf("r1[1].val[2] = ");
    printmem8(r1[1].val[2]);
    printf("r1[1].val[3] = ");
    printmem8(r1[1].val[3]);*/
    /* Store the results in the destination vector */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        vst1_u8(dest + total_elements*(j+l*4) + k*8, r1[l].val[j]);
      }
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
                 "\x13\x21\x17\xc8\xc9\x34\x25\x11\x67\x74\x4e\xe8\x67\x74\x4e\xe8"
                 "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
                 "\x45\x3e\x5f\xdf\xa2\x43\x41\x25\x77\xae\xfd\x22\x19\x1a\x38\x2b"
                 "\x56\x93\xab\xc3\x61\xa8\x7d\xfc\xbb\x98\xf6\xd1\x29\xce\xe7\x58"
                 "\x73\x4c\xd3\x12\x3f\xcf\x46\x94\xba\xfa\x49\x83\x71\x1e\x35\x5f"
                 "\xbc\x2d\x3f\x7c\xf8\xb4\xb9\xa8\xc9\x9f\x8d\x9d\x11\xc4\xc3\x23"
                 "\x44\x3a\x11\x4f\xf2\x41\x31\xb8\x19\xbe\xad\x72\xdc\x3a\xbc\x34"
                 "\x53\xa7\xc6\xb3\x71\xc8\x83\x27\xb3\x45\x82\xd8\x95\x9e\x71\x92"
                 "\x88\x4f\xdd\x66\xbf\xc5\xd6\x42\x33\x18\x33\xf7\xaf\xab\x42\x47"
                 "\x13\x21\x17\xc8\xc9\x34\x25\x11\x67\x74\x4e\xe8\x67\x74\x4e\xe8";

  uint8_t *dest = calloc(288,2);
  size_t vectorizable_elements = 32;
  size_t total_elements = 36;

  shuffle8_neon(dest, src, vectorizable_elements, total_elements);
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


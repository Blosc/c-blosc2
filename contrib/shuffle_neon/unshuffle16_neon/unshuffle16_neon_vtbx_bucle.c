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
void unshuffle16_neon(uint8_t* const dest, const uint8_t* const src,
                 const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 16;
  uint8x8x4_t r0[4], r1[4];

  uint8_t *index1_top = "\x00\x01\x02\x03\x08\x09\x0a\x0b"
                        "\x10\x11\x12\x13\x18\x19\x1a\x1b"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\x00\x01\x02\x03\x08\x09\x0a\x0b"
                        "\x10\x11\x12\x13\x18\x19\x1a\x1b";

  uint8_t *index1_bottom = "\x04\x05\x06\x07\x0c\x0d\x0e\x0f"
                           "\x14\x15\x16\x17\x1c\x1d\x1e\x1f"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\x04\x05\x06\x07\x0c\x0d\x0e\x0f"
                           "\x14\x15\x16\x17\x1c\x1d\x1e\x1f";

  uint8_t *index2_top = "\x00\x10\xff\xff\x04\x14\xff\xff"
                        "\x08\x18\xff\xff\x0c\x1c\xff\xff"
                        "\x01\x11\xff\xff\x05\x15\xff\xff"
                        "\x09\x19\xff\xff\x0d\x1d\xff\xff"
                        "\xff\xff\x00\x10\xff\xff\x04\x14"
                        "\xff\xff\x08\x18\xff\xff\x0c\x1c"
                        "\xff\xff\x01\x11\xff\xff\x05\x15"
                        "\xff\xff\x09\x19\xff\xff\x0d\x1d";

  uint8_t *index2_bottom = "\x02\x12\xff\xff\x06\x16\xff\xff"
                           "\x0a\x1a\xff\xff\x0e\x1e\xff\xff"
                           "\x03\x13\xff\xff\x07\x17\xff\xff"
                           "\x0b\x1b\xff\xff\x0f\x1f\xff\xff"
                           "\xff\xff\x02\x12\xff\xff\x06\x16"
                           "\xff\xff\x0a\x1a\xff\xff\x0e\x1e"
                           "\xff\xff\x03\x13\xff\xff\x07\x17"
                           "\xff\xff\x0b\x1b\xff\xff\x0f\x1f";

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 128, k++) {
    /* Load 16 groups of 8 bytes to the structures */
    //printf("\t\tLoad i = %d\n", i);
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 4; l++) {
        r0[j].val[l] = vld1_u8(src + j*total_elements + l*4*total_elements + k*8);
        //printf("r0[%d].val[%d] = ",j,l);
        //printmem8(r0[j].val[l]);
      }
    }
    /* Extended table look up to rearrange the distribution of the structures */
    for(j = 0; j < 4; j++) {
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[0], vld1_u8(index1_top +j*8));
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[1], vld1_u8(index1_top + 32 + j*8));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[2], vld1_u8(index1_top + j*8));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[3], vld1_u8(index1_top + 32 + j*8));
      r1[2].val[j] = vtbx4_u8(r1[2].val[j], r0[0], vld1_u8(index1_bottom + j*8));
      r1[2].val[j] = vtbx4_u8(r1[2].val[j], r0[1], vld1_u8(index1_bottom + 32 + j*8));
      r1[3].val[j] = vtbx4_u8(r1[3].val[j], r0[2], vld1_u8(index1_bottom + j*8));
      r1[3].val[j] = vtbx4_u8(r1[3].val[j], r0[3], vld1_u8(index1_bottom + 32 + j*8));
    }
    for(j = 0; j < 4; j++) {
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[0], vld1_u8(index2_top + j*8));
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[1], vld1_u8(index2_top + 32 + j*8));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[0], vld1_u8(index2_bottom + j*8));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[1], vld1_u8(index2_bottom + 32 + j*8));
      r0[2].val[j] = vtbx4_u8(r0[2].val[j], r1[2], vld1_u8(index2_top + j*8));
      r0[2].val[j] = vtbx4_u8(r0[2].val[j], r1[3], vld1_u8(index2_top + 32 + j*8));
      r0[3].val[j] = vtbx4_u8(r0[3].val[j], r1[2], vld1_u8(index2_bottom + j*8));
      r0[3].val[j] = vtbx4_u8(r0[3].val[j], r1[3], vld1_u8(index2_bottom + 32 + j*8));
    }
    /*printf("Objetivo final :\n");
    for (j=0;j<4;j++) {
      for(l=0;l<4;l++) {
        printf("r0[%d].val[%d] = ",j,l);
        printmem8(r0[j].val[l]);
      }
    }*/
    for(j = 0; j < 4; j++) {
      for(l = 0; l < 4; l++) {
        vst1_u8(dest + l*8 + j*32 + i, r0[j].val[l]);
      }
    }
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


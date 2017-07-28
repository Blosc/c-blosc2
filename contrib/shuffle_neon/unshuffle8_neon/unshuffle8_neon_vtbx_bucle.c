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
unshuffle8_neon(uint8_t* const dest, const uint8_t* const src,
                const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 8;
  uint8x8x4_t r0[2], r1[2];

  uint8_t *index = "\x00\x04\x08\x0c\x10\x14\x18\x1c"
                   "\x01\x05\x09\x0d\x11\x15\x19\x1d"
                   "\x02\x06\x0a\x0e\x12\x16\x1a\x1e"
                   "\x03\x07\x0b\x0f\x13\x17\x1b\x1f";

  uint8_t *index_top = "\x00\x01\x02\x03\x08\x09\x0a\x0b"
                       "\x10\x11\x12\x13\x18\x19\x1a\x1b"
                       "\xff\xff\xff\xff\xff\xff\xff\xff"
                       "\xff\xff\xff\xff\xff\xff\xff\xff"
                       "\xff\xff\xff\xff\xff\xff\xff\xff"
                       "\xff\xff\xff\xff\xff\xff\xff\xff"
                       "\x00\x01\x02\x03\x08\x09\x0a\x0b"
                       "\x10\x11\x12\x13\x18\x19\x1a\x1b";

  uint8_t *index_bottom = "\x04\x05\x06\x07\x0c\x0d\x0e\x0f"
                          "\x14\x15\x16\x17\x1c\x1d\x1e\x1f"
                          "\xff\xff\xff\xff\xff\xff\xff\xff"
                          "\xff\xff\xff\xff\xff\xff\xff\xff"
                          "\xff\xff\xff\xff\xff\xff\xff\xff"
                          "\xff\xff\xff\xff\xff\xff\xff\xff"
                          "\x04\x05\x06\x07\x0c\x0d\x0e\x0f"
                          "\x14\x15\x16\x17\x1c\x1d\x1e\x1f";

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 2*sizeof(r0[0]), k++) {
    /* Load 8 byte groups to the structures */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        r0[l].val[j] = vld1_u8(src + total_elements*(j+l*4) + k*sizeof(r0[l].val[j]));
      }
    }
    /* Rearragnement of the structures distribution  */
      printf("\nReordenamos\n");
    for(j = 0; j < 4; j++) {
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[0], vld1_u8(index_top + j*sizeof(r1[0].val[j])));
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[1], vld1_u8(index_top + sizeof(r1[0]) +j*sizeof(r1[0].val[j])));
      printf("r1[0].val[%d] = ", j);
      printmem8(r1[0].val[j]);
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[0], vld1_u8(index_bottom +j*sizeof(r1[1].val[j])));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[1], vld1_u8(index_bottom + sizeof(r1[1]) +j*sizeof(r1[1 ].val[j])));
      printf("\t\t\tr1[1].val[%d] = ", j);
      printmem8(r1[1].val[j]);
    }
    /* Table look up to put together bytes */
    for (j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        r0[l].val[j] = vtbl4_u8(r1[l], vld1_u8(index + j*sizeof(r0[l].val[j])));
      }
    }
    /* Store the results in the destination vector */
    for(j = 0; j < 4; j++) {
      for(l = 0; l < 2; l++) {
        vst1_u8(dest + i + l*sizeof(r0[l]) + j*sizeof(r0[l].val[j]), r0[l].val[j]);
      }
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


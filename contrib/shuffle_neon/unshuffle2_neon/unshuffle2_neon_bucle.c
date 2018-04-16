#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <string.h>

static void printmem16(uint8x16_t buf)
{
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

static void
unshuffle2_neon(uint8_t* const dest, const uint8_t* const src,
                const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 2;
  uint8x16x2_t r0;

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += sizeof(r0), k++) {
    /* Load 32 bytes to the structure r0 */
    for(j = 0; j < 2; j++) {
      r0.val[j] = vld1q_u8(src + total_elements*j + k*sizeof(r0.val[j]));
    }
    /* Store (with permutation) the results in the destination vector */
    vst2q_u8(dest+k*sizeof(r0),r0);
  }
}

main() {

 uint8_t *src = "\xcb\xf1\x24\xb1\x69\xee\x99\x7a\x45\x5f\xa2\x41\x77\xfd\x19\x38"
                 "\x56\xab\x61\x7d\xbb\xf6\x29\xe7\x73\xd3\x3f\x46\xba\x49\x71\x35"
                 "\x13\x21\x17"
                 "\xff\x79\x7c\x58\xd2\xdd\x9a\x86\x3e\xdf\x43\x25\xae\x22\x1a\x2b"
                 "\x93\xc3\xa8\xfc\x98\xd1\xce\x58\x4c\x12\xcf\x94\xfa\x83\x1e\x5f"
                 "\xc8\xc9\x34";

  uint8_t *dest = calloc(70,2);
  size_t vectorizable_elements = 32;
  size_t total_elements = 35;

  unshuffle2_neon(dest, src, vectorizable_elements, total_elements);
  printf("\n");
  printmem(dest);
  printmem(dest+32);
  printmem(dest+64);
  printmem(dest+96);
  printmem(dest+128);

  free(dest);
}

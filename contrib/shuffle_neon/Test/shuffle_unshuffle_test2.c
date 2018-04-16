#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <assert.h>

/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 2;
  uint8x16x2_t r0;

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 32, k++) {
    /* Load (and permute) 32 bytes to the structure r0 */
    r0 = vld2q_u8(src + i);
    /* Store the results in the destination vector */
    vst1q_u8(dest + total_elements*0 + k*16, r0.val[0]);
    vst1q_u8(dest + total_elements*1 + k*16, r0.val[1]);
  }
}
/* Routine optimized for unshuffling a buffer for a type size of 2 bytes. */
static void
unshuffle2_neon(uint8_t* const dest, const uint8_t* const src,
                const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 2;
  uint8x16x2_t r0;

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 32, k++) {
    /* Load 32 bytes to the structure r0 */
    r0.val[0] = vld1q_u8(src + total_elements*0 + k*16);
    r0.val[1] = vld1q_u8(src + total_elements*1 + k*16);
    /* Store (with permutation) the results in the destination vector */
    vst2q_u8(dest + k*32, r0);
  }
}

main()
{
  uint8_t *src = "\xcb\xff\xf1\x79\x24\x7c\xb1\x58\x69\xd2\xee\xdd\x99\x9a\x7a\x86"
                 "\x45\x3e\x5f\xdf\xa2\x43\x41\x25\x77\xae\xfd\x22\x19\x1a\x38\x2b"
                 "\x56\x93\xab\xc3\x61\xa8\x7d\xfc\xbb\x98\xf6\xd1\x29\xce\xe7\x58"
                 "\x73\x4c\xd3\x12\x3f\xcf\x46\x94\xba\xfa\x49\x83\x71\x1e\x35\x5f"
                 "\x13\x21\x17\xc8\xc9\x34";

  size_t vectorizable_elements = 32;
  size_t total_elements = 35;
  size_t i;

  uint8_t *dest1 = calloc(70,2);
  uint8_t *dest2 = calloc(70,2);

  shuffle2_neon(dest1, src, vectorizable_elements, total_elements);
  unshuffle2_neon(dest2, dest1, vectorizable_elements, total_elements);

  for (i = 0; i < 64; i++) {
    assert(dest2[i] == src[i]);
  }

  free(dest1);
  free(dest2);
}

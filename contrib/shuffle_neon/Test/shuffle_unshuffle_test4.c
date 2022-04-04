#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <assert.h>

/* Routine optimized for shuffling a buffer for a type size of 4 bytes. */
static void
shuffle4_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 4;
  uint8x16x4_t r0;

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* Load (and permute) 64 bytes to the structure r0 */
    r0 = vld4q_u8(src + i);
    /* Store the results in the destination vector */
    vst1q_u8(dest + total_elements*0 + k*16, r0.val[0]);
    vst1q_u8(dest + total_elements*1 + k*16, r0.val[1]);
    vst1q_u8(dest + total_elements*2 + k*16, r0.val[2]);
    vst1q_u8(dest + total_elements*3 + k*16, r0.val[3]);
  }
}
/* Routine optimized for unshuffling a buffer for a type size of 4 bytes. */
static void
unshuffle4_neon(uint8_t* const dest, const uint8_t* const src,
                const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 4;
  uint8x16x4_t r0;

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* load 64 bytes to the structure r0 */
    r0.val[0] = vld1q_u8(src + total_elements*0 + k*16);
    r0.val[1] = vld1q_u8(src + total_elements*1 + k*16);
    r0.val[2] = vld1q_u8(src + total_elements*2 + k*16);
    r0.val[3] = vld1q_u8(src + total_elements*3 + k*16);
    /* Store (with permutation) the results in the destination vector */
    vst4q_u8(dest + k*64, r0);
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
                 "\x13\x21\x17\xc8\xc9\x34\x25\x11\x67\x74\x4e\xe8\x67\x74\x4e\xe8";

  size_t vectorizable_elements = 32;
  size_t total_elements = 36;
  size_t i;

  uint8_t *dest1 = calloc(144,2);
  uint8_t *dest2 = calloc(144,2);

  shuffle4_neon(dest1, src, vectorizable_elements, total_elements);
  unshuffle4_neon(dest2, dest1, vectorizable_elements, total_elements);

  for (i = 0; i < 128; i++) {
    assert(dest2[i] == src[i]);
  }

  free(dest1);
  free(dest2);
}

#include <stdlib.h>
#include <stdint.h>
#include <arm_neon.h>
#include <assert.h>

/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
shuffle8_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 8;
  uint8x8x2_t r0[4];
  uint16x4x2_t r1[4];
  uint32x2x2_t r2[4];

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
    r0[0] = vzip_u8(vld1_u8(src + i + 0*8), vld1_u8(src + i +1*8));
    r0[1] = vzip_u8(vld1_u8(src + i + 2*8), vld1_u8(src + i +3*8));
    r0[2] = vzip_u8(vld1_u8(src + i + 4*8), vld1_u8(src + i +5*8));
    r0[3] = vzip_u8(vld1_u8(src + i + 6*8), vld1_u8(src + i +7*8));
    /* Interleave 16 bytes */
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    /* Interleave 32 bytes */
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
    /* Store the results in the destination vector */
    vst1_u8(dest + k*8 + 0*total_elements, vreinterpret_u8_u32(r2[0].val[0]));
    vst1_u8(dest + k*8 + 1*total_elements, vreinterpret_u8_u32(r2[0].val[1]));
    vst1_u8(dest + k*8 + 2*total_elements, vreinterpret_u8_u32(r2[1].val[0]));
    vst1_u8(dest + k*8 + 3*total_elements, vreinterpret_u8_u32(r2[1].val[1]));
    vst1_u8(dest + k*8 + 4*total_elements, vreinterpret_u8_u32(r2[2].val[0]));
    vst1_u8(dest + k*8 + 5*total_elements, vreinterpret_u8_u32(r2[2].val[1]));
    vst1_u8(dest + k*8 + 6*total_elements, vreinterpret_u8_u32(r2[3].val[0]));
    vst1_u8(dest + k*8 + 7*total_elements, vreinterpret_u8_u32(r2[3].val[1]));
  }
}

/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
unshuffle8_neon(uint8_t* const dest, const uint8_t* const src,
                const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k;
  static const size_t bytesoftype = 8;
  uint8x8x2_t r0[4];
  uint16x4x2_t r1[4];
  uint32x2x2_t r2[4];

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
    r0[0] = vzip_u8(vld1_u8(src + 0*total_elements + k*8), vld1_u8(src + 1*total_elements + k*8));
    r0[1] = vzip_u8(vld1_u8(src + 2*total_elements + k*8), vld1_u8(src + 3*total_elements + k*8));
    r0[2] = vzip_u8(vld1_u8(src + 4*total_elements + k*8), vld1_u8(src + 5*total_elements + k*8));
    r0[3] = vzip_u8(vld1_u8(src + 6*total_elements + k*8), vld1_u8(src + 7*total_elements + k*8));
    /* Interleave 16 bytes */
    r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
    r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
    r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
    r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
    /* Interleave 32 bytes */
    r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
    r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
    r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
    r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
    /* Store the results in the destination vector */
    vst1_u8(dest + i + 0*8, vreinterpret_u8_u32(r2[0].val[0]));
    vst1_u8(dest + i + 1*8, vreinterpret_u8_u32(r2[0].val[1]));
    vst1_u8(dest + i + 2*8, vreinterpret_u8_u32(r2[1].val[0]));
    vst1_u8(dest + i + 3*8, vreinterpret_u8_u32(r2[1].val[1]));
    vst1_u8(dest + i + 4*8, vreinterpret_u8_u32(r2[2].val[0]));
    vst1_u8(dest + i + 5*8, vreinterpret_u8_u32(r2[2].val[1]));
    vst1_u8(dest + i + 6*8, vreinterpret_u8_u32(r2[3].val[0]));
    vst1_u8(dest + i + 7*8, vreinterpret_u8_u32(r2[3].val[1]));

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

  size_t vectorizable_elements = 32;
  size_t total_elements = 36;
  size_t i;

  uint8_t *dest1 = calloc(288,2);
  uint8_t *dest2 = calloc(288,2);

  shuffle8_neon(dest1, src, vectorizable_elements, total_elements);
  unshuffle8_neon(dest2, dest1, vectorizable_elements, total_elements);

  for (i = 0; i < 256; i++) {
    assert(dest2[i] == src[i]);
  }

  free(dest1);
  free(dest2);
}

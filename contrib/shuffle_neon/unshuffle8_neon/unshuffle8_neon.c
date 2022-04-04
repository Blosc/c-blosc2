/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
void unshuffle8_neon(uint8_t* const dest, const uint8_t* const src,
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


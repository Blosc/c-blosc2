/* Routine for shuffling a buffer for a type size of 8 bytes. Second version (non-optimized) */
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
    /* Load (and permute) 64 bytes to the structure r0*/
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
    /* Store the results in the destination vector */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        vst1_u8(dest + total_elements*(j+l*4) + k*8, r1[l].val[j]);
      }
    }
  }
}


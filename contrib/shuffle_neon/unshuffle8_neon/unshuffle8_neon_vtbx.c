/* Routine for unshuffling a buffer for a type size of 8 bytes. Non-optimized version */
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
    for(j = 0; j < 4; j++) {
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[0], vld1_u8(index_top + j*sizeof(r1[0].val[j])));
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[1], vld1_u8(index_top + sizeof(r1[0]) +j*sizeof(r1[0].val[j])));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[0], vld1_u8(index_bottom +j*sizeof(r1[1].val[j])));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[1], vld1_u8(index_bottom + sizeof(r1[1]) +j*sizeof(r1[1 ].val[j])));
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


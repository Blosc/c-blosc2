/* Routine for shuffling a buffer for a type size of 8 bytes. First version (non-optimized) */
void shuffle8_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 8;
  uint8x8x4_t r0[2], r1[2];

  uint8_t *index = "\x00\x08\x10\x18\x01\x09\x11\x19"
                   "\x02\x0a\x12\x1a\x03\x0b\x13\x1b"
                   "\x04\x0c\x14\x1c\x05\x0d\x15\x1d"
                   "\x06\x0e\x16\x1e\x07\x0f\x17\x1f";

  uint8_t *index_top = "\x00\x01\x02\x03\xff\xff\xff\xff"
                       "\x04\x05\x06\x07\xff\xff\xff\xff"
                       "\x08\x09\x0a\x0b\xff\xff\xff\xff"
                       "\x0c\x0d\x0e\x0f\xff\xff\xff\xff"
                       "\x10\x11\x12\x13\xff\xff\xff\xff"
                       "\x14\x15\x16\x17\xff\xff\xff\xff"
                       "\x18\x19\x1a\x1b\xff\xff\xff\xff"
                       "\x1c\x1d\x1e\x1f\xff\xff\xff\xff";

  uint8_t *index_bottom = "\xff\xff\xff\xff\x00\x01\x02\x03"
                          "\xff\xff\xff\xff\x04\x05\x06\x07"
                          "\xff\xff\xff\xff\x08\x09\x0a\x0b"
                          "\xff\xff\xff\xff\x0c\x0d\x0e\x0f"
                          "\xff\xff\xff\xff\x10\x11\x12\x13"
                          "\xff\xff\xff\xff\x14\x15\x16\x17"
                          "\xff\xff\xff\xff\x18\x19\x1a\x1b"
                          "\xff\xff\xff\xff\x1c\x1d\x1e\x1f";

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 64, k++) {
    /* Load 8 byte groups to the structures */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        r0[l].val[j] = vld1_u8(src + l*32 + k*32 + k*sizeof(r0[l]) + j*sizeof(r0[l].val[j]));
      }
    }
    /* Table look up to separate bytes */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        r1[l].val[j] = vtbl4_u8(r0[l], vld1_u8(index + j*sizeof(r1[l].val[j])));
      }
    }
    /* Rearrange the distribution of the structures */
    for(j = 0; j < 4; j++) {
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[0], vld1_u8(index_top + j*sizeof(r0[0].val[j])));
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[1], vld1_u8(index_bottom + j*sizeof(r0[0].val[j])));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[0], vld1_u8(index_top + 32 + j*sizeof(r0[1].val[j])));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[1], vld1_u8(index_bottom + 32 + j*sizeof(r0[1].val[j])));
    }
    /* Store the results in the destination vector */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 2; l++) {
        vst1_u8(dest + total_elements*(j+l*4) + k*sizeof(r0[l].val[j]), r0[l].val[j]);
      }
    }
  }
}


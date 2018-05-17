/* Routine for shuffling a buffer for a type size of 16 bytes. Non-optimized version */
shuffle16_neon(uint8_t* const dest, const uint8_t* const src,
               const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 16;
  uint8x8x4_t r0[4], r1[4];

  uint8_t *index1_top = "\x00\x10\xff\xff\x01\x11\xff\xff"
                        "\x02\x12\xff\xff\x03\x13\xff\xff"
                        "\x04\x14\xff\xff\x05\x15\xff\xff"
                        "\x06\x16\xff\xff\x07\x17\xff\xff"
                        "\x08\x18\xff\xff\x09\x19\xff\xff"
                        "\x0a\x1a\xff\xff\x0b\x1b\xff\xff"
                        "\x0c\x1c\xff\xff\x0d\x1d\xff\xff"
                        "\x0e\x1e\xff\xff\x0f\x1f\xff\xff";

  uint8_t *index1_bottom = "\xff\xff\x00\x10\xff\xff\x01\x11"
                           "\xff\xff\x02\x12\xff\xff\x03\x13"
                           "\xff\xff\x04\x14\xff\xff\x05\x15"
                           "\xff\xff\x06\x16\xff\xff\x07\x17"
                           "\xff\xff\x08\x18\xff\xff\x09\x19"
                           "\xff\xff\x0a\x1a\xff\xff\x0b\x1b"
                           "\xff\xff\x0c\x1c\xff\xff\x0d\x1d"
                           "\xff\xff\x0e\x1e\xff\xff\x0f\x1f";

  uint8_t *index2_top = "\x00\x01\x02\x03\xff\xff\xff\xff"
                        "\x04\x05\x06\x07\xff\xff\xff\xff"
                        "\x08\x09\x0a\x0b\xff\xff\xff\xff"
                        "\x0c\x0d\x0e\x0f\xff\xff\xff\xff"
                        "\x10\x11\x12\x13\xff\xff\xff\xff"
                        "\x14\x15\x16\x17\xff\xff\xff\xff"
                        "\x18\x19\x1a\x1b\xff\xff\xff\xff"
                        "\x1c\x1d\x1e\x1f\xff\xff\xff\xff";

  uint8_t *index2_bottom = "\xff\xff\xff\xff\x00\x01\x02\x03"
                           "\xff\xff\xff\xff\x04\x05\x06\x07"
                           "\xff\xff\xff\xff\x08\x09\x0a\x0b"
                           "\xff\xff\xff\xff\x0c\x0d\x0e\x0f"
                           "\xff\xff\xff\xff\x10\x11\x12\x13"
                           "\xff\xff\xff\xff\x14\x15\x16\x17"
                           "\xff\xff\xff\xff\x18\x19\x1a\x1b"
                           "\xff\xff\xff\xff\x1c\x1d\x1e\x1f";

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 128, k++) {
    /* Load 16 groups of 8 bytes to the structures */
    for(j = 0; j < 4; j++) {
      for(l = 0; l < 4; l++) {
        r0[j].val[l] = vld1_u8(src + j*32 + k*128 + l*sizeof(r0[j].val[l]));
       }
    }
    /* Extended table look up to rearragne the distribution of the structures */
    for(j = 0; j < 4; j++) {
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[0], vld1_u8(index1_top + 0*32 + j*sizeof(r1[0].val[j])));
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[1], vld1_u8(index1_bottom + 0*32 + j*sizeof(r1[0].val[j])));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[0], vld1_u8(index1_top + 1*32 + j*sizeof(r0[1].val[j])));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[1], vld1_u8(index1_bottom + 1*32 + j*sizeof(r0[1].val[j])));
      r1[2].val[j] = vtbx4_u8(r1[2].val[j], r0[2], vld1_u8(index1_top + 0*32 + j*sizeof(r1[2].val[j])));
      r1[2].val[j] = vtbx4_u8(r1[2].val[j], r0[3], vld1_u8(index1_bottom + 0*32 + j*sizeof(r1[2].val[j])));
      r1[3].val[j] = vtbx4_u8(r1[3].val[j], r0[2], vld1_u8(index1_top + 1*32 + j*sizeof(r0[3].val[j])));
      r1[3].val[j] = vtbx4_u8(r1[3].val[j], r0[3], vld1_u8(index1_bottom + 1*32 + j*sizeof(r0[3].val[j])));
    }
    for(j = 0; j < 4; j++) {
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[0], vld1_u8(index2_top + 0*32 + j*sizeof(r0[0].val[j])));
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[2], vld1_u8(index2_bottom + 0*32 + j*sizeof(r0[0].val[j])));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[0], vld1_u8(index2_top + 1*32 + j*sizeof(r0[1].val[j])));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[2], vld1_u8(index2_bottom + 1*32 + j*sizeof(r0[1].val[j])));
      r0[2].val[j] = vtbx4_u8(r0[2].val[j], r1[1], vld1_u8(index2_top + 0*32 + j*sizeof(r0[2].val[j])));
      r0[2].val[j] = vtbx4_u8(r0[2].val[j], r1[3], vld1_u8(index2_bottom + 0*32 + j*sizeof(r0[2].val[j])));
      r0[3].val[j] = vtbx4_u8(r0[3].val[j], r1[1], vld1_u8(index2_top + 1*32 + j*sizeof(r0[3].val[j])));
      r0[3].val[j] = vtbx4_u8(r0[3].val[j], r1[3], vld1_u8(index2_bottom + 1*32 + j*sizeof(r0[3].val[j])));
    }
    /* Store the results in the destination vector */
    for(j = 0; j < 4; j++) {
      for(l = 0; l < 4; l++) {
        vst1_u8(dest + total_elements*(j+l*4) + k*sizeof(r0[l].val[j]), r0[l].val[j]);
      }
    }
  }
}


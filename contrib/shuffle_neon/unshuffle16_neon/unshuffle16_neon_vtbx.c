/* Routine for unshuffling a buffer for a type size of 16 bytes. Non-optimized version*/
void unshuffle16_neon(uint8_t* const dest, const uint8_t* const src,
                 const size_t vectorizable_elements, const size_t total_elements)
{
  size_t i, j, k, l;
  static const size_t bytesoftype = 16;
  uint8x8x4_t r0[4], r1[4];

  uint8_t *index1_top = "\x00\x01\x02\x03\x08\x09\x0a\x0b"
                        "\x10\x11\x12\x13\x18\x19\x1a\x1b"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\xff\xff\xff\xff\xff\xff\xff\xff"
                        "\x00\x01\x02\x03\x08\x09\x0a\x0b"
                        "\x10\x11\x12\x13\x18\x19\x1a\x1b";

  uint8_t *index1_bottom = "\x04\x05\x06\x07\x0c\x0d\x0e\x0f"
                           "\x14\x15\x16\x17\x1c\x1d\x1e\x1f"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\xff\xff\xff\xff\xff\xff\xff\xff"
                           "\x04\x05\x06\x07\x0c\x0d\x0e\x0f"
                           "\x14\x15\x16\x17\x1c\x1d\x1e\x1f";

  uint8_t *index2_top = "\x00\x10\xff\xff\x04\x14\xff\xff"
                        "\x08\x18\xff\xff\x0c\x1c\xff\xff"
                        "\x01\x11\xff\xff\x05\x15\xff\xff"
                        "\x09\x19\xff\xff\x0d\x1d\xff\xff"
                        "\xff\xff\x00\x10\xff\xff\x04\x14"
                        "\xff\xff\x08\x18\xff\xff\x0c\x1c"
                        "\xff\xff\x01\x11\xff\xff\x05\x15"
                        "\xff\xff\x09\x19\xff\xff\x0d\x1d";

  uint8_t *index2_bottom = "\x02\x12\xff\xff\x06\x16\xff\xff"
                           "\x0a\x1a\xff\xff\x0e\x1e\xff\xff"
                           "\x03\x13\xff\xff\x07\x17\xff\xff"
                           "\x0b\x1b\xff\xff\x0f\x1f\xff\xff"
                           "\xff\xff\x02\x12\xff\xff\x06\x16"
                           "\xff\xff\x0a\x1a\xff\xff\x0e\x1e"
                           "\xff\xff\x03\x13\xff\xff\x07\x17"
                           "\xff\xff\x0b\x1b\xff\xff\x0f\x1f";

  for(i = 0, k = 0; i < vectorizable_elements*bytesoftype; i += 128, k++) {
    /* Load 16 groups of 8 bytes to the structures */
    for(j = 0; j < 4; j++) {
      for (l = 0; l < 4; l++) {
        r0[j].val[l] = vld1_u8(src + j*total_elements + l*4*total_elements + k*8);
      }
    }
    /* Extended table look up to rearrange the distribution of the structures */
    for(j = 0; j < 4; j++) {
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[0], vld1_u8(index1_top +j*8));
      r1[0].val[j] = vtbx4_u8(r1[0].val[j], r0[1], vld1_u8(index1_top + 32 + j*8));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[2], vld1_u8(index1_top + j*8));
      r1[1].val[j] = vtbx4_u8(r1[1].val[j], r0[3], vld1_u8(index1_top + 32 + j*8));
      r1[2].val[j] = vtbx4_u8(r1[2].val[j], r0[0], vld1_u8(index1_bottom + j*8));
      r1[2].val[j] = vtbx4_u8(r1[2].val[j], r0[1], vld1_u8(index1_bottom + 32 + j*8));
      r1[3].val[j] = vtbx4_u8(r1[3].val[j], r0[2], vld1_u8(index1_bottom + j*8));
      r1[3].val[j] = vtbx4_u8(r1[3].val[j], r0[3], vld1_u8(index1_bottom + 32 + j*8));
    }
    for(j = 0; j < 4; j++) {
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[0], vld1_u8(index2_top + j*8));
      r0[0].val[j] = vtbx4_u8(r0[0].val[j], r1[1], vld1_u8(index2_top + 32 + j*8));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[0], vld1_u8(index2_bottom + j*8));
      r0[1].val[j] = vtbx4_u8(r0[1].val[j], r1[1], vld1_u8(index2_bottom + 32 + j*8));
      r0[2].val[j] = vtbx4_u8(r0[2].val[j], r1[2], vld1_u8(index2_top + j*8));
      r0[2].val[j] = vtbx4_u8(r0[2].val[j], r1[3], vld1_u8(index2_top + 32 + j*8));
      r0[3].val[j] = vtbx4_u8(r0[3].val[j], r1[2], vld1_u8(index2_bottom + j*8));
      r0[3].val[j] = vtbx4_u8(r0[3].val[j], r1[3], vld1_u8(index2_bottom + 32 + j*8));
    }
    for(j = 0; j < 4; j++) {
      for(l = 0; l < 4; l++) {
        vst1_u8(dest + l*8 + j*32 + i, r0[j].val[l]);
      }
    }
  }
}


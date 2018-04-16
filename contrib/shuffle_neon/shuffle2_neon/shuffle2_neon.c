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


/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  *       Jerome Kieffer <jerome.kieffer@esrf.fr>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "shuffle-generic.h"
#include "shuffle-altivec.h"

/* Make sure ALTIVEC is available for the compilation target and compiler. */
#if !defined(__ALTIVEC__)
  #error ALTIVEC is not supported by the target architecture/platform and/or this compiler.
#endif

#include <emmintrin.h>
#include <altivec.h>
#include <stdio.h>


static const __vector uint8_t even = (const __vector uint8_t) {0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
                                                               0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e};
static const __vector uint8_t odd = (const __vector uint8_t) {0x01, 0x03, 0x05, 0x07, 0x09, 0x0b, 0x0d, 0x0f,
                                                              0x11, 0x13, 0x15, 0x17, 0x19, 0x1b, 0x1d, 0x1f};


// Store a vector to an unaligned location in memory ... not optimal nor thread-safe
static void vec_st_unaligned(__vector uint8_t v, int32_t position,  __vector uint8_t * dst){
  // Load the surrounding area
  __vector uint8_t low = vec_ld(position, dst);
  __vector uint8_t high = vec_ld(position + 16, dst);
  // Prepare the constants that we need
  uint8_t offset = ((size_t)dst) % 16;
  __vector uint8_t permute_lo, permute_hi;
  for (uint8_t i = 0; i < 16; i++){
      if (i<offset){
          permute_lo[i] = i;
          permute_hi[i] = 32 + i - offset;
      }
      else{
          permute_lo[i] = 16 + i - offset;
          permute_hi[i] = i;
      }
  }
  // Insert our data into the low and high vectors
  low = vec_perm(low, v, permute_lo);
  high = vec_perm(high, v, permute_hi);
  // Store the two aligned result vectors
  vec_st(low, position, dst);
  vec_st(high, position + 16, dst);
}


/* Missaligned storage helper function with better performances
 * 
 * param:
 *     data: the (misaligned) vector to store
 *     position: index in dst where to store
 *     dst: the pointer of where to store
 *     previous: the remaining vector of the last write
 *     shuffle_lo: the vector for shuffling the lower part
 *     shuffle_hi: the vector for shuffling the higher part
 * 
 * return: The remainder to be still written in next vector filled with zeros
 */
static inline __vector uint8_t helper_misaligned_store(__vector uint8_t data, 
                                                       int32_t position,  
                                                       uint8_t * dst,
                                                       __vector uint8_t previous,
                                                       __vector uint8_t shuffle_lo,
                                                       __vector uint8_t shuffle_hi){  
                                                         
  const __vector uint8_t zero = vec_splat_u8(0);
  __vector uint8_t low, high;
  // Insert our data into the low and high vectors
  low = vec_perm(previous, data, shuffle_lo);
  high = vec_perm(zero, data, shuffle_hi);
  // Store only the lower part
  vec_st(low, position, dst);
  // and return the higher part which is the next "previous" vector
  return high;
}                                                 

/* Calculate the permutation vector for storing data in the lower part
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_low(int32_t offset){
  __vector uint8_t permute;
  for (uint8_t i = 0; i < 16; i++){
    if (i < offset) 
      permute[i] = i;
    else 
      permute[i] = 16 + i - offset;
  }
  return permute;
} 

/* Calculate the permutation vector for storing data in the upper part
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_high(int32_t offset){
  __vector uint8_t permute;
  for (int32_t i = 0; i < 16; i++)
  {
    if (i < offset) 
      permute[i] = 32 + i - offset;
    else 
      permute[i] = i;
  }
  return permute;
} 

/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  const int32_t bytesoftype = 2;
  const int32_t offset = total_elements % 16;
  static __vector uint8_t permute_lo, permute_hi;
  int32_t j;
  __vector uint8_t inp0, inp1, out0, out1, low;
  uint8_t* const dest1 = &dest[total_elements];
  
  //printf("vectorizable_elements: %d total_elements: %d dst: %d dst1: %d delta: %d offset: %d\n", vectorizable_elements, total_elements, dest, dest1, dest1-dest, offset);
  
  if (offset)
  {
     // Initialize the two permutation vectors
     permute_lo = gen_permute_low(offset);
     permute_hi = gen_permute_high(offset);
     low = vec_splat_u8(0);
  }
  for (j = 0; j < vectorizable_elements; j += 16){
    /* Fetch 16 elements (32 bytes) */
    inp0 = vec_ld(bytesoftype * j, src);
    inp1 = vec_ld(bytesoftype * j + 16, src);
    /* Transpose vectors */
    out0 = vec_perm(inp0, inp1, even);
    out1 = vec_perm(inp0, inp1, odd);
    /* Store the result vectors */
    vec_st(out0, j, dest);
    if (offset)
      low = helper_misaligned_store(out1, j, dest1, low, permute_lo, permute_hi);
    else
      vec_st(out1, j, dest1);
  }
  // Store the reminder 
  if (offset)
      vec_st(low, j, dest1);
}

/* Routine optimized for shuffling a buffer for a type size of 4 bytes. */
static void
shuffle4_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  static const int32_t bytesoftype = 4;
  int32_t i, j, k;
  uint8_t offset[4];
  uint8_t* desti[4];
  __vector uint8_t inp[4], out[4], perm_lo[4], perm_hi[4], low[4];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < 4; i++){
    offset[i] = (i*total_elements) % 16;
    desti[i] = &dest[i * total_elements];
    if (offset[i]){
      perm_lo[i] = gen_permute_low(offset[i]);
      perm_hi[i] = gen_permute_high(offset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  //printf("vectorizable_elements: %d total_elements: %d dst: %d offset1: %d offset2: %d offset3: %d\n", vectorizable_elements, total_elements, dest, offset[1], offset[2], offset[3]);
  
  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (64 bytes, 4 vectors) */
    for (i = 0; i < 4; i++){
      inp[i] = vec_ld((bytesoftype * j) + (i * 16), src);
    }
    
    /* Transpose vectors 0-1*/
    for (i = 0; i < 4; i += 2){
      out[i  ] = vec_perm(inp[i], inp[i+1], even);
      out[i+1] = vec_perm(inp[i], inp[i+1], odd);
    }
    /* Transpose vectors 0-2*/
    inp[0] = vec_perm(out[0], out[2], even);
    inp[1] = vec_perm(out[1], out[3], even);
    inp[2] = vec_perm(out[0], out[2], odd);
    inp[3] = vec_perm(out[1], out[3], odd);
    
    /* Store the result vectors */
    for (i = 0; i < 4; i ++){
      if (offset[i])
        low[i] = helper_misaligned_store(inp[i], j, desti[i], low[i], perm_lo[i], perm_hi[i]);
      else
        vec_st(inp[i], j, desti[i]);
    }        
  }
  /* Store the remainder */
  for (i = 0; i < 4; i++){
    if (offset[i]){
      __vector uint8_t mask;
      for (k = 0; k < 16; k++)
        mask[k] = (k<offset[i])?0:0xFF;
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), mask), j, desti[i]);   
    }
  }
}


/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
static void
shuffle8_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 8;
  int32_t i, j, k;
  uint8_t offset[8];
  uint8_t* desti[8];
  __vector uint8_t inp[8], out[8], perm_lo[8], perm_hi[8], low[8];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < 8; i++){
    offset[i] = (i*total_elements) %16;
    desti[i] = &dest[i*total_elements];
    if (offset[i]){
      perm_lo[i] = gen_permute_low(offset[i]);
      perm_hi[i] = gen_permute_high(offset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  //printf("vectorizable_elements: %d total_elements: %d dst: %d offset1: %d offset2: %d offset3: %d\n", vectorizable_elements, total_elements, dest, offset[1], offset[2], offset[3]);
  
  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (128 bytes, 8 vectors) */
    for (i = 0; i < 8; i++){
      inp[i] = vec_ld((bytesoftype * j) + (i * 16), src);
    }
    
    /* Transpose vectors 0-1*/
    for (i = 0; i < 8; i += 2){
      out[i] = vec_perm(inp[i], inp[i+1], even);
      out[i+1] = vec_perm(inp[i], inp[i+1], odd);
    }
    /* Transpose vectors 0-2*/
    for (i = 0; i < 8; i += 4){
      for (k = 0; k<2; k++){
        inp[i+k] = vec_perm(out[i+k], out[i+k+2], even);
        inp[i+k+2] = vec_perm(out[i+k], out[i+k+2], odd);
      }
    }
    /* Transpose vectors 0-4*/
    for (k = 0; k<4; k++){
    out[k] = vec_perm(inp[k], inp[k+4], even);
    out[k+4] = vec_perm(inp[k], inp[k+4], odd);
    }
    /* Store the result vectors */
    for (i = 0; i < 8; i ++){
      if (offset[i])
        low[i] = helper_misaligned_store(out[i], j, desti[i], low[i], perm_lo[i], perm_hi[i]);
      else
        vec_st(out[i], j, desti[i]);
    }
  }
  /* Store the remainder */
  for (i = 0; i < 8; i++){
    if (offset[i]){
      __vector uint8_t mask;
      for (k = 0; k < 16; k++)
        mask[k] = (k<offset[i])?0:0xFF;
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), mask), j, desti[i]);   
    }
  }
}

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
static void
shuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                  const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 16;
  int32_t i, j, k;
  uint8_t offset[16];
  uint8_t* desti[16];
  __vector uint8_t inp[16], out[16], perm_lo[16], perm_hi[16], low[16];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < 16; i++){
    offset[i] = (i*total_elements) %16;
    desti[i] = &dest[i*total_elements];
    if (offset[i]){
      perm_lo[i] = gen_permute_low(offset[i]);
      perm_hi[i] = gen_permute_high(offset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  printf("vectorizable_elements: %d total_elements: %d dst: %d offset1: %d offset2: %d offset3: %d\n", vectorizable_elements, total_elements, dest, offset[1], offset[2], offset[3]);
  
  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (256 bytes, 16 vectors) */
    for (i = 0; i < 16; i++){
      inp[i] = vec_ld((bytesoftype * j) + (i * 16), src);
    }

    /* Transpose vectors 0-1*/
    for (i = 0; i < 16; i += 2){
      out[i] = vec_perm(inp[i], inp[i+1], even);
      out[i+1] = vec_perm(inp[i], inp[i+1], odd);
    }
    /* Transpose vectors 0-2*/
    for (i = 0; i < 16; i += 4){
      for (k = 0; k<2; k++){
        inp[i+k] = vec_perm(out[i+k], out[i+k+2], even);
        inp[i+k+2] = vec_perm(out[i+k], out[i+k+2], odd);
      }
    }
    /* Transpose vectors 0-4*/
    for (i = 0; i < 16; i += 8){
      for (k = 0; k<4; k++){
      out[i+k] = vec_perm(inp[i+k], inp[i+k+4], even);
      out[i+k+4] = vec_perm(inp[i+k], inp[i+k+4], odd);
      }
    }
    /* Transpose vectors 0-8*/
    for (k = 0; k<8; k++){
      inp[k] = vec_perm(out[k], out[k+8], even);
      inp[k+8] = vec_perm(out[k], out[k+8], odd);
      }
    /* Store the result vectors */
    for (i = 0; i < 16; i ++){
      if (offset[i])
        low[i] = helper_misaligned_store(inp[i], j, desti[i], low[i], perm_lo[i], perm_hi[i]);
      else
        vec_st(inp[i], j, desti[i]);
    }
  }
  /* Store the remainder */
  for (i = 0; i < 16; i++){
    if (offset[i]){
      __vector uint8_t mask;
      for (k = 0; k < 16; k++)
        mask[k] = (k<offset[i])?0:0xFF;
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), mask), j, desti[i]);   
    }
  }
}

/* Routine optimized for shuffling a buffer for a type size larger than 16 bytes. */
static void
shuffle16_tiled_altivec(uint8_t* const dest, const uint8_t* const src,
                        const int32_t vectorizable_elements, const int32_t total_elements, const int32_t bytesoftype) {
  int32_t j;
  const int32_t vecs_per_el_rem = bytesoftype % sizeof(__m128i);
  int k, l;
  uint8_t* dest_for_jth_element;
  __m128i xmm0[16], xmm1[16];

  for (j = 0; j < vectorizable_elements; j += sizeof(__m128i)) {
    /* Advance the offset into the type by the vector size (in bytes), unless this is
    the initial iteration and the type size is not a multiple of the vector size.
    In that case, only advance by the number of bytes necessary so that the number
    of remaining bytes in the type will be a multiple of the vector size. */
    int32_t offset_into_type;
    for (offset_into_type = 0; offset_into_type < bytesoftype;
         offset_into_type += (offset_into_type == 0 &&
                              vecs_per_el_rem > 0 ? vecs_per_el_rem : (int32_t)sizeof(__m128i))) {

      /* Fetch elements in groups of 256 bytes */
      const uint8_t* const src_with_offset = src + offset_into_type;
      for (k = 0; k < 16; k++) {
        xmm0[k] = _mm_loadu_si128((__m128i*)(src_with_offset + (j + k) * bytesoftype));
      }
      /* Transpose bytes */
      for (k = 0, l = 0; k < 8; k++, l += 2) {
        xmm1[k * 2] = _mm_unpacklo_epi8(xmm0[l], xmm0[l + 1]);
        xmm1[k * 2 + 1] = _mm_unpackhi_epi8(xmm0[l], xmm0[l + 1]);
      }
      /* Transpose words */
      for (k = 0, l = -2; k < 8; k++, l++) {
        if ((k % 2) == 0) l += 2;
        xmm0[k * 2] = _mm_unpacklo_epi16(xmm1[l], xmm1[l + 2]);
        xmm0[k * 2 + 1] = _mm_unpackhi_epi16(xmm1[l], xmm1[l + 2]);
      }
      /* Transpose double words */
      for (k = 0, l = -4; k < 8; k++, l++) {
        if ((k % 4) == 0) l += 4;
        xmm1[k * 2] = _mm_unpacklo_epi32(xmm0[l], xmm0[l + 4]);
        xmm1[k * 2 + 1] = _mm_unpackhi_epi32(xmm0[l], xmm0[l + 4]);
      }
      /* Transpose quad words */
      for (k = 0; k < 8; k++) {
        xmm0[k * 2] = _mm_unpacklo_epi64(xmm1[k], xmm1[k + 8]);
        xmm0[k * 2 + 1] = _mm_unpackhi_epi64(xmm1[k], xmm1[k + 8]);
      }
      /* Store the result vectors */
      dest_for_jth_element = dest + j;
      for (k = 0; k < 16; k++) {
        _mm_storeu_si128((__m128i*)(dest_for_jth_element + (total_elements * (offset_into_type + k))), xmm0[k]);
      }
    }
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 2 bytes. */
static void
unshuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 2;
  int32_t i;
  int j;
  __m128i xmm0[2], xmm1[2];

  for (i = 0; i < vectorizable_elements; i += sizeof(__m128i)) {
    /* Load 16 elements (32 bytes) into 2 XMM registers. */
    const uint8_t* const src_for_ith_element = src + i;
    for (j = 0; j < 2; j++) {
      xmm0[j] = _mm_loadu_si128((__m128i*)(src_for_ith_element + (j * total_elements)));
    }
    /* Shuffle bytes */
    /* Compute the low 32 bytes */
    xmm1[0] = _mm_unpacklo_epi8(xmm0[0], xmm0[1]);
    /* Compute the hi 32 bytes */
    xmm1[1] = _mm_unpackhi_epi8(xmm0[0], xmm0[1]);
    /* Store the result vectors in proper order */
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (0 * sizeof(__m128i))), xmm1[0]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (1 * sizeof(__m128i))), xmm1[1]);
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 4 bytes. */
static void
unshuffle4_altivec(uint8_t* const dest, const uint8_t* const src,
                const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 4;
  int32_t i;
  int j;
  __m128i xmm0[4], xmm1[4];

  for (i = 0; i < vectorizable_elements; i += sizeof(__m128i)) {
    /* Load 16 elements (64 bytes) into 4 XMM registers. */
    const uint8_t* const src_for_ith_element = src + i;
    for (j = 0; j < 4; j++) {
      xmm0[j] = _mm_loadu_si128((__m128i*)(src_for_ith_element + (j * total_elements)));
    }
    /* Shuffle bytes */
    for (j = 0; j < 2; j++) {
      /* Compute the low 32 bytes */
      xmm1[j] = _mm_unpacklo_epi8(xmm0[j * 2], xmm0[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[2 + j] = _mm_unpackhi_epi8(xmm0[j * 2], xmm0[j * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (j = 0; j < 2; j++) {
      /* Compute the low 32 bytes */
      xmm0[j] = _mm_unpacklo_epi16(xmm1[j * 2], xmm1[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm0[2 + j] = _mm_unpackhi_epi16(xmm1[j * 2], xmm1[j * 2 + 1]);
    }
    /* Store the result vectors in proper order */
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (0 * sizeof(__m128i))), xmm0[0]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (1 * sizeof(__m128i))), xmm0[2]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (2 * sizeof(__m128i))), xmm0[1]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (3 * sizeof(__m128i))), xmm0[3]);
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
static void
unshuffle8_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 8;
  int32_t i;
  int j;
  __m128i xmm0[8], xmm1[8];

  for (i = 0; i < vectorizable_elements; i += sizeof(__m128i)) {
    /* Load 16 elements (128 bytes) into 8 XMM registers. */
    const uint8_t* const src_for_ith_element = src + i;
    for (j = 0; j < 8; j++) {
      xmm0[j] = _mm_loadu_si128((__m128i*)(src_for_ith_element + (j * total_elements)));
    }
    /* Shuffle bytes */
    for (j = 0; j < 4; j++) {
      /* Compute the low 32 bytes */
      xmm1[j] = _mm_unpacklo_epi8(xmm0[j * 2], xmm0[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[4 + j] = _mm_unpackhi_epi8(xmm0[j * 2], xmm0[j * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (j = 0; j < 4; j++) {
      /* Compute the low 32 bytes */
      xmm0[j] = _mm_unpacklo_epi16(xmm1[j * 2], xmm1[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm0[4 + j] = _mm_unpackhi_epi16(xmm1[j * 2], xmm1[j * 2 + 1]);
    }
    /* Shuffle 4-byte dwords */
    for (j = 0; j < 4; j++) {
      /* Compute the low 32 bytes */
      xmm1[j] = _mm_unpacklo_epi32(xmm0[j * 2], xmm0[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[4 + j] = _mm_unpackhi_epi32(xmm0[j * 2], xmm0[j * 2 + 1]);
    }
    /* Store the result vectors in proper order */
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (0 * sizeof(__m128i))), xmm1[0]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (1 * sizeof(__m128i))), xmm1[4]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (2 * sizeof(__m128i))), xmm1[2]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (3 * sizeof(__m128i))), xmm1[6]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (4 * sizeof(__m128i))), xmm1[1]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (5 * sizeof(__m128i))), xmm1[5]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (6 * sizeof(__m128i))), xmm1[3]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (7 * sizeof(__m128i))), xmm1[7]);
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
static void
unshuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                    const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 16;
  int32_t i;
  int j;
  __m128i xmm1[16], xmm2[16];

  for (i = 0; i < vectorizable_elements; i += sizeof(__m128i)) {
    /* Load 16 elements (256 bytes) into 16 XMM registers. */
    const uint8_t* const src_for_ith_element = src + i;
    for (j = 0; j < 16; j++) {
      xmm1[j] = _mm_loadu_si128((__m128i*)(src_for_ith_element + (j * total_elements)));
    }
    /* Shuffle bytes */
    for (j = 0; j < 8; j++) {
      /* Compute the low 32 bytes */
      xmm2[j] = _mm_unpacklo_epi8(xmm1[j * 2], xmm1[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm2[8 + j] = _mm_unpackhi_epi8(xmm1[j * 2], xmm1[j * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (j = 0; j < 8; j++) {
      /* Compute the low 32 bytes */
      xmm1[j] = _mm_unpacklo_epi16(xmm2[j * 2], xmm2[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[8 + j] = _mm_unpackhi_epi16(xmm2[j * 2], xmm2[j * 2 + 1]);
    }
    /* Shuffle 4-byte dwords */
    for (j = 0; j < 8; j++) {
      /* Compute the low 32 bytes */
      xmm2[j] = _mm_unpacklo_epi32(xmm1[j * 2], xmm1[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm2[8 + j] = _mm_unpackhi_epi32(xmm1[j * 2], xmm1[j * 2 + 1]);
    }
    /* Shuffle 8-byte qwords */
    for (j = 0; j < 8; j++) {
      /* Compute the low 32 bytes */
      xmm1[j] = _mm_unpacklo_epi64(xmm2[j * 2], xmm2[j * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[8 + j] = _mm_unpackhi_epi64(xmm2[j * 2], xmm2[j * 2 + 1]);
    }

    /* Store the result vectors in proper order */
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (0 * sizeof(__m128i))), xmm1[0]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (1 * sizeof(__m128i))), xmm1[8]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (2 * sizeof(__m128i))), xmm1[4]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (3 * sizeof(__m128i))), xmm1[12]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (4 * sizeof(__m128i))), xmm1[2]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (5 * sizeof(__m128i))), xmm1[10]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (6 * sizeof(__m128i))), xmm1[6]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (7 * sizeof(__m128i))), xmm1[14]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (8 * sizeof(__m128i))), xmm1[1]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (9 * sizeof(__m128i))), xmm1[9]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (10 * sizeof(__m128i))), xmm1[5]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (11 * sizeof(__m128i))), xmm1[13]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (12 * sizeof(__m128i))), xmm1[3]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (13 * sizeof(__m128i))), xmm1[11]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (14 * sizeof(__m128i))), xmm1[7]);
    _mm_storeu_si128((__m128i*)(dest + (i * bytesoftype) + (15 * sizeof(__m128i))), xmm1[15]);
  }
}

/* Routine optimized for unshuffling a buffer for a type size larger than 16 bytes. */
static void
unshuffle16_tiled_altivec(uint8_t* const dest, const uint8_t* const orig,
                          const int32_t vectorizable_elements, const int32_t total_elements, const int32_t bytesoftype) {
  int32_t i;
  const int32_t vecs_per_el_rem = bytesoftype % sizeof(__m128i);

  int j;
  uint8_t* dest_with_offset;
  __m128i xmm1[16], xmm2[16];

  /* The unshuffle loops are inverted (compared to shuffle_tiled16_altivec)
     to optimize cache utilization. */
  int32_t offset_into_type;
  for (offset_into_type = 0; offset_into_type < bytesoftype;
       offset_into_type += (offset_into_type == 0 &&
           vecs_per_el_rem > 0 ? vecs_per_el_rem : (int32_t)sizeof(__m128i))) {
    for (i = 0; i < vectorizable_elements; i += sizeof(__m128i)) {
      /* Load the first 128 bytes in 16 XMM registers */
      const uint8_t* const src_for_ith_element = orig + i;
      for (j = 0; j < 16; j++) {
        xmm1[j] = _mm_loadu_si128((__m128i*)(src_for_ith_element + (total_elements * (offset_into_type + j))));
      }
      /* Shuffle bytes */
      for (j = 0; j < 8; j++) {
        /* Compute the low 32 bytes */
        xmm2[j] = _mm_unpacklo_epi8(xmm1[j * 2], xmm1[j * 2 + 1]);
        /* Compute the hi 32 bytes */
        xmm2[8 + j] = _mm_unpackhi_epi8(xmm1[j * 2], xmm1[j * 2 + 1]);
      }
      /* Shuffle 2-byte words */
      for (j = 0; j < 8; j++) {
        /* Compute the low 32 bytes */
        xmm1[j] = _mm_unpacklo_epi16(xmm2[j * 2], xmm2[j * 2 + 1]);
        /* Compute the hi 32 bytes */
        xmm1[8 + j] = _mm_unpackhi_epi16(xmm2[j * 2], xmm2[j * 2 + 1]);
      }
      /* Shuffle 4-byte dwords */
      for (j = 0; j < 8; j++) {
        /* Compute the low 32 bytes */
        xmm2[j] = _mm_unpacklo_epi32(xmm1[j * 2], xmm1[j * 2 + 1]);
        /* Compute the hi 32 bytes */
        xmm2[8 + j] = _mm_unpackhi_epi32(xmm1[j * 2], xmm1[j * 2 + 1]);
      }
      /* Shuffle 8-byte qwords */
      for (j = 0; j < 8; j++) {
        /* Compute the low 32 bytes */
        xmm1[j] = _mm_unpacklo_epi64(xmm2[j * 2], xmm2[j * 2 + 1]);
        /* Compute the hi 32 bytes */
        xmm1[8 + j] = _mm_unpackhi_epi64(xmm2[j * 2], xmm2[j * 2 + 1]);
      }

      /* Store the result vectors in proper order */
      dest_with_offset = dest + offset_into_type;
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 0) * bytesoftype), xmm1[0]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 1) * bytesoftype), xmm1[8]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 2) * bytesoftype), xmm1[4]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 3) * bytesoftype), xmm1[12]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 4) * bytesoftype), xmm1[2]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 5) * bytesoftype), xmm1[10]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 6) * bytesoftype), xmm1[6]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 7) * bytesoftype), xmm1[14]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 8) * bytesoftype), xmm1[1]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 9) * bytesoftype), xmm1[9]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 10) * bytesoftype), xmm1[5]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 11) * bytesoftype), xmm1[13]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 12) * bytesoftype), xmm1[3]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 13) * bytesoftype), xmm1[11]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 14) * bytesoftype), xmm1[7]);
      _mm_storeu_si128((__m128i*)(dest_with_offset + (i + 15) * bytesoftype), xmm1[15]);
    }
  }
}

/* Shuffle a block.  This can never fail. */
void
shuffle_altivec(const int32_t bytesoftype, const int32_t blocksize,
                const uint8_t *_src, uint8_t *_dest) 
{  
	int32_t vectorized_chunk_size;
    vectorized_chunk_size = bytesoftype * 16;


  /* If the blocksize is not a multiple of both the typesize and
     the vector size, round the blocksize down to the next value
     which is a multiple of both. The vectorized shuffle can be
     used for that portion of the data, and the naive implementation
     can be used for the remaining portion. */
  const int32_t vectorizable_bytes = blocksize - (blocksize % vectorized_chunk_size);
  const int32_t vectorizable_elements = vectorizable_bytes / bytesoftype;
  const int32_t total_elements = blocksize / bytesoftype;

  /* If the block size is too small to be vectorized,
     use the generic implementation. */
  if (blocksize < vectorized_chunk_size) {
    shuffle_generic(bytesoftype, blocksize, _src, _dest);
    return;
  }

  /* Optimized shuffle implementations */
  switch (bytesoftype) {
    case 2:
      shuffle2_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    case 4:
      shuffle4_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    case 8:
      shuffle8_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    case 16:
      shuffle16_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    default:
      if (bytesoftype > (int32_t)sizeof(__m128i)) {
        shuffle16_tiled_altivec(_dest, _src, vectorizable_elements, total_elements, bytesoftype);
      }
      else {
        /* Non-optimized shuffle */
        shuffle_generic(bytesoftype, blocksize, _src, _dest);
        /* The non-optimized function covers the whole buffer,
           so we're done processing here. */
        return;
      }
  }

  /* If the buffer had any bytes at the end which couldn't be handled
     by the vectorized implementations, use the non-optimized version
     to finish them up. */
  if (vectorizable_bytes < blocksize) {
    shuffle_generic_inline(bytesoftype, vectorizable_bytes, blocksize, _src, _dest);
  }
}

/* Unshuffle a block.  This can never fail. */
void
unshuffle_altivec(const int32_t bytesoftype, const int32_t blocksize,
                  const uint8_t *_src, uint8_t *_dest) {
  const int32_t vectorized_chunk_size = bytesoftype * sizeof(__m128i);
  /* If the blocksize is not a multiple of both the typesize and
     the vector size, round the blocksize down to the next value
     which is a multiple of both. The vectorized unshuffle can be
     used for that portion of the data, and the naive implementation
     can be used for the remaining portion. */
  const int32_t vectorizable_bytes = blocksize - (blocksize % vectorized_chunk_size);
  const int32_t vectorizable_elements = vectorizable_bytes / bytesoftype;
  const int32_t total_elements = blocksize / bytesoftype;

  /* If the block size is too small to be vectorized,
     use the generic implementation. */
  if (blocksize < vectorized_chunk_size) {
    unshuffle_generic(bytesoftype, blocksize, _src, _dest);
    return;
  }

  /* Warning in case of unaligned buffers*/
  if (((size_t)_dest) % 16)
  {
       printf("shuffle2_altivec: Unaligned destination !\n");
  }
  if (((size_t)_src) % 16) 
  {
      printf("shuffle2_altivec: Unaligned source !\n");
  }


  /* Optimized unshuffle implementations */
  switch (bytesoftype) {
    case 2:
      unshuffle2_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    case 4:
      unshuffle4_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    case 8:
      unshuffle8_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    case 16:
      unshuffle16_altivec(_dest, _src, vectorizable_elements, total_elements);
      break;
    default:
      if (bytesoftype > (int32_t)sizeof(__m128i)) {
        unshuffle16_tiled_altivec(_dest, _src, vectorizable_elements, total_elements, bytesoftype);
      }
      else {
        /* Non-optimized unshuffle */
        unshuffle_generic(bytesoftype, blocksize, _src, _dest);
        /* The non-optimized function covers the whole buffer,
           so we're done processing here. */
        return;
      }
  }

  /* If the buffer had any bytes at the end which couldn't be handled
     by the vectorized implementations, use the non-optimized version
     to finish them up. */
  if (vectorizable_bytes < blocksize) {
    unshuffle_generic_inline(bytesoftype, vectorizable_bytes, blocksize, _src, _dest);
  }
}

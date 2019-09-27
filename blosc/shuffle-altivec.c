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


/* Missaligned load helper function with better performances
 * 
 * param:
 *     position: index in src where to start reading
 *     src: the pointer of where to read
 *     previous: the pointer to the remaining vector of the last read
 *     shuffle_lo: the vector for shuffling the lower part
 *     shuffle_hi: the vector for shuffling the higher part
 * 
 * return: The properly read vector
 */
static inline __vector uint8_t helper_misaligned_read( int32_t position,  
                                                       const uint8_t * src,
                                                       __vector uint8_t * previous,
                                                       __vector uint8_t shuffle){  
  __vector uint8_t xmm0, xmm1, data;
  // Insert our data into the low and high vectors
  if (! previous)
    xmm0 = vec_ld(position, src);
  else
    xmm0 = previous[0];
  xmm1 = vec_ld(position + 16, src);
  data = vec_perm(xmm0, xmm1, shuffle);
  // Store only the high part in the previous buffer to re-use in next read.
  previous[0] = xmm1;
  // and return the data 
  return data;
}

/* Missaligned storage helper function with better performances
 * 
 * param:
 *     data: the (misaligned) vector to store
 *     position: index in dst where to store
 *     dst: the pointer of where to store
 *     previous: Pointer to the remaining vector of the last write: read and write !
 *     shuffle_lo: the vector for shuffling the lower part
 *     shuffle_hi: the vector for shuffling the higher part
 */
static inline __vector uint8_t helper_misaligned_store(__vector uint8_t data, 
                                                       int32_t position,  
                                                       uint8_t * dst,
                                                       __vector uint8_t * previous,
                                                       __vector uint8_t shuffle_lo,
                                                       __vector uint8_t shuffle_hi){  
                                                         
  const __vector uint8_t zero = vec_splat_u8(0);
  __vector uint8_t low, high;
  // Insert our data into the low and high vectors
  low = vec_perm(previous[0], data, shuffle_lo);
  high = vec_perm(zero, data, shuffle_hi);
  // Store only the lower part
  vec_st(low, position, dst);
  // and return the higher part which is the next "previous" vector
  previous[0] = high;
}

/* Calculate the permutation vector for storing data in the lower part
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_low(uint8_t offset){
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
static inline __vector uint8_t gen_permute_high(uint8_t offset){
  __vector uint8_t permute;
  for (uint8_t i = 0; i < 16; i++)
  {
    if (i < offset) 
      permute[i] = 32 + i - offset;
    else 
      permute[i] = i;
  }
  return permute;
} 

/* Calculate the permutation vector for reading at misaligned positions
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_read(uint8_t offset){
  __vector uint8_t permute;
  for (uint8_t i = 0; i < 16; i++)
      permute[i] = i + offset;
  return permute;
} 

static inline __vector uint8_t gen_save_mask(uint8_t offset){
  __vector uint8_t mask;
  for (uint8_t k = 0; k < 16; k++)
    mask[k] = (k<offset)?0:0xFF;
  return mask;
}


/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  static const uint8_t bytesoftype = 2;
  const uint8_t roffset = ((size_t) src) % 16;
  static __vector uint8_t perm_lo[2], perm_hi[2], perm;
  uint32_t j;
  uint8_t i;
  __vector uint8_t xmm0[2], xmm1[2], low[2], lor;
  uint8_t woffset[2];
  uint8_t* desti[2];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < bytesoftype; i++){
    woffset[i] = (((size_t) dest) + i * total_elements) % 16;
    desti[i] = &dest[i * total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  if (roffset){
    perm = gen_permute_read(roffset);
    lor = vec_ld(0, src);
  }
  //printf("vectorizable_elements: %d total_elements: %d dst: %d dst1: %d delta: %d offset: %d\n", vectorizable_elements, total_elements, dest, dest1, dest1-dest, offset);

  for (j = 0; j < vectorizable_elements; j += 16){
    /* Fetch 16 elements (32 bytes) */
    if (roffset){
      for (i = 0; i < bytesoftype; i++)
        xmm0[i] = helper_misaligned_read(bytesoftype * j + 16 * i, src, &lor, perm);  
    }
    else{
      for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_ld(bytesoftype * j + 16 * i, src);
    }
    /* Transpose vectors */
    xmm1[0] = vec_perm(xmm0[0], xmm0[1], even);
    xmm1[1] = vec_perm(xmm0[0], xmm0[1], odd);
    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i ++){
      if (woffset[i])
        helper_misaligned_store(xmm1[i], j, desti[i], &(low[i]), perm_lo[i], perm_hi[i]);
      else
        vec_st(xmm1[i], j, desti[i]);
    }        
  }
  /* Store the remainder if needed*/
  for (i = 0; i < bytesoftype; i++){
    if (woffset[i])
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), 
                     gen_save_mask(woffset[i])), 
             j, desti[i]);   
  }   
}

/* Routine optimized for shuffling a buffer for a type size of 4 bytes. */
static void
shuffle4_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  static const uint8_t bytesoftype = 4;
  int32_t j;
  uint8_t i;
  
  uint8_t woffset[4];
  uint8_t* desti[4];
  __vector uint8_t xmm0[4], xmm1[4], perm_lo[4], perm_hi[4], low[4];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < bytesoftype; i++){
    woffset[i] = (i*total_elements) % 16;
    desti[i] = &dest[i * total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  
  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (64 bytes, 4 vectors) */
    for (i = 0; i < bytesoftype; i++){
      xmm0[i] = vec_ld((bytesoftype * j) + (i * 16), src);
    }
    
    /* Transpose vectors 0-1*/
    for (i = 0; i < bytesoftype; i += 2){
      xmm1[i  ] = vec_perm(xmm0[i], xmm0[i+1], even);
      xmm1[i+1] = vec_perm(xmm0[i], xmm0[i+1], odd);
    }
    /* Transpose vectors 0-2*/
    xmm0[0] = vec_perm(xmm1[0], xmm1[2], even);
    xmm0[1] = vec_perm(xmm1[1], xmm1[3], even);
    xmm0[2] = vec_perm(xmm1[0], xmm1[2], odd);
    xmm0[3] = vec_perm(xmm1[1], xmm1[3], odd);
    
    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i ++){
      if (woffset[i])
        helper_misaligned_store(xmm0[i], j, desti[i], &(low[i]), perm_lo[i], perm_hi[i]);
      else
        vec_st(xmm0[i], j, desti[i]);
    }        
  }
  /* Store the remainder */
  for (i = 0; i < bytesoftype; i++){
    if (woffset[i])
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), gen_save_mask(woffset[i])), j, desti[i]);
  }
}


/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
static void
shuffle8_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements) {
  static const uint8_t bytesoftype = 8;
  int32_t j;
  uint8_t i, k;
  uint8_t woffset[8];
  uint8_t* desti[8];
  __vector uint8_t xmm0[8], xmm1[8], perm_lo[8], perm_hi[8], low[8];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < bytesoftype; i++){
    woffset[i] = (i*total_elements) % 16;
    desti[i] = &dest[i*total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  //printf("vectorizable_elements: %d total_elements: %d dst: %d offset1: %d offset2: %d offset3: %d\n", vectorizable_elements, total_elements, dest, offset[1], woffset[2], woffset[3]);
  
  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (128 bytes, 8 vectors) */
    for (i = 0; i < bytesoftype; i++){
      xmm0[i] = vec_ld((bytesoftype * j) + (i * 16), src);
    }
    
    /* Transpose vectors 0-1*/
    for (i = 0; i < bytesoftype; i += 2){
      xmm1[i] = vec_perm(xmm0[i], xmm0[i+1], even);
      xmm1[i+1] = vec_perm(xmm0[i], xmm0[i+1], odd);
    }
    /* Transpose vectors 0-2*/
    for (i = 0; i < bytesoftype; i += 4){
      for (k = 0; k<2; k++){
        xmm0[i+k] = vec_perm(xmm1[i+k], xmm1[i+k+2], even);
        xmm0[i+k+2] = vec_perm(xmm1[i+k], xmm1[i+k+2], odd);
      }
    }
    /* Transpose vectors 0-4*/
    for (k = 0; k < 4; k++){
    xmm1[k] = vec_perm(xmm0[k], xmm0[k+4], even);
    xmm1[k+4] = vec_perm(xmm0[k], xmm0[k+4], odd);
    }
    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i++){
      if (woffset[i])
        helper_misaligned_store(xmm1[i], j, desti[i], &(low[i]), perm_lo[i], perm_hi[i]);
      else
        vec_st(xmm1[i], j, desti[i]);
    }
  }
  /* Store the remainder */
  for (i = 0; i < bytesoftype; i++){
    if (woffset[i])
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), gen_save_mask(woffset[i])), j, desti[i]);
  }
}

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
static void
shuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                  const int32_t vectorizable_elements, const int32_t total_elements) {
  static const uint8_t bytesoftype = 16;
  uint8_t i, k;
  int32_t j;
  uint8_t woffset[16];
  uint8_t* desti[16];
  __vector uint8_t xmm0[16], xmm1[16], perm_lo[16], perm_hi[16], low[16];
  
  // Initialize the offset, destinations and permutations
  for (i = 0; i < 16; i++){
    woffset[i] = (i*total_elements) %16;
    desti[i] = &dest[i*total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_splat_u8(0);
    } 
  }
  //printf("vectorizable_elements: %d total_elements: %d dst: %d offset1: %d offset2: %d offset3: %d\n", vectorizable_elements, total_elements, dest, woffset[1], woffset[2], woffset[3]);
  
  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (256 bytes, 16 vectors) */
    for (i = 0; i < bytesoftype; i++){
      xmm0[i] = vec_ld((bytesoftype * j) + (i * 16), src);
    }

    /* Transpose vectors 0-1*/
    for (i = 0; i < bytesoftype; i += 2){
      xmm1[i] = vec_perm(xmm0[i], xmm0[i+1], even);
      xmm1[i+1] = vec_perm(xmm0[i], xmm0[i+1], odd);
    }
    /* Transpose vectors 0-2*/
    for (i = 0; i < bytesoftype; i += 4){
      for (k = 0; k<2; k++){
        xmm0[i+k] = vec_perm(xmm1[i+k], xmm1[i+k+2], even);
        xmm0[i+k+2] = vec_perm(xmm1[i+k], xmm1[i+k+2], odd);
      }
    }
    /* Transpose vectors 0-4*/
    for (i = 0; i < bytesoftype; i += 8){
      for (k = 0; k<4; k++){
      xmm1[i+k] = vec_perm(xmm0[i+k], xmm0[i+k+4], even);
      xmm1[i+k+4] = vec_perm(xmm0[i+k], xmm0[i+k+4], odd);
      }
    }
    /* Transpose vectors 0-8*/
    for (k = 0; k<8; k++){
      xmm0[k] = vec_perm(xmm1[k], xmm1[k+8], even);
      xmm0[k+8] = vec_perm(xmm1[k], xmm1[k+8], odd);
      }
    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i ++){
      if (woffset[i])
        helper_misaligned_store(xmm0[i], j, desti[i], &(low[i]), perm_lo[i], perm_hi[i]);
      else
        vec_st(xmm0[i], j, desti[i]);
    }
  }
  /* Store the remainder */
  for (i = 0; i < bytesoftype; i++){
    if (woffset[i])
      vec_st(vec_sel(low[i], vec_ld(j, desti[i]), gen_save_mask(woffset[i])), j, desti[i]);
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
  static const uint8_t bytesoftype = 2;
  uint32_t j;
  uint8_t i;
  const uint8_t woffset = ((size_t) dest) % 16;
  uint8_t roffset[2];
  uint8_t* srci[2];
  __vector uint8_t xmm0[2], xmm1[2], perm[2], low, perm_lo, perm_hi, lor[2];

  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }
  // Initialize the source, offsets and permutations
  for (i = 0; i < bytesoftype; i++){
    roffset[i] = (((size_t)src) + i * total_elements) % 16;
    srci[i] = (uint8_t*) (&src[i * total_elements]);
    if (roffset[i]){
      perm[i] = gen_permute_read(roffset[i]);
      lor[i] = vec_ld(0, srci[i]);
    } 
  }
  printf("vectorizable_elements: %d total_elements: %d dst: %d woffset, offsetr0: %d offsetr1: %d\n", vectorizable_elements, total_elements, woffset, roffset[0], roffset[1]);

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (32 bytes) into 2 vectors registers. */
    for (i = 0; i < bytesoftype; i++) {
      if (roffset[i])
        xmm0[i] = helper_misaligned_read(j, srci[i], &(lor[i]), perm[i]);
      else
        xmm0[i] = vec_ld(j, srci[i]);
    }
    /* Shuffle bytes */
    /* Compute the low 32 bytes */
    xmm1[1] = vec_vmrglb (xmm0[0], xmm0[1]);
    /* Compute the hi 32 bytes */
    xmm1[0] = vec_vmrghb (xmm0[0], xmm0[1]);
    /* Store the result vectors in proper order */
    if (woffset){
      helper_misaligned_store(xmm1[0], 2 * j, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[1], 2 * j + 16, dest, &low, perm_lo, perm_hi);
    }
    else{
      vec_st(xmm1[0], 2 * j, dest);
      vec_st(xmm1[1], 2 * j + 16, dest);
    }
  }
  /* Store the remainder */
  if (woffset)
    vec_st(vec_sel(low, vec_ld(j, dest), gen_save_mask(woffset)), j, dest);
}

/* Routine optimized for unshuffling a buffer for a type size of 4 bytes. */
static void
unshuffle4_altivec(uint8_t* const dest, const uint8_t* const src,
                const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 4;
  uint32_t j;
  uint8_t i;
  const uint8_t woffset = ((size_t) dest) % 16;
  uint8_t roffset[4];
  uint8_t* srci[4];
  __vector uint8_t xmm0[4], xmm1[4], perm[4], low, perm_lo, perm_hi, lor[4];

  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }
  // Initialize the source, offsets and permutations
  for (i = 0; i < bytesoftype; i++){
    roffset[i] = (((size_t)src) + i * total_elements) % 16;
    srci[i] = (uint8_t*) (&src[i * total_elements]);
    if (roffset[i]){
      perm[i] = gen_permute_read(roffset[i]);
      lor[i] = vec_ld(0, srci[i]);
    } 
  }
  printf("vectorizable_elements: %d total_elements: %d dst: %d woffset, offsetr0: %d offsetr1: %d\n", vectorizable_elements, total_elements, woffset, roffset[0], roffset[1]);

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++) {
      if (roffset[i])
        xmm0[i] = helper_misaligned_read(j, srci[i], &(lor[i]), perm[i]);
      else
        xmm0[i] = vec_ld(j, srci[i]);
    }
    /* Shuffle bytes */
    for (i = 0; i < 2; i++) {
      /* Compute the low 32 bytes */
      xmm1[i] = vec_vmrglb(xmm0[i * 2], xmm0[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[2 + i] = vec_vmrghb(xmm0[i * 2], xmm0[i * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (i = 0; i < 2; i++) {
      /* Compute the low 32 bytes */
      xmm0[i] = (__vector uint8_t) vec_vmrglh((__vector uint16_t)xmm1[i * 2], (__vector uint16_t) xmm1[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm0[2 + i] = (__vector uint8_t) vec_vmrghh((__vector uint16_t)xmm1[i * 2], (__vector uint16_t)xmm1[i * 2 + 1]);
    }
    /* Store the result vectors in proper order */
    if (woffset){
      helper_misaligned_store(xmm0[3], bytesoftype * j, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[1], bytesoftype * j + 16, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[2], bytesoftype * j + 32, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[0], bytesoftype * j + 48, dest, &low, perm_lo, perm_hi);
    }
    else{
      vec_st(xmm0[3], bytesoftype * j, dest);
      vec_st(xmm0[1], bytesoftype * j + 16, dest);
      vec_st(xmm0[2], bytesoftype * j + 32, dest);
      vec_st(xmm0[0], bytesoftype * j + 48, dest);
    }
  }
  /* Store the remainder */
  if (woffset)
    vec_st(vec_sel(low, vec_ld(j, dest), gen_save_mask(woffset)), j, dest);
}

/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
static void
unshuffle8_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const uint8_t bytesoftype = 8;
  uint32_t j;
  uint8_t i;
  const uint8_t woffset = ((size_t) dest) % 16;
  uint8_t roffset[8];
  uint8_t* srci[8];
  __vector uint8_t xmm0[8], xmm1[8], perm[8], low, perm_lo, perm_hi, lor[8];
  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }
  // Initialize the source, offsets and permutations
  for (i = 0; i < bytesoftype; i++){
    roffset[i] = (((size_t)src) + i * total_elements) % 16;
    srci[i] = (uint8_t*) (&src[i * total_elements]);
    if (roffset[i]){
      perm[i] = gen_permute_read(roffset[i]);
      lor[i] = vec_ld(0, srci[i]);
    } 
  }
  printf("vectorizable_elements: %d total_elements: %d dst: %d woffset, offsetr0: %d offsetr1: %d\n", vectorizable_elements, total_elements, woffset, roffset[0], roffset[1]);

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++) {
      if (roffset[i])
        xmm0[i] = helper_misaligned_read(j, srci[i], &(lor[i]), perm[i]);
      else
        xmm0[i] = vec_ld(j, srci[i]);
    }
    /* Shuffle bytes */
    for (i = 0; i < 4; i++) {
      /* Compute the low 32 bytes */
      xmm1[i] = vec_vmrglb(xmm0[i * 2], xmm0[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[4 + i] = vec_vmrghb(xmm0[i * 2], xmm0[i * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (i = 0; i < 4; i++) {
      /* Compute the low 32 bytes */
      xmm0[i] = (__vector uint8_t)vec_vmrglh((__vector uint16_t)xmm1[i * 2], (__vector uint16_t)xmm1[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm0[4 + i] = (__vector uint8_t)vec_vmrghh((__vector uint16_t)xmm1[i * 2], (__vector uint16_t)xmm1[i * 2 + 1]);
    }
    /* Shuffle 4-byte dwords */
    for (i = 0; i < 4; i++) {
      /* Compute the low 32 bytes */
      xmm1[i] = (__vector uint8_t)vec_vmrglw((__vector uint32_t)xmm0[i * 2], (__vector uint32_t)xmm0[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[4 + i] = (__vector uint8_t)vec_vmrghw((__vector uint32_t)xmm0[i * 2], (__vector uint32_t)xmm0[i * 2 + 1]);
    }
    /* Store the result vectors in proper order */
    if (woffset){
      helper_misaligned_store(xmm1[7], bytesoftype * j, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[3], bytesoftype * j + 16, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[5], bytesoftype * j + 32, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[1], bytesoftype * j + 48, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[6], bytesoftype * j + 64, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[2], bytesoftype * j + 80, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[4], bytesoftype * j + 96, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm1[0], bytesoftype * j + 112, dest, &low, perm_lo, perm_hi);    
      }
    else{
      vec_st(xmm1[7], bytesoftype * j, dest);
      vec_st(xmm1[3], bytesoftype * j + 16, dest);
      vec_st(xmm1[5], bytesoftype * j + 32, dest);
      vec_st(xmm1[1], bytesoftype * j + 48, dest);
      vec_st(xmm1[6], bytesoftype * j + 64, dest);
      vec_st(xmm1[2], bytesoftype * j + 80, dest);
      vec_st(xmm1[4], bytesoftype * j + 96, dest);
      vec_st(xmm1[0], bytesoftype * j + 112, dest);    
      }
  }
  /* Store the remainder */
  if (woffset)
    vec_st(vec_sel(low, vec_ld(j, dest), gen_save_mask(woffset)), j, dest);
}


/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
static void
unshuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                    const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 16;
  uint32_t j;
  uint8_t i;
  const uint8_t woffset = ((size_t) dest) % 16;
  uint8_t roffset[16];
  uint8_t* srci[16];
  __vector uint8_t xmm0[16], xmm1[16], perm[16], low, perm_lo, perm_hi, lor[16], msk8; 
  msk8 = gen_save_mask(8);

  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }
  // Initialize the source, offsets and permutations
  for (i = 0; i < bytesoftype; i++){
    roffset[i] = (((size_t)src) + i * total_elements) % 16;
    srci[i] = (uint8_t*) (&src[i * total_elements]);
    if (roffset[i]){
      perm[i] = gen_permute_read(roffset[i]);
      lor[i] = vec_ld(0, srci[i]);
    } 
  }
  printf("vectorizable_elements: %d total_elements: %d dst: %d woffset, offsetr0: %d offsetr1: %d\n", vectorizable_elements, total_elements, woffset, roffset[0], roffset[1]);

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++) {
      if (roffset[i])
        xmm0[i] = helper_misaligned_read(j, srci[i], &(lor[i]), perm[i]);
      else
        xmm0[i] = vec_ld(j, srci[i]);
    }
    /* Shuffle bytes */
    for (i = 0; i < 8; i++) {
      /* Compute the low 32 bytes */
      xmm1[i] = vec_vmrghb(xmm0[i * 2], xmm0[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[8 + i] = vec_vmrglb(xmm0[i * 2], xmm0[i * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (i = 0; i < 8; i++) {
      /* Compute the low 32 bytes */
      xmm0[i] = (__vector uint8_t) vec_vmrghh((__vector uint16_t)xmm1[i * 2], (__vector uint16_t)xmm1[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm0[8 + i] = (__vector uint8_t) vec_vmrglh((__vector uint16_t)xmm1[i * 2], (__vector uint16_t)xmm1[i * 2 + 1]);
    }
    /* Shuffle 4-byte dwords */
    for (i = 0; i < 8; i++) {
      /* Compute the low 32 bytes */
      xmm1[i] = (__vector uint8_t) vec_vmrghw((__vector uint32_t)xmm0[i * 2], (__vector uint32_t)xmm0[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm1[8 + i] = (__vector uint8_t) vec_vmrglw((__vector uint32_t)xmm0[i * 2], (__vector uint32_t)xmm0[i * 2 + 1]);
    }
    /* Shuffle 8-byte qwords */
    for (i = 0; i < 8; i++) {
      /* Compute the low 32 bytes */
      xmm0[i] = vec_sel(xmm1[i * 2], xmm1[i * 2 + 1], msk8);
      /* Compute the hi 32 bytes */
      xmm0[i + 8 ] = vec_sel(xmm1[i * 2 + 1], xmm1[i * 2], msk8);
    }

    /* Store the result vectors in proper order */
    /* Store the result vectors in proper order */
    if (woffset){
      helper_misaligned_store(xmm0[0], bytesoftype * j, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[1], bytesoftype * j + 16, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[2], bytesoftype * j + 32, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[3], bytesoftype * j + 48, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[4], bytesoftype * j + 64, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[5], bytesoftype * j + 80, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[6], bytesoftype * j + 96, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[7], bytesoftype * j + 112, dest, &low, perm_lo, perm_hi);    
      helper_misaligned_store(xmm0[8], bytesoftype * j + 128, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[9], bytesoftype * j + 144, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[10], bytesoftype * j + 160, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[11], bytesoftype * j + 176, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[12], bytesoftype * j + 192, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[13], bytesoftype * j + 208, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[14], bytesoftype * j + 224, dest, &low, perm_lo, perm_hi);
      helper_misaligned_store(xmm0[15], bytesoftype * j + 240, dest, &low, perm_lo, perm_hi);    
      }
    else{
      vec_st(xmm0[0], bytesoftype * j, dest);
      vec_st(xmm0[1], bytesoftype * j + 16, dest);
      vec_st(xmm0[2], bytesoftype * j + 32, dest);
      vec_st(xmm0[3], bytesoftype * j + 48, dest);
      vec_st(xmm0[4], bytesoftype * j + 64, dest);
      vec_st(xmm0[5], bytesoftype * j + 80, dest);
      vec_st(xmm0[6], bytesoftype * j + 96, dest);
      vec_st(xmm0[7], bytesoftype * j + 112, dest);    
      vec_st(xmm0[8], bytesoftype * j + 128, dest);
      vec_st(xmm0[9], bytesoftype * j + 144, dest);
      vec_st(xmm0[10], bytesoftype * j + 160, dest);
      vec_st(xmm0[11], bytesoftype * j + 176, dest);
      vec_st(xmm0[12], bytesoftype * j + 192, dest);
      vec_st(xmm0[13], bytesoftype * j + 208, dest);
      vec_st(xmm0[14], bytesoftype * j + 224, dest);
      vec_st(xmm0[15], bytesoftype * j + 240, dest);    
    }
  }
  /* Store the remainder */
  if (woffset)
    vec_st(vec_sel(low, vec_ld(j, dest), gen_save_mask(woffset)), j, dest);
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

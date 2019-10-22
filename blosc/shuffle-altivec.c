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

#include <altivec.h>
//#include <stdio.h>

#include "transpose-altivec.h"

/*
static void helper_print(__vector uint8_t v, char* txt){
  printf("%s %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",txt,
  v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]); 
}
*/

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

/* Calculate the permutation vector for unpacking lower/higher bytes of 
 * a larger oject of given size
 * 
 * param:
 *   - size: the size of the object: 2, 4, 8 or 16 are the only possible values
 *   - part: Take the upper or lower part of the datatype
 * return: permutation vector
 */

static inline __vector uint8_t gen_permute_unpack(int32_t size, int32_t part){
  __vector uint8_t permute;
  int32_t pos, half, nel, item, byte;
  nel = 16 / size;
  half = size>>1;
  pos = 0;
  
  if (part) {
    for (item = 0; item < nel ; item++){
      for (byte = 0; byte<size; byte++){
        permute[pos] = (uint8_t)((byte<half)?pos:pos+16-half);
        pos++;
      }
    }
  }
  else {
    for (item = 0; item < nel ; item++){
      for (byte = 0; byte<size; byte++){
        permute[pos] = (uint8_t)((byte<half)?pos+half:pos+16);
        pos++;
      }
    }
  }
  return permute;
} 


/* Calculate the permutation vector for storing data in the lower part
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_low(int32_t offset){
  __vector uint8_t permute;
  int32_t i;
  for (i = 0; i < 16; i++){
    if (i < offset) 
      permute[i] = (uint8_t)i;
    else 
      permute[i] = (uint8_t)(16 + i - offset);
  }
  return permute;
} 

/* Calculate the permutation vector for storing data in the upper part
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_high(int32_t offset){
  __vector uint8_t permute;
  int32_t i;
  for (i = 0; i < 16; i++)
  {
    if (i < offset) 
      permute[i] = (uint8_t)(32 + i - offset);
    else 
      permute[i] = (uint8_t)(i);
  }
  return permute;
} 

/* Calculate the permutation vector for reading at misaligned positions
 * 
 * param: offset, the position
 */
static inline __vector uint8_t gen_permute_read(int32_t offset){
  __vector uint8_t permute;
  int32_t i;
  for (i = 0; i < 16; i++)
      permute[i] = (uint8_t)(i + offset);
  return permute;
} 

static inline __vector uint8_t gen_save_mask(int32_t offset){
  __vector uint8_t mask;
  int32_t k;
  for (k = 0; k < 16; k++)
    mask[k] = (k<offset)?0:0xFF;
  return mask;
}

// Store a vector to an unaligned location in memory ... may be sub-optimal
static void vec_st_generic(__vector uint8_t vector, int32_t position,  uint8_t * dst){
  const int32_t offset = ((size_t)dst + position) & 0xF; // check alignment on 16 bytes
  if (offset){ // Actually unaligned store
    vec_vsx_st(vector, position, dst);
  }
  else{ // Aligned store, the usual way
    vec_st(vector, position, dst);
  }
}


/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  static const int32_t bytesoftype = 2;
  uint32_t i, j, woffset[2];
  __vector uint8_t perm_lo[2], perm_hi[2], perm;
  __vector uint8_t xmm0[2], xmm1[2], low[2];
  uint8_t* desti[2];

  // Initialize the offset, destinations and permutations
  for (i = 0; i < bytesoftype; i++){
    woffset[i] = (((size_t) dest) + i * total_elements) & 0xF; // modulo 16
    desti[i] = &dest[i * total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_ld(0, desti[i]);
    } 
  }
  for (j = 0; j < vectorizable_elements; j += 16){
    /* Fetch 16 elements (32 bytes) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);
    
    /* Transpose vectors */
    transpose2x16(xmm0);

    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i++){
      if (woffset[i])
        helper_misaligned_store(xmm0[i], j, desti[i], &(low[i]), perm_lo[i], perm_hi[i]);
      else
        vec_st(xmm0[i], j, desti[i]);
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
  static const int32_t bytesoftype = 4;
  int32_t i, j, woffset[4];
  uint8_t* desti[4];
  __vector uint8_t perm_lo[4], perm_hi[4], perm, xmm0[4], xmm1[4], low[4];

  // Initialize the offset, destinations and permutations
  for (i = 0; i < bytesoftype; i++){
    woffset[i] = (i*total_elements) & 0xF;
    desti[i] = &dest[i * total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_ld(0, desti[i]);
    } 
  }

  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (64 bytes, 4 vectors) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);


    /* Transpose vectors */
    transpose4x16(xmm0);
    
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
  int32_t i, j, k, woffset[8];
  uint8_t* desti[8];
  __vector uint8_t perm_lo[8], perm_hi[8], perm, xmm0[8], low[8];

  // Initialize the offset, destinations and permutations
  for (i = 0; i < bytesoftype; i++){
    woffset[i] = (i*total_elements) & 0xF;
    desti[i] = &dest[i*total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_ld(0, desti[i]);
    } 
  }

  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (128 bytes, 8 vectors) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);

    /* Transpose vectors */
    transpose8x16(xmm0);

    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i++){
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

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
static void
shuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                  const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 16;
  int32_t i, j, k, woffset[16];
  uint8_t* desti[16];
  __vector uint8_t xmm0[16], low[16], perm_lo[16], perm_hi[16], perm;

  // Initialize the offset, destinations and permutations
  for (i = 0; i < 16; i++){
    woffset[i] = (i * total_elements) & 0xF;
    desti[i] = &dest[i*total_elements];
    if (woffset[i]){
      perm_lo[i] = gen_permute_low(woffset[i]);
      perm_hi[i] = gen_permute_high(woffset[i]);
      low[i] = vec_ld(0, desti[i]);
    } 
  }

  for (j = 0; j < vectorizable_elements; j += 16) 
  {
    /* Fetch 16 elements (256 bytes, 16 vectors) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);
    
    // Do the job !
    transpose16x16(xmm0);

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
/* Routine optimized for shuffling a buffer for a type size larger than 16 bytes. */
static void
shuffle16_tiled_altivec(uint8_t* const dest, const uint8_t* const src,
                        const int32_t vectorizable_elements, const int32_t total_elements, const int32_t bytesoftype) {
  int32_t j, k, l;
  const int32_t vecs_per_el_rem = bytesoftype & 0xF;
  uint8_t* dest_for_jth_element;
  __vector uint8_t xmm[16];

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Advance the offset into the type by the vector size (in bytes), unless this is
    the initial iteration and the type size is not a multiple of the vector size.
    In that case, only advance by the number of bytes necessary so that the number
    of remaining bytes in the type will be a multiple of the vector size. */
    int32_t offset_into_type;
    for (offset_into_type = 0; offset_into_type < bytesoftype;
         offset_into_type += (offset_into_type == 0 &&
                              vecs_per_el_rem > 0 ? vecs_per_el_rem : 16)) {

      /* Fetch elements in groups of 256 bytes */
      const uint8_t* const src_with_offset = src + offset_into_type;
      for (k = 0; k < 16; k++)
        xmm[k] = vec_xl((j + k) * bytesoftype, src_with_offset);
      // Do the Job!
      transpose16x16(xmm);
      /* Store the result vectors */
      dest_for_jth_element = dest + j;
      for (k = 0; k < 16; k++) {
        vec_st_generic(xmm[k], total_elements * (offset_into_type + k), dest_for_jth_element);
      }
    }
  }
}
/* Routine optimized for unshuffling a buffer for a type size of 2 bytes. */
static void
unshuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 2;
  uint32_t i, j, woffset = ((size_t) dest) & 0xF;
  __vector uint8_t xmm0[2], xmm1[2], perm[2], low, perm_lo, perm_hi;

  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (32 bytes) into 2 vectors registers. */
    for (i = 0; i < bytesoftype; i++) {
        xmm0[i] = vec_xl(j + i * total_elements, src);
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
  uint32_t i, j, woffset = ((size_t) dest) & 0xF;
  __vector uint8_t xmm0[4], xmm1[4], perm[4], low, perm_lo, perm_hi;

  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_xl(j + i * total_elements, src);
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
  const uint32_t woffset = ((size_t) dest) & 0xF;
  uint32_t i, j;
  __vector uint8_t xmm0[8], xmm1[8], perm[8], low, perm_lo, perm_hi;
  
  // Initialize permutations for writing
  
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_xl(j + i * total_elements, src);
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
  uint32_t i, j, woffset;
  __vector uint8_t xmm0[16], perm[16], low, perm_lo, perm_hi, unpacklo, unpackhi; 

  woffset = ((size_t) dest) & 0xF;
  unpacklo = gen_permute_unpack(16, 0);
  unpackhi = gen_permute_unpack(16, 1);

  // Initialize permutations for writing
  if (woffset){
    perm_lo = gen_permute_low(woffset);
    perm_hi = gen_permute_high(woffset);
    low = vec_ld(0, dest);
  }

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_xl(j + i * total_elements, src);
    
    // Do the Job!
    transpose16x16(xmm0);
    
    /* Store the result vectors*/
    if (woffset){
      for (i = 0; i < 16; i++) 
        helper_misaligned_store(xmm0[i], bytesoftype * (i+j), dest, &low, perm_lo, perm_hi);
      }
    else{
      for (i = 0; i < 16; i++)
        vec_st(xmm0[i], bytesoftype * (i+j), dest);
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
  int32_t i, j, offset_into_type;
  const int32_t vecs_per_el_rem = bytesoftype &  0xF;
  uint8_t* dest_with_offset;
  __vector uint8_t xmm[16];
  
  
  /* Advance the offset into the type by the vector size (in bytes), unless this is
    the initial iteration and the type size is not a multiple of the vector size.
    In that case, only advance by the number of bytes necessary so that the number
    of remaining bytes in the type will be a multiple of the vector size. */
    
  for (offset_into_type = 0; offset_into_type < bytesoftype;
       offset_into_type += (offset_into_type == 0 &&
           vecs_per_el_rem > 0 ? vecs_per_el_rem : 16)) {
    for (i = 0; i < vectorizable_elements; i += 16) {
      /* Load the first 128 bytes in 16 XMM registers */
      const uint8_t* const src_for_ith_element = orig + i;
      for (j = 0; j < 16; j++)
        xmm[j] = vec_xl(total_elements * (offset_into_type + j), src_for_ith_element);

      // Do the Job !
      transpose16x16(xmm);
      
      /* Store the result vectors in proper order */
      dest_with_offset = dest + offset_into_type;
      for (j = 0; j < 16; j++) {
        vec_st_generic(xmm[j], (i + j) * bytesoftype, dest_with_offset);
      }
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
      if (bytesoftype > 16) {
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
  const int32_t vectorized_chunk_size = bytesoftype * 16;
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
      if (bytesoftype > 16) {
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

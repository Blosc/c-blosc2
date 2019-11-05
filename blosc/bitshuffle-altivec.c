/*
 * Bitshuffle - Filter for improving compression of typed binary data.
 *
 * Author: Kiyoshi Masui <kiyo@physics.ubc.ca>
 * Website: http://www.github.com/kiyo-masui/bitshuffle
 * Created: 2014
 *
 * Note: Adapted for c-blosc by Francesc Alted
 *       Altivec/VSX version by Jerome Kieffer.
 *
 * See LICENSES/BITSHUFFLE.txt file for details about copyright and
 * rights to use.
 *
 */

#include "bitshuffle-generic.h"
#include "bitshuffle-altivec.h"

/* Make sure ALTIVEC is available for the compilation target and compiler. */
#if !defined(__ALTIVEC__)
  #error ALTIVEC is not supported by the target architecture/platform and/or this compiler.
#endif
#include <altivec.h>

#include <emmintrin.h>
#include <stdio.h>

/* The next is useful for debugging purposes */
#if 1
#include <stdio.h>
#include <string.h>

static void helper_print(__vector uint8_t v, char* txt){
  printf("%s %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",txt,
  v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]); 
}
#endif


static inline __vector uint8_t gen_save_mask(size_t offset){
  __vector uint8_t mask;
  size_t k;
  for (k = 0; k < 16; k++)
    mask[k] = (k<offset)?0:0xFF;
  return mask;
}

// Unpack and interleave 8-bit integers from the low half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpacklo_epi8(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi8_low = (const __vector uint8_t) {0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 
                                                                     0x04, 0x14, 0x05, 0x15, 0x06, 0x16, 0x07, 0x17};
  return vec_perm(xmm0, xmm1, epi8_low);
}
// Unpack and interleave 8-bit integers from the high half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpackhi_epi8(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi8_hi = (const __vector uint8_t) {0x08, 0x18, 0x09, 0x19, 0x0a, 0x1a, 0x0b, 0x1b, 
                                                                     0x0c, 0x1c, 0x0d, 0x1d, 0x0e, 0x1e, 0x0f, 0x1f};
  return vec_perm(xmm0, xmm1, epi8_hi);
}

// Unpack and interleave 16-bit integers from the low half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpacklo_epi16(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi16_low = (const __vector uint8_t) {0x00, 0x01, 0x10, 0x11, 0x02, 0x03, 0x12, 0x13,
                                                                      0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x16, 0x17};
  return vec_perm(xmm0, xmm1, epi16_low);
}
// Unpack and interleave 16-bit integers from the high half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpackhi_epi16(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi16_hi = (const __vector uint8_t) {0x08, 0x09, 0x18, 0x19, 0x0a, 0x0b, 0x1a, 0x1b, 
                                                                     0x0c, 0x0d, 0x1c, 0x1d, 0x0e, 0x0f, 0x1e, 0x1f};
  return vec_perm(xmm0, xmm1, epi16_hi);
}

// Unpack and interleave 32-bit integers from the low half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpacklo_epi32(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi32_low = (const __vector uint8_t) {0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13,
                                                                      0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17};
  return vec_perm(xmm0, xmm1, epi32_low);
}
// Unpack and interleave 32-bit integers from the high half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpackhi_epi32(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi32_hi = (const __vector uint8_t) {0x08, 0x09, 0x0a, 0x0b, 0x18, 0x19, 0x1a, 0x1b, 
                                                                     0x0c, 0x0d, 0x0e, 0x0f, 0x1c, 0x1d, 0x1e, 0x1f};
  return vec_perm(xmm0, xmm1, epi32_hi);
}

// Unpack and interleave 64-bit integers from the low half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpacklo_epi64(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi64_low = (const __vector uint8_t) {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
                                                                     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  return vec_perm(xmm0, xmm1, epi64_low);
}
// Unpack and interleave 64-bit integers from the high half of xmm0 and xmm1, and return the results. 
static __vector uint8_t unpackhi_epi64(__vector uint8_t xmm0, __vector uint8_t xmm1){
  static const __vector uint8_t epi64_hi = (const __vector uint8_t) {0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 
                                                                     0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
  return vec_perm(xmm0, xmm1, epi64_hi);
}

// Build and return a bit-permutation mask
static __vector uint8_t make_bitperm_mask(int type_size, int bit) {
  __vector uint8_t result;
  int i;
  if (type_size == 1) {
    // data_type is 8 bits long
    for (i=0; i<16; i++)
      result[i] = 8 * (15-i) + (7-bit);
  }
  else if (type_size == 2) {
    // data_type is 16 bits long
    for (i=0; i<8; i++) {
      result[i] = 16 * i + 2 * bit;
      result[i+8] = 16 * i + 2 * bit + 1;
    }
  }
  return result;
}

/* Routine optimized for bit-unshuffling a buffer for a type size of 1 byte. 
 * 
 * Strategy: Read 8 vectors of 128bits, hence 128 elements, 
 *           Transpose byte-wise, 2 neighboring elements (x8) remain in each vector : 24 operations
 *           Transpose bit-wise within a vector: 8x8 bitwise-transposition: 64 operations
 *           Saving is perform by shorts (2 bytes at a time)
 * Total cost: 8 vector read, 88 transpositions, 64 writes, 
 *             14 mask vectors, 16 work-vectors
 * */
void
bitunshuffle1_altivec(void* _src, void* dest, const size_t size, const size_t elem_size) {
  size_t ii, jj, kk, vp;
  const uint8_t* in_b = (const uint8_t*)_src;
  uint16_t* out_s = (uint16_t*)dest;
  size_t nrows = 8 * elem_size;
  size_t nbyte_row = size / 8;

  // working vectors
  __vector uint8_t xmm0[8], xmm1[8], masks[8];
  // Masks vectors
  static const __vector uint8_t lo01 = (const __vector uint8_t) {0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x0d, 0x10, 0x11, 0x14, 0x15, 0x18, 0x19, 0x1c, 0x1d};
  static const __vector uint8_t hi01 = (const __vector uint8_t) {0x02, 0x03, 0x06, 0x07, 0x0a, 0x0b, 0x0e, 0x0f, 0x12, 0x13, 0x16, 0x17, 0x1a, 0x1b, 0x1e, 0x1f};
  static const __vector uint8_t lo02 = (const __vector uint8_t) {0x00, 0x01, 0x08, 0x09, 0x10, 0x11, 0x18, 0x19, 0x02, 0x03, 0x0a, 0x0b, 0x12, 0x13, 0x1a, 0x1b};
  static const __vector uint8_t hi02 = (const __vector uint8_t) {0x04, 0x05, 0x0c, 0x0d, 0x14, 0x15, 0x1c, 0x1d, 0x06, 0x07, 0x0e, 0x0f, 0x16, 0x17, 0x1e, 0x1f};
  
  //static const __vector uint8_t msk0 = (const __vector uint8_t) {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71};
  
  for (kk = 0; kk < 8; kk++){
    __vector uint8_t msk;
    for (ii = 0; ii < 8; ii++){
      msk[ii] = 127-(16*ii+2*kk);
      msk[ii+8] = 127-(16*ii+2*kk+1);
    }
    //helper_print(msk, "Mask");
    masks[kk] = msk;
  }
  vp = 0;
  //TODO: CHECK_MULT_EIGHT(size);
  // read the data
  for (ii = 0; ii + 7 < nrows; ii += 8) {
    for (jj = 0; jj + 15 < nbyte_row; jj += 16) {
      for (kk = 0; kk < 8; kk++){
        xmm0[kk] = vec_xl((ii +kk) * nbyte_row + jj, in_b);
        //helper_print(xmm0[kk], "vector read");
      }
      
      // transpositions 0-1
      xmm1[0] = vec_perm(xmm0[0], xmm0[1], lo01);
      xmm1[1] = vec_perm(xmm0[0], xmm0[1], hi01);    
      xmm1[2] = vec_perm(xmm0[2], xmm0[3], lo01);
      xmm1[3] = vec_perm(xmm0[2], xmm0[3], hi01);
      xmm1[4] = vec_perm(xmm0[4], xmm0[5], lo01);
      xmm1[5] = vec_perm(xmm0[4], xmm0[5], hi01);
      xmm1[6] = vec_perm(xmm0[6], xmm0[7], lo01);
      xmm1[7] = vec_perm(xmm0[6], xmm0[7], hi01);
      // transpositions 0-2
      xmm0[0] = vec_perm(xmm1[0], xmm1[2], lo02);
      xmm0[2] = vec_perm(xmm1[0], xmm1[2], hi02);
      xmm0[1] = vec_perm(xmm1[1], xmm1[3], lo02);
      xmm0[3] = vec_perm(xmm1[1], xmm1[3], hi02);
      xmm0[4] = vec_perm(xmm1[4], xmm1[6], lo02);
      xmm0[6] = vec_perm(xmm1[4], xmm1[6], hi02);
      xmm0[5] = vec_perm(xmm1[5], xmm1[7], lo02);
      xmm0[7] = vec_perm(xmm1[5], xmm1[7], hi02);
      // transpositions 0-4
      xmm1[0] = unpacklo_epi64(xmm0[0], xmm0[4]);
      xmm1[2] = unpackhi_epi64(xmm0[0], xmm0[4]);
      xmm1[1] = unpacklo_epi64(xmm0[1], xmm0[5]);
      xmm1[3] = unpackhi_epi64(xmm0[1], xmm0[5]);
      xmm1[4] = unpacklo_epi64(xmm0[2], xmm0[6]);
      xmm1[6] = unpackhi_epi64(xmm0[2], xmm0[6]);
      xmm1[5] = unpacklo_epi64(xmm0[3], xmm0[7]);
      xmm1[7] = unpackhi_epi64(xmm0[3], xmm0[7]);
      

      //At this stage each vector xmm1 contains the data from 16 adjacent bytes
      for (int ll=0; ll<8; ll++){
        __vector uint8_t xmm = xmm1[ll];
        //helper_print(xmm, "vector transposed");
        for (kk=0; kk<8; kk++) {
           __vector uint16_t tmp;
           tmp = (__vector uint16_t) vec_bperm(xmm, masks[kk]);
           //printf("%d %d\n", vp, tmp[4]);
           //helper_print((__vector uint8_t)tmp, "tmp");
           out_s[vp++] = tmp[4];
        }
      }
    }
  }
}

/* Routine optimized for bit-shuffling a buffer for a type size of 1 byte: 
 * non coalesced as the vector write was slower. Loop unrolling neither helps */
int64_t bitshuffle1_altivec(void* src, void* dest, const size_t size,  const size_t elem_size) {
  // Nota elem_size==1 and size%8==0 !
  const uint8_t* b_src = (const uint8_t*) src;
  uint8_t* b_dest = (uint8_t*) dest;
  __vector uint8_t masks[8], data;
  size_t i, j;
  const size_t nbyte = elem_size * size;
  int64_t count;
  
  //Generate all 8 needed masks
  for (i=0; i<8; i++){
    masks[i] = make_bitperm_mask(1, i);
  }

  for (j=0; j+15<size; j+=16) {
    data = vec_xl(j, b_src);
    for (i=0; i<8; i++) {
      __vector uint16_t tmp;
      uint16_t* out_ui16;
      tmp = (__vector uint16_t) vec_bperm(data, masks[i]);
      out_ui16 = (uint16_t*)&b_dest[(j + i*size) >> 3];
      *out_ui16 = tmp[4];
    }
  }
  count = bshuf_trans_bit_byte_remainder(src, dest, size, elem_size,
                                         nbyte - nbyte % 16);
  return count;
}  

/* Transpose bytes within elements for 16 bit elements. */
int64_t bshuf_trans_byte_elem_SSE_16(void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 2;
  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  __vector uint8_t xmm0[2], xmm1[2];
  size_t ii, j;
  
  for (ii = 0; ii + 15 < size; ii += 16) {
    for (j=0; j<bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype*ii + 16*j, in_b);
    
    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);

    xmm0[0] = unpacklo_epi8(xmm1[0], xmm1[1]);
    xmm0[1] = unpackhi_epi8(xmm1[0], xmm1[1]);

    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);

    xmm0[0] = unpacklo_epi8(xmm1[0], xmm1[1]);
    xmm0[1] = unpackhi_epi8(xmm1[0], xmm1[1]);

    for (j=0; j<bytesoftype; j++)
      vec_xst(xmm0[j], ii + j*size, out_b);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 32 bit elements. */
int64_t bshuf_trans_byte_elem_SSE_32(void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 4;
  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  __vector uint8_t xmm0[4], xmm1[4];
  size_t ii, j;

  for (ii = 0; ii + 15 < size; ii += 16) {
    for (j=0; j<bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype * ii + 16*j, in_b);

    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);
    xmm1[2] = unpacklo_epi8(xmm0[2], xmm0[3]);
    xmm1[3] = unpackhi_epi8(xmm0[2], xmm0[3]);

    xmm0[0] = unpacklo_epi8(xmm1[0], xmm1[1]);
    xmm0[1] = unpackhi_epi8(xmm1[0], xmm1[1]);
    xmm0[2] = unpacklo_epi8(xmm1[2], xmm1[3]);
    xmm0[3] = unpackhi_epi8(xmm1[2], xmm1[3]);

    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);
    xmm1[2] = unpacklo_epi8(xmm0[2], xmm0[3]);
    xmm1[3] = unpackhi_epi8(xmm0[2], xmm0[3]);

    xmm0[0] = unpacklo_epi64(xmm1[0], xmm1[2]);
    xmm0[1] = unpackhi_epi64(xmm1[0], xmm1[2]);
    xmm0[2] = unpacklo_epi64(xmm1[1], xmm1[3]);
    xmm0[3] = unpackhi_epi64(xmm1[1], xmm1[3]);

    for (j=0; j<bytesoftype; j++)
      vec_xst(xmm0[j], ii + j*size, out_b);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 64 bit elements. */
int64_t bshuf_trans_byte_elem_SSE_64(void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 8;
  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  __vector uint8_t xmm0[8], xmm1[8];
  size_t ii, j;

  for (ii = 0; ii + 15 < size; ii += 16) {
    for (j=0; j<bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype * ii + 16*j, in_b);

    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);
    xmm1[2] = unpacklo_epi8(xmm0[2], xmm0[3]);
    xmm1[3] = unpackhi_epi8(xmm0[2], xmm0[3]);
    xmm1[4] = unpacklo_epi8(xmm0[4], xmm0[5]);
    xmm1[5] = unpackhi_epi8(xmm0[4], xmm0[5]);
    xmm1[6] = unpacklo_epi8(xmm0[6], xmm0[7]);
    xmm1[7] = unpackhi_epi8(xmm0[6], xmm0[7]);

    xmm0[0] = unpacklo_epi8(xmm1[0], xmm1[1]);
    xmm0[1] = unpackhi_epi8(xmm1[0], xmm1[1]);
    xmm0[2] = unpacklo_epi8(xmm1[2], xmm1[3]);
    xmm0[3] = unpackhi_epi8(xmm1[2], xmm1[3]);
    xmm0[4] = unpacklo_epi8(xmm1[4], xmm1[5]);
    xmm0[5] = unpackhi_epi8(xmm1[4], xmm1[5]);
    xmm0[6] = unpacklo_epi8(xmm1[6], xmm1[7]);
    xmm0[7] = unpackhi_epi8(xmm1[6], xmm1[7]);

    xmm1[0] = unpacklo_epi32(xmm0[0], xmm0[2]);
    xmm1[1] = unpackhi_epi32(xmm0[0], xmm0[2]);
    xmm1[2] = unpacklo_epi32(xmm0[1], xmm0[3]);
    xmm1[3] = unpackhi_epi32(xmm0[1], xmm0[3]);
    xmm1[4] = unpacklo_epi32(xmm0[4], xmm0[6]);
    xmm1[5] = unpackhi_epi32(xmm0[4], xmm0[6]);
    xmm1[6] = unpacklo_epi32(xmm0[5], xmm0[7]);
    xmm1[7] = unpackhi_epi32(xmm0[5], xmm0[7]);

    xmm0[0] = unpacklo_epi64(xmm1[0], xmm1[4]);
    xmm0[1] = unpackhi_epi64(xmm1[0], xmm1[4]);
    xmm0[2] = unpacklo_epi64(xmm1[1], xmm1[5]);
    xmm0[3] = unpackhi_epi64(xmm1[1], xmm1[5]);
    xmm0[4] = unpacklo_epi64(xmm1[2], xmm1[6]);
    xmm0[5] = unpackhi_epi64(xmm1[2], xmm1[6]);
    xmm0[6] = unpacklo_epi64(xmm1[3], xmm1[7]);
    xmm0[7] = unpackhi_epi64(xmm1[3], xmm1[7]);

    for (j=0; j<bytesoftype; j++)
      vec_xst(xmm0[j], ii + j*size, out_b);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Memory copy with bshuf call signature. */
int64_t bshuf_copy(void* in, void* out, const size_t size,
                   const size_t elem_size) {

  char* in_b = (char*)in;
  char* out_b = (char*)out;

  memcpy(out_b, in_b, size * elem_size);
  return size * elem_size;
}


/* Transpose bytes within elements using best SSE algorithm available. */
int64_t bshuf_trans_byte_elem_altivec(void* in, void* out, const size_t size,
                                      const size_t elem_size, void* tmp_buf) {

  int64_t count;

  /*  Trivial cases: power of 2 bytes. */
  switch (elem_size) {
    case 1:
      count = bshuf_copy(in, out, size, elem_size);
      return count;
    case 2:
      count = bshuf_trans_byte_elem_SSE_16(in, out, size);
      return count;
    case 4:
      count = bshuf_trans_byte_elem_SSE_32(in, out, size);
      return count;
    case 8:
      count = bshuf_trans_byte_elem_SSE_64(in, out, size);
      return count;
  }

  /*  Worst case: odd number of bytes. Turns out that this is faster for */
  /*  (odd * 2) byte elements as well (hence % 4). */
  if (elem_size % 4) {
    count = bshuf_trans_byte_elem_scal(in, out, size, elem_size);
    return count;
  }

  /*  Multiple of power of 2: transpose hierarchically. */
  {
    size_t nchunk_elem;

    if ((elem_size % 8) == 0) {
      nchunk_elem = elem_size / 8;
      TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int64_t);
      count = bshuf_trans_byte_elem_SSE_64(out, tmp_buf,
                                           size * nchunk_elem);
      bshuf_trans_elem(tmp_buf, out, 8, nchunk_elem, size);
    } else if ((elem_size % 4) == 0) {
      nchunk_elem = elem_size / 4;
      TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int32_t);
      count = bshuf_trans_byte_elem_SSE_32(out, tmp_buf,
                                           size * nchunk_elem);
      bshuf_trans_elem(tmp_buf, out, 4, nchunk_elem, size);
    } else {
      /*  Not used since scalar algorithm is faster. */
      nchunk_elem = elem_size / 2;
      TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int16_t);
      count = bshuf_trans_byte_elem_SSE_16(out, tmp_buf,
                                           size * nchunk_elem);
      bshuf_trans_elem(tmp_buf, out, 2, nchunk_elem, size);
    }

    return count;
  }
}


/* Transpose bits within bytes. */
int64_t bshuf_trans_bit_byte_altivec(void* in, void* out, const size_t size,
                                     const size_t elem_size) {

  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  int64_t count;
  size_t nbyte = elem_size * size;
  __vector uint8_t data, masks[8];
  size_t ii, kk;

  CHECK_MULT_EIGHT(nbyte);
   
  //Generate all 8 needed masks
  for (kk=0; kk<8; kk++){
    masks[kk] = make_bitperm_mask(1, kk);
  }

  for (ii = 0; ii + 15 < nbyte; ii += 16) {
    data = vec_xl(ii, in_b);
    for (kk = 0; kk < 8; kk++) {
      __vector uint16_t tmp;
      uint16_t* oui16;
      tmp = (__vector uint16_t) vec_bperm(data, masks[kk]);
      oui16 = (uint16_t*)&out_b[(ii + kk*nbyte) >> 3];
      *oui16 = tmp[4];
    }
  }
  count = bshuf_trans_bit_byte_remainder(in, out, size, elem_size,
                                         nbyte - nbyte % 16);
  return count;
}


/* Transpose bits within elements. */
int64_t bshuf_trans_bit_elem_altivec(void* in, void* out, const size_t size,
                                     const size_t elem_size, void* tmp_buf) {

  int64_t count;

  CHECK_MULT_EIGHT(size);

  count = bshuf_trans_byte_elem_altivec(in, out, size, elem_size, tmp_buf);
  CHECK_ERR(count);
  //bshuf_trans_bit_byte_altivec / bitshuffle1_altivec
  count = bshuf_trans_bit_byte_altivec(out, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count = bshuf_trans_bitrow_eight(tmp_buf, out, size, elem_size);

  return count;
}


/* For data organized into a row for each bit (8 * elem_size rows), transpose
 * the bytes. */
int64_t bshuf_trans_byte_bitrow_altivec(void* in, void* out, const size_t size,
                                        const size_t elem_size) {

  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  size_t nrows = 8 * elem_size;
  size_t nbyte_row = size / 8;
  size_t ii, jj;

  __vector uint8_t a0, b0, c0, d0, e0, f0, g0, h0;
  __vector uint8_t a1, b1, c1, d1, e1, f1, g1, h1;
  __m128* as, * bs, * cs, * ds, * es, * fs, * gs, * hs;

  CHECK_MULT_EIGHT(size);

  for (ii = 0; ii + 7 < nrows; ii += 8) {
    for (jj = 0; jj + 15 < nbyte_row; jj += 16) {
      a0 = vec_xl((ii + 0) * nbyte_row + jj,in_b);
      b0 = vec_xl((ii + 1) * nbyte_row + jj,in_b);
      c0 = vec_xl((ii + 2) * nbyte_row + jj,in_b);
      d0 = vec_xl((ii + 3) * nbyte_row + jj,in_b);
      e0 = vec_xl((ii + 4) * nbyte_row + jj,in_b);
      f0 = vec_xl((ii + 5) * nbyte_row + jj,in_b);
      g0 = vec_xl((ii + 6) * nbyte_row + jj,in_b);
      h0 = vec_xl((ii + 7) * nbyte_row + jj,in_b);

      a1 = unpacklo_epi8(a0, b0);
      b1 = unpacklo_epi8(c0, d0);
      c1 = unpacklo_epi8(e0, f0);
      d1 = unpacklo_epi8(g0, h0);
      e1 = unpackhi_epi8(a0, b0);
      f1 = unpackhi_epi8(c0, d0);
      g1 = unpackhi_epi8(e0, f0);
      h1 = unpackhi_epi8(g0, h0);


      a0 = unpacklo_epi16(a1, b1);
      b0 = unpacklo_epi16(c1, d1);
      c0 = unpackhi_epi16(a1, b1);
      d0 = unpackhi_epi16(c1, d1);
      e0 = unpacklo_epi16(e1, f1);
      f0 = unpacklo_epi16(g1, h1);
      g0 = unpackhi_epi16(e1, f1);
      h0 = unpackhi_epi16(g1, h1);


      a1 = unpacklo_epi32(a0, b0);
      b1 = unpackhi_epi32(a0, b0);
      c1 = unpacklo_epi32(c0, d0);
      d1 = unpackhi_epi32(c0, d0);
      e1 = unpacklo_epi32(e0, f0);
      f1 = unpackhi_epi32(e0, f0);
      g1 = unpacklo_epi32(g0, h0);
      h1 = unpackhi_epi32(g0, h0);

      /*  We don't have a storeh instruction for integers, so interpret */
      /*  as a float. Have a storel (_mm_storel_epi64). */
      as = (__m128*)&a1;
      bs = (__m128*)&b1;
      cs = (__m128*)&c1;
      ds = (__m128*)&d1;
      es = (__m128*)&e1;
      fs = (__m128*)&f1;
      gs = (__m128*)&g1;
      hs = (__m128*)&h1;

      _mm_storel_pi((__m64*)&out_b[(jj + 0) * nrows + ii], *as);
      _mm_storel_pi((__m64*)&out_b[(jj + 2) * nrows + ii], *bs);
      _mm_storel_pi((__m64*)&out_b[(jj + 4) * nrows + ii], *cs);
      _mm_storel_pi((__m64*)&out_b[(jj + 6) * nrows + ii], *ds);
      _mm_storel_pi((__m64*)&out_b[(jj + 8) * nrows + ii], *es);
      _mm_storel_pi((__m64*)&out_b[(jj + 10) * nrows + ii], *fs);
      _mm_storel_pi((__m64*)&out_b[(jj + 12) * nrows + ii], *gs);
      _mm_storel_pi((__m64*)&out_b[(jj + 14) * nrows + ii], *hs);

      _mm_storeh_pi((__m64*)&out_b[(jj + 1) * nrows + ii], *as);
      _mm_storeh_pi((__m64*)&out_b[(jj + 3) * nrows + ii], *bs);
      _mm_storeh_pi((__m64*)&out_b[(jj + 5) * nrows + ii], *cs);
      _mm_storeh_pi((__m64*)&out_b[(jj + 7) * nrows + ii], *ds);
      _mm_storeh_pi((__m64*)&out_b[(jj + 9) * nrows + ii], *es);
      _mm_storeh_pi((__m64*)&out_b[(jj + 11) * nrows + ii], *fs);
      _mm_storeh_pi((__m64*)&out_b[(jj + 13) * nrows + ii], *gs);
      _mm_storeh_pi((__m64*)&out_b[(jj + 15) * nrows + ii], *hs);
    }
    for (jj = nbyte_row - nbyte_row % 16; jj < nbyte_row; jj++) {
      out_b[jj * nrows + ii + 0] = in_b[(ii + 0) * nbyte_row + jj];
      out_b[jj * nrows + ii + 1] = in_b[(ii + 1) * nbyte_row + jj];
      out_b[jj * nrows + ii + 2] = in_b[(ii + 2) * nbyte_row + jj];
      out_b[jj * nrows + ii + 3] = in_b[(ii + 3) * nbyte_row + jj];
      out_b[jj * nrows + ii + 4] = in_b[(ii + 4) * nbyte_row + jj];
      out_b[jj * nrows + ii + 5] = in_b[(ii + 5) * nbyte_row + jj];
      out_b[jj * nrows + ii + 6] = in_b[(ii + 6) * nbyte_row + jj];
      out_b[jj * nrows + ii + 7] = in_b[(ii + 7) * nbyte_row + jj];
    }
  }
  return size * elem_size;
}


/* Shuffle bits within the bytes of eight element blocks. */
int64_t bshuf_shuffle_bit_eightelem_altivec(void* in, void* out, const size_t size,
                                            const size_t elem_size) {
  /*  With a bit of care, this could be written such that such that it is */
  /*  in_buf = out_buf safe. */
  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  size_t nbyte = elem_size * size;
  __vector uint8_t masks[8], data;
  size_t ii, jj, kk;
  size_t ind;

  CHECK_MULT_EIGHT(size);

  //Generate all 8 needed masks
  for (kk=0; kk<8; kk++){
    masks[kk] = make_bitperm_mask(1, kk);
  }

  if (elem_size % 2) {
    bshuf_shuffle_bit_eightelem_scal(in, out, size, elem_size);
  } else {
    for (ii = 0; ii + 8 * elem_size - 1 < nbyte;
         ii += 8 * elem_size) {
      for (jj = 0; jj + 15 < 8 * elem_size; jj += 16) {
        data = vec_xl(ii + jj, in_b);
        for (kk = 0; kk < 8; kk++) {
          __vector uint16_t tmp;
          uint16_t* oui16;
          tmp = (__vector uint16_t) vec_bperm(data, masks[kk]);
          oui16 = (uint16_t*)&out_b[ii + (jj>>3) + kk * elem_size];
          *oui16 = tmp[4];
        }
      }
    }
  }
  return size * elem_size;
}


/* Untranspose bits within elements. */
int64_t bshuf_untrans_bit_elem_altivec(void* in, void* out, const size_t size,
                                       const size_t elem_size, void* tmp_buf) {

  int64_t count;

  CHECK_MULT_EIGHT(size);

  count = bshuf_trans_byte_bitrow_altivec(in, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count = bshuf_shuffle_bit_eightelem_altivec(tmp_buf, out, size, elem_size);

  return count;
}

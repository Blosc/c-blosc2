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

//mind to use  vec_bperm(a, epi8_hi) for bitshuffling

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


// Load a vector from any location in memory ... may be sub-optimal
static __vector uint8_t vec_ld_generic(int32_t position,  const uint8_t* const src){
  const int32_t offset = ((size_t)src + position) & 0xF; // check alignment on 16 bytes
  __vector uint8_t vector;
  if (offset){ // Actually unaligned load
    vector = vec_vsx_ld(position, src);
  }
  else{ // Aligned load, the usual way
    vector = vec_ld(position, src);
  }
  return vector;
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


/* Transpose bytes within elements for 16 bit elements. */
int64_t bshuf_trans_byte_elem_SSE_16(void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 2;
  char* in_b = (char*)in;
  char* out_b = (char*)out;
  __vector uint8_t xmm0[2], xmm1[2];
  size_t ii, j;
  
  for (ii = 0; ii + 15 < size; ii += 16) {
    for (j=0; j<bytesoftype; j++)
      xmm0[j] = vec_ld_generic(2*ii + 16*j, in_b);
    
    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);

    xmm0[0] = unpacklo_epi8(xmm1[0], xmm1[1]);
    xmm0[1] = unpackhi_epi8(xmm1[0], xmm1[1]);

    xmm1[0] = unpacklo_epi8(xmm0[0], xmm0[1]);
    xmm1[1] = unpackhi_epi8(xmm0[0], xmm0[1]);

    xmm0[0] = unpacklo_epi8(xmm1[0], xmm1[1]);
    xmm0[1] = unpackhi_epi8(xmm1[0], xmm1[1]);

    for (j=0; j<bytesoftype; j++)
      vec_st_generic(xmm0[j], ii + j*size, out_b);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 32 bit elements. */
int64_t bshuf_trans_byte_elem_SSE_32(void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 4;
  char* in_b = (char*)in;
  char* out_b = (char*)out;
  __vector uint8_t xmm0[4], xmm1[4];
  size_t ii, j;

  for (ii = 0; ii + 15 < size; ii += 16) {
    for (j=0; j<bytesoftype; j++)
      xmm0[j] = vec_ld_generic(bytesoftype * ii + 16*j, in_b);

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
      vec_st_generic(xmm0[j], ii + j*size, out_b);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 64 bit elements. */
int64_t bshuf_trans_byte_elem_SSE_64(void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 8;
  char* in_b = (char*)in;
  char* out_b = (char*)out;
  __vector uint8_t xmm0[8], xmm1[8];
  size_t ii, j;

  for (ii = 0; ii + 15 < size; ii += 16) {
    for (j=0; j<bytesoftype; j++)
      xmm0[j] = vec_ld_generic(bytesoftype * ii + 16*j, in_b);

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
      vec_st_generic(xmm0[j], ii + j*size, out_b);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, 8,
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

  char* in_b = (char*)in;
  char* out_b = (char*)out;
  uint16_t* out_ui16;
  int64_t count;
  size_t nbyte = elem_size * size;
  __m128i xmm;
  int32_t bt;
  size_t ii, kk;

  CHECK_MULT_EIGHT(nbyte);

  for (ii = 0; ii + 15 < nbyte; ii += 16) {
    xmm = _mm_loadu_si128((__m128i*)&in_b[ii]);
    for (kk = 0; kk < 8; kk++) {
      bt = _mm_movemask_epi8(xmm);
      xmm = _mm_slli_epi16(xmm, 1);
      out_ui16 = (uint16_t*)&out_b[((7 - kk) * nbyte + ii) / 8];
      *out_ui16 = (uint16_t)bt;
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
  count = bshuf_trans_bit_byte_altivec(out, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count = bshuf_trans_bitrow_eight(tmp_buf, out, size, elem_size);

  return count;
}


/* For data organized into a row for each bit (8 * elem_size rows), transpose
 * the bytes. */
int64_t bshuf_trans_byte_bitrow_altivec(void* in, void* out, const size_t size,
                                        const size_t elem_size) {

  char* in_b = (char*)in;
  char* out_b = (char*)out;
  size_t nrows = 8 * elem_size;
  size_t nbyte_row = size / 8;
  size_t ii, jj;

  __m128i a0, b0, c0, d0, e0, f0, g0, h0;
  __m128i a1, b1, c1, d1, e1, f1, g1, h1;
  __m128* as, * bs, * cs, * ds, * es, * fs, * gs, * hs;

  CHECK_MULT_EIGHT(size);

  for (ii = 0; ii + 7 < nrows; ii += 8) {
    for (jj = 0; jj + 15 < nbyte_row; jj += 16) {
      a0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 0) * nbyte_row + jj]);
      b0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 1) * nbyte_row + jj]);
      c0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 2) * nbyte_row + jj]);
      d0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 3) * nbyte_row + jj]);
      e0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 4) * nbyte_row + jj]);
      f0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 5) * nbyte_row + jj]);
      g0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 6) * nbyte_row + jj]);
      h0 = _mm_loadu_si128((__m128i*)&in_b[(ii + 7) * nbyte_row + jj]);


      a1 = _mm_unpacklo_epi8(a0, b0);
      b1 = _mm_unpacklo_epi8(c0, d0);
      c1 = _mm_unpacklo_epi8(e0, f0);
      d1 = _mm_unpacklo_epi8(g0, h0);
      e1 = _mm_unpackhi_epi8(a0, b0);
      f1 = _mm_unpackhi_epi8(c0, d0);
      g1 = _mm_unpackhi_epi8(e0, f0);
      h1 = _mm_unpackhi_epi8(g0, h0);


      a0 = _mm_unpacklo_epi16(a1, b1);
      b0 = _mm_unpacklo_epi16(c1, d1);
      c0 = _mm_unpackhi_epi16(a1, b1);
      d0 = _mm_unpackhi_epi16(c1, d1);

      e0 = _mm_unpacklo_epi16(e1, f1);
      f0 = _mm_unpacklo_epi16(g1, h1);
      g0 = _mm_unpackhi_epi16(e1, f1);
      h0 = _mm_unpackhi_epi16(g1, h1);


      a1 = _mm_unpacklo_epi32(a0, b0);
      b1 = _mm_unpackhi_epi32(a0, b0);

      c1 = _mm_unpacklo_epi32(c0, d0);
      d1 = _mm_unpackhi_epi32(c0, d0);

      e1 = _mm_unpacklo_epi32(e0, f0);
      f1 = _mm_unpackhi_epi32(e0, f0);

      g1 = _mm_unpacklo_epi32(g0, h0);
      h1 = _mm_unpackhi_epi32(g0, h0);

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
  char* in_b = (char*)in;
  uint16_t* out_ui16 = (uint16_t*)out;

  size_t nbyte = elem_size * size;

  __m128i xmm;
  int32_t bt;
  size_t ii, jj, kk;
  size_t ind;

  CHECK_MULT_EIGHT(size);

  if (elem_size % 2) {
    bshuf_shuffle_bit_eightelem_scal(in, out, size, elem_size);
  } else {
    for (ii = 0; ii + 8 * elem_size - 1 < nbyte;
         ii += 8 * elem_size) {
      for (jj = 0; jj + 15 < 8 * elem_size; jj += 16) {
        xmm = _mm_loadu_si128((__m128i*)&in_b[ii + jj]);
        for (kk = 0; kk < 8; kk++) {
          bt = _mm_movemask_epi8(xmm);
          xmm = _mm_slli_epi16(xmm, 1);
          ind = (ii + jj / 8 + (7 - kk) * elem_size);
          out_ui16[ind / 2] = (uint16_t)bt;
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

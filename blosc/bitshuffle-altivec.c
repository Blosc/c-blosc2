/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  Bitshuffle - Filter for improving compression of typed binary data.

  Author: Kiyoshi Masui <kiyo@physics.ubc.ca>
  Website: https://github.com/kiyo-masui/bitshuffle

  Note: Adapted for c-blosc by Francesc Alted
        Altivec/VSX version by Jerome Kieffer.

  See LICENSES/BITSHUFFLE.txt file for details about copyright and
  rights to use.
**********************************************************************/


#include "bitshuffle-altivec.h"
#include "bitshuffle-generic.h"
#include <stdlib.h>

/* Make sure ALTIVEC is available for the compilation target and compiler. */
#if defined(__ALTIVEC__) && defined(__VSX__) && defined(_ARCH_PWR8)

#include "transpose-altivec.h"

#include <altivec.h>

#include <stdint.h>

/* The next is useful for debugging purposes */
#if 0
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


// Build and return a bit-permutation mask
static __vector uint8_t make_bitperm_mask(int type_size, int bit) {
  __vector uint8_t result;
  if (type_size == 1) {
    // data_type is 8 bits long
    for (int i = 0; i < 16; i++)
      result[i] = 8 * (15 - i) + (7 - bit);
  }
  else if (type_size == 2) {
    // data_type is 16 bits long
    for (int i = 0; i < 8; i++) {
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
  // Vector masks
  static const __vector uint8_t lo01 = (const __vector uint8_t) {
    0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x0d,
    0x10, 0x11, 0x14, 0x15, 0x18, 0x19, 0x1c, 0x1d};
  static const __vector uint8_t hi01 = (const __vector uint8_t) {
    0x02, 0x03, 0x06, 0x07, 0x0a, 0x0b, 0x0e, 0x0f,
    0x12, 0x13, 0x16, 0x17, 0x1a, 0x1b, 0x1e, 0x1f};
  static const __vector uint8_t lo02 = (const __vector uint8_t) {
    0x00, 0x01, 0x08, 0x09, 0x10, 0x11, 0x18, 0x19,
    0x02, 0x03, 0x0a, 0x0b, 0x12, 0x13, 0x1a, 0x1b};
  static const __vector uint8_t hi02 = (const __vector uint8_t) {
    0x04, 0x05, 0x0c, 0x0d, 0x14, 0x15, 0x1c, 0x1d,
    0x06, 0x07, 0x0e, 0x0f, 0x16, 0x17, 0x1e, 0x1f};
  static const __vector uint8_t epi64_low = (const __vector uint8_t) {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  static const __vector uint8_t epi64_hi = (const __vector uint8_t) {
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

  for (kk = 0; kk < 8; kk++){
    __vector uint8_t msk;
    for (ii = 0; ii < 8; ii++){
      msk[ii] = 127-(16*ii+2*kk);
      msk[ii+8] = 127-(16*ii+2*kk+1);
    }
    //helper_print(msk, "Mask");
    masks[kk] = msk;
  }

  // read the data
  vp = 0;
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
      xmm1[0] = vec_perm(xmm0[0], xmm1[4], epi64_low);
      xmm1[2] = vec_perm(xmm0[0], xmm1[4], epi64_hi);
      xmm1[1] = vec_perm(xmm0[1], xmm1[5], epi64_low);
      xmm1[3] = vec_perm(xmm0[1], xmm1[5], epi64_hi);
      xmm1[4] = vec_perm(xmm0[2], xmm1[6], epi64_low);
      xmm1[6] = vec_perm(xmm0[2], xmm1[6], epi64_hi);
      xmm1[5] = vec_perm(xmm0[3], xmm1[7], epi64_low);
      xmm1[7] = vec_perm(xmm0[3], xmm1[7], epi64_hi);

      // At this stage each vector xmm1 contains the data from 16 adjacent bytes
      for (int ll = 0; ll < 8; ll++){
        __vector uint8_t xmm = xmm1[ll];
        //helper_print(xmm, "vector transposed");
        for (kk = 0; kk < 8; kk++) {
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


/* Transpose bytes within elements for 16 bit elements. */
int64_t bshuf_trans_byte_elem_16(const void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 2;
  __vector uint8_t xmm0[2];

  for (size_t i = 0; i + 15 < size; i += 16) {
    for (int j = 0; j < bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype * i + 16 * j, (const uint8_t*)in);

    /* Transpose vectors */
    transpose2x16(xmm0);

    for (int j = 0; j < bytesoftype; j++)
      vec_xst(xmm0[j], i + j * size, (uint8_t*)out);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 32 bit elements. */
int64_t bshuf_trans_byte_elem_32(const void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 4;
  __vector uint8_t xmm0[4];

  for (size_t i = 0; i + 15 < size; i += 16) {
    for (int j = 0; j < bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype * i + 16 * j, (const uint8_t*)in);

    /* Transpose vectors */
    transpose4x16(xmm0);

    for (int j = 0; j < bytesoftype; j++)
      vec_xst(xmm0[j], i + j * size, (uint8_t*)out);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 64 bit elements. */
int64_t bshuf_trans_byte_elem_64(const void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 8;
  __vector uint8_t xmm0[8];

  for (size_t i = 0; i + 15 < size; i += 16) {
    for (int j = 0; j < bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype * i + 16 * j, (const uint8_t*)in);

    /* Transpose vectors */
    transpose8x16(xmm0);

    for (int j = 0; j < bytesoftype; j++)
      vec_xst(xmm0[j], i + j * size, (uint8_t*)out);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements for 128 bit elements. */
int64_t bshuf_trans_byte_elem_128(const void* in, void* out, const size_t size) {
  static const uint8_t bytesoftype = 16;
  __vector uint8_t xmm0[16];

  for (size_t i = 0; i + 15 < size; i += 16) {
    for (int j = 0; j < bytesoftype; j++)
      xmm0[j] = vec_xl(bytesoftype * i + 16 * j, (const uint8_t*)in);

    /* Transpose vectors */
    transpose16x16(xmm0);

    for (int j = 0; j < bytesoftype; j++)
      vec_xst(xmm0[j], i + j * size, (uint8_t*)out);
  }
  return bshuf_trans_byte_elem_remainder(in, out, size, bytesoftype,
                                         size - size % 16);
}


/* Transpose bytes within elements using best SSE algorithm available. */
int64_t bshuf_trans_byte_elem_altivec(const void* in, void* out, const size_t size,
                                      const size_t elem_size, void* tmp_buf) {

  int64_t count;

  /*  Trivial cases: power of 2 bytes. */
  switch (elem_size) {
    case 1:
      count = bshuf_copy(in, out, size, elem_size);
      return count;
    case 2:
      count = bshuf_trans_byte_elem_16(in, out, size);
      return count;
    case 4:
      count = bshuf_trans_byte_elem_32(in, out, size);
      return count;
    case 8:
      count = bshuf_trans_byte_elem_64(in, out, size);
      return count;
    case 16:
      count = bshuf_trans_byte_elem_128(in, out, size);
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

    if ((elem_size % 16) == 0) {
      nchunk_elem = elem_size / 16;
      TRANS_ELEM_TYPE(in, out, size, nchunk_elem, __vector uint8_t);
      count = bshuf_trans_byte_elem_128(out, tmp_buf,
                                        size * nchunk_elem);
      bshuf_trans_elem(tmp_buf, out, 16, nchunk_elem, size);
    } else if ((elem_size % 8) == 0) {
        nchunk_elem = elem_size / 8;
        TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int64_t);
        count = bshuf_trans_byte_elem_64(out, tmp_buf,
                                         size * nchunk_elem);
        bshuf_trans_elem(tmp_buf, out, 8, nchunk_elem, size);
    } else if ((elem_size % 4) == 0) {
      nchunk_elem = elem_size / 4;
      TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int32_t);
      count = bshuf_trans_byte_elem_32(out, tmp_buf,
                                           size * nchunk_elem);
      bshuf_trans_elem(tmp_buf, out, 4, nchunk_elem, size);
    } else {
      /*  Not used since scalar algorithm is faster. */
      nchunk_elem = elem_size / 2;
      TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int16_t);
      count = bshuf_trans_byte_elem_16(out, tmp_buf, size * nchunk_elem);
      bshuf_trans_elem(tmp_buf, out, 2, nchunk_elem, size);
    }

    return count;
  }
}


/* Transpose bits within bytes. */
int64_t bshuf_trans_bit_byte_altivec(const void* in, void* out, const size_t size,
                                     const size_t elem_size) {

  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  int64_t count;
  size_t nbyte = elem_size * size;
  __vector uint8_t data, masks[8];
  size_t ii, kk;

  CHECK_MULT_EIGHT(nbyte);

  // Generate all 8 needed masks
  for (kk = 0; kk < 8; kk++){
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
int64_t bshuf_trans_bit_elem_altivec(const void* in, void* out, const size_t size,
                                     const size_t elem_size) {

  int64_t count;

  CHECK_MULT_EIGHT(size);

  void* tmp_buf = malloc(size * elem_size);
  if (tmp_buf == NULL) return -1;

  count = bshuf_trans_byte_elem_altivec(in, out, size, elem_size, tmp_buf);
  CHECK_ERR(count);
  // bshuf_trans_bit_byte_altivec / bitshuffle1_altivec
  count = bshuf_trans_bit_byte_altivec(out, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count = bshuf_trans_bitrow_eight(tmp_buf, out, size, elem_size);

  free(tmp_buf);

  return count;
}

/* For data organized into a row for each bit (8 * elem_size rows), transpose
 * the bytes. */
int64_t bshuf_trans_byte_bitrow_altivec(const void* in, void* out, const size_t size,
                                        const size_t elem_size) {
  static const __vector uint8_t epi8_low = (const __vector uint8_t) {
    0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13,
    0x04, 0x14, 0x05, 0x15, 0x06, 0x16, 0x07, 0x17};
  static const __vector uint8_t epi8_hi = (const __vector uint8_t) {
    0x08, 0x18, 0x09, 0x19, 0x0a, 0x1a, 0x0b, 0x1b,
    0x0c, 0x1c, 0x0d, 0x1d, 0x0e, 0x1e, 0x0f, 0x1f};
  static const __vector uint8_t epi16_low = (const __vector uint8_t) {
    0x00, 0x01, 0x10, 0x11, 0x02, 0x03, 0x12, 0x13,
    0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x16, 0x17};
  static const __vector uint8_t epi16_hi = (const __vector uint8_t) {
    0x08, 0x09, 0x18, 0x19, 0x0a, 0x0b, 0x1a, 0x1b,
    0x0c, 0x0d, 0x1c, 0x1d, 0x0e, 0x0f, 0x1e, 0x1f};
  static const __vector uint8_t epi32_low = (const __vector uint8_t) {
    0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13,
    0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17};
  static const __vector uint8_t epi32_hi = (const __vector uint8_t) {
    0x08, 0x09, 0x0a, 0x0b, 0x18, 0x19, 0x1a, 0x1b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x1c, 0x1d, 0x1e, 0x1f};
  static const __vector uint8_t epi64_low = (const __vector uint8_t) {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  static const __vector uint8_t epi64_hi = (const __vector uint8_t) {
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  size_t nrows = 8 * elem_size;
  size_t nbyte_row = size / 8;
  __vector uint8_t xmm0[16], xmm1[16];

  CHECK_MULT_EIGHT(size);

  // The optimized algorithms can only deal with even values or 1 for elem_size
  if ((elem_size > 1) && (elem_size % 2)) {
    return bshuf_trans_byte_bitrow_scal(in, out, size, elem_size);
  }

  int nvectors = (elem_size == 1) ? 8 : 16;
  for (size_t ii = 0; ii + (nvectors - 1) < nrows; ii += nvectors) {
    for (size_t jj = 0; jj + 15 < nbyte_row; jj += 16) {  // vectors of 16 elements

      if (elem_size == 1) {
        for (int k = 0; k < 8; k++) {
          xmm0[k] = vec_xl((ii + k) * nbyte_row + jj, in_b);
        }

        xmm1[0] = vec_perm(xmm0[0], xmm0[1], epi8_low);
        xmm1[1] = vec_perm(xmm0[2], xmm0[3], epi8_low);
        xmm1[2] = vec_perm(xmm0[4], xmm0[5], epi8_low);
        xmm1[3] = vec_perm(xmm0[6], xmm0[7], epi8_low);
        xmm1[4] = vec_perm(xmm0[0], xmm0[1], epi8_hi);
        xmm1[5] = vec_perm(xmm0[2], xmm0[3], epi8_hi);
        xmm1[6] = vec_perm(xmm0[4], xmm0[5], epi8_hi);
        xmm1[7] = vec_perm(xmm0[6], xmm0[7], epi8_hi);

        xmm0[0] = vec_perm(xmm1[0], xmm1[1], epi16_low);
        xmm0[1] = vec_perm(xmm1[2], xmm1[3], epi16_low);
        xmm0[2] = vec_perm(xmm1[0], xmm1[1], epi16_hi);
        xmm0[3] = vec_perm(xmm1[2], xmm1[3], epi16_hi);
        xmm0[4] = vec_perm(xmm1[4], xmm1[5], epi16_low);
        xmm0[5] = vec_perm(xmm1[6], xmm1[7], epi16_low);
        xmm0[6] = vec_perm(xmm1[4], xmm1[5], epi16_hi);
        xmm0[7] = vec_perm(xmm1[6], xmm1[7], epi16_hi);

        xmm1[0] = vec_perm(xmm0[0], xmm0[1], epi32_low);
        xmm1[1] = vec_perm(xmm0[0], xmm0[1], epi32_hi);
        xmm1[2] = vec_perm(xmm0[2], xmm0[3], epi32_low);
        xmm1[3] = vec_perm(xmm0[2], xmm0[3], epi32_hi);
        xmm1[4] = vec_perm(xmm0[4], xmm0[5], epi32_low);
        xmm1[5] = vec_perm(xmm0[4], xmm0[5], epi32_hi);
        xmm1[6] = vec_perm(xmm0[6], xmm0[7], epi32_low);
        xmm1[7] = vec_perm(xmm0[6], xmm0[7], epi32_hi);

        for (int k = 0; k < 8; k++) {
          vec_xst(xmm1[k], (jj + k * 2) * nrows + ii, out_b);
        }

        continue;
      }

      for (int k = 0; k < 16; k++) {
        xmm0[k] = vec_xl((ii + k) * nbyte_row + jj, in_b);
      }

      for (int k = 0; k < 16; k += 8) {
        xmm1[k + 0] = vec_perm(xmm0[k + 0], xmm0[k + 1], epi8_low);
        xmm1[k + 1] = vec_perm(xmm0[k + 2], xmm0[k + 3], epi8_low);
        xmm1[k + 2] = vec_perm(xmm0[k + 4], xmm0[k + 5], epi8_low);
        xmm1[k + 3] = vec_perm(xmm0[k + 6], xmm0[k + 7], epi8_low);
        xmm1[k + 4] = vec_perm(xmm0[k + 0], xmm0[k + 1], epi8_hi);
        xmm1[k + 5] = vec_perm(xmm0[k + 2], xmm0[k + 3], epi8_hi);
        xmm1[k + 6] = vec_perm(xmm0[k + 4], xmm0[k + 5], epi8_hi);
        xmm1[k + 7] = vec_perm(xmm0[k + 6], xmm0[k + 7], epi8_hi);
      }

      for (int k = 0; k < 16; k += 8) {
        xmm0[k + 0] = vec_perm(xmm1[k + 0], xmm1[k + 1], epi16_low);
        xmm0[k + 1] = vec_perm(xmm1[k + 2], xmm1[k + 3], epi16_low);
        xmm0[k + 2] = vec_perm(xmm1[k + 0], xmm1[k + 1], epi16_hi);
        xmm0[k + 3] = vec_perm(xmm1[k + 2], xmm1[k + 3], epi16_hi);
        xmm0[k + 4] = vec_perm(xmm1[k + 4], xmm1[k + 5], epi16_low);
        xmm0[k + 5] = vec_perm(xmm1[k + 6], xmm1[k + 7], epi16_low);
        xmm0[k + 6] = vec_perm(xmm1[k + 4], xmm1[k + 5], epi16_hi);
        xmm0[k + 7] = vec_perm(xmm1[k + 6], xmm1[k + 7], epi16_hi);
      }

      for (int k = 0; k < 16; k += 8) {
        xmm1[k + 0] = vec_perm(xmm0[k + 0], xmm0[k + 1], epi32_low);
        xmm1[k + 1] = vec_perm(xmm0[k + 0], xmm0[k + 1], epi32_hi);
        xmm1[k + 2] = vec_perm(xmm0[k + 2], xmm0[k + 3], epi32_low);
        xmm1[k + 3] = vec_perm(xmm0[k + 2], xmm0[k + 3], epi32_hi);
        xmm1[k + 4] = vec_perm(xmm0[k + 4], xmm0[k + 5], epi32_low);
        xmm1[k + 5] = vec_perm(xmm0[k + 4], xmm0[k + 5], epi32_hi);
        xmm1[k + 6] = vec_perm(xmm0[k + 6], xmm0[k + 7], epi32_low);
        xmm1[k + 7] = vec_perm(xmm0[k + 6], xmm0[k + 7], epi32_hi);
      }

      for (int k = 0; k < 8; k += 4) {
        xmm0[k * 2 + 0] = vec_perm(xmm1[k + 0], xmm1[k + 8], epi64_low);
        xmm0[k * 2 + 1] = vec_perm(xmm1[k + 0], xmm1[k + 8], epi64_hi);
        xmm0[k * 2 + 2] = vec_perm(xmm1[k + 1], xmm1[k + 9], epi64_low);
        xmm0[k * 2 + 3] = vec_perm(xmm1[k + 1], xmm1[k + 9], epi64_hi);
        xmm0[k * 2 + 4] = vec_perm(xmm1[k + 2], xmm1[k + 10], epi64_low);
        xmm0[k * 2 + 5] = vec_perm(xmm1[k + 2], xmm1[k + 10], epi64_hi);
        xmm0[k * 2 + 6] = vec_perm(xmm1[k + 3], xmm1[k + 11], epi64_low);
        xmm0[k * 2 + 7] = vec_perm(xmm1[k + 3], xmm1[k + 11], epi64_hi);
      }

      for (int k = 0; k < 16; k++) {
        vec_xst(xmm0[k], (jj + k) * nrows + ii, out_b);
      }

    }

    // Copy the remainder
    for (size_t jj = nbyte_row - nbyte_row % 16; jj < nbyte_row; jj++) {
      for (int k = 0; k < nvectors; k++) {
        out_b[jj * nrows + ii + k] = in_b[(ii + k) * nbyte_row + jj];
      }
    }

  }

  return size * elem_size;
}


/* Shuffle bits within the bytes of eight element blocks. */
int64_t bshuf_shuffle_bit_eightelem_altivec(const void* in, void* out, const size_t size,
                                            const size_t elem_size) {
  /*  With a bit of care, this could be written such that such that it is */
  /*  in_buf = out_buf safe. */
  const uint8_t* in_b = (const uint8_t*)in;
  uint8_t* out_b = (uint8_t*)out;
  size_t nbyte = elem_size * size;
  __vector uint8_t masks[8], data;

  CHECK_MULT_EIGHT(size);

  // Generate all 8 needed masks
  for (int kk = 0; kk < 8; kk++){
    masks[kk] = make_bitperm_mask(1, kk);
  }

  if (elem_size % 2) {
    bshuf_shuffle_bit_eightelem_scal(in, out, size, elem_size);
  } else {
    for (size_t ii = 0; ii + 8 * elem_size - 1 < nbyte;
         ii += 8 * elem_size) {
      for (size_t jj = 0; jj + 15 < 8 * elem_size; jj += 16) {
        data = vec_xl(ii + jj, in_b);
        for (size_t kk = 0; kk < 8; kk++) {
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
int64_t bshuf_untrans_bit_elem_altivec(const void* in, void* out, const size_t size,
                                       const size_t elem_size) {

  int64_t count;

  CHECK_MULT_EIGHT(size);

  void* tmp_buf = malloc(size * elem_size);
  if (tmp_buf == NULL) return -1;

  count = bshuf_trans_byte_bitrow_altivec(in, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count = bshuf_shuffle_bit_eightelem_altivec(tmp_buf, out, size, elem_size);

  free(tmp_buf);
  return count;
}


const bool is_bshuf_altivec = true;

#else /* defined(__ALTIVEC__) && defined(__VSX__) && defined(_ARCH_PWR8) */

const bool is_bshuf_altivec = false;

int64_t
bshuf_trans_bit_elem_altivec(const void* in, void* out, const size_t size,
                             const size_t elem_size) {
  abort();
}

int64_t
bshuf_untrans_bit_elem_altivec(const void* in, void* out, const size_t size,
                               const size_t elem_size) {
  abort();
}

#endif /* defined(__ALTIVEC__) && defined(__VSX__) && defined(_ARCH_PWR8) */

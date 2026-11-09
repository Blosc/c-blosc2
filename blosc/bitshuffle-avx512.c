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

  Note: Adapted for c-blosc2 by Francesc Alted.

  See LICENSES/BITSHUFFLE.txt file for details about copyright and
  rights to use.
**********************************************************************/

#include "bitshuffle-avx512.h"
#include "bitshuffle-avx2.h"
#include "bitshuffle-sse2.h"
#include "bitshuffle-generic.h"
#include <stdlib.h>

/* Make sure AVX512 is available for the compilation target and compiler. */
#if defined(__AVX512F__) && defined (__AVX512BW__)
#include <immintrin.h>


/* Transpose bits within bytes. */
int64_t bshuf_trans_bit_byte_AVX512(const void* in, void* out, const size_t size,
                                    const size_t elem_size) {

  size_t ii, kk;
  const char* in_b = (const char*) in;
  char* out_b = (char*) out;
  size_t nbyte = elem_size * size;
  int64_t count;

  int64_t* out_i64;
  __m512i zmm;
  __mmask64 bt;
  if (nbyte >= 64) {
    const __m512i mask = _mm512_set1_epi8(0);

    for (ii = 0; ii + 63 < nbyte; ii += 64) {
      zmm = _mm512_loadu_si512((__m512i *) &in_b[ii]);
      for (kk = 0; kk < 8; kk++) {
        bt = _mm512_cmp_epi8_mask(zmm, mask, 1);
        zmm = _mm512_slli_epi16(zmm, 1);
        out_i64 = (int64_t*) &out_b[((7 - kk) * nbyte + ii) / 8];
        *out_i64 = (int64_t)bt;
      }
    }
  }

  __m256i ymm;
  int32_t bt32;
  int32_t* out_i32;
  size_t start = nbyte - nbyte % 64;
  for (ii = start; ii + 31 < nbyte; ii += 32) {
    ymm = _mm256_loadu_si256((__m256i *) &in_b[ii]);
    for (kk = 0; kk < 8; kk++) {
      bt32 = _mm256_movemask_epi8(ymm);
      ymm = _mm256_slli_epi16(ymm, 1);
      out_i32 = (int32_t*) &out_b[((7 - kk) * nbyte + ii) / 8];
      *out_i32 = bt32;
    }
  }


  count = bshuf_trans_bit_byte_remainder(in, out, size, elem_size,
                                         nbyte - nbyte % 64 % 32);

  return count;
}


/* Transpose bits within elements. */
int64_t bshuf_trans_bit_elem_AVX512(const void* in, void* out, const size_t size,
                                    const size_t elem_size, void* tmp_buf) {

  int64_t count;

  CHECK_MULT_EIGHT(size);

  count = bshuf_trans_byte_elem_SSE(in, out, size, elem_size, tmp_buf);
  CHECK_ERR(count);
  count = bshuf_trans_bit_byte_AVX512(out, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count = bshuf_trans_bitrow_eight(tmp_buf, out, size, elem_size);

  return count;

}

/* Transpose the 8x8 bit matrix in each 64-bit lane (as TRANS_BIT_8X8). */
static inline __m512i trans_bit_8x8_AVX512(__m512i x) {
  __m512i t;
  t = _mm512_and_si512(_mm512_xor_si512(x, _mm512_srli_epi64(x, 7)),
                       _mm512_set1_epi64(0x00AA00AA00AA00AALL));
  x = _mm512_xor_si512(x, _mm512_xor_si512(t, _mm512_slli_epi64(t, 7)));
  t = _mm512_and_si512(_mm512_xor_si512(x, _mm512_srli_epi64(x, 14)),
                       _mm512_set1_epi64(0x0000CCCC0000CCCCLL));
  x = _mm512_xor_si512(x, _mm512_xor_si512(t, _mm512_slli_epi64(t, 14)));
  t = _mm512_and_si512(_mm512_xor_si512(x, _mm512_srli_epi64(x, 28)),
                       _mm512_set1_epi64(0x00000000F0F0F0F0LL));
  x = _mm512_xor_si512(x, _mm512_xor_si512(t, _mm512_slli_epi64(t, 28)));
  return x;
}


/* Shuffle bits within the bytes of eight element blocks. */
int64_t bshuf_shuffle_bit_eightelem_AVX512(const void* in, void* out, const size_t size,
                                           const size_t elem_size) {

  CHECK_MULT_EIGHT(size);

  // With a bit of care, this could be written such that such that it is
  // in_buf = out_buf safe.
  const char* in_b = (const char*) in;
  char* out_b = (char*) out;

  size_t ii, jj, kk;
  size_t nbyte = elem_size * size;

  __m512i zmm;
  __mmask64 bt;

  if (elem_size <= 2) {
    // Each 8-byte word holds the 8 bit rows of 8 one-byte elements, or of
    // the low or the high bytes of 8 two-byte elements: transposing its
    // bits gives those bytes.
    for (ii = 0; ii + 63 < nbyte; ii += 64) {
      zmm = trans_bit_8x8_AVX512(_mm512_loadu_si512((__m512i *) &in_b[ii]));
      if (elem_size == 2) {
        // Each 128-bit lane is [low bytes, high bytes] for ONE group of
        // eight elements.  Both unpacks intentionally stay within that lane.
        zmm = _mm512_unpacklo_epi8(zmm, _mm512_unpackhi_epi64(zmm, zmm));
      }
      _mm512_storeu_si512((__m512i *) &out_b[ii], zmm);
    }
    if (ii < nbyte) {
      bshuf_shuffle_bit_eightelem_AVX(&in_b[ii], &out_b[ii],
                                      (nbyte - ii) / elem_size, elem_size);
    }
  } else if (elem_size % 8) {
    return bshuf_shuffle_bit_eightelem_AVX(in, out, size, elem_size);
  } else {
    const __m512i mask = _mm512_set1_epi8(0);
    for (jj = 0; jj + 63 < 8 * elem_size; jj += 64) {
      for (ii = 0; ii + 8 * elem_size - 1 < nbyte;
           ii += 8 * elem_size) {
        zmm = _mm512_loadu_si512((__m512i *) &in_b[ii + jj]);
        for (kk = 0; kk < 8; kk++) {
          bt = _mm512_cmp_epi8_mask(zmm, mask, 1);
          zmm = _mm512_slli_epi16(zmm, 1);
          size_t ind = (ii + jj / 8 + (7 - kk) * elem_size);
          * (int64_t *) &out_b[ind] = bt;
        }
      }
    }

  }
  return size * elem_size;
}

/* Untranspose bits within elements. */
int64_t bshuf_untrans_bit_elem_AVX512(const void* in, void* out, const size_t size,
                                      const size_t elem_size, void* tmp_buf) {

  int64_t count;

  CHECK_MULT_EIGHT(size);

  count = bshuf_trans_byte_bitrow_AVX(in, tmp_buf, size, elem_size);
  CHECK_ERR(count);
  count =  bshuf_shuffle_bit_eightelem_AVX512(tmp_buf, out, size, elem_size);

  return count;
}

const bool is_bshuf_AVX512 = true;

#else /* defined(__AVX512F__) && defined (__AVX512BW__) */

const bool is_bshuf_AVX512 = false;

int64_t
bshuf_trans_bit_elem_AVX512(const void* in, void* out, const size_t size,
                            const size_t elem_size, void* tmp_buf) {
  abort();
}

int64_t
bshuf_untrans_bit_elem_AVX512(const void* in, void* out, const size_t size,
                              const size_t elem_size, void* tmp_buf) {
  abort();
}

#endif /* defined(__AVX512F__) && defined (__AVX512BW__) */

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

#include "bitshuffle-neon.h"
#include "bitshuffle-generic.h"
#include "shuffle-neon.h"
#include <string.h>
#include <stdlib.h>

/* Make sure NEON is available for the compilation target and compiler. */
#if defined(__ARM_NEON)

#include <arm_neon.h>
#include "bitshuffle-constant.h"

/* Independent 8x8 bit transposes in the two 64-bit lanes.  Unlike an
 * emulated movemask this never extracts individual bits into scalar registers. */
static inline uint8x16_t trans_bit_8x8_NEON(uint8x16_t bytes) {
    uint64x2_t x = vreinterpretq_u64_u8(bytes), t;
    t = vandq_u64(veorq_u64(x, vshrq_n_u64(x, 7)), vdupq_n_u64(UINT64_C(0x00aa00aa00aa00aa)));
    x = veorq_u64(x, veorq_u64(t, vshlq_n_u64(t, 7)));
    t = vandq_u64(veorq_u64(x, vshrq_n_u64(x, 14)), vdupq_n_u64(UINT64_C(0x0000cccc0000cccc)));
    x = veorq_u64(x, veorq_u64(t, vshlq_n_u64(t, 14)));
    t = vandq_u64(veorq_u64(x, vshrq_n_u64(x, 28)), vdupq_n_u64(UINT64_C(0x00000000f0f0f0f0)));
    x = veorq_u64(x, veorq_u64(t, vshlq_n_u64(t, 28)));
    return vreinterpretq_u8_u64(x);
}


/* The next is useful for debugging purposes */
#if 0
#include <stdio.h>
#include <string.h>

static void printmem(uint8_t* buf)
{
  printf("%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\n",
          buf[0], buf[1], buf[2], buf[3],
          buf[4], buf[5], buf[6], buf[7],
          buf[8], buf[9], buf[10], buf[11],
          buf[12], buf[13], buf[14], buf[15]);
}
#endif


/* ---- Worker code that uses Arm NEON ----
 *
 * The following code makes use of the Arm NEON instruction set.
 * NEON technology is the implementation of the ARM Advanced Single
 * Instruction Multiple Data (SIMD) extension.
 * The NEON unit is the component of the processor that executes SIMD instructions.
 * It is also called the NEON Media Processing Engine (MPE).
 *
 */

/* Transpose bytes within elements for 16 bit elements. */
int64_t bshuf_trans_byte_elem_NEON_16(const void* in, void* out, const size_t size) {

    size_t ii;
    const int8_t *in_b = (const int8_t*) in;
    int8_t *out_b = (int8_t*) out;
    int8x16_t a0, b0, a1, b1;

    for (ii=0; ii + 15 < size; ii += 16) {
        a0 = vld1q_s8(in_b + 2*ii + 0*16);
        b0 = vld1q_s8(in_b + 2*ii + 1*16);

        a1 = vzip1q_s8(a0, b0);
        b1 = vzip2q_s8(a0, b0);

        a0 = vzip1q_s8(a1, b1);
        b0 = vzip2q_s8(a1, b1);

        a1 = vzip1q_s8(a0, b0);
        b1 = vzip2q_s8(a0, b0);

        a0 = vzip1q_s8(a1, b1);
        b0 = vzip2q_s8(a1, b1);

        vst1q_s8(out_b + 0*size + ii, a0);
        vst1q_s8(out_b + 1*size + ii, b0);
    }

    return bshuf_trans_byte_elem_remainder(in, out, size, 2,
            size - size % 16);
}


/* Transpose bytes within elements for 32 bit elements. */
int64_t bshuf_trans_byte_elem_NEON_32(const void* in, void* out, const size_t size) {

    size_t ii;
    const int8_t *in_b;
    int8_t *out_b;
    in_b = (const int8_t*) in;
    out_b = (int8_t*) out;
    int8x16_t a0, b0, c0, d0, a1, b1, c1, d1;
    int64x2_t a2, b2, c2, d2;

    for (ii=0; ii + 15 < size; ii += 16) {
        a0 = vld1q_s8(in_b + 4*ii + 0*16);
        b0 = vld1q_s8(in_b + 4*ii + 1*16);
        c0 = vld1q_s8(in_b + 4*ii + 2*16);
        d0 = vld1q_s8(in_b + 4*ii + 3*16);

        a1 = vzip1q_s8(a0, b0);
        b1 = vzip2q_s8(a0, b0);
        c1 = vzip1q_s8(c0, d0);
        d1 = vzip2q_s8(c0, d0);

        a0 = vzip1q_s8(a1, b1);
        b0 = vzip2q_s8(a1, b1);
        c0 = vzip1q_s8(c1, d1);
        d0 = vzip2q_s8(c1, d1);

        a1 = vzip1q_s8(a0, b0);
        b1 = vzip2q_s8(a0, b0);
        c1 = vzip1q_s8(c0, d0);
        d1 = vzip2q_s8(c0, d0);

        a2 = vzip1q_s64(vreinterpretq_s64_s8(a1), vreinterpretq_s64_s8(c1));
        b2 = vzip2q_s64(vreinterpretq_s64_s8(a1), vreinterpretq_s64_s8(c1));
        c2 = vzip1q_s64(vreinterpretq_s64_s8(b1), vreinterpretq_s64_s8(d1));
        d2 = vzip2q_s64(vreinterpretq_s64_s8(b1), vreinterpretq_s64_s8(d1));

        vst1q_s64((int64_t *) (out_b + 0*size + ii), a2);
        vst1q_s64((int64_t *) (out_b + 1*size + ii), b2);
        vst1q_s64((int64_t *) (out_b + 2*size + ii), c2);
        vst1q_s64((int64_t *) (out_b + 3*size + ii), d2);
    }

    return bshuf_trans_byte_elem_remainder(in, out, size, 4,
            size - size % 16);
}


/* Transpose bytes within elements for 64 bit elements. */
int64_t bshuf_trans_byte_elem_NEON_64(const void* in, void* out, const size_t size) {

    size_t ii;
    const int8_t* in_b = (const int8_t*) in;
    int8_t* out_b = (int8_t*) out;
    int8x16_t a0, b0, c0, d0, e0, f0, g0, h0;
    int8x16_t a1, b1, c1, d1, e1, f1, g1, h1;

    for (ii=0; ii + 15 < size; ii += 16) {
        a0 = vld1q_s8(in_b + 8*ii + 0*16);
        b0 = vld1q_s8(in_b + 8*ii + 1*16);
        c0 = vld1q_s8(in_b + 8*ii + 2*16);
        d0 = vld1q_s8(in_b + 8*ii + 3*16);
        e0 = vld1q_s8(in_b + 8*ii + 4*16);
        f0 = vld1q_s8(in_b + 8*ii + 5*16);
        g0 = vld1q_s8(in_b + 8*ii + 6*16);
        h0 = vld1q_s8(in_b + 8*ii + 7*16);

        a1 = vzip1q_s8 (a0, b0);
        b1 = vzip2q_s8 (a0, b0);
        c1 = vzip1q_s8 (c0, d0);
        d1 = vzip2q_s8 (c0, d0);
        e1 = vzip1q_s8 (e0, f0);
        f1 = vzip2q_s8 (e0, f0);
        g1 = vzip1q_s8 (g0, h0);
        h1 = vzip2q_s8 (g0, h0);

        a0 = vzip1q_s8 (a1, b1);
        b0 = vzip2q_s8 (a1, b1);
        c0 = vzip1q_s8 (c1, d1);
        d0 = vzip2q_s8 (c1, d1);
        e0 = vzip1q_s8 (e1, f1);
        f0 = vzip2q_s8 (e1, f1);
        g0 = vzip1q_s8 (g1, h1);
        h0 = vzip2q_s8 (g1, h1);

        a1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (a0), vreinterpretq_s32_s8 (c0));
        b1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (a0), vreinterpretq_s32_s8 (c0));
        c1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (b0), vreinterpretq_s32_s8 (d0));
        d1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (b0), vreinterpretq_s32_s8 (d0));
        e1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (e0), vreinterpretq_s32_s8 (g0));
        f1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (e0), vreinterpretq_s32_s8 (g0));
        g1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (f0), vreinterpretq_s32_s8 (h0));
        h1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (f0), vreinterpretq_s32_s8 (h0));

        a0 = (int8x16_t) vzip1q_s64 (vreinterpretq_s64_s8 (a1), vreinterpretq_s64_s8 (e1));
        b0 = (int8x16_t) vzip2q_s64 (vreinterpretq_s64_s8 (a1), vreinterpretq_s64_s8 (e1));
        c0 = (int8x16_t) vzip1q_s64 (vreinterpretq_s64_s8 (b1), vreinterpretq_s64_s8 (f1));
        d0 = (int8x16_t) vzip2q_s64 (vreinterpretq_s64_s8 (b1), vreinterpretq_s64_s8 (f1));
        e0 = (int8x16_t) vzip1q_s64 (vreinterpretq_s64_s8 (c1), vreinterpretq_s64_s8 (g1));
        f0 = (int8x16_t) vzip2q_s64 (vreinterpretq_s64_s8 (c1), vreinterpretq_s64_s8 (g1));
        g0 = (int8x16_t) vzip1q_s64 (vreinterpretq_s64_s8 (d1), vreinterpretq_s64_s8 (h1));
        h0 = (int8x16_t) vzip2q_s64 (vreinterpretq_s64_s8 (d1), vreinterpretq_s64_s8 (h1));

        vst1q_s8(out_b + 0*size + ii, a0);
        vst1q_s8(out_b + 1*size + ii, b0);
        vst1q_s8(out_b + 2*size + ii, c0);
        vst1q_s8(out_b + 3*size + ii, d0);
        vst1q_s8(out_b + 4*size + ii, e0);
        vst1q_s8(out_b + 5*size + ii, f0);
        vst1q_s8(out_b + 6*size + ii, g0);
        vst1q_s8(out_b + 7*size + ii, h0);
    }

    return bshuf_trans_byte_elem_remainder(in, out, size, 8,
            size - size % 16);
}


/* Transpose bytes within elements using best NEON algorithm available. */
int64_t bshuf_trans_byte_elem_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size, void* tmp_buf) {

    int64_t count;

    // Trivial cases: power of 2 bytes.
    switch (elem_size) {
        case 1:
            count = bshuf_copy(in, out, size, elem_size);
            return count;
        case 2:
            count = bshuf_trans_byte_elem_NEON_16(in, out, size);
            return count;
        case 4:
            count = bshuf_trans_byte_elem_NEON_32(in, out, size);
            return count;
        case 8:
            count = bshuf_trans_byte_elem_NEON_64(in, out, size);
            return count;
    }

    // Worst case: odd number of bytes. Turns out that this is faster for
    // (odd * 2) byte elements as well (hence % 4).
    if (elem_size % 4) {
        count = bshuf_trans_byte_elem_scal(in, out, size, elem_size);
        return count;
    }

    // Multiple of power of 2: transpose hierarchically.
    {
        size_t nchunk_elem;

        if ((elem_size % 8) == 0) {
            nchunk_elem = elem_size / 8;
            TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int64_t);
            count = bshuf_trans_byte_elem_NEON_64(out, tmp_buf,
                    size * nchunk_elem);
            bshuf_trans_elem(tmp_buf, out, 8, nchunk_elem, size);
        } else if ((elem_size % 4) == 0) {
            nchunk_elem = elem_size / 4;
            TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int32_t);
            count = bshuf_trans_byte_elem_NEON_32(out, tmp_buf,
                    size * nchunk_elem);
            bshuf_trans_elem(tmp_buf, out, 4, nchunk_elem, size);
        } else {
            // Not used since scalar algorithm is faster.
            nchunk_elem = elem_size / 2;
            TRANS_ELEM_TYPE(in, out, size, nchunk_elem, int16_t);
            count = bshuf_trans_byte_elem_NEON_16(out, tmp_buf,
                    size * nchunk_elem);
            bshuf_trans_elem(tmp_buf, out, 2, nchunk_elem, size);
        }

        return count;
    }
}


/* Creates a mask made up of the most significant
 * bit of each byte of 'input'
 */
int32_t move_byte_mask_neon(uint8x16_t input) {

    return (  ((input[0] & 0x80) >> 7)          | (((input[1] & 0x80) >> 7) << 1)   | (((input[2] & 0x80) >> 7) << 2)   | (((input[3] & 0x80) >> 7) << 3)
            | (((input[4] & 0x80) >> 7) << 4)   | (((input[5] & 0x80) >> 7) << 5)   | (((input[6] & 0x80) >> 7) << 6)   | (((input[7] & 0x80) >> 7) << 7)
            | (((input[8] & 0x80) >> 7) << 8)   | (((input[9] & 0x80) >> 7) << 9)   | (((input[10] & 0x80) >> 7) << 10) | (((input[11] & 0x80) >> 7) << 11)
            | (((input[12] & 0x80) >> 7) << 12) | (((input[13] & 0x80) >> 7) << 13) | (((input[14] & 0x80) >> 7) << 14) | (((input[15] & 0x80) >> 7) << 15)
           );
}

/* Transpose bits within bytes. */
int64_t bshuf_trans_bit_byte_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size) {

    size_t ii;
    const int8_t* in_b = (const int8_t*) in;
    int8_t* out_b = (int8_t*) out;

    int64_t count;

    size_t nbyte = elem_size * size;

    CHECK_MULT_EIGHT(nbyte);

    ii = bshuf_emit_constant_prefix(in, out, nbyte);
    if (ii == nbyte) return nbyte;

    /* An entire 128-byte tile lets us replace 64 short scalar stores with
     * eight vector stores.  First transpose bits in 64-bit lanes, then
     * transpose the resulting eight rows of 16-bit bit pairs. */
    for (; ii + 127 < nbyte; ii += 128) {
        uint8x16_t x[8], y[8];
        for (size_t j = 0; j < 8; ++j) {
            uint8x16_t v = trans_bit_8x8_NEON(vld1q_u8((const uint8_t*)in_b + ii + 16 * j));
            x[j] = vzip1q_u8(v, vextq_u8(v, v, 8));
        }
        for (size_t j = 0; j < 4; ++j) {
            y[2*j] = vreinterpretq_u8_u16(vzip1q_u16(vreinterpretq_u16_u8(x[2*j]),
                                                       vreinterpretq_u16_u8(x[2*j+1])));
            y[2*j+1] = vreinterpretq_u8_u16(vzip2q_u16(vreinterpretq_u16_u8(x[2*j]),
                                                         vreinterpretq_u16_u8(x[2*j+1])));
        }
        for (size_t j = 0; j < 2; ++j) {
            for (size_t k = 0; k < 2; ++k) {
                x[4*j+2*k] = vreinterpretq_u8_u32(vzip1q_u32(vreinterpretq_u32_u8(y[4*j+k]),
                                                               vreinterpretq_u32_u8(y[4*j+k+2])));
                x[4*j+2*k+1] = vreinterpretq_u8_u32(vzip2q_u32(vreinterpretq_u32_u8(y[4*j+k]),
                                                                 vreinterpretq_u32_u8(y[4*j+k+2])));
            }
        }
        for (size_t j = 0; j < 4; ++j) {
            uint64x2_t a = vreinterpretq_u64_u8(x[j]), b = vreinterpretq_u64_u8(x[j+4]);
            vst1q_u8((uint8_t*)out_b + (2*j*nbyte + ii) / 8,
                     vreinterpretq_u8_u64(vzip1q_u64(a, b)));
            vst1q_u8((uint8_t*)out_b + ((2*j+1)*nbyte + ii) / 8,
                     vreinterpretq_u8_u64(vzip2q_u64(a, b)));
        }
    }
    for (; ii + 15 < nbyte; ii += 16) {
        uint8x16_t x = trans_bit_8x8_NEON(vld1q_u8((const uint8_t*)in_b + ii));
        x = vzip1q_u8(x, vextq_u8(x, x, 8));
#define STORE_BIT_ROW(k) do { \
        uint16_t bits = vgetq_lane_u16(vreinterpretq_u16_u8(x), k); \
        memcpy(out_b + ((k) * nbyte + ii) / 8, &bits, sizeof(bits)); \
    } while (0)
        STORE_BIT_ROW(0); STORE_BIT_ROW(1); STORE_BIT_ROW(2); STORE_BIT_ROW(3);
        STORE_BIT_ROW(4); STORE_BIT_ROW(5); STORE_BIT_ROW(6); STORE_BIT_ROW(7);
#undef STORE_BIT_ROW
    }
    count = bshuf_trans_bit_byte_remainder(in, out, size, elem_size,
            nbyte - nbyte % 16);
    return count;
}


/* Transpose bits within elements. */
int64_t bshuf_trans_bit_elem_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size, void* tmp_buf) {

    int64_t count;

    CHECK_MULT_EIGHT(size);

    if ((elem_size == 1 || elem_size == 2 || elem_size == 4 || elem_size == 8 || elem_size == 16)
        && size <= INT32_MAX / elem_size) {
        const uint8_t* bytes = (const uint8_t*)in;
        if (elem_size != 1) {
            /* Write the final bit rows directly, avoiding the bitrow reorder pass. */
            shuffle_neon((int32_t)elem_size, (int32_t)(size * elem_size), in, tmp_buf);
            bytes = (const uint8_t*)tmp_buf;
        }
        for (size_t j = 0; j < elem_size; ++j) {
            count = bshuf_trans_bit_byte_NEON(bytes + j * size,
                     (uint8_t*)out + j * size, size, 1);
            CHECK_ERR(count);
        }
        return size * elem_size;
    }
    return bshuf_trans_bit_elem_scal(in, out, size, elem_size, tmp_buf);
}


/* For data organized into a row for each bit (8 * elem_size rows), transpose
 * the bytes.  transpose_bits is only used with elem_size == 1 and fuses the
 * final 8x8 bit transpose into each vector store. */
static int64_t trans_byte_bitrow_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size, const bool transpose_bits) {

    size_t ii, jj;
    const int8_t* in_b = (const int8_t*) in;
    int8_t* out_b = (int8_t*) out;

    CHECK_MULT_EIGHT(size);

    size_t nrows = 8 * elem_size;
    size_t nbyte_row = size / 8;

    int8x16_t a0, b0, c0, d0, e0, f0, g0, h0;
    int8x16_t a1, b1, c1, d1, e1, f1, g1, h1;
    int64x1_t *as, *bs, *cs, *ds, *es, *fs, *gs, *hs;

    for (ii = 0; ii + 7 < nrows; ii += 8) {
        for (jj = 0; jj + 15 < nbyte_row; jj += 16) {
            a0 = vld1q_s8(in_b + (ii + 0)*nbyte_row + jj);
            b0 = vld1q_s8(in_b + (ii + 1)*nbyte_row + jj);
            c0 = vld1q_s8(in_b + (ii + 2)*nbyte_row + jj);
            d0 = vld1q_s8(in_b + (ii + 3)*nbyte_row + jj);
            e0 = vld1q_s8(in_b + (ii + 4)*nbyte_row + jj);
            f0 = vld1q_s8(in_b + (ii + 5)*nbyte_row + jj);
            g0 = vld1q_s8(in_b + (ii + 6)*nbyte_row + jj);
            h0 = vld1q_s8(in_b + (ii + 7)*nbyte_row + jj);

            a1 = vzip1q_s8(a0, b0);
            b1 = vzip1q_s8(c0, d0);
            c1 = vzip1q_s8(e0, f0);
            d1 = vzip1q_s8(g0, h0);
            e1 = vzip2q_s8(a0, b0);
            f1 = vzip2q_s8(c0, d0);
            g1 = vzip2q_s8(e0, f0);
            h1 = vzip2q_s8(g0, h0);

            a0 = (int8x16_t) vzip1q_s16 (vreinterpretq_s16_s8 (a1), vreinterpretq_s16_s8 (b1));
            b0=  (int8x16_t) vzip1q_s16 (vreinterpretq_s16_s8 (c1), vreinterpretq_s16_s8 (d1));
            c0 = (int8x16_t) vzip2q_s16 (vreinterpretq_s16_s8 (a1), vreinterpretq_s16_s8 (b1));
            d0 = (int8x16_t) vzip2q_s16 (vreinterpretq_s16_s8 (c1), vreinterpretq_s16_s8 (d1));
            e0 = (int8x16_t) vzip1q_s16 (vreinterpretq_s16_s8 (e1), vreinterpretq_s16_s8 (f1));
            f0 = (int8x16_t) vzip1q_s16 (vreinterpretq_s16_s8 (g1), vreinterpretq_s16_s8 (h1));
            g0 = (int8x16_t) vzip2q_s16 (vreinterpretq_s16_s8 (e1), vreinterpretq_s16_s8 (f1));
            h0 = (int8x16_t) vzip2q_s16 (vreinterpretq_s16_s8 (g1), vreinterpretq_s16_s8 (h1));

            a1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (a0), vreinterpretq_s32_s8 (b0));
            b1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (a0), vreinterpretq_s32_s8 (b0));
            c1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (c0), vreinterpretq_s32_s8 (d0));
            d1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (c0), vreinterpretq_s32_s8 (d0));
            e1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (e0), vreinterpretq_s32_s8 (f0));
            f1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (e0), vreinterpretq_s32_s8 (f0));
            g1 = (int8x16_t) vzip1q_s32 (vreinterpretq_s32_s8 (g0), vreinterpretq_s32_s8 (h0));
            h1 = (int8x16_t) vzip2q_s32 (vreinterpretq_s32_s8 (g0), vreinterpretq_s32_s8 (h0));

            if (transpose_bits) {
#define STORE_BYTES(v, k) vst1q_u8((uint8_t*)out + (jj + (k)) * 8, \
                                      trans_bit_8x8_NEON(vreinterpretq_u8_s8(v)))
                STORE_BYTES(a1, 0); STORE_BYTES(b1, 2);
                STORE_BYTES(c1, 4); STORE_BYTES(d1, 6);
                STORE_BYTES(e1, 8); STORE_BYTES(f1, 10);
                STORE_BYTES(g1, 12); STORE_BYTES(h1, 14);
#undef STORE_BYTES
                continue;
            }
            as = (int64x1_t *) &a1;
            bs = (int64x1_t *) &b1;
            cs = (int64x1_t *) &c1;
            ds = (int64x1_t *) &d1;
            es = (int64x1_t *) &e1;
            fs = (int64x1_t *) &f1;
            gs = (int64x1_t *) &g1;
            hs = (int64x1_t *) &h1;

            vst1_s64((int64_t *)(out_b + (jj + 0) * nrows + ii), *as);
            vst1_s64((int64_t *)(out_b + (jj + 1) * nrows + ii), *(as + 1));
            vst1_s64((int64_t *)(out_b + (jj + 2) * nrows + ii), *bs);
            vst1_s64((int64_t *)(out_b + (jj + 3) * nrows + ii), *(bs + 1));
            vst1_s64((int64_t *)(out_b + (jj + 4) * nrows + ii), *cs);
            vst1_s64((int64_t *)(out_b + (jj + 5) * nrows + ii), *(cs + 1));
            vst1_s64((int64_t *)(out_b + (jj + 6) * nrows + ii), *ds);
            vst1_s64((int64_t *)(out_b + (jj + 7) * nrows + ii), *(ds + 1));
            vst1_s64((int64_t *)(out_b + (jj + 8) * nrows + ii), *es);
            vst1_s64((int64_t *)(out_b + (jj + 9) * nrows + ii), *(es + 1));
            vst1_s64((int64_t *)(out_b + (jj + 10) * nrows + ii), *fs);
            vst1_s64((int64_t *)(out_b + (jj + 11) * nrows + ii), *(fs + 1));
            vst1_s64((int64_t *)(out_b + (jj + 12) * nrows + ii), *gs);
            vst1_s64((int64_t *)(out_b + (jj + 13) * nrows + ii), *(gs + 1));
            vst1_s64((int64_t *)(out_b + (jj + 14) * nrows + ii), *hs);
            vst1_s64((int64_t *)(out_b + (jj + 15) * nrows + ii), *(hs + 1));
        }
        for (jj = nbyte_row - nbyte_row % 16; jj < nbyte_row; jj ++) {
            out_b[jj * nrows + ii + 0] = in_b[(ii + 0)*nbyte_row + jj];
            out_b[jj * nrows + ii + 1] = in_b[(ii + 1)*nbyte_row + jj];
            out_b[jj * nrows + ii + 2] = in_b[(ii + 2)*nbyte_row + jj];
            out_b[jj * nrows + ii + 3] = in_b[(ii + 3)*nbyte_row + jj];
            out_b[jj * nrows + ii + 4] = in_b[(ii + 4)*nbyte_row + jj];
            out_b[jj * nrows + ii + 5] = in_b[(ii + 5)*nbyte_row + jj];
            out_b[jj * nrows + ii + 6] = in_b[(ii + 6)*nbyte_row + jj];
            out_b[jj * nrows + ii + 7] = in_b[(ii + 7)*nbyte_row + jj];
            if (transpose_bits) {
                uint64_t x, t;
                memcpy(&x, out_b + jj * 8, 8);
                TRANS_BIT_8X8(x, t);
                memcpy(out_b + jj * 8, &x, 8);
            }
        }
    }
    return size * elem_size;
}

int64_t bshuf_trans_byte_bitrow_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size) {
    return trans_byte_bitrow_NEON(in, out, size, elem_size, false);
}

/* Shuffle bits within the bytes of eight element blocks. */
int64_t bshuf_shuffle_bit_eightelem_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size) {

    CHECK_MULT_EIGHT(size);

    // With a bit of care, this could be written such that such that it is
    // in_buf = out_buf safe.
    const int8_t* in_b = (const int8_t*) in;
    uint16_t* out_ui16 = (uint16_t*) out;

    size_t ii, jj, kk;
    size_t nbyte = elem_size * size;

    int16x8_t xmm;
    int32_t bt;

    if (elem_size % 2) {
        bshuf_shuffle_bit_eightelem_scal(in, out, size, elem_size);
    } else {
        for (ii = 0; ii + 8 * elem_size - 1 < nbyte;
                ii += 8 * elem_size) {
            for (jj = 0; jj + 15 < 8 * elem_size; jj += 16) {
                xmm = vld1q_s16((int16_t *) &in_b[ii + jj]);
                for (kk = 0; kk < 8; kk++) {
                    bt = move_byte_mask_neon((uint8x16_t) xmm);
                    xmm = vshlq_n_s16(xmm, 1);
                    size_t ind = (ii + jj / 8 + (7 - kk) * elem_size);
                    out_ui16[ind / 2] = bt;
                }
            }
        }
    }
    return size * elem_size;
}


/* Untranspose bits within elements. */
int64_t bshuf_untrans_bit_elem_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size, void* tmp_buf) {

    CHECK_MULT_EIGHT(size);

    if (elem_size == 1) {
        return trans_byte_bitrow_NEON(in, out, size, 1, true);
    }
    if ((elem_size == 2 || elem_size == 4 || elem_size == 8 || elem_size == 16)
        && size <= INT32_MAX / elem_size) {
        for (size_t j = 0; j < elem_size; ++j) {
            /* Fuse bitrow gathering and bit transpose into one byte plane. */
            trans_byte_bitrow_NEON((const uint8_t*)in + j * size,
                                   (uint8_t*)tmp_buf + j * size, size, 1, true);
        }
        unshuffle_neon((int32_t)elem_size, (int32_t)(size * elem_size), tmp_buf, out);
        return size * elem_size;
    }
    return bshuf_untrans_bit_elem_scal(in, out, size, elem_size, tmp_buf);
}

const bool is_bshuf_NEON = true;

#else /* defined(__ARM_NEON) */

const bool is_bshuf_NEON = false;

int64_t bshuf_trans_bit_elem_NEON(const void* in, void* out, const size_t size,
                                  const size_t elem_size, void* tmp_buf) {
  abort();
}

int64_t bshuf_untrans_bit_elem_NEON(const void* in, void* out, const size_t size,
                                    const size_t elem_size, void* tmp_buf) {
  abort();
}

#endif /* defined(__ARM_NEON) */

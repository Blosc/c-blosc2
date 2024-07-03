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
#include <stdlib.h>

/* Make sure NEON is available for the compilation target and compiler. */
#if defined(__ARM_NEON)

#include <arm_neon.h>


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
    const char *in_b = (const char*) in;
    char *out_b = (char*) out;
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
    const char *in_b;
    char *out_b;
    in_b = (const char*) in;
    out_b = (char*) out;
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
    const char* in_b = (const char*) in;
    char* out_b = (char*) out;
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
         const size_t elem_size) {

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
        void* tmp_buf = malloc(size * elem_size);
        if (tmp_buf == NULL) return -1;

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

        free(tmp_buf);
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

    size_t ii, kk;
    const char* in_b = (const char*) in;
    char* out_b = (char*) out;
    uint16_t* out_ui16;

    int64_t count;

    size_t nbyte = elem_size * size;

    CHECK_MULT_EIGHT(nbyte);

    int16x8_t xmm;
    int32_t bt;

    for (ii = 0; ii + 15 < nbyte; ii += 16) {
        xmm = vld1q_s16((int16_t *) (in_b + ii));
        for (kk = 0; kk < 8; kk++) {
            bt = move_byte_mask_neon((uint8x16_t) xmm);
            xmm = vshlq_n_s16(xmm, 1);
            out_ui16 = (uint16_t*) &out_b[((7 - kk) * nbyte + ii) / 8];
            *out_ui16 = bt;
        }
    }
    count = bshuf_trans_bit_byte_remainder(in, out, size, elem_size,
            nbyte - nbyte % 16);
    return count;
}


/* Transpose bits within elements. */
int64_t bshuf_trans_bit_elem_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size) {

    int64_t count;

    CHECK_MULT_EIGHT(size);

    void* tmp_buf = malloc(size * elem_size);
    if (tmp_buf == NULL) return -1;

    count = bshuf_trans_byte_elem_NEON(in, out, size, elem_size);
    CHECK_ERR_FREE(count, tmp_buf);
    count = bshuf_trans_bit_byte_NEON(out, tmp_buf, size, elem_size);
    CHECK_ERR_FREE(count, tmp_buf);
    count = bshuf_trans_bitrow_eight(tmp_buf, out, size, elem_size);

    free(tmp_buf);

    return count;
}


/* For data organized into a row for each bit (8 * elem_size rows), transpose
 * the bytes. */
int64_t bshuf_trans_byte_bitrow_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size) {

    size_t ii, jj;
    const char* in_b = (const char*) in;
    char* out_b = (char*) out;

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
        }
    }
    return size * elem_size;
}


/* Shuffle bits within the bytes of eight element blocks. */
int64_t bshuf_shuffle_bit_eightelem_NEON(const void* in, void* out, const size_t size,
         const size_t elem_size) {

    CHECK_MULT_EIGHT(size);

    // With a bit of care, this could be written such that such that it is
    // in_buf = out_buf safe.
    const char* in_b = (const char*) in;
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
         const size_t elem_size) {

    int64_t count;

    CHECK_MULT_EIGHT(size);

    void* tmp_buf = malloc(size * elem_size);
    if (tmp_buf == NULL) return -1;

    count = bshuf_trans_byte_bitrow_NEON(in, tmp_buf, size, elem_size);
    CHECK_ERR_FREE(count, tmp_buf);
    count =  bshuf_shuffle_bit_eightelem_NEON(tmp_buf, out, size, elem_size);

    free(tmp_buf);

    return count;
}

const bool is_bshuf_NEON = true;

#else /* defined(__ARM_NEON) */

const bool is_bshuf_NEON = false;

int64_t bshuf_trans_bit_elem_NEON(const void* in, void* out, const size_t size,
                                  const size_t elem_size) {
  abort();
}

int64_t bshuf_untrans_bit_elem_NEON(const void* in, void* out, const size_t size,
                                    const size_t elem_size) {
  abort();
}

#endif /* defined(__ARM_NEON) */

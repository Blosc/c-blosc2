/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Lucian Marc <ruben.lucian@gmail.com>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "shuffle-neon.h"
#include "shuffle-generic.h"
#include <stdlib.h>

/* Make sure NEON is available for the compilation target and compiler. */
#if defined(__ARM_NEON)

#include <arm_neon.h>

#include <stdint.h>

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


/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_neon(uint8_t *const dest, const uint8_t *const src,
              const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 2;
    const size_t total_bytes = vectorizable_elements * bytesoftype;

    for (i = 0, k = 0; i + 128 <= total_bytes; i += 128, k += 4) {
        /* Load and deinterleave 128 bytes (4x unrolled for 64-byte store streams) */
        uint8x16x2_t r0 = vuzpq_u8(vld1q_u8(src + i + 0), vld1q_u8(src + i + 16));
        uint8x16x2_t r1 = vuzpq_u8(vld1q_u8(src + i + 32), vld1q_u8(src + i + 48));
        uint8x16x2_t r2 = vuzpq_u8(vld1q_u8(src + i + 64), vld1q_u8(src + i + 80));
        uint8x16x2_t r3 = vuzpq_u8(vld1q_u8(src + i + 96), vld1q_u8(src + i + 112));
        vst1q_u8(dest + total_elements * 0 + k * 16 + 0, r0.val[0]);
        vst1q_u8(dest + total_elements * 0 + k * 16 + 16, r1.val[0]);
        vst1q_u8(dest + total_elements * 0 + k * 16 + 32, r2.val[0]);
        vst1q_u8(dest + total_elements * 0 + k * 16 + 48, r3.val[0]);
        vst1q_u8(dest + total_elements * 1 + k * 16 + 0, r0.val[1]);
        vst1q_u8(dest + total_elements * 1 + k * 16 + 16, r1.val[1]);
        vst1q_u8(dest + total_elements * 1 + k * 16 + 32, r2.val[1]);
        vst1q_u8(dest + total_elements * 1 + k * 16 + 48, r3.val[1]);
    }
    for (; i < total_bytes; i += 32, k++) {
        uint8x16x2_t r0 = vuzpq_u8(vld1q_u8(src + i), vld1q_u8(src + i + 16));
        vst1q_u8(dest + total_elements * 0 + k * 16, r0.val[0]);
        vst1q_u8(dest + total_elements * 1 + k * 16, r0.val[1]);
    }
}

/* Routine optimized for shuffling a buffer for a type size of 4 bytes. */
static void
shuffle4_neon(uint8_t *const dest, const uint8_t *const src,
              const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 4;
    uint8x16x4_t r0;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 64, k++) {
        /* Load (and permute) 64 bytes to the structure r0 */
        r0 = vld4q_u8(src + i);
        /* Store the results in the destination vector */
        vst1q_u8(dest + total_elements * 0 + k * 16, r0.val[0]);
        vst1q_u8(dest + total_elements * 1 + k * 16, r0.val[1]);
        vst1q_u8(dest + total_elements * 2 + k * 16, r0.val[2]);
        vst1q_u8(dest + total_elements * 3 + k * 16, r0.val[3]);
    }
}

static inline void
shuffle8_block_neon(uint8_t *const d, const size_t stride,
                    const uint8x16_t a0, const uint8x16_t a1,
                    const uint8x16_t a2, const uint8x16_t a3,
                    const uint8x16_t a4, const uint8x16_t a5,
                    const uint8x16_t a6, const uint8x16_t a7) {
    /* Interleave bytes twice */
    uint8x16x2_t b0 = vzipq_u8(a0, a1);
    uint8x16x2_t b1 = vzipq_u8(a2, a3);
    uint8x16x2_t b2 = vzipq_u8(a4, a5);
    uint8x16x2_t b3 = vzipq_u8(a6, a7);
    uint8x16x2_t c0 = vzipq_u8(b0.val[0], b0.val[1]);
    uint8x16x2_t c1 = vzipq_u8(b1.val[0], b1.val[1]);
    uint8x16x2_t c2 = vzipq_u8(b2.val[0], b2.val[1]);
    uint8x16x2_t c3 = vzipq_u8(b3.val[0], b3.val[1]);
    /* Interleave 32-bit groups */
    uint32x4x2_t d0 = vzipq_u32(vreinterpretq_u32_u8(c0.val[0]), vreinterpretq_u32_u8(c1.val[0]));
    uint32x4x2_t d1 = vzipq_u32(vreinterpretq_u32_u8(c0.val[1]), vreinterpretq_u32_u8(c1.val[1]));
    uint32x4x2_t d2 = vzipq_u32(vreinterpretq_u32_u8(c2.val[0]), vreinterpretq_u32_u8(c3.val[0]));
    uint32x4x2_t d3 = vzipq_u32(vreinterpretq_u32_u8(c2.val[1]), vreinterpretq_u32_u8(c3.val[1]));
    /* Combine 64-bit halves and store 8 streams of 16 bytes */
    uint64x2_t x0 = vreinterpretq_u64_u32(d0.val[0]), y0 = vreinterpretq_u64_u32(d2.val[0]);
    uint64x2_t x1 = vreinterpretq_u64_u32(d0.val[1]), y1 = vreinterpretq_u64_u32(d2.val[1]);
    uint64x2_t x2 = vreinterpretq_u64_u32(d1.val[0]), y2 = vreinterpretq_u64_u32(d3.val[0]);
    uint64x2_t x3 = vreinterpretq_u64_u32(d1.val[1]), y3 = vreinterpretq_u64_u32(d3.val[1]);
    vst1q_u8(d + 0 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_low_u64(x0), vget_low_u64(y0))));
    vst1q_u8(d + 1 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_high_u64(x0), vget_high_u64(y0))));
    vst1q_u8(d + 2 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_low_u64(x1), vget_low_u64(y1))));
    vst1q_u8(d + 3 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_high_u64(x1), vget_high_u64(y1))));
    vst1q_u8(d + 4 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_low_u64(x2), vget_low_u64(y2))));
    vst1q_u8(d + 5 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_high_u64(x2), vget_high_u64(y2))));
    vst1q_u8(d + 6 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_low_u64(x3), vget_low_u64(y3))));
    vst1q_u8(d + 7 * stride, vreinterpretq_u8_u64(vcombine_u64(vget_high_u64(x3), vget_high_u64(y3))));
}

/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
static void
shuffle8_neon(uint8_t *const dest, const uint8_t *const src,
              const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 8;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 128, k++) {
        shuffle8_block_neon(dest + k * 16, total_elements,
            vld1q_u8(src + i + 0 * 16), vld1q_u8(src + i + 1 * 16),
            vld1q_u8(src + i + 2 * 16), vld1q_u8(src + i + 3 * 16),
            vld1q_u8(src + i + 4 * 16), vld1q_u8(src + i + 5 * 16),
            vld1q_u8(src + i + 6 * 16), vld1q_u8(src + i + 7 * 16));
    }
}

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
static void
shuffle16_neon(uint8_t *const dest, const uint8_t *const src,
               const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k, half;
    static const size_t bytesoftype = 16;

    for (half = 0; half < 2; half++) {
        for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 256, k++) {
            const uint8_t *s = src + i + half * 8;
            /* Process 8 active streams at a time to avoid L1D set thrashing */
            shuffle8_block_neon(dest + k * 16 + half * 8 * total_elements, total_elements,
                vcombine_u8(vld1_u8(s + 0 * 16), vld1_u8(s + 1 * 16)),
                vcombine_u8(vld1_u8(s + 2 * 16), vld1_u8(s + 3 * 16)),
                vcombine_u8(vld1_u8(s + 4 * 16), vld1_u8(s + 5 * 16)),
                vcombine_u8(vld1_u8(s + 6 * 16), vld1_u8(s + 7 * 16)),
                vcombine_u8(vld1_u8(s + 8 * 16), vld1_u8(s + 9 * 16)),
                vcombine_u8(vld1_u8(s + 10 * 16), vld1_u8(s + 11 * 16)),
                vcombine_u8(vld1_u8(s + 12 * 16), vld1_u8(s + 13 * 16)),
                vcombine_u8(vld1_u8(s + 14 * 16), vld1_u8(s + 15 * 16)));
        }
    }
}

/* Routine optimized for unshuffling a buffer for a type size of 2 bytes. */
static void
unshuffle2_neon(uint8_t *const dest, const uint8_t *const src,
                const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 2;
    uint8x16x2_t r0;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 32, k++) {
        /* Load 32 bytes to the structure r0 */
        r0.val[0] = vld1q_u8(src + total_elements * 0 + k * 16);
        r0.val[1] = vld1q_u8(src + total_elements * 1 + k * 16);
        /* Store (with permutation) the results in the destination vector */
        vst2q_u8(dest + k * 32, r0);
    }
}

/* Routine optimized for unshuffling a buffer for a type size of 4 bytes. */
static void
unshuffle4_neon(uint8_t *const dest, const uint8_t *const src,
                const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 4;
    uint8x16x4_t r0;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 64, k++) {
        /* load 64 bytes to the structure r0 */
        r0.val[0] = vld1q_u8(src + total_elements * 0 + k * 16);
        r0.val[1] = vld1q_u8(src + total_elements * 1 + k * 16);
        r0.val[2] = vld1q_u8(src + total_elements * 2 + k * 16);
        r0.val[3] = vld1q_u8(src + total_elements * 3 + k * 16);
        /* Store (with permutation) the results in the destination vector */
        vst4q_u8(dest + k * 64, r0);
    }
}

static inline void
unshuffle8_block_neon(const uint8_t *const s, const size_t stride, uint8x16_t out[8]) {
    /* Load and interleave groups of 16 bytes across 8 streams */
    uint8x16x2_t r00 = vzipq_u8(vld1q_u8(s + 0 * stride), vld1q_u8(s + 1 * stride));
    uint8x16x2_t r01 = vzipq_u8(vld1q_u8(s + 2 * stride), vld1q_u8(s + 3 * stride));
    uint8x16x2_t r02 = vzipq_u8(vld1q_u8(s + 4 * stride), vld1q_u8(s + 5 * stride));
    uint8x16x2_t r03 = vzipq_u8(vld1q_u8(s + 6 * stride), vld1q_u8(s + 7 * stride));
    /* Interleave 16-bit pairs */
    uint16x8x2_t r10 = vzipq_u16(vreinterpretq_u16_u8(r00.val[0]), vreinterpretq_u16_u8(r01.val[0]));
    uint16x8x2_t r11 = vzipq_u16(vreinterpretq_u16_u8(r02.val[0]), vreinterpretq_u16_u8(r03.val[0]));
    uint16x8x2_t r12 = vzipq_u16(vreinterpretq_u16_u8(r00.val[1]), vreinterpretq_u16_u8(r01.val[1]));
    uint16x8x2_t r13 = vzipq_u16(vreinterpretq_u16_u8(r02.val[1]), vreinterpretq_u16_u8(r03.val[1]));
    /* Interleave 32-bit quads */
    uint32x4x2_t r20 = vzipq_u32(vreinterpretq_u32_u16(r10.val[0]), vreinterpretq_u32_u16(r11.val[0]));
    uint32x4x2_t r21 = vzipq_u32(vreinterpretq_u32_u16(r10.val[1]), vreinterpretq_u32_u16(r11.val[1]));
    uint32x4x2_t r22 = vzipq_u32(vreinterpretq_u32_u16(r12.val[0]), vreinterpretq_u32_u16(r13.val[0]));
    uint32x4x2_t r23 = vzipq_u32(vreinterpretq_u32_u16(r12.val[1]), vreinterpretq_u32_u16(r13.val[1]));
    out[0] = vreinterpretq_u8_u32(r20.val[0]);
    out[1] = vreinterpretq_u8_u32(r20.val[1]);
    out[2] = vreinterpretq_u8_u32(r21.val[0]);
    out[3] = vreinterpretq_u8_u32(r21.val[1]);
    out[4] = vreinterpretq_u8_u32(r22.val[0]);
    out[5] = vreinterpretq_u8_u32(r22.val[1]);
    out[6] = vreinterpretq_u8_u32(r23.val[0]);
    out[7] = vreinterpretq_u8_u32(r23.val[1]);
}

/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
static void
unshuffle8_neon(uint8_t *const dest, const uint8_t *const src,
                const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 8;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 128, k++) {
        uint8x16_t out[8];
        unshuffle8_block_neon(src + k * 16, total_elements, out);
#if defined(__aarch64__)
        /* Keep full-width contiguous stores: Clang can otherwise fold ZIP and
           individual stores into partial-width ST2, slowing streaming output. */
        uint8x16x4_t first = {{out[0], out[1], out[2], out[3]}};
        uint8x16x4_t second = {{out[4], out[5], out[6], out[7]}};
        vst1q_u8_x4(dest + i, first);
        vst1q_u8_x4(dest + i + 64, second);
#else
        for (size_t j = 0; j < 8; j++) {
            vst1q_u8(dest + i + j * 16, out[j]);
        }
#endif
    }
}

/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
static void
unshuffle16_neon(uint8_t *const dest, const uint8_t *const src,
                 const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 16;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 256, k++) {
        uint8x16_t lo[8], hi[8];
        unshuffle8_block_neon(src + k * 16, total_elements, lo);
        unshuffle8_block_neon(src + k * 16 + 8 * total_elements, total_elements, hi);
        for (size_t j = 0; j < 8; j++) {
            uint64x2_t x = vreinterpretq_u64_u8(lo[j]);
            uint64x2_t y = vreinterpretq_u64_u8(hi[j]);
            vst1q_u8(dest + i + (2 * j) * 16, vreinterpretq_u8_u64(vcombine_u64(vget_low_u64(x), vget_low_u64(y))));
            vst1q_u8(dest + i + (2 * j + 1) * 16, vreinterpretq_u8_u64(vcombine_u64(vget_high_u64(x), vget_high_u64(y))));
        }
    }
}


/* Shuffle a block.  This can never fail. */
void
shuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
             const uint8_t *const _src, uint8_t *_dest) {
    int32_t vectorized_chunk_size = 1;
    if (bytesoftype == 2 || bytesoftype == 4 || bytesoftype == 8 || bytesoftype == 16) {
        vectorized_chunk_size = bytesoftype * 16;
    }
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
            shuffle2_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        case 4:
            shuffle4_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        case 8:
            shuffle8_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        case 16:
            shuffle16_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        default:
            /* Non-optimized shuffle */
            shuffle_generic(bytesoftype, blocksize, _src, _dest);
            /* The non-optimized function covers the whole buffer,
               so we're done processing here. */
            return;
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
unshuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
               const uint8_t *const _src, uint8_t *_dest) {
    int32_t vectorized_chunk_size = 1;
    if (bytesoftype == 2 || bytesoftype == 4 || bytesoftype == 8 || bytesoftype == 16) {
        vectorized_chunk_size = bytesoftype * 16;
    }
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
            unshuffle2_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        case 4:
            unshuffle4_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        case 8:
            unshuffle8_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        case 16:
            unshuffle16_neon(_dest, _src, vectorizable_elements, total_elements);
            break;
        default:
            /* Non-optimized unshuffle */
            unshuffle_generic(bytesoftype, blocksize, _src, _dest);
            /* The non-optimized function covers the whole buffer,
               so we're done processing here. */
            return;
    }

    /* If the buffer had any bytes at the end which couldn't be handled
       by the vectorized implementations, use the non-optimized version
       to finish them up. */
    if (vectorizable_bytes < blocksize) {
        unshuffle_generic_inline(bytesoftype, vectorizable_bytes, blocksize, _src, _dest);
    }
}

const bool is_shuffle_neon = true;

#else /* defined(__ARM_NEON) */

const bool is_shuffle_neon = false;

void shuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
                  const uint8_t* const _src, uint8_t* const _dest) {
  (void)bytesoftype;
  (void)blocksize;
  (void)_src;
  (void)_dest;
  abort();
}

void unshuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
                    const uint8_t *_src, uint8_t *_dest) {
  (void)bytesoftype;
  (void)blocksize;
  (void)_src;
  (void)_dest;
  abort();
}

#endif /* defined(__ARM_NEON) */

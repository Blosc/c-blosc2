/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  Lucian Marc <ruben.lucian@gmail.com>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "shuffle-generic.h"
#include "shuffle-neon.h"

/* Make sure NEON is available for the compilation target and compiler. */
#if !defined(__ARM_NEON)
#error NEON is not supported by the target architecture/platform and/or this compiler.
#endif

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


/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_neon(uint8_t *const dest, const uint8_t *const src,
              const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 2;
    uint8x16x2_t r0;

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 32, k++) {
        /* Load (and permute) 32 bytes to the structure r0 */
        r0 = vld2q_u8(src + i);
        /* Store the results in the destination vector */
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

/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
static void
shuffle8_neon(uint8_t *const dest, const uint8_t *const src,
              const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 8;
    uint8x8x2_t r0[4];
    uint16x4x2_t r1[4];
    uint32x2x2_t r2[4];

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 64, k++) {
        /* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
        r0[0] = vzip_u8(vld1_u8(src + i + 0 * 8), vld1_u8(src + i + 1 * 8));
        r0[1] = vzip_u8(vld1_u8(src + i + 2 * 8), vld1_u8(src + i + 3 * 8));
        r0[2] = vzip_u8(vld1_u8(src + i + 4 * 8), vld1_u8(src + i + 5 * 8));
        r0[3] = vzip_u8(vld1_u8(src + i + 6 * 8), vld1_u8(src + i + 7 * 8));
        /* Interleave 16 bytes */
        r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
        r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
        r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
        r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
        /* Interleave 32 bytes */
        r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
        r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
        r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
        r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
        /* Store the results in the destination vector */
        vst1_u8(dest + k * 8 + 0 * total_elements, vreinterpret_u8_u32(r2[0].val[0]));
        vst1_u8(dest + k * 8 + 1 * total_elements, vreinterpret_u8_u32(r2[0].val[1]));
        vst1_u8(dest + k * 8 + 2 * total_elements, vreinterpret_u8_u32(r2[1].val[0]));
        vst1_u8(dest + k * 8 + 3 * total_elements, vreinterpret_u8_u32(r2[1].val[1]));
        vst1_u8(dest + k * 8 + 4 * total_elements, vreinterpret_u8_u32(r2[2].val[0]));
        vst1_u8(dest + k * 8 + 5 * total_elements, vreinterpret_u8_u32(r2[2].val[1]));
        vst1_u8(dest + k * 8 + 6 * total_elements, vreinterpret_u8_u32(r2[3].val[0]));
        vst1_u8(dest + k * 8 + 7 * total_elements, vreinterpret_u8_u32(r2[3].val[1]));
    }
}

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
static void
shuffle16_neon(uint8_t *const dest, const uint8_t *const src,
               const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 16;
    uint8x8x2_t r0[8];
    uint16x4x2_t r1[8];
    uint32x2x2_t r2[8];

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 128, k++) {
        /* Load and interleave groups of 16 bytes (128 bytes) to the structure r0 */
        r0[0] = vzip_u8(vld1_u8(src + i + 0 * 8), vld1_u8(src + i + 2 * 8));
        r0[1] = vzip_u8(vld1_u8(src + i + 1 * 8), vld1_u8(src + i + 3 * 8));
        r0[2] = vzip_u8(vld1_u8(src + i + 4 * 8), vld1_u8(src + i + 6 * 8));
        r0[3] = vzip_u8(vld1_u8(src + i + 5 * 8), vld1_u8(src + i + 7 * 8));
        r0[4] = vzip_u8(vld1_u8(src + i + 8 * 8), vld1_u8(src + i + 10 * 8));
        r0[5] = vzip_u8(vld1_u8(src + i + 9 * 8), vld1_u8(src + i + 11 * 8));
        r0[6] = vzip_u8(vld1_u8(src + i + 12 * 8), vld1_u8(src + i + 14 * 8));
        r0[7] = vzip_u8(vld1_u8(src + i + 13 * 8), vld1_u8(src + i + 15 * 8));
        /* Interleave 16 bytes */
        r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[2].val[0]));
        r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[2].val[1]));
        r1[2] = vzip_u16(vreinterpret_u16_u8(r0[1].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
        r1[3] = vzip_u16(vreinterpret_u16_u8(r0[1].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
        r1[4] = vzip_u16(vreinterpret_u16_u8(r0[4].val[0]), vreinterpret_u16_u8(r0[6].val[0]));
        r1[5] = vzip_u16(vreinterpret_u16_u8(r0[4].val[1]), vreinterpret_u16_u8(r0[6].val[1]));
        r1[6] = vzip_u16(vreinterpret_u16_u8(r0[5].val[0]), vreinterpret_u16_u8(r0[7].val[0]));
        r1[7] = vzip_u16(vreinterpret_u16_u8(r0[5].val[1]), vreinterpret_u16_u8(r0[7].val[1]));
        /* Interleave 32 bytes */
        r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[4].val[0]));
        r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[4].val[1]));
        r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[5].val[0]));
        r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[5].val[1]));
        r2[4] = vzip_u32(vreinterpret_u32_u16(r1[2].val[0]), vreinterpret_u32_u16(r1[6].val[0]));
        r2[5] = vzip_u32(vreinterpret_u32_u16(r1[2].val[1]), vreinterpret_u32_u16(r1[6].val[1]));
        r2[6] = vzip_u32(vreinterpret_u32_u16(r1[3].val[0]), vreinterpret_u32_u16(r1[7].val[0]));
        r2[7] = vzip_u32(vreinterpret_u32_u16(r1[3].val[1]), vreinterpret_u32_u16(r1[7].val[1]));
        /* Store the results to the destination vector */
        vst1_u8(dest + k * 8 + 0 * total_elements, vreinterpret_u8_u32(r2[0].val[0]));
        vst1_u8(dest + k * 8 + 1 * total_elements, vreinterpret_u8_u32(r2[0].val[1]));
        vst1_u8(dest + k * 8 + 2 * total_elements, vreinterpret_u8_u32(r2[1].val[0]));
        vst1_u8(dest + k * 8 + 3 * total_elements, vreinterpret_u8_u32(r2[1].val[1]));
        vst1_u8(dest + k * 8 + 4 * total_elements, vreinterpret_u8_u32(r2[2].val[0]));
        vst1_u8(dest + k * 8 + 5 * total_elements, vreinterpret_u8_u32(r2[2].val[1]));
        vst1_u8(dest + k * 8 + 6 * total_elements, vreinterpret_u8_u32(r2[3].val[0]));
        vst1_u8(dest + k * 8 + 7 * total_elements, vreinterpret_u8_u32(r2[3].val[1]));
        vst1_u8(dest + k * 8 + 8 * total_elements, vreinterpret_u8_u32(r2[4].val[0]));
        vst1_u8(dest + k * 8 + 9 * total_elements, vreinterpret_u8_u32(r2[4].val[1]));
        vst1_u8(dest + k * 8 + 10 * total_elements, vreinterpret_u8_u32(r2[5].val[0]));
        vst1_u8(dest + k * 8 + 11 * total_elements, vreinterpret_u8_u32(r2[5].val[1]));
        vst1_u8(dest + k * 8 + 12 * total_elements, vreinterpret_u8_u32(r2[6].val[0]));
        vst1_u8(dest + k * 8 + 13 * total_elements, vreinterpret_u8_u32(r2[6].val[1]));
        vst1_u8(dest + k * 8 + 14 * total_elements, vreinterpret_u8_u32(r2[7].val[0]));
        vst1_u8(dest + k * 8 + 15 * total_elements, vreinterpret_u8_u32(r2[7].val[1]));
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

/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
static void
unshuffle8_neon(uint8_t *const dest, const uint8_t *const src,
                const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 8;
    uint8x8x2_t r0[4];
    uint16x4x2_t r1[4];
    uint32x2x2_t r2[4];

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 64, k++) {
        /* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
        r0[0] = vzip_u8(vld1_u8(src + 0 * total_elements + k * 8), vld1_u8(src + 1 * total_elements + k * 8));
        r0[1] = vzip_u8(vld1_u8(src + 2 * total_elements + k * 8), vld1_u8(src + 3 * total_elements + k * 8));
        r0[2] = vzip_u8(vld1_u8(src + 4 * total_elements + k * 8), vld1_u8(src + 5 * total_elements + k * 8));
        r0[3] = vzip_u8(vld1_u8(src + 6 * total_elements + k * 8), vld1_u8(src + 7 * total_elements + k * 8));
        /* Interleave 16 bytes */
        r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
        r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
        r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
        r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
        /* Interleave 32 bytes */
        r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
        r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
        r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
        r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
        /* Store the results in the destination vector */
        vst1_u8(dest + i + 0 * 8, vreinterpret_u8_u32(r2[0].val[0]));
        vst1_u8(dest + i + 1 * 8, vreinterpret_u8_u32(r2[0].val[1]));
        vst1_u8(dest + i + 2 * 8, vreinterpret_u8_u32(r2[1].val[0]));
        vst1_u8(dest + i + 3 * 8, vreinterpret_u8_u32(r2[1].val[1]));
        vst1_u8(dest + i + 4 * 8, vreinterpret_u8_u32(r2[2].val[0]));
        vst1_u8(dest + i + 5 * 8, vreinterpret_u8_u32(r2[2].val[1]));
        vst1_u8(dest + i + 6 * 8, vreinterpret_u8_u32(r2[3].val[0]));
        vst1_u8(dest + i + 7 * 8, vreinterpret_u8_u32(r2[3].val[1]));

    }
}

/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
static void
unshuffle16_neon(uint8_t *const dest, const uint8_t *const src,
                 const size_t vectorizable_elements, const size_t total_elements) {
    size_t i, k;
    static const size_t bytesoftype = 16;
    uint8x8x2_t r0[8];
    uint16x4x2_t r1[8];
    uint32x2x2_t r2[8];

    for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 128, k++) {
        /* Load and interleave groups of 16 bytes (128 bytes) to the structure r0*/
        r0[0] = vzip_u8(vld1_u8(src + k * 8 + 0 * total_elements), vld1_u8(src + k * 8 + 1 * total_elements));
        r0[1] = vzip_u8(vld1_u8(src + k * 8 + 2 * total_elements), vld1_u8(src + k * 8 + 3 * total_elements));
        r0[2] = vzip_u8(vld1_u8(src + k * 8 + 4 * total_elements), vld1_u8(src + k * 8 + 5 * total_elements));
        r0[3] = vzip_u8(vld1_u8(src + k * 8 + 6 * total_elements), vld1_u8(src + k * 8 + 7 * total_elements));
        r0[4] = vzip_u8(vld1_u8(src + k * 8 + 8 * total_elements), vld1_u8(src + k * 8 + 9 * total_elements));
        r0[5] = vzip_u8(vld1_u8(src + k * 8 + 10 * total_elements), vld1_u8(src + k * 8 + 11 * total_elements));
        r0[6] = vzip_u8(vld1_u8(src + k * 8 + 12 * total_elements), vld1_u8(src + k * 8 + 13 * total_elements));
        r0[7] = vzip_u8(vld1_u8(src + k * 8 + 14 * total_elements), vld1_u8(src + k * 8 + 15 * total_elements));
        /* Interleave 16 bytes */
        r1[0] = vzip_u16(vreinterpret_u16_u8(r0[0].val[0]), vreinterpret_u16_u8(r0[1].val[0]));
        r1[1] = vzip_u16(vreinterpret_u16_u8(r0[0].val[1]), vreinterpret_u16_u8(r0[1].val[1]));
        r1[2] = vzip_u16(vreinterpret_u16_u8(r0[2].val[0]), vreinterpret_u16_u8(r0[3].val[0]));
        r1[3] = vzip_u16(vreinterpret_u16_u8(r0[2].val[1]), vreinterpret_u16_u8(r0[3].val[1]));
        r1[4] = vzip_u16(vreinterpret_u16_u8(r0[4].val[0]), vreinterpret_u16_u8(r0[5].val[0]));
        r1[5] = vzip_u16(vreinterpret_u16_u8(r0[4].val[1]), vreinterpret_u16_u8(r0[5].val[1]));
        r1[6] = vzip_u16(vreinterpret_u16_u8(r0[6].val[0]), vreinterpret_u16_u8(r0[7].val[0]));
        r1[7] = vzip_u16(vreinterpret_u16_u8(r0[6].val[1]), vreinterpret_u16_u8(r0[7].val[1]));
        /* Interleave 32 bytes */
        r2[0] = vzip_u32(vreinterpret_u32_u16(r1[0].val[0]), vreinterpret_u32_u16(r1[2].val[0]));
        r2[1] = vzip_u32(vreinterpret_u32_u16(r1[0].val[1]), vreinterpret_u32_u16(r1[2].val[1]));
        r2[2] = vzip_u32(vreinterpret_u32_u16(r1[1].val[0]), vreinterpret_u32_u16(r1[3].val[0]));
        r2[3] = vzip_u32(vreinterpret_u32_u16(r1[1].val[1]), vreinterpret_u32_u16(r1[3].val[1]));
        r2[4] = vzip_u32(vreinterpret_u32_u16(r1[4].val[0]), vreinterpret_u32_u16(r1[6].val[0]));
        r2[5] = vzip_u32(vreinterpret_u32_u16(r1[4].val[1]), vreinterpret_u32_u16(r1[6].val[1]));
        r2[6] = vzip_u32(vreinterpret_u32_u16(r1[5].val[0]), vreinterpret_u32_u16(r1[7].val[0]));
        r2[7] = vzip_u32(vreinterpret_u32_u16(r1[5].val[1]), vreinterpret_u32_u16(r1[7].val[1]));
        /* Store the results in the destination vector */
        vst1_u8(dest + i + 0 * 8, vreinterpret_u8_u32(r2[0].val[0]));
        vst1_u8(dest + i + 1 * 8, vreinterpret_u8_u32(r2[4].val[0]));
        vst1_u8(dest + i + 2 * 8, vreinterpret_u8_u32(r2[0].val[1]));
        vst1_u8(dest + i + 3 * 8, vreinterpret_u8_u32(r2[4].val[1]));
        vst1_u8(dest + i + 4 * 8, vreinterpret_u8_u32(r2[1].val[0]));
        vst1_u8(dest + i + 5 * 8, vreinterpret_u8_u32(r2[5].val[0]));
        vst1_u8(dest + i + 6 * 8, vreinterpret_u8_u32(r2[1].val[1]));
        vst1_u8(dest + i + 7 * 8, vreinterpret_u8_u32(r2[5].val[1]));
        vst1_u8(dest + i + 8 * 8, vreinterpret_u8_u32(r2[2].val[0]));
        vst1_u8(dest + i + 9 * 8, vreinterpret_u8_u32(r2[6].val[0]));
        vst1_u8(dest + i + 10 * 8, vreinterpret_u8_u32(r2[2].val[1]));
        vst1_u8(dest + i + 11 * 8, vreinterpret_u8_u32(r2[6].val[1]));
        vst1_u8(dest + i + 12 * 8, vreinterpret_u8_u32(r2[3].val[0]));
        vst1_u8(dest + i + 13 * 8, vreinterpret_u8_u32(r2[7].val[0]));
        vst1_u8(dest + i + 14 * 8, vreinterpret_u8_u32(r2[3].val[1]));
        vst1_u8(dest + i + 15 * 8, vreinterpret_u8_u32(r2[7].val[1]));
    }
}


/* Shuffle a block.  This can never fail. */
void
shuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
             const uint8_t *const _src, uint8_t *_dest) {
    int32_t vectorized_chunk_size = 1;
    if (bytesoftype == 2 || bytesoftype == 4) {
        vectorized_chunk_size = bytesoftype * 16;
    } else if (bytesoftype == 8 || bytesoftype == 16) {
        vectorized_chunk_size = bytesoftype * 8;
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
    if (bytesoftype == 2 || bytesoftype == 4) {
        vectorized_chunk_size = bytesoftype * 16;
    } else if (bytesoftype == 8 || bytesoftype == 16) {
        vectorized_chunk_size = bytesoftype * 8;
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

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Lucian Marc <ruben.lucian@gmail.com>

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
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
shuffle2_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements) {
  size_t i, j, k;
  static const size_t bytesoftype = 2;
  uint8x16x2_t r0;

  for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 32, k++) {
    /* Load (and permute) 32 bytes to the structure r0 */
    r0 = vld2q_u8(src + i);
    /* Store the results in the destination vector */
    for (j = 0; j < 2; j++) {
      vst1q_u8(dest + total_elements * j + k * 16, r0.val[j]);
    }
  }
}

/* Routine optimized for shuffling a buffer for a type size of 4 bytes. */
static void
shuffle4_neon(uint8_t* const dest, const uint8_t* const src,
              const size_t vectorizable_elements, const size_t total_elements) {
  size_t i, j, k;
  static const size_t bytesoftype = 4;
  uint8x16x4_t r0;

  for (i = 0, k = 0; i < vectorizable_elements * bytesoftype; i += 64, k++) {
    /* Load (and permute) 64 bytes to the structure r0 */
    r0 = vld4q_u8(src + i);
    /* Store the results in the destination vector */
    for (j = 0; j < 4; j++) {
      vst1q_u8(dest + total_elements * j + k * 16, r0.val[j]);
    }
  }
}

/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
shuffle8_neon(uint8_t
* const dest,
const uint8_t* const src,
const size_t vectorizable_elements,
const size_t total_elements
)
{
size_t i, j, k, l;
static const size_t bytesoftype = 8;
uint8x8x2_t r0[4];
uint16x4x2_t r1[4];
uint32x2x2_t r2[4];

for(
i = 0, k = 0;
i<vectorizable_elements*bytesoftype;
i += 64, k++) {
/* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
for(
j = 0;
j < 4; j++) {
r0[j] =
vzip_u8(vld1_u8(src + i + (2 * j) * 8), vld1_u8(src + i + (2 * j + 1) * 8)
);
}
/* Interleave 16 bytes */
for(
j = 0;
j < 2; j++) {
for(
l = 0;
l < 2; l++) {
r1[j*2+l] =
vzip_u16(vreinterpret_u16_u8(r0[j * 2].val[l]), vreinterpret_u16_u8(r0[j * 2 + 1].val[l])
);
}
}
/* Interleave 32 bytes */
for(
j = 0;
j < 2; j++) {
for(
l = 0;
l < 2; l++) {
r2[j*2+l] =
vzip_u32(vreinterpret_u32_u16(r1[j].val[l]), vreinterpret_u32_u16(r1[j + 2].val[l])
);
}
}
/* Store the results in the destination vector */
for(
j = 0;
j < 4; j++) {
for(
l = 0;
l < 2; l++) {
vst1_u8(dest
+ k*8 + (j*2+l)*total_elements,
vreinterpret_u8_u32(r2[j]
.val[l]));
}
}
}
}

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
shuffle16_neon(uint8_t
* const dest,
const uint8_t* const src,
const size_t vectorizable_elements,
const size_t total_elements
)
{
size_t i, j, k, l, m;
static const size_t bytesoftype = 16;
uint8x8x2_t r0[8];
uint16x4x2_t r1[8];
uint32x2x2_t r2[8];

for(
i = 0, k = 0;
i<vectorizable_elements*bytesoftype;
i += 128, k++) {
/* Load and interleave groups of 16 bytes (128 bytes) to the structure r0 */
for(
j = 0;
j < 8; j++) { ;
l = (j % 2) ? 1 : 0;
r0[j] =
vzip_u8(vld1_u8(src + i + (2 * j - l) * 8), vld1_u8(src + i + (2 * j - l + 2) * 8)
);
}
/* Interleave 16 bytes */
for(
j = 0;
j < 2; j++) {
for(
l = 0;
l < 2; l++) {
for(
m = 0;
m < 2; m++) {
r1[j*2+l+4*m] =
vzip_u16(vreinterpret_u16_u8(r0[j + 4 * m].val[l]), vreinterpret_u16_u8(r0[j + 2 + 4 * m].val[l])
);
}
}
}
/* Interleave 32 bytes */
for(
j = 0;
j < 4; j++) {
for(
l = 0;
l < 2; l++) {
r2[2*j+l] =
vzip_u32(vreinterpret_u32_u16(r1[j].val[l]), vreinterpret_u32_u16(r1[j + 4].val[l])
);
}
}
/* Store the results to the destination vector */
for(
j = 0;
j < 8; j++) {
for(
l = 0;
l < 2; l++) {
vst1_u8(dest
+ k*8 + (2*j+l)*total_elements,
vreinterpret_u8_u32(r2[j]
.val[l]));
}
}
}
}

/* Routine optimized for unshuffling a buffer for a type size of 2 bytes. */
static void
    unshuffle2_neon(uint8_t * const
dest,
const uint8_t* const src,
const size_t vectorizable_elements,
const size_t total_elements
)
{
size_t i, j, k;
static const size_t bytesoftype = 2;
uint8x16x2_t r0;

for(
i = 0, k = 0;
i<vectorizable_elements*bytesoftype;
i += 32, k++) {
/* Load 32 bytes to the structure r0 */
for(
j = 0;
j < 2; j++) {
r0.val[j] =
vld1q_u8(src
+
total_elements* j
+ k*16);
}
/* Store (with permutation) the results in the destination vector */
vst2q_u8(dest
+ k*32, r0);
}
}

/* Routine optimized for unshuffling a buffer for a type size of 4 bytes. */
static void
    unshuffle4_neon(uint8_t * const
dest,
const uint8_t* const src,
const size_t vectorizable_elements,
const size_t total_elements
)
{
size_t i, j, k;
static const size_t bytesoftype = 4;
uint8x16x4_t r0;

for(
i = 0, k = 0;
i<vectorizable_elements*bytesoftype;
i += 64, k++) {
/* load 64 bytes to the structure r0 */
for(
j = 0;
j < 4; j++) {
r0.val[j] =
vld1q_u8(src
+
total_elements* j
+ k*16);
}
/* Store (with permutation) the results in the destination vector */
vst4q_u8(dest
+ k*64, r0);
}
}

/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
unshuffle8_neon(uint8_t
* const dest,
const uint8_t* const src,
const size_t vectorizable_elements,
const size_t total_elements
)
{
size_t i, j, k, l;
static const size_t bytesoftype = 8;
uint8x8x2_t r0[4];
uint16x4x2_t r1[4];
uint32x2x2_t r2[4];

for(
i = 0, k = 0;
i<vectorizable_elements*bytesoftype;
i += 64, k++) {
/* Load and interleave groups of 8 bytes (64 bytes) to the structure r0 */
for(
j = 0;
j < 4; j++) {
r0[j] =
vzip_u8(vld1_u8(src + (2 * j) * total_elements + k * 8), vld1_u8(src + (2 * j + 1) * total_elements + k * 8)
);
}
/* Interleave 16 bytes */
for(
j = 0;
j < 2; j++) {
for(
l = 0;
l < 2; l++) {
r1[j*2+l] =
vzip_u16(vreinterpret_u16_u8(r0[j * 2].val[l]), vreinterpret_u16_u8(r0[j * 2 + 1].val[l])
);
}
}
/* Interleave 32 bytes */
for(
j = 0;
j < 2; j++) {
for(
l = 0;
l < 2; l++) {
r2[j*2+l] =
vzip_u32(vreinterpret_u32_u16(r1[j].val[l]), vreinterpret_u32_u16(r1[j + 2].val[l])
);
}
}
/* Store the results in the destination vector */
for(
j = 0;
j < 4; j++) {
for(
l = 0;
l < 2; l++) {
vst1_u8(dest
+ i + (j*2+l)*8,
vreinterpret_u8_u32(r2[j]
.val[l]));
}
}
}
}

/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
unshuffle16_neon(uint8_t
* const dest,
const uint8_t* const src,
const size_t vectorizable_elements,
const size_t total_elements
)
{
size_t i, j, k, l, m;
static const size_t bytesoftype = 16;
uint8x8x2_t r0[8];
uint16x4x2_t r1[8];
uint32x2x2_t r2[8];

for(
i = 0, k = 0;
i<vectorizable_elements*bytesoftype;
i += 128, k++) {
/* Load and interleave groups of 16 bytes (128 bytes) to the structure r0*/
for(
j = 0;
j < 8; j++) {
r0[j] =
vzip_u8(vld1_u8(src + (2 * j) * total_elements + k * 8), vld1_u8(src + (2 * j + 1) * total_elements + k * 8)
);
}
/* Interleave 16 bytes */
for(
j = 0;
j < 4; j++) {
for(
l = 0;
l < 2; l++) {
r1[2*j+l] =
vzip_u16(vreinterpret_u16_u8(r0[2 * j].val[l]), vreinterpret_u16_u8(r0[2 * j + 1].val[l])
);
}
}
/* Interleave 32 bytes */
for(
j = 0;
j < 2; j++) {
for(
l = 0;
l < 2; l++) {
for(
m = 0;
m < 2; m++) {
r2[j*2+l+4*m] =
vzip_u32(vreinterpret_u32_u16(r1[j + 4 * m].val[l]), vreinterpret_u32_u16(r1[j + 2 + 4 * m].val[l])
);
}
}
}
/* Store the results in the destination vector */
for(
j = 0;
j < 4; j++) {
for(
l = 0;
l < 2; l++) {
for(
m = 0;
m < 2; m++) {
vst1_u8(dest
+ i + (4*j+m+2*l)*8,
vreinterpret_u8_u32(r2[j + 4 * m]
.val[l]));
}
}
}
}
}

/* Shuffle a block.  This can never fail. */
void
shuffle_neon(const size_t bytesoftype, const size_t blocksize,
             const uint8_t* const _src, uint8_t* const _dest) {
  size_t vectorized_chunk_size;
  if (bytesoftype == 2 | bytesoftype == 4) {
    vectorized_chunk_size = bytesoftype * 16;
  } else if (bytesoftype == 8 | bytesoftype == 16) {
    vectorized_chunk_size = bytesoftype * 8;
  }
  /* If the blocksize is not a multiple of both the typesize and
     the vector size, round the blocksize down to the next value
     which is a multiple of both. The vectorized shuffle can be
     used for that portion of the data, and the naive implementation
     can be used for the remaining portion. */
  const size_t vectorizable_bytes = blocksize - (blocksize % vectorized_chunk_size);
  const size_t vectorizable_elements = vectorizable_bytes / bytesoftype;
  const size_t total_elements = blocksize / bytesoftype;

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
unshuffle_neon(const size_t bytesoftype, const size_t blocksize,
               const uint8_t* const _src, uint8_t* const _dest) {
  size_t vectorized_chunk_size;
  if (bytesoftype == 2 | bytesoftype == 4) {
    vectorized_chunk_size = bytesoftype * 16;
  } else if (bytesoftype == 8 | bytesoftype == 16) {
    vectorized_chunk_size = bytesoftype * 8;
  }
  /* If the blocksize is not a multiple of both the typesize and
     the vector size, round the blocksize down to the next value
     which is a multiple of both. The vectorized unshuffle can be
     used for that portion of the data, and the naive implementation
     can be used for the remaining portion. */
  const size_t vectorizable_bytes = blocksize - (blocksize % vectorized_chunk_size);
  const size_t vectorizable_elements = vectorizable_bytes / bytesoftype;
  const size_t total_elements = blocksize / bytesoftype;


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

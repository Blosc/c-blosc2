/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc developers <blosc@blosc.org> and Jerome Kieffer <jerome.kieffer@esrf.fr>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "shuffle-generic.h"
#include "shuffle-altivec.h"

/* Make sure ALTIVEC is available for the compilation target and compiler. */
#if !defined(__ALTIVEC__)
  #error ALTIVEC is not supported by the target architecture/platform and/or this compiler.
#endif

#include <altivec.h>
#include "transpose-altivec.h"

/* Routine optimized for shuffling a buffer for a type size of 2 bytes. */
static void
shuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  static const int32_t bytesoftype = 2;
  uint32_t i, j;
  __vector uint8_t xmm0[2];

  for (j = 0; j < vectorizable_elements; j += 16){
    /* Fetch 16 elements (32 bytes) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);

    /* Transpose vectors */
    transpose2x16(xmm0);

    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i++)
        vec_xst(xmm0[i], j + i * total_elements, dest);
  }
}

/* Routine optimized for shuffling a buffer for a type size of 4 bytes. */
static void
shuffle4_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements){
  static const int32_t bytesoftype = 4;
  int32_t i, j;
  __vector uint8_t xmm0[4];

  for (j = 0; j < vectorizable_elements; j += 16)
  {
    /* Fetch 16 elements (64 bytes, 4 vectors) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);


    /* Transpose vectors */
    transpose4x16(xmm0);

    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i ++){
        vec_xst(xmm0[i], j + i*total_elements, dest);
    }
  }
}


/* Routine optimized for shuffling a buffer for a type size of 8 bytes. */
static void
shuffle8_altivec(uint8_t* const dest, const uint8_t* const src,
                 const int32_t vectorizable_elements, const int32_t total_elements) {
  static const uint8_t bytesoftype = 8;
  int32_t i, j;
  __vector uint8_t xmm0[8];

  for (j = 0; j < vectorizable_elements; j += 16)
  {
    /* Fetch 16 elements (128 bytes, 8 vectors) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);

    /* Transpose vectors */
    transpose8x16(xmm0);

    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i++)
      vec_xst(xmm0[i], j + i*total_elements, dest);
  }
}

/* Routine optimized for shuffling a buffer for a type size of 16 bytes. */
static void
shuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                  const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 16;
  int32_t i, j;
  __vector uint8_t xmm0[16];

  for (j = 0; j < vectorizable_elements; j += 16)
  {
    /* Fetch 16 elements (256 bytes, 16 vectors) */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(bytesoftype * j + 16 * i, src);

    // Do the job !
    transpose16x16(xmm0);

    /* Store the result vectors */
    for (i = 0; i < bytesoftype; i ++)
      vec_xst(xmm0[i], j + i * total_elements, dest);
  }
}


/* Routine optimized for shuffling a buffer for a type size larger than 16 bytes. */
static void
shuffle16_tiled_altivec(uint8_t* const dest, const uint8_t* const src,
                        const int32_t vectorizable_elements, const int32_t total_elements,
                        const int32_t bytesoftype) {
  int32_t j, k;
  const int32_t vecs_per_el_rem = bytesoftype & 0xF;
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
      for (k = 0; k < 16; k++) {
        vec_xst(xmm[k], j + total_elements * (offset_into_type + k), dest);
      }
    }
  }
}
/* Routine optimized for unshuffling a buffer for a type size of 2 bytes. */
static void
unshuffle2_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 2;
  uint32_t i, j;
  __vector uint8_t xmm0[2], xmm1[2];

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (32 bytes) into 2 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
      xmm0[i] = vec_xl(j + i * total_elements, src);

    /* Shuffle bytes */
    /* Note the shuffling is different from intel's SSE2 */
    xmm1[0] = vec_vmrghb(xmm0[0], xmm0[1]);
    xmm1[1] = vec_vmrglb(xmm0[0], xmm0[1]);

    /* Store the result vectors*/
    for (i = 0; i < bytesoftype; i++)
      vec_xst(xmm1[i], bytesoftype * j + 16 * i, dest);
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 4 bytes. */
static void
unshuffle4_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 4;
  uint32_t i, j;
  __vector uint8_t xmm0[4], xmm1[4];

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_xl(j + i * total_elements, src);

    /* Shuffle bytes */
    for (i = 0; i < 2; i++) {
      xmm1[i  ] = vec_vmrghb(xmm0[i * 2], xmm0[i * 2 + 1]);
      xmm1[i+2] = vec_vmrglb(xmm0[i * 2], xmm0[i * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (i = 0; i < 2; i++) {
      /* Compute the low 32 bytes */
      xmm0[i] = (__vector uint8_t) vec_vmrghh((__vector uint16_t)xmm1[i * 2],
                                              (__vector uint16_t) xmm1[i * 2 + 1]);
      /* Compute the hi 32 bytes */
      xmm0[i+2] = (__vector uint8_t) vec_vmrglh((__vector uint16_t)xmm1[i * 2],
                                                (__vector uint16_t)xmm1[i * 2 + 1]);
    }
    /* Store the result vectors in proper order */
    vec_xst(xmm0[0], bytesoftype * j, dest);
    vec_xst(xmm0[2], bytesoftype * j + 16, dest);
    vec_xst(xmm0[1], bytesoftype * j + 32, dest);
    vec_xst(xmm0[3], bytesoftype * j + 48, dest);
  }
}

/* Routine optimized for unshuffling a buffer for a type size of 8 bytes. */
static void
unshuffle8_altivec(uint8_t* const dest, const uint8_t* const src,
                   const int32_t vectorizable_elements, const int32_t total_elements) {
  static const uint8_t bytesoftype = 8;
  uint32_t i, j;
  __vector uint8_t xmm0[8], xmm1[8];

  // Initialize permutations for writing
  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_xl(j + i * total_elements, src);
    /* Shuffle bytes */
    for (i = 0; i < 4; i++) {
      xmm1[i] = vec_vmrghb(xmm0[i * 2], xmm0[i * 2 + 1]);
      xmm1[4 + i] = vec_vmrglb(xmm0[i * 2], xmm0[i * 2 + 1]);
    }
    /* Shuffle 2-byte words */
    for (i = 0; i < 4; i++) {
      xmm0[i] = (__vector uint8_t)vec_vmrghh((__vector uint16_t)xmm1[i * 2],
                                             (__vector uint16_t)xmm1[i * 2 + 1]);
      xmm0[4 + i] = (__vector uint8_t)vec_vmrglh((__vector uint16_t)xmm1[i * 2],
                                                 (__vector uint16_t)xmm1[i * 2 + 1]);
    }
    /* Shuffle 4-byte dwords */
    for (i = 0; i < 4; i++) {
      xmm1[i] = (__vector uint8_t)vec_vmrghw((__vector uint32_t)xmm0[i * 2],
                                             (__vector uint32_t)xmm0[i * 2 + 1]);
      xmm1[4 + i] = (__vector uint8_t)vec_vmrglw((__vector uint32_t)xmm0[i * 2],
                                                 (__vector uint32_t)xmm0[i * 2 + 1]);
    }
    /* Store the result vectors in proper order */
    vec_xst(xmm1[0], bytesoftype * j, dest);
    vec_xst(xmm1[4], bytesoftype * j + 16, dest);
    vec_xst(xmm1[2], bytesoftype * j + 32, dest);
    vec_xst(xmm1[6], bytesoftype * j + 48, dest);
    vec_xst(xmm1[1], bytesoftype * j + 64, dest);
    vec_xst(xmm1[5], bytesoftype * j + 80, dest);
    vec_xst(xmm1[3], bytesoftype * j + 96, dest);
    vec_xst(xmm1[7], bytesoftype * j + 112, dest);
  }
}


/* Routine optimized for unshuffling a buffer for a type size of 16 bytes. */
static void
unshuffle16_altivec(uint8_t* const dest, const uint8_t* const src,
                    const int32_t vectorizable_elements, const int32_t total_elements) {
  static const int32_t bytesoftype = 16;
  uint32_t i, j;
  __vector uint8_t xmm0[16];

  for (j = 0; j < vectorizable_elements; j += 16) {
    /* Load 16 elements (64 bytes) into 4 vectors registers. */
    for (i = 0; i < bytesoftype; i++)
        xmm0[i] = vec_xl(j + i * total_elements, src);

    // Do the Job!
    transpose16x16(xmm0);

    /* Store the result vectors*/
    for (i = 0; i < 16; i++)
      vec_st(xmm0[i], bytesoftype * (i+j), dest);
  }
}


/* Routine optimized for unshuffling a buffer for a type size larger than 16 bytes. */
static void
unshuffle16_tiled_altivec(uint8_t* const dest, const uint8_t* const orig,
                          const int32_t vectorizable_elements, const int32_t total_elements,
                          const int32_t bytesoftype) {
  int32_t i, j, offset_into_type;
  const int32_t vecs_per_el_rem = bytesoftype &  0xF;
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
      for (j = 0; j < 16; j++)
        xmm[j] = vec_xl(total_elements * (offset_into_type + j) + i, orig);

      // Do the Job !
      transpose16x16(xmm);

      /* Store the result vectors in proper order */
      for (j = 0; j < 16; j++)
        vec_xst(xmm[j], (i + j) * bytesoftype + offset_into_type, dest);
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

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  This codec is meant to leverage multidimensionality for getting
  better compression ratios.  The idea is to look for similarities
  in places that are closer in a euclidean metric, not the typical
  linear one.
**********************************************************************/

#include "ndlz8x8.h"
#include "xxhash.h"
#include "b2nd.h"

#include <stdlib.h>
#include <string.h>

/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__GNUC__) && (__GNUC__ > 2)
#define NDLZ_EXPECT_CONDITIONAL(c)    (__builtin_expect((c), 1))
#define NDLZ_UNEXPECT_CONDITIONAL(c)  (__builtin_expect((c), 0))
#else
#define NDLZ_EXPECT_CONDITIONAL(c)    (c)
#define NDLZ_UNEXPECT_CONDITIONAL(c)  (c)
#endif

/*
 * Use inlined functions for supported systems.
 */
#if defined(_MSC_VER) && !defined(__cplusplus)   /* Visual Studio */
#define inline __inline  /* Visual C is not C99, but supports some kind of inline */
#endif

#define MAX_COPY 32U
#define MAX_DISTANCE 65535


#ifdef BLOSC_STRICT_ALIGN
#define NDLZ_READU16(p) ((p)[0] | (p)[1]<<8)
#define NDLZ_READU32(p) ((p)[0] | (p)[1]<<8 | (p)[2]<<16 | (p)[3]<<24)
#else
#define NDLZ_READU16(p) *((const uint16_t*)(p))
#define NDLZ_READU32(p) *((const uint32_t*)(p))
#endif

#define HASH_LOG (12)


int ndlz8_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                   uint8_t meta, blosc2_cparams *cparams) {
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_ERROR_NULL(cparams, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(cparams->schunk, BLOSC2_ERROR_NULL_POINTER);
  uint8_t *smeta;
  int32_t smeta_len;

  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }

  const int cell_shape = 8;
  const int cell_size = 64;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  if (ndim != 2) {
    BLOSC_TRACE_ERROR("This codec only works for ndim = 2");
    return BLOSC2_ERROR_FAILURE;
  }

  if (input_len != (blockshape[0] * blockshape[1])) {
    BLOSC_TRACE_ERROR("Length not equal to blocksize");
    return BLOSC2_ERROR_FAILURE;
  }

  if (NDLZ_UNEXPECT_CONDITIONAL(output_len < (int) (1 + ndim * sizeof(int32_t)))) {
    BLOSC_TRACE_ERROR("Output too small");
    return BLOSC2_ERROR_FAILURE;
  }

  uint8_t *ip = (uint8_t *) input;
  uint8_t *op = (uint8_t *) output;
  uint8_t *op_limit;
  uint32_t hval, hash_cell;
  uint32_t hash_triple[6] = {0};
  uint32_t hash_pair[7] = {0};
  uint8_t *bufarea = malloc(cell_size);
  uint8_t *buf_cell = bufarea;
  uint8_t *buf_aux;
  uint32_t tab_cell[1U << 12U] = {0};
  uint32_t tab_triple[1U << 12U] = {0};
  uint32_t tab_pair[1U << 12U] = {0};
  uint32_t update_triple[6] = {0};
  uint32_t update_pair[7] = {0};

  // Minimum cratios before issuing and _early giveup_
  // Remind that ndlz is not meant for cratios <= 2 (too costly to decompress)

  op_limit = op + output_len;

  // Initialize the hash table to distances of 0
  for (unsigned i = 0; i < (1U << 12U); i++) {
    tab_cell[i] = 0;
    tab_triple[i] = 0;
    tab_pair[i] = 0;
  }

  /* input and output buffer cannot be less than 64 (cells are 8x8) */
  int overhead = 17 + (blockshape[0] * blockshape[1] / cell_size - 1) * 2;
  if (input_len < cell_size || output_len < overhead) {
    BLOSC_TRACE_ERROR("Incorrect length or maxout");
    return 0;
  }

  uint8_t *obase = op;

  /* we start with literal copy */
  *op++ = ndim;
  memcpy(op, &blockshape[0], 4);
  op += 4;
  memcpy(op, &blockshape[1], 4);
  op += 4;

  uint32_t i_stop[2];
  for (int i = 0; i < 2; ++i) {
    i_stop[i] = (blockshape[i] + cell_shape - 1) / cell_shape;
  }


  /* main loop */
  uint32_t padding[2];
  uint32_t ii[2];
  for (ii[0] = 0; ii[0] < i_stop[0]; ++ii[0]) {
    for (ii[1] = 0; ii[1] < i_stop[1]; ++ii[1]) {      // for each cell
      for (int h = 0; h < 7; h++) {         // new cell -> new possible references
        update_pair[h] = 0;
        if (h != 6) {
          update_triple[h] = 0;
        }
      }

      if (NDLZ_UNEXPECT_CONDITIONAL(op + cell_size + 1 > op_limit)) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        free(bufarea);
        return 0;
      }

      uint32_t orig = ii[0] * cell_shape * blockshape[1] + ii[1] * cell_shape;
      if (((blockshape[0] % cell_shape != 0) && (ii[0] == i_stop[0] - 1)) ||
          ((blockshape[1] % cell_shape != 0) && (ii[1] == i_stop[1] - 1))) {
        uint8_t token = 0;                                   // padding -> literal copy
        *op++ = token;
        if (ii[0] == i_stop[0] - 1) {
          padding[0] = (blockshape[0] % cell_shape == 0) ? cell_shape : blockshape[0] % cell_shape;
        } else {
          padding[0] = cell_shape;
        }
        if (ii[1] == i_stop[1] - 1) {
          padding[1] = (blockshape[1] % cell_shape == 0) ? cell_shape : blockshape[1] % cell_shape;
        } else {
          padding[1] = cell_shape;
        }
        for (uint32_t i = 0; i < padding[0]; i++) {
          memcpy(op, &ip[orig + i * blockshape[1]], padding[1]);
          op += padding[1];
        }
      } else {
        for (uint64_t i = 0; i < (uint64_t) cell_shape; i++) {           // fill cell buffer
          uint64_t ind = orig + i * blockshape[1];
          memcpy(buf_cell, &ip[ind], cell_shape);
          buf_cell += cell_shape;
        }
        buf_cell -= cell_size;

        const uint8_t *ref;
        uint32_t distance;
        uint8_t *anchor = op;    /* comparison starting-point */

        /* find potential match */
        hash_cell = XXH32(buf_cell, cell_size, 1);        // calculate cell hash
        hash_cell >>= 32U - 12U;
        ref = obase + tab_cell[hash_cell];

        /* calculate distance to the match */
        if (tab_cell[hash_cell] == 0) {
          distance = 0;
        } else {
          bool same = true;
          buf_aux = obase + tab_cell[hash_cell];
          for (int i = 0; i < cell_size; i++) {
            if (buf_cell[i] != buf_aux[i]) {
              same = false;
              break;
            }
          }
          if (same) {
            distance = (int32_t) (anchor - ref);
          } else {
            distance = 0;
          }
        }

        bool alleq = true;
        for (int i = 1; i < cell_size; i++) {
          if (buf_cell[i] != buf_cell[0]) {
            alleq = false;
            break;
          }
        }
        if (alleq) {                              // all elements of the cell equal
          uint8_t token = (uint8_t) (1U << 6U);
          *op++ = token;
          *op++ = buf_cell[0];

        } else if (distance == 0 || (distance >= MAX_DISTANCE)) {   // no cell match
          bool literal = true;

          // rows triples matches
          for (int i = 0; i < 6; i++) {
            int triple_start = i * cell_shape;
            hval = XXH32(&buf_cell[triple_start], 24, 1);        // calculate triple hash
            hval >>= 32U - 12U;
            /* calculate distance to the match */
            bool same = true;
            uint16_t offset;
            if (tab_triple[hval] != 0) {
              buf_aux = obase + tab_triple[hval];
              for (int l = 0; l < 24; l++) {
                if (buf_cell[triple_start + l] != buf_aux[l]) {
                  same = false;
                  break;
                }
              }
              offset = (uint16_t) (anchor - obase - tab_triple[hval]);
            } else {
              same = false;
              update_triple[i] = (uint32_t) (anchor + 1 + triple_start - obase);     /* update hash table */
              hash_triple[i] = hval;
            }
            ref = obase + tab_triple[hval];
            if (same) {
              distance = (int32_t) (anchor + triple_start - ref);
            } else {
              distance = 0;
            }
            if ((distance != 0) && (distance < MAX_DISTANCE)) {     // 3 rows match
              literal = false;
              uint8_t token = (uint8_t) ((21 << 3U) | i);
              *op++ = token;
              memcpy(op, &offset, 2);
              op += 2;
              for (int l = 0; l < 8; l++) {
                if ((l < i) || (l > i + 2)) {
                  memcpy(op, &buf_cell[l * cell_shape], cell_shape);
                  op += cell_shape;
                }
              }
              goto match;
            }
          }

          // rows pairs matches
          for (int i = 0; i < 7; i++) {
            int pair_start = i * cell_shape;
            hval = XXH32(&buf_cell[pair_start], 16, 1);        // calculate rows pair hash
            hval >>= 32U - 12U;
            ref = obase + tab_pair[hval];
            /* calculate distance to the match */
            bool same = true;
            uint16_t offset;
            if (tab_pair[hval] != 0) {
              buf_aux = obase + tab_pair[hval];
              for (int k = 0; k < 16; k++) {
                if (buf_cell[pair_start + k] != buf_aux[k]) {
                  same = false;
                  break;
                }
              }
              offset = (uint16_t) (anchor - obase - tab_pair[hval]);
            } else {
              same = false;
              update_pair[i] = (uint32_t) (anchor + 1 + pair_start - obase);     /* update hash table */
              hash_pair[i] = hval;
            }
            if (same) {
              distance = (int32_t) (anchor + pair_start - ref);
            } else {
              distance = 0;
            }
            if ((distance != 0) && (distance < MAX_DISTANCE)) {     /* 1 rows pair match */
              literal = false;
              uint8_t token = (uint8_t) ((17 << 3U) | i);
              *op++ = token;
              offset = (uint16_t) (anchor - obase - tab_pair[hval]);
              memcpy(op, &offset, 2);
              op += 2;
              for (int l = 0; l < 8; l++) {
                if ((l < i) || (l > i + 1)) {
                  memcpy(op, &buf_cell[l * cell_shape], cell_shape);
                  op += cell_shape;
                }
              }
              goto match;
            }
          }

          match:
          if (literal) {
            tab_cell[hash_cell] = (uint32_t) (anchor + 1 - obase);     /* update hash tables */

            if (update_triple[0] != 0) {
              for (int h = 0; h < 6; h++) {
                tab_triple[hash_triple[h]] = update_triple[h];
              }
            }
            if (update_pair[0] != 0) {
              for (int h = 0; h < 7; h++) {
                tab_pair[hash_pair[h]] = update_pair[h];
              }
            }
            uint8_t token = 0;
            *op++ = token;
            memcpy(op, buf_cell, cell_size);
            op += cell_size;

          }

        } else {   // cell match
          uint8_t token = (uint8_t) ((1U << 7U) | (1U << 6U));
          *op++ = token;
          uint16_t offset = (uint16_t) (anchor - obase - tab_cell[hash_cell]);
          memcpy(op, &offset, 2);
          op += 2;

        }

      }
      if ((op - obase) > input_len) {
        free(shape);
        free(chunkshape);
        free(blockshape);
        free(bufarea);
        BLOSC_TRACE_ERROR("Compressed data is bigger than input!");
        return 0;
      }
    }
  }

  free(shape);
  free(chunkshape);
  free(blockshape);
  free(bufarea);

  return (int) (op - obase);
}


// See https://habr.com/en/company/yandex/blog/457612/
#ifdef __AVX2__

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#endif
#endif
#define ALIGNED_TYPE_(t, x) t ALIGNED_(x)

static unsigned char* copy_match_16(unsigned char *op, const unsigned char *match, int32_t len)
{
  size_t offset = op - match;
  while (len >= 16) {

    static const ALIGNED_TYPE_(uint8_t, 16) masks[] =
      {
                0,  1,  2,  1,  4,  1,  4,  2,  8,  7,  6,  5,  4,  3,  2,  1, // offset = 0, not used as mask, but for shift
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // offset = 1
                0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,
                0,  1,  2,  0,  1,  2,  0,  1,  2,  0,  1,  2,  0,  1,  2,  0,
                0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,
                0,  1,  2,  3,  4,  5,  0,  1,  2,  3,  4,  5,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  5,  6,  0,  1,  2,  3,  4,  5,  6,  0,  1,
                0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  0,  1,  2,  3,  4,  5,  6,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  1,  2,  3,  4,  5,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,  0,  1,  2,  3,  4,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,  0,  1,  2,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,  0,  1,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  0,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  15, // offset = 16
      };

    _mm_storeu_si128((__m128i *)(op),
                     _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(match)),
                                      _mm_load_si128((const __m128i *)(masks) + offset)));

    match += masks[offset];

    op += 16;
    len -= 16;
  }
  // Deal with remainders
  for (; len > 0; len--) {
    *op++ = *match++;
  }
  return op;
}
#endif


int ndlz8_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                     uint8_t meta, blosc2_dparams *dparams) {
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_UNUSED_PARAM(dparams);
  BLOSC_ERROR_NULL(input, BLOSC2_ERROR_NULL_POINTER);
  BLOSC_ERROR_NULL(output, BLOSC2_ERROR_NULL_POINTER);

  const int cell_shape = 8;
  const int cell_size = 64;
  uint8_t *ip = (uint8_t *) input;
  uint8_t *ip_limit = ip + input_len;
  uint8_t *op = (uint8_t *) output;
  uint8_t ndim;
  int32_t blockshape[2];
  int32_t eshape[2];
  uint8_t *buffercpy;
  uint8_t token;
  if (NDLZ_UNEXPECT_CONDITIONAL(input_len < 8)) {
    return 0;
  }

  /* we start with literal copy */
  ndim = *ip;
  ip++;
  if (ndim != 2) {
    BLOSC_TRACE_ERROR("This codec only works for ndim = 2");
    return BLOSC2_ERROR_FAILURE;
  }
  memcpy(&blockshape[0], ip, 4);
  ip += 4;
  memcpy(&blockshape[1], ip, 4);
  ip += 4;

  // Sanity check.  See https://www.cve.org/CVERecord?id=CVE-2024-3203
  if (output_len < 0 || blockshape[0] < 0 || blockshape[1] < 0) {
    BLOSC_TRACE_ERROR("Output length or blockshape is negative");
    return BLOSC2_ERROR_FAILURE;
  }

  eshape[0] = ((blockshape[0] + 7) / cell_shape) * cell_shape;
  eshape[1] = ((blockshape[1] + 7) / cell_shape) * cell_shape;

  if (NDLZ_UNEXPECT_CONDITIONAL((int64_t)output_len < (int64_t)blockshape[0] * (int64_t)blockshape[1])) {
    BLOSC_TRACE_ERROR("The blockshape is bigger than the output buffer");
    return 0;
  }
  memset(op, 0, blockshape[0] * blockshape[1]);

  int32_t i_stop[2];
  for (int i = 0; i < 2; ++i) {
    i_stop[i] = eshape[i] / cell_shape;
  }

  /* main loop */
  int32_t ii[2];
  int32_t padding[2] = {0};
  int32_t ind = 0;
  uint8_t *local_buffer = malloc(cell_size);
  uint8_t *cell_aux = malloc(cell_size);
  for (ii[0] = 0; ii[0] < i_stop[0]; ++ii[0]) {
    for (ii[1] = 0; ii[1] < i_stop[1]; ++ii[1]) {      // for each cell
      if (NDLZ_UNEXPECT_CONDITIONAL(ip > ip_limit)) {
        free(local_buffer);
        free(cell_aux);
        BLOSC_TRACE_ERROR("Exceeding input length");
        return BLOSC2_ERROR_FAILURE;
      }
      if (ii[0] == i_stop[0] - 1) {
        padding[0] = (blockshape[0] % cell_shape == 0) ? cell_shape : blockshape[0] % cell_shape;
      } else {
        padding[0] = cell_shape;
      }
      if (ii[1] == i_stop[1] - 1) {
        padding[1] = (blockshape[1] % cell_shape == 0) ? cell_shape : blockshape[1] % cell_shape;
      } else {
        padding[1] = cell_shape;
      }
      token = *ip++;
      uint8_t match_type = (token >> 3U);
      if (token == 0) {    // no match
        buffercpy = ip;
        ip += padding[0] * padding[1];
      } else if (token == (uint8_t) ((1U << 7U) | (1U << 6U))) {  // cell match
        uint16_t offset = *((uint16_t *) ip);
        buffercpy = ip - offset - 1;
        ip += 2;
      } else if (token == (uint8_t) (1U << 6U)) { // whole cell of same element
        buffercpy = cell_aux;
        memset(buffercpy, *ip, cell_size);
        ip++;
      } else if (match_type == 21) {    // triple match
        buffercpy = local_buffer;
        int row = (int) (token & 7);
        uint16_t offset = *((uint16_t *) ip);
        ip += 2;
        for (int l = 0; l < 3; l++) {
          memcpy(&buffercpy[(row + l) * cell_shape],
                 ip - sizeof(token) - sizeof(offset) - offset + l * cell_shape, cell_shape);
        }
        for (int l = 0; l < cell_shape; l++) {
          if ((l < row) || (l > row + 2)) {
            memcpy(&buffercpy[l * cell_shape], ip, cell_shape);
            ip += cell_shape;
          }
        }
      } else if (match_type == 17) {    // pair match
        buffercpy = local_buffer;
        int row = (int) (token & 7);
        uint16_t offset = *((uint16_t *) ip);
        ip += 2;
        for (int l = 0; l < 2; l++) {
          memcpy(&buffercpy[(row + l) * cell_shape],
                 ip - sizeof(token) - sizeof(offset) - offset + l * cell_shape, cell_shape);
        }
        for (int l = 0; l < cell_shape; l++) {
          if ((l < row) || (l > row + 1)) {
            memcpy(&buffercpy[l * cell_shape], ip, cell_shape);
            ip += cell_shape;
          }
        }
      } else {
        free(local_buffer);
        free(cell_aux);
        BLOSC_TRACE_ERROR("Invalid token: %u at cell [%d, %d]\n", token, ii[0], ii[1]);
        return BLOSC2_ERROR_FAILURE;
      }

      int32_t orig = ii[0] * cell_shape * blockshape[1] + ii[1] * cell_shape;
      for (int32_t i = 0; i < (int32_t) cell_shape; i++) {
        if (i < padding[0]) {
          ind = orig + i * blockshape[1];
          memcpy(&op[ind], buffercpy, padding[1]);
        }
        buffercpy += padding[1];
      }
      if (ind > output_len) {
        free(local_buffer);
        free(cell_aux);
        BLOSC_TRACE_ERROR("Exceeding output size");
        return BLOSC2_ERROR_FAILURE;
      }
    }
  }
  ind += padding[1];

  free(cell_aux);
  free(local_buffer);

  if (ind != (blockshape[0] * blockshape[1])) {
    BLOSC_TRACE_ERROR("Output size is not compatible with embedded blockshape");
    return BLOSC2_ERROR_FAILURE;
  }
  if (ind > output_len) {
    BLOSC_TRACE_ERROR("Exceeding output size");
    return BLOSC2_ERROR_FAILURE;
  }

  return (int) ind;
}

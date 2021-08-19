/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  The code in this file is heavily based on FastLZ, a lightning-fast
  lossless compression library.  See LICENSES/FASTLZ.txt for details.
**********************************************************************/


#include <stdio.h>
#include <stdbool.h>
#include "blosclz.h"
#include "fastcopy.h"
#include "blosc2/blosc2-common.h"


/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__GNUC__) && (__GNUC__ > 2)
#define BLOSCLZ_LIKELY(c)    (__builtin_expect((c), 1))
#define BLOSCLZ_UNLIKELY(c)  (__builtin_expect((c), 0))
#else
#define BLOSCLZ_LIKELY(c)    (c)
#define BLOSCLZ_UNLIKELY(c)  (c)
#endif

/*
 * Use inlined functions for supported systems.
 */
#if defined(_MSC_VER) && !defined(__cplusplus)   /* Visual Studio */
#define inline __inline  /* Visual C is not C99, but supports some kind of inline */
#endif

#define MAX_COPY 32U
#define MAX_DISTANCE 8191
#define MAX_FARDISTANCE (65535 + MAX_DISTANCE - 1)

#ifdef BLOSC_STRICT_ALIGN
  #define BLOSCLZ_READU16(p) ((p)[0] | (p)[1]<<8)
  #define BLOSCLZ_READU32(p) ((p)[0] | (p)[1]<<8 | (p)[2]<<16 | (p)[3]<<24)
#else
  #define BLOSCLZ_READU16(p) *((const uint16_t*)(p))
  #define BLOSCLZ_READU32(p) *((const uint32_t*)(p))
#endif

#define HASH_LOG (14U)
#define HASH_LOG2 (12U)

// This is used in LZ4 and seems to work pretty well here too
#define HASH_FUNCTION(v, s, h) {      \
  v = (s * 2654435761U) >> (32U - h); \
}


#if defined(__AVX2__)
static uint8_t *get_run_32(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
    uint8_t x = ip[-1];

    while (ip < (ip_bound - (sizeof(__m256i)))) {
        __m256i value, value2, cmp;
        /* Broadcast the value for every byte in a 256-bit register */
        memset(&value, x, sizeof(__m256i));
        value2 = _mm256_loadu_si256((__m256i *)ref);
        cmp = _mm256_cmpeq_epi64(value, value2);
        if ((unsigned)_mm256_movemask_epi8(cmp) != 0xFFFFFFFF) {
            /* Return the byte that starts to differ */
            while (*ref++ == x) ip++;
            return ip;
        }
        else {
            ip += sizeof(__m256i);
            ref += sizeof(__m256i);
        }
    }
    /* Look into the remainder */
    while ((ip < ip_bound) && (*ref++ == x)) ip++;
    return ip;
}
#endif

#if defined(__SSE2__)
static uint8_t *get_run_16(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  uint8_t x = ip[-1];

  while (ip < (ip_bound - sizeof(__m128i))) {
    __m128i value, value2, cmp;
    /* Broadcast the value for every byte in a 128-bit register */
    memset(&value, x, sizeof(__m128i));
    value2 = _mm_loadu_si128((__m128i *)ref);
    cmp = _mm_cmpeq_epi32(value, value2);
    if (_mm_movemask_epi8(cmp) != 0xFFFF) {
      /* Return the byte that starts to differ */
      while (*ref++ == x) ip++;
      return ip;
    }
    else {
      ip += sizeof(__m128i);
      ref += sizeof(__m128i);
    }
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == x)) ip++;
  return ip;
}

#endif


static uint8_t *get_run(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  uint8_t x = ip[-1];
  int64_t value, value2;
  /* Broadcast the value for every byte in a 64-bit register */
  memset(&value, x, 8);
  /* safe because the outer check against ip limit */
  while (ip < (ip_bound - sizeof(int64_t))) {
#if defined(BLOSC_STRICT_ALIGN)
    memcpy(&value2, ref, 8);
#else
    value2 = ((int64_t*)ref)[0];
#endif
    if (value != value2) {
      /* Return the byte that starts to differ */
      while (*ref++ == x) ip++;
      return ip;
    }
    else {
      ip += 8;
      ref += 8;
    }
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == x)) ip++;
  return ip;
}


/* Return the byte that starts to differ */
static uint8_t *get_match(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
#if !defined(BLOSC_STRICT_ALIGN)
  while (ip < (ip_bound - sizeof(int64_t))) {
    if (*(int64_t*)ref != *(int64_t*)ip) {
      /* Return the byte that starts to differ */
      while (*ref++ == *ip++) {}
      return ip;
    }
    else {
      ip += sizeof(int64_t);
      ref += sizeof(int64_t);
    }
  }
#endif
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == *ip++)) {}
  return ip;
}


#if defined(__SSE2__)
static uint8_t *get_match_16(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  __m128i value, value2, cmp;

  while (ip < (ip_bound - sizeof(__m128i))) {
    value = _mm_loadu_si128((__m128i *) ip);
    value2 = _mm_loadu_si128((__m128i *) ref);
    cmp = _mm_cmpeq_epi32(value, value2);
    if (_mm_movemask_epi8(cmp) != 0xFFFF) {
      /* Return the byte that starts to differ */
      while (*ref++ == *ip++) {}
      return ip;
    }
    else {
      ip += sizeof(__m128i);
      ref += sizeof(__m128i);
    }
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == *ip++)) {}
  return ip;
}
#endif


#if defined(__AVX2__)
static uint8_t *get_match_32(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {

  while (ip < (ip_bound - sizeof(__m256i))) {
    __m256i value, value2, cmp;
    value = _mm256_loadu_si256((__m256i *) ip);
    value2 = _mm256_loadu_si256((__m256i *)ref);
    cmp = _mm256_cmpeq_epi64(value, value2);
    if ((unsigned)_mm256_movemask_epi8(cmp) != 0xFFFFFFFF) {
      /* Return the byte that starts to differ */
      while (*ref++ == *ip++) {}
      return ip;
    }
    else {
      ip += sizeof(__m256i);
      ref += sizeof(__m256i);
    }
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == *ip++)) {}
  return ip;
}
#endif


static uint8_t* get_run_or_match(uint8_t* ip, uint8_t* ip_bound, const uint8_t* ref, bool run) {
  if (BLOSCLZ_UNLIKELY(run)) {
#if defined(__AVX2__)
    // Extensive experiments on AMD Ryzen3 say that regular get_run is faster
    // ip = get_run_32(ip, ip_bound, ref);
    ip = get_run(ip, ip_bound, ref);
#elif defined(__SSE2__)
    // Extensive experiments on AMD Ryzen3 say that regular get_run is faster
    // ip = get_run_16(ip, ip_bound, ref);
    ip = get_run(ip, ip_bound, ref);
#else
    ip = get_run(ip, ip_bound, ref);
#endif
  }
  else {
#if defined(__AVX2__)
    // Extensive experiments on AMD Ryzen3 say that regular get_match_16 is faster
    // ip = get_match_32(ip, ip_bound, ref);
    ip = get_match_16(ip, ip_bound, ref);
#elif defined(__SSE2__)
    ip = get_match_16(ip, ip_bound, ref);
#else
    ip = get_match(ip, ip_bound, ref);
#endif
  }

  return ip;
}


#define LITERAL(ip, op, op_limit, anchor, copy) {       \
  if (BLOSCLZ_UNLIKELY(op + 2 > op_limit))              \
    goto out;                                           \
  *op++ = *anchor++;                                    \
  ip = anchor;                                          \
  copy++;                                               \
  if (BLOSCLZ_UNLIKELY(copy == MAX_COPY)) {             \
    copy = 0;                                           \
    *op++ = MAX_COPY-1;                                 \
  }                                                     \
}

#define LITERAL2(ip, oc, anchor, copy) {                \
  oc++; anchor++;                                       \
  ip = anchor;                                          \
  copy++;                                               \
  if (BLOSCLZ_UNLIKELY(copy == MAX_COPY)) {             \
    copy = 0;                                           \
    oc++;                                               \
  }                                                     \
}

#define MATCH_SHORT(op, op_limit, len, distance) {      \
  if (BLOSCLZ_UNLIKELY(op + 2 > op_limit))              \
    goto out;                                           \
  *op++ = (uint8_t)((len << 5U) + (distance >> 8U));    \
  *op++ = (uint8_t)((distance & 255U));                 \
}

#define MATCH_LONG(op, op_limit, len, distance) {       \
  if (BLOSCLZ_UNLIKELY(op + 1 > op_limit))              \
    goto out;                                           \
  *op++ = (uint8_t)((7U << 5U) + (distance >> 8U));     \
  for (len -= 7; len >= 255; len -= 255) {              \
    if (BLOSCLZ_UNLIKELY(op + 1 > op_limit))            \
      goto out;                                         \
    *op++ = 255;                                        \
  }                                                     \
  if (BLOSCLZ_UNLIKELY(op + 2 > op_limit))              \
    goto out;                                           \
  *op++ = (uint8_t)len;                                 \
  *op++ = (uint8_t)((distance & 255U));                 \
}

#define MATCH_SHORT_FAR(op, op_limit, len, distance) {      \
  if (BLOSCLZ_UNLIKELY(op + 4 > op_limit))                  \
    goto out;                                               \
  *op++ = (uint8_t)((len << 5U) + 31);                      \
  *op++ = 255;                                              \
  *op++ = (uint8_t)(distance >> 8U);                        \
  *op++ = (uint8_t)(distance & 255U);                       \
}

#define MATCH_LONG_FAR(op, op_limit, len, distance) {       \
  if (BLOSCLZ_UNLIKELY(op + 1 > op_limit))                  \
    goto out;                                               \
  *op++ = (7U << 5U) + 31;                                  \
  for (len -= 7; len >= 255; len -= 255) {                  \
    if (BLOSCLZ_UNLIKELY(op + 1 > op_limit))                \
      goto out;                                             \
    *op++ = 255;                                            \
  }                                                         \
  if (BLOSCLZ_UNLIKELY(op + 4 > op_limit))                  \
    goto out;                                               \
  *op++ = (uint8_t)len;                                     \
  *op++ = 255;                                              \
  *op++ = (uint8_t)(distance >> 8U);                        \
  *op++ = (uint8_t)(distance & 255U);                       \
}


// Get the compressed size of a buffer.  Useful for testing compression ratios for high clevels.
static int get_csize(uint8_t* ibase, int maxlen, int minlen, int clevel, int ipshift) {
  uint32_t maxlen8 = maxlen / 8;
  uint8_t* ip = ibase;
  int32_t oc = 0;
  uint8_t* ip_bound = ibase + maxlen - 1;
  uint8_t* ip_limit = ibase + maxlen - 12;
  uint32_t htab[1U << (uint8_t)HASH_LOG2];
  uint32_t hval;
  uint32_t seq;
  uint8_t copy;

  // Initialize the hash table to distances of 0
  memset(htab, 0, (1U << HASH_LOG2) * sizeof(uint32_t));

  /* we start with literal copy */
  copy = 4;
  oc += 5;

  /* main loop */
  while (BLOSCLZ_LIKELY(ip < ip_limit)) {
    const uint8_t* ref;
    unsigned distance;
    uint8_t* anchor = ip;    /* comparison starting-point */

    /* find potential match */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, HASH_LOG2)
    ref = ibase + htab[hval];

    /* calculate distance to the match */
    distance = (unsigned int)(anchor - ref);

    /* update hash table */
    htab[hval] = (uint32_t) (anchor - ibase);

    if (distance == 0 || (distance >= MAX_FARDISTANCE)) {
      LITERAL2(ip, oc, anchor, copy)
      continue;
    }

    /* is this a match? check the first 4 bytes */
    if (BLOSCLZ_READU32(ref) == BLOSCLZ_READU32(ip)) {
      ref += 4;
    }
    else {
      /* no luck, copy as a literal */
      LITERAL2(ip, oc, anchor, copy)
      continue;
    }

    /* last matched byte */
    ip = anchor + 4;

    /* distance is biased */
    distance--;

    /* get runs or matches; zero distance means a run */
    ip = get_run_or_match(ip, ip_bound, ref, !distance);

    ip -= ipshift;
    unsigned len = (int)(ip - anchor);
    if (len < minlen) {
      LITERAL2(ip, oc, anchor, copy)
      continue;
    }

    /* if we have'nt copied anything, adjust the output counter */
    if (!copy)
      oc--;
    /* reset literal counter */
    copy = 0;

    /* encode the match */
    if (distance < MAX_DISTANCE) {
      if (len >= 7) {
        oc += ((len - 7) / 255) + 1;
      }
      oc += 2;
    }
    else {
      /* far away, but not yet in the another galaxy... */
      if (len >= 7) {
        oc += ((len - 7) / 255) + 1;
      }
      oc += 4;
    }

    /* update the hash at match boundary */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, HASH_LOG2)
    htab[hval] = (uint32_t)(ip++ - ibase);
    seq >>= 8U;
    HASH_FUNCTION(hval, seq, HASH_LOG2)
    htab[hval] = (uint32_t) (ip++ - ibase);
    /* assuming literal copy */
    oc++;

    int32_t ic = (int32_t)(ip - ibase);
    if (clevel == 9) {
      // Exit as soon as we have a decent sample to compare with
      if ((ic > maxlen8) || (ic > 4096)) {
        return oc;
      }
    }
    else {
      // Exit early if we are detecting compression.
      // We cannot do that for clevel==9 because we need a better
      // assessment because we are comparing different minlen's.
      if ((ic > maxlen8) && (oc < 2 * ic)) {
        return oc;
      }
    }

  }

  return (int)oc;
}


int blosclz_compress(const int clevel, const void* input, int length,
                     void* output, int maxout) {
  uint8_t* ibase = (uint8_t*)input;
  uint8_t* ip = ibase;
  uint8_t* ip_bound = ibase + length - 1;
  uint8_t* ip_limit = ibase + length - 12;
  uint8_t* op = (uint8_t*)output;
  uint8_t* op_limit;
  uint32_t htab[1U << (uint8_t)HASH_LOG];
  uint32_t hval;
  uint32_t seq;
  uint8_t copy;

  op_limit = op + maxout;

  // Minimum lengths for encoding
  // The next parameters have been extensively fine tuned for large and small cratios
  unsigned minlen_[10] = {0, 16, 16, 16, 3, 3, 3, 3, 3, 3};

  // Minimum compression ratios for initiating encoding
  // The next parameters have been extensively fine tuned for large and small cratios
  double cratio_[10] = {0, 4, 4, 4, 3, 3, 1.6, 1.4, 1.2, 1.1};

  uint8_t hashlog_[10] = {0, HASH_LOG, HASH_LOG, HASH_LOG, HASH_LOG,
                          HASH_LOG, HASH_LOG, HASH_LOG, HASH_LOG, HASH_LOG};
  uint8_t hashlog = hashlog_[clevel];
  // Initialize the hash table to distances of 0
  memset(htab, 0, (1U << hashlog) * sizeof(uint32_t));

  /* input and output buffer cannot be less than 16 and 66 bytes or we can get into trouble */
  if (length < 16 || maxout < 66) {
    return 0;
  }

  /* When we go back in a match (shift), we obtain quite different compression properties.
   * It looks like 4 is more useful in combination with bitshuffle and small typesizes
   * Fallback to 3 because it provides more consistent results on small and large cratios.
   *
   * In this block we also check cratios for the beginning of the buffers and
   * eventually discard those that are small (take too long to decompress).
   * This process is called _entropy probing_.
   */
  const int ipshift = 3;
  int maxlen;  // maximum length for entropy probing
  int csize;
  int csize_4;
  double cratio = 0;
  unsigned minlen = minlen_[clevel];
  switch (clevel) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      maxlen = length / 8;
      csize = get_csize(ibase, maxlen, minlen, clevel, ipshift);
      cratio = (double)maxlen / csize;
      break;
    case 6:
    case 7:
    case 8:
      maxlen = length / 4;
      csize = get_csize(ibase, maxlen, minlen, clevel, ipshift);
      cratio = (double)maxlen / csize;
      //printf("cr: %.2f; ", cratio);
      //cratio = 3;
      break;
    case 9:
      maxlen = length / 4;
      // case 9 is special.  we need to assess the optimal minlen (3 o 4).
      // maxlen can be quite less here because the blocksize is larger.
      csize_4 = get_csize(ibase, maxlen, 4, clevel, ipshift);
      csize = get_csize(ibase, maxlen, 3, clevel, ipshift);
      minlen = (csize_4 <= csize) ? 4 : 3;
      cratio = (csize_4 <= csize) ? ((double)maxlen / csize_4) : ((double)maxlen / csize);
      break;
    default:
      break;
  }
  // discard probes with small compression ratios (too expensive)
  if (cratio < cratio_[clevel]) {
    goto out;
  }

  /* we start with literal copy */
  copy = 4;
  *op++ = MAX_COPY - 1;
  *op++ = *ip++;
  *op++ = *ip++;
  *op++ = *ip++;
  *op++ = *ip++;

  /* main loop */
  while (BLOSCLZ_LIKELY(ip < ip_limit)) {
    const uint8_t* ref;
    unsigned distance;
    uint8_t* anchor = ip;    /* comparison starting-point */

    /* find potential match */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, hashlog)
    ref = ibase + htab[hval];

    /* calculate distance to the match */
    distance = (unsigned int)(anchor - ref);

    /* update hash table */
    htab[hval] = (uint32_t) (anchor - ibase);

    if (distance == 0 || (distance >= MAX_FARDISTANCE)) {
      LITERAL(ip, op, op_limit, anchor, copy)
      continue;
    }

    /* is this a match? check the first 4 bytes */
    if (BLOSCLZ_UNLIKELY(BLOSCLZ_READU32(ref) == BLOSCLZ_READU32(ip))) {
      ref += 4;
    } else {
      /* no luck, copy as a literal */
      LITERAL(ip, op, op_limit, anchor, copy)
      continue;
    }

    /* last matched byte */
    ip = anchor + 4;

    /* distance is biased */
    distance--;

    /* get runs or matches; zero distance means a run */
    ip = get_run_or_match(ip, ip_bound, ref, !distance);

    /* length is biased, '1' means a match of 3 bytes */
    ip -= ipshift;

    unsigned len = (int)(ip - anchor);

    // Encoding short lengths is expensive during decompression
    if (len < minlen) {
      LITERAL(ip, op, op_limit, anchor, copy)
      continue;
    }

    /* if we have copied something, adjust the copy count */
    if (copy)
      /* copy is biased, '0' means 1 byte copy */
      *(op - copy - 1) = (uint8_t)(copy - 1);
    else
      /* back, to overwrite the copy count */
      op--;
    /* reset literal counter */
    copy = 0;

    /* encode the match */
    if (distance < MAX_DISTANCE) {
      if (len < 7) {
        MATCH_SHORT(op, op_limit, len, distance)
      } else {
        MATCH_LONG(op, op_limit, len, distance)
      }
    } else {
      /* far away, but not yet in the another galaxy... */
      distance -= MAX_DISTANCE;
      if (len < 7) {
        MATCH_SHORT_FAR(op, op_limit, len, distance)
      } else {
        MATCH_LONG_FAR(op, op_limit, len, distance)
      }
    }

    /* update the hash at match boundary */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, hashlog)
    htab[hval] = (uint32_t) (ip++ - ibase);
    seq >>= 8U;
    HASH_FUNCTION(hval, seq, hashlog)
    htab[hval] = (uint32_t) (ip++ - ibase);
    /* assuming literal copy */

    if (BLOSCLZ_UNLIKELY(op + 1 > op_limit))
      goto out;
    *op++ = MAX_COPY - 1;
  }

  /* left-over as literal copy */
  while (BLOSCLZ_UNLIKELY(ip <= ip_bound)) {
    if (BLOSCLZ_UNLIKELY(op + 2 > op_limit)) goto out;
    *op++ = *ip++;
    copy++;
    if (BLOSCLZ_UNLIKELY(copy == MAX_COPY)) {
      copy = 0;
      *op++ = MAX_COPY - 1;
    }
  }

  /* if we have copied something, adjust the copy length */
  if (copy)
    *(op - copy - 1) = (uint8_t)(copy - 1);
  else
    op--;

  /* marker for blosclz */
  *(uint8_t*)output |= (1U << 5U);

  return (int)(op - (uint8_t*)output);

  out:
  return 0;
}

// See https://habr.com/en/company/yandex/blog/457612/
#if defined(__AVX2__)

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

// LZ4 wildCopy which can reach excellent copy bandwidth (even if insecure)
static inline void wild_copy(uint8_t *out, const uint8_t* from, uint8_t* end) {
  uint8_t* d = out;
  const uint8_t* s = from;
  uint8_t* const e = end;

  do { memcpy(d,s,8); d+=8; s+=8; } while (d<e);
}

int blosclz_decompress(const void* input, int length, void* output, int maxout) {
  const uint8_t* ip = (const uint8_t*)input;
  const uint8_t* ip_limit = ip + length;
  uint8_t* op = (uint8_t*)output;
  uint32_t ctrl;
  uint8_t* op_limit = op + maxout;
  if (BLOSCLZ_UNLIKELY(length == 0)) {
    return 0;
  }
  ctrl = (*ip++) & 31U;

  while (1) {
    if (ctrl >= 32) {
      // match
      int32_t len = (ctrl >> 5U) - 1 ;
      int32_t ofs = (ctrl & 31U) << 8U;
      uint8_t code;
      const uint8_t* ref = op - ofs;

      if (len == 7 - 1) {
        do {
          if (BLOSCLZ_UNLIKELY(ip + 1 >= ip_limit)) {
            return 0;
          }
          code = *ip++;
          len += code;
        } while (code == 255);
      }
      else {
        if (BLOSCLZ_UNLIKELY(ip + 1 >= ip_limit)) {
          return 0;
        }
      }
      code = *ip++;
      len += 3;
      ref -= code;

      /* match from 16-bit distance */
      if (BLOSCLZ_UNLIKELY(code == 255)) {
        if (ofs == (31U << 8U)) {
          if (ip + 1 >= ip_limit) {
            return 0;
          }
          ofs = (*ip++) << 8U;
          ofs += *ip++;
          ref = op - ofs - MAX_DISTANCE;
        }
      }

      if (BLOSCLZ_UNLIKELY(op + len > op_limit)) {
        return 0;
      }

      if (BLOSCLZ_UNLIKELY(ref - 1 < (uint8_t*)output)) {
        return 0;
      }

      if (BLOSCLZ_UNLIKELY(ip >= ip_limit)) break;
      ctrl = *ip++;

      ref--;
      if (ref == op - 1) {
        /* optimized copy for a run */
        memset(op, *ref, len);
        op += len;
      }
      else if ((op - ref >= 8) && (op_limit - op >= len + 8)) {
        // copy with an overlap not larger than 8
        wild_copy(op, ref, op + len);
        op += len;
      }
      else {
        // general copy with any overlap
#if defined(__AVX2__)
        if (op - ref <= 16) {
          // This is not faster on a combination of compilers (clang, gcc, icc) or machines, but
          // it is not slower either.  Let's activate here for experimentation.
          op = copy_match_16(op, ref, len);
        }
        else {
#endif
          op = copy_match(op, ref, (unsigned) len);
#if defined(__AVX2__)
        }
#endif
      }
    }
    else {
      // literal
      ctrl++;
      if (BLOSCLZ_UNLIKELY(op + ctrl > op_limit)) {
        return 0;
      }
      if (BLOSCLZ_UNLIKELY(ip + ctrl > ip_limit)) {
        return 0;
      }

      memcpy(op, ip, ctrl); op += ctrl; ip += ctrl;
      // On GCC-6, fastcopy this is still faster than plain memcpy
      // However, using recent CLANG/LLVM 9.0, there is almost no difference
      // in performance.
      // And starting on CLANG/LLVM 10 and GCC 9, memcpy is generally faster.
      // op = fastcopy(op, ip, (unsigned) ctrl); ip += ctrl;

      if (BLOSCLZ_UNLIKELY(ip >= ip_limit)) break;
      ctrl = *ip++;
    }
  }

  return (int)(op - (uint8_t*)output);
}

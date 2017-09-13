#include "blosclz2.h"

//
// Config
//

// MATCH_DIST cannot exceed 0xffffu
// lowering this improves compression speed but potentially reduces compression ratio
// MAX_MATCH_DIST has a minor effect on the speed of
// blosclz2_compress()
#define MAX_MATCH_DIST 0xffffU

// number of bytes required = DICT_SIZE * sizeof( blosclz2_dict_entry )
// raising this can increase compression ratio slightly, but has a huge impact
// on the amount of working memory required.
// The default value (0xffffU) requires 256KB working memory.
#define DICT_SIZE 0xffffU

#if !defined(SPU) && !defined(BLOSCLZ2_NO_UNALIGNED_ACCESS)
#define BLOSCLZ2_UNALIGNED_ACCESS
#endif

//
// End Config
//

#define BLOCKSIZE           4
#define MIN_MATCH_LEN       (BLOCKSIZE + 1)
#define MAX_MATCH_LEN       (( 0xffU-1 ) + MIN_MATCH_LEN)

// The maximum length of uncompressible data, if this limit is reached,
// another block must be emitted.  In practice, raising this helps ratio a very
// slight amount, but is not worth the cost of making our compression block
// bigger.
#define MAX_SEQUENTIAL_LITERALS 0xffU

typedef uintptr_t ireg_t;
typedef intptr_t ureg_t;

#define LOG2_8BIT(v)  ( 8 - 90/(((v)/4+14)|1) - 2/((v)/2+1) )
#define LOG2_16BIT(v) ( 8*((v)>255) + LOG2_8BIT((v) >>8*((v)>255)) )
#define LOG2_32BIT(v) ( 16*((v)>65535L) + LOG2_16BIT((v)*1L >>16*((v)>65535L)) )
#define HASH_SHIFT    ( 31 - LOG2_32BIT( DICT_SIZE ) )

#define HASHPTR(x)  (( *( (uint32_t*)(x) ) * 2654435761U )  >> (HASH_SHIFT ))


typedef struct blosclz2_block {
  uint16_t dist;
  uint8_t length;
  uint8_t nliterals;  // how many literals are there until the next block
} blosclz2_block;

typedef struct blosclz2_header {
  int32_t compressed_size;
  int32_t decompressed_size;
  blosclz2_block first_block;
} blosclz2_header;

typedef struct blosclz2_dict_entry {
  const uint8_t *inPos;
} blosclz2_dict_entry;


ureg_t blosclz2_memcmp(const uint8_t *a, const uint8_t *b, ureg_t maxlen);

void blosclz2_memcpy(uint8_t *dst, const uint8_t *src, int32_t size);

void blosclz2_memset(uint8_t *dst, uint8_t value, int32_t size);


/* for big/little endian detection */
static int is_big_endian(void) {
  int i = 1;
  char *p = (char *) &i;
  return p[0] != 1 ? 1 : 0;
}


void endian_swap16(uint16_t *data) {
  *data = ((*data & 0xFF00) >> 8) | ((*data & 0x00FF) << 8);
}

void endian_swap32(int32_t *data) {
  *data = ((*data & 0xFF000000) >> 24) |
          ((*data & 0x00FF0000) >> 8) |
          ((*data & 0x0000FF00) << 8) |
          ((*data & 0x000000FF) << 24);
}

#ifndef NULL
#define NULL 0
#endif

//#define BLOSCLZ2_DBG	// writes compression and decompression logs -- if
// everything is working, these should match!

#ifdef BLOSCLZ2_DBG
#include <stdio.h>
#define BLOSCLZ2_DBG_COMPRESS_INIT      FILE* dbgFh = fopen( "compress.txt", "wb" );
#define BLOSCLZ2_DBG_DECOMPRESS_INIT    FILE* dbgFh = fopen( "decompress.txt", "wb" );
#define BLOSCLZ2_DBG_PRINT( ... )       fprintf( dbgFh, __VA_ARGS__ ); fflush( dbgFh );
#define BLOSCLZ2_DBG_SHUTDOWN           fclose( dbgFh );
#else
#define BLOSCLZ2_DBG_COMPRESS_INIT
#define BLOSCLZ2_DBG_DECOMPRESS_INIT
#define BLOSCLZ2_DBG_PRINT(...)
#define BLOSCLZ2_DBG_SHUTDOWN
#endif


uint32_t blosclz2_get_workdict_size() {
  return (DICT_SIZE + 1) * sizeof(blosclz2_dict_entry);
}


int32_t blosclz2_compress(const uint8_t *in, const int32_t length,
                          uint8_t *out, const int32_t maxout,
                          uint8_t *workdict) {
  blosclz2_header header;
  blosclz2_block *block = &header.first_block;
  uint8_t *dst = out + sizeof(blosclz2_header);
  uint32_t compressed_size = MIN_MATCH_LEN + sizeof(blosclz2_header);
  const uint8_t *src = in;
  ureg_t bytes_left = (ureg_t) length;
  ureg_t nliterals;
  blosclz2_dict_entry *dict = (blosclz2_dict_entry *) workdict;
  const int swap_endian = is_big_endian();

  block->dist = block->length = 0;

  BLOSCLZ2_DBG_COMPRESS_INIT

  BLOSCLZ2_DBG_PRINT("blosclz2_compress( %u )\n", length);

  // init dictionary
  blosclz2_memset((uint8_t *) dict, 0, blosclz2_get_workdict_size());

  // starting literal characters
  {
    const uint8_t *literals_end = src +
                                  (MIN_MATCH_LEN > bytes_left ? bytes_left
                                                              : MIN_MATCH_LEN);
    for (; src != literals_end; ++src, ++dst, --bytes_left) {
      if (bytes_left >= 4) {
        const uint32_t hash = HASHPTR(src);
        dict[hash].inPos = src;
      }
      *dst = *src;
      BLOSCLZ2_DBG_PRINT("  literal [0x%02X] [%c]\n", *src, *src);
    }
    nliterals = src - in;
  }

  // iterate through input bytes
  while (bytes_left >= MIN_MATCH_LEN) {
    const uint32_t hash = HASHPTR(src);
    const uint8_t **dict_entry = &dict[hash].inPos;
    const uint8_t *matchpos = *dict_entry;
    const uint8_t *window_start = src - MAX_MATCH_DIST;
    ureg_t matchLength = 0;
    const ureg_t maxMatchLen =
            MAX_MATCH_LEN > bytes_left ? bytes_left : MAX_MATCH_LEN;

    *dict_entry = src;

    // a match was found, ensure it really is a match and not a hash collision, and determine its length
    if (matchpos != NULL && matchpos >= window_start) {
      matchLength = blosclz2_memcmp(src, matchpos, maxMatchLen);
    }
    if (matchLength >= MIN_MATCH_LEN) {
      const int32_t matchDist = (int32_t) (src - matchpos);

      block->nliterals = (uint8_t) nliterals;
      if (swap_endian != 0) { endian_swap16(&block->dist); }
      block = (blosclz2_block *) dst;
      bytes_left -= matchLength;
      dst += BLOCKSIZE;
      if (dst - out > maxout) return -1;
      src += matchLength;
      block->dist = (uint16_t) matchDist;
      block->length = (uint8_t) (matchLength - MIN_MATCH_LEN + 1);
      nliterals = 0;

      BLOSCLZ2_DBG_PRINT("  backtrack [%u] len [%u]\n", matchDist, matchLength);
#ifdef BLOSCLZ2_DBG
      blosclz2_totalBackTrackDist += matchDist;
      blosclz2_totalBackTrackLength += matchLength;
      ++blosclz2_numBackTracks;
#endif

      compressed_size += BLOCKSIZE;
    }

      // output a literal byte: no entries for this position found, entry is too
      // far away, entry was a hash collision, or the entry did not meet the
      // minimum match length
    else {
      // if we've hit the max number of sequential literals, we need to output
      // a compression block header
      if (nliterals == MAX_SEQUENTIAL_LITERALS) {
        block->nliterals = (uint8_t) nliterals;
        if (swap_endian) {
          endian_swap16(&block->dist);
        }
        block = (blosclz2_block *) dst;
        dst += BLOCKSIZE;
        if (dst - out > maxout) return -1;
        block->dist = block->length = 0;
        nliterals = 0;
        compressed_size += BLOCKSIZE;
      }

      ++nliterals;
      --bytes_left;
      BLOSCLZ2_DBG_PRINT("  literal [0x%02X] [%c]\n", *src, *src);
      *dst++ = *src++;
      if (dst - out > maxout) return -1;
      ++compressed_size;
    }
  }
  // output final few bytes as literals, these are not worth compressing
  while (bytes_left) {
    // if we've hit the max number of sequential literals, we need to output
    // a compression block header
    if (nliterals == MAX_SEQUENTIAL_LITERALS) {
      block->nliterals = (uint8_t) nliterals;
      if (swap_endian != 0) { endian_swap16(&block->dist); }
      block = (blosclz2_block *) dst;
      dst += BLOCKSIZE;
      if (dst - out > maxout) return -1;
      block->dist = block->length = 0;
      nliterals = 0;
      compressed_size += BLOCKSIZE;
    }

    ++nliterals;
    --bytes_left;
    BLOSCLZ2_DBG_PRINT("  literal [0x%02X] [%c]\n", *src, *src);
    *dst++ = *src++;
    if (dst - out > maxout) return -1;
    ++compressed_size;
  }

  // append the 'end' block
  {
    block->nliterals = (uint8_t) nliterals;
    if (swap_endian) {
      endian_swap16(&block->dist);
    }
    block = (blosclz2_block *) dst;
    dst += BLOCKSIZE;
    if (dst - out > maxout) return -1;
    block->dist = block->length = block->nliterals = 0;
    compressed_size += BLOCKSIZE;
  }

  // save the header
  header.compressed_size = compressed_size;
  header.decompressed_size = length;
  if (swap_endian) {
    endian_swap32(&header.compressed_size);
    endian_swap32(&header.decompressed_size);
  }
  *((blosclz2_header *) out) = header;

  BLOSCLZ2_DBG_SHUTDOWN

  return compressed_size;
}


int32_t blosclz2_decompress(const uint8_t *in, uint8_t *out, int32_t maxout) {
  const int swap_endian = is_big_endian();
  blosclz2_header *header = (blosclz2_header *) in;
  uint8_t *dst = out;
  const uint8_t *src = in + sizeof(blosclz2_header);
  ureg_t nliterals = header->first_block.nliterals;
  int32_t decompressed_size = header->decompressed_size;
  blosclz2_block *block;
  ureg_t len = 0;
  uint16_t dist = 0;

  BLOSCLZ2_DBG_DECOMPRESS_INIT
  BLOSCLZ2_DBG_PRINT("blosclz2_decompress()\n");

  if (swap_endian) endian_swap32(&decompressed_size);
  if (decompressed_size > maxout) return 0;

  for (;;) {
    // literals
    if (nliterals) {
#if 1
      do {
        BLOSCLZ2_DBG_PRINT("  literal [0x%02X] [%c]\n", *src, *src);
        *dst++ = *src++;
        --nliterals;
      } while (nliterals);
#else	// good if lots of uncompressible data, but there usually isn't
      blosclz2_memcpy( dst, src, nliterals );
      src += nliterals;
      dst += nliterals;
#endif
    } else if (dist == 0 && len == 0) // we've reached the end of the input
    {
      BLOSCLZ2_DBG_SHUTDOWN
      return decompressed_size;
    }

    // block header
    block = (blosclz2_block *) src;
    nliterals = block->nliterals;
#ifndef BLOSCLZ2_UNALIGNED_ACCESS
    ( (uint8_t*)&dist )[ 0 ] = ( (uint8_t*)&block->dist )[ 0 ];
    ( (uint8_t*)&dist )[ 1 ] = ( (uint8_t*)&block->dist )[ 1 ];
#else
    dist = block->dist;
#endif
    if (swap_endian) endian_swap16(&dist);
    len = (ureg_t) block->length;

    if (len != 0) {
#ifdef BLOSCLZ2_UNALIGNED_ACCESS
      const uint8_t *cpysrc = dst - dist;
      len += MIN_MATCH_LEN - 1;
      BLOSCLZ2_DBG_PRINT("  backtrack [%u] len [%u]\n", dist, len);
      if (len <= dist) // no overlap
      {
        const ureg_t nblocks = len / sizeof(ureg_t);
        ureg_t i;
        for (i = 0; i != nblocks; ++i) {
          *((ureg_t *) dst) = *((ureg_t *) cpysrc);
          dst += sizeof(ureg_t);
          cpysrc += sizeof(ureg_t);
        }
        switch (len % sizeof(ureg_t)) {
          // compilers smart enough to realize 32 bit ureg_t doesnt need cases 7-4 ?
          case 7:
            *dst++ = *cpysrc++;
          case 6:
            *dst++ = *cpysrc++;
          case 5:
            *dst++ = *cpysrc++;
            //case 4: *dst++ = *cpysrc++;
          case 4:
            *((uint32_t *) dst) = *((uint32_t *) cpysrc);
            dst += 4;
            cpysrc += 4;
            break;
          case 3:
            *dst++ = *cpysrc++;
          case 2:
            *dst++ = *cpysrc++;
          case 1:
            *dst++ = *cpysrc++;
          case 0:
            break;
        }
      } else {
        ureg_t n = (len + 7) / 8;
        switch (len % 8) {
          case 0:
            do {
              *dst++ = *cpysrc++;
              case 7:
                *dst++ = *cpysrc++;
              case 6:
                *dst++ = *cpysrc++;
              case 5:
                *dst++ = *cpysrc++;
              case 4:
                *dst++ = *cpysrc++;
              case 3:
                *dst++ = *cpysrc++;
              case 2:
                *dst++ = *cpysrc++;
              case 1:
                *dst++ = *cpysrc++;
            } while (--n > 0);
        }
      }
#else
      len += MIN_MATCH_LEN - 1;
      BLOSCLZ2_DBG_PRINT( "  backtrack [%u] len [%u]\n", dist, len );
      const uint8_t* cpysrc = dst - dist;
      ireg_t n = (len+7) / 8;
      switch( len % 8 )
      {
        case 0: do { *dst++ = *cpysrc++;
        case 7:      *dst++ = *cpysrc++;
        case 6:      *dst++ = *cpysrc++;
        case 5:      *dst++ = *cpysrc++;
        case 4:      *dst++ = *cpysrc++;
        case 3:      *dst++ = *cpysrc++;
        case 2:      *dst++ = *cpysrc++;
        case 1:      *dst++ = *cpysrc++;
        } while( --n > 0 );
      }
#endif
    }
    src += BLOCKSIZE;
  }
}


/*
 * Utility functions below, not exposed publicly
 */

/*
 * Returns the number of sequential matching characters.  Not the same as 
 * memcmp!!!
*/

ureg_t
blosclz2_memcmp(const uint8_t *a, const uint8_t *b, const ureg_t maxlen) {
#ifdef BLOSCLZ2_UNALIGNED_ACCESS
  ureg_t matched = 0;
  const ureg_t nblocks = maxlen / sizeof(ureg_t);
  ureg_t i;

  for (i = 0; i != nblocks; ++i) {
    if (*((ureg_t *) a) != *((ureg_t *) b)) {
      for (i = 0; i != (sizeof(ureg_t) - 1); ++i) {
        if (*a != *b) { break; }
        ++a;
        ++b;
        ++matched;
      }
      return matched;
    } else {
      matched += sizeof(ureg_t);
      a += sizeof(ureg_t);
      b += sizeof(ureg_t);
    }
  }
  {
    const ureg_t remain = maxlen % sizeof(ureg_t);
    for (i = 0; i != remain; ++i) {
      if (*a != *b) { break; }
      ++a;
      ++b;
      ++matched;
    }
  }
  return matched;
#else
  ureg_t matched = 0;
  while( *a++ == *b++ && matched < maxlen ) ++matched;
  return matched;
#endif
}


void blosclz2_memcpy(uint8_t *dst, const uint8_t *src, const int32_t size) {
#ifdef BLOSCLZ2_UNALIGNED_ACCESS
  int32_t n = (size + 7) / 8;
  switch (size % 8) {
    case 0:
      do {
        *dst++ = *src++;
        case 7:
          *dst++ = *src++;
        case 6:
          *dst++ = *src++;
        case 5:
          *dst++ = *src++;
        case 4:
          *dst++ = *src++;
        case 3:
          *dst++ = *src++;
        case 2:
          *dst++ = *src++;
        case 1:
          *dst++ = *src++;
      } while (--n > 0);
  }
#else
  for(int32_t i = 0; i != size; ++i ) *dst++ = *src++;
#endif
}


void blosclz2_memset(uint8_t *dst, const uint8_t value, const int32_t size) {
  for (int32_t i = 0; i != size; ++i) *dst++ = value;
}

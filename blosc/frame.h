/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FRAME_H
#define BLOSC_FRAME_H

#include <stdio.h>
#include <stdint.h>

#define FRAME_VERSION 0
#define HEADER2_MAGIC 2
#define HEADER2_LEN (HEADER2_MAGIC + 8 + 1)  // 11
#define FRAME_LEN (HEADER2_LEN + 4 + 1)  // 16
#define FRAME_FLAGS (FRAME_LEN + 8 + 1)  // 25
#define FRAME_FILTERS (FRAME_FLAGS + 1)  // 26
#define FRAME_COMPCODE (FRAME_FLAGS + 2)  // 27
#define FRAME_NBYTES (FRAME_FLAGS + 4 + 1)  // 30
#define FRAME_CBYTES (FRAME_NBYTES + 8 + 1)  // 39
#define FRAME_TYPESIZE (FRAME_CBYTES + 8 + 1) // 48
#define FRAME_CHUNKSIZE (FRAME_TYPESIZE + 4 + 1)  // 53
#define FRAME_NTHREADS_C (FRAME_CHUNKSIZE + 4 + 1)  // 58
#define FRAME_NTHREADS_D (FRAME_NTHREADS_C + 2 + 1)  // 61
#define FRAME_HAS_NSPACES (FRAME_NTHREADS_D + 2)  // 63
#define HEADER2_MINLEN (FRAME_HAS_NSPACES + 1)  // 64 <- minimum length
#define FRAME_NAMESPACES (FRAME_HAS_NSPACES + 1)  // 64
#define FRAME_IDX_SIZE (FRAME_NAMESPACES + 1 + 1)  // 66


void* frame_append_chunk(blosc2_frame* frame, void* chunk, blosc2_schunk* schunk);
int frame_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free);
int frame_decompress_chunk(blosc2_frame *frame, int nchunk, void *dest, size_t nbytes);

#endif //BLOSC_FRAME_H

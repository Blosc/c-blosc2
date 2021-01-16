/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FRAME_H
#define BLOSC_FRAME_H

#include <stdio.h>
#include <stdint.h>

// Constants for metadata placement in header
#define FRAME_HEADER_MAGIC 2
#define FRAME_HEADER_LEN (FRAME_HEADER_MAGIC + 8 + 1)  // 11
#define FRAME_LEN (FRAME_HEADER_LEN + 4 + 1)  // 16
#define FRAME_FLAGS (FRAME_LEN + 8 + 1)  // 25
#define FRAME_CODECS (FRAME_FLAGS + 2)  // 27
#define FRAME_NBYTES (FRAME_FLAGS + 4 + 1)  // 30
#define FRAME_CBYTES (FRAME_NBYTES + 8 + 1)  // 39
#define FRAME_TYPESIZE (FRAME_CBYTES + 8 + 1) // 48
#define FRAME_CHUNKSIZE (FRAME_TYPESIZE + 4 + 1)  // 53
#define FRAME_NTHREADS_C (FRAME_CHUNKSIZE + 4 + 1)  // 58
#define FRAME_NTHREADS_D (FRAME_NTHREADS_C + 2 + 1)  // 61
#define FRAME_HAS_USERMETA (FRAME_NTHREADS_D + 2)  // 63
#define FRAME_FILTER_PIPELINE (FRAME_HAS_USERMETA + 1 + 1) // 65
#define FRAME_HEADER_MINLEN (FRAME_FILTER_PIPELINE + 1 + 16)  // 82 <- minimum length
#define FRAME_METALAYERS (FRAME_HEADER_MINLEN)  // 82
#define FRAME_IDX_SIZE (FRAME_METALAYERS + 1 + 1)  // 84

#define FRAME_FILTER_PIPELINE_MAX (8)  // the maximum number of filters that can be stored in header

#define FRAME_TRAILER_VERSION_BETA2 (0U)  // for beta.2 and former
#define FRAME_TRAILER_VERSION (1U)        // can be up to 127

#define FRAME_TRAILER_USERMETA_LEN_OFFSET (3)  // offset to usermeta length
#define FRAME_TRAILER_USERMETA_OFFSET (7)  // offset to usermeta chunk
#define FRAME_TRAILER_MINLEN (30)  // minimum length for the trailer (msgpack overhead)
#define FRAME_TRAILER_LEN_OFFSET (22)  // offset to trailer length (counting from the end)

void* frame_append_chunk(blosc2_frame* frame, void* chunk, blosc2_schunk* schunk);
void* frame_insert_chunk(blosc2_frame* frame, int nchunk, void* chunk, blosc2_schunk* schunk);
void* frame_update_chunk(blosc2_frame* frame, int nchunk, void* chunk, blosc2_schunk* schunk);
int frame_reorder_offsets(blosc2_frame *frame, int *offsets_order, blosc2_schunk* schunk);

int frame_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free);
int frame_get_lazychunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free);
int frame_decompress_chunk(blosc2_context *dctx, blosc2_frame *frame, int nchunk,
                           void *dest, int32_t nbytes);

int frame_update_header(blosc2_frame* frame, blosc2_schunk* schunk, bool new);
int frame_update_trailer(blosc2_frame* frame, blosc2_schunk* schunk);

#endif //BLOSC_FRAME_H

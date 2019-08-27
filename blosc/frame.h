/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FRAME_H
#define BLOSC_FRAME_H

#include <stdio.h>
#include <stdint.h>

// Constants for metadata placement in header
#define FRAME_HEADER2_MAGIC 2
#define FRAME_HEADER2_LEN (FRAME_HEADER2_MAGIC + 8 + 1)  // 11
#define FRAME_LEN (FRAME_HEADER2_LEN + 4 + 1)  // 16
#define FRAME_FLAGS (FRAME_LEN + 8 + 1)  // 25
#define FRAME_FILTERS (FRAME_FLAGS + 1)  // 26
#define FRAME_CODECS (FRAME_FLAGS + 2)  // 27
#define FRAME_NBYTES (FRAME_FLAGS + 4 + 1)  // 30
#define FRAME_CBYTES (FRAME_NBYTES + 8 + 1)  // 39
#define FRAME_TYPESIZE (FRAME_CBYTES + 8 + 1) // 48
#define FRAME_CHUNKSIZE (FRAME_TYPESIZE + 4 + 1)  // 53
#define FRAME_NTHREADS_C (FRAME_CHUNKSIZE + 4 + 1)  // 58
#define FRAME_NTHREADS_D (FRAME_NTHREADS_C + 2 + 1)  // 61
#define FRAME_HAS_USERMETA (FRAME_NTHREADS_D + 2)  // 63
#define FRAME_HEADER2_MINLEN (FRAME_HAS_USERMETA + 1)  // 64 <- minimum length
#define FRAME_NAMESPACES (FRAME_HAS_USERMETA + 1)  // 64
#define FRAME_IDX_SIZE (FRAME_NAMESPACES + 1 + 1)  // 66

// Other constants
#define FRAME_FILTERS_PIPE_START_BIT (3U)
#define FRAME_FILTERS_PIPE_DESCRIBED_HERE (2U)  // 0b10
#define FRAME_FILTER_PIPE_DESCRIPTION (120U)  // 0b1111000
#define FRAME_HEADER_NFIELDS_NOMETALAYER (11)
#define FRAME_HEADER_NFIELDS_METALAYER (12)
#define FRAME_TRAILER_VERSION (0U)  // can be up to 127
#define FRAME_TRAILER_USERMETA_LEN_OFFSET (3)  // offset to usermeta length
#define FRAME_TRAILER_USERMETA_OFFSET (7)  // offset to usermeta chunk
#define FRAME_TRAILER_MIN_LENGTH (30)  // minimum length for the trailer (msgpack overhead)

#define FRAME_FILTER_PIPELINE_NAME "_filter_pipeline"

void* frame_append_chunk(blosc2_frame* frame, void* chunk, blosc2_schunk* schunk);
int frame_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free);
int frame_decompress_chunk(blosc2_frame *frame, int nchunk, void *dest, size_t nbytes);
int frame_update_metalayers(blosc2_frame* frame, blosc2_schunk* schunk, bool new);
int frame_update_trailer(blosc2_frame* frame, blosc2_schunk* schunk);

#endif //BLOSC_FRAME_H

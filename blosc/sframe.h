/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_SFRAME_H
#define BLOSC_SFRAME_H


void* sframe_create_chunk(blosc2_frame_s* frame, uint8_t* chunk, int32_t nchunk, int64_t cbytes);
int sframe_get_chunk(blosc2_frame_s* frame, int32_t nchunk, uint8_t** chunk, bool* needs_free);

#endif //BLOSC_SFRAME_H

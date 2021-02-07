/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_SFRAME_H
#define BLOSC_SFRAME_H

FILE* sframe_open_index(const char* urlpath, const char* mode);
FILE* sframe_open_chunk(const char* urlpath, int64_t nchunk, const char* mode);
void* sframe_create_chunk(blosc2_frame_s* frame, uint8_t* chunk, int32_t nchunk, int64_t cbytes);
int sframe_get_chunk(blosc2_frame_s* frame, int32_t nchunk, uint8_t** chunk, bool* needs_free);

#endif //BLOSC_SFRAME_H

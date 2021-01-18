/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_EFRAME_H
#define BLOSC_EFRAME_H


void* eframe_create_chunk(blosc2_frame* frame, uint8_t* chunk, int32_t nchunk, int64_t cbytes);
int eframe_get_chunk(blosc2_frame* frame, int32_t nchunk, uint8_t** chunk, bool* needs_free);

#endif //BLOSC_EFRAME_H
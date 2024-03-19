/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_SFRAME_H
#define BLOSC_SFRAME_H

#include "frame.h"

#include <stdbool.h>
#include <stdint.h>

void* sframe_open_index(const char* urlpath, const char* mode, const blosc2_io *io);
void* sframe_open_chunk(const char* urlpath, int64_t nchunk, const char* mode, const blosc2_io *io);
int sframe_delete_chunk(const char* urlpath, int64_t nchunk);
void* sframe_create_chunk(blosc2_frame_s* frame, uint8_t* chunk, int64_t nchunk, int64_t cbytes);
int32_t sframe_get_chunk(blosc2_frame_s* frame, int64_t nchunk, uint8_t** chunk, bool* needs_free);

#endif /* BLOSC_SFRAME_H */

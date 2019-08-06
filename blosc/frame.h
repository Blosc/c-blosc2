/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FRAME_H
#define BLOSC_FRAME_H

#include <stdio.h>
#include <stdint.h>

void* frame_append_chunk(blosc2_frame* frame, void* chunk, blosc2_schunk* schunk);


#endif //BLOSC_FRAME_H

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_DELTA_H
#define BLOSC_DELTA_H

int delta_encoder8(void* filters_chunk, int offset, int nbytes, unsigned char* src, unsigned char* dest);

int delta_decoder8(void* filters_chunk, int offset, int nbytes, unsigned char* src);

#endif //BLOSC_DELTA_H

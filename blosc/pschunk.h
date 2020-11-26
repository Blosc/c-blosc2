/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PSCHUNK_H
#define BLOSC_PSCHUNK_H


void* pschunk_append_chunk(blosc2_schunk *schunk, uint8_t *chunk);
int pschunk_get_chunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk, bool *needs_free);

int pschunk_update_header(blosc2_schunk *schunk);
int pschunk_new_trailer(blosc2_schunk* schunk);

#endif //BLOSC_PSCHUNK_H
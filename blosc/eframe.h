/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_EFRAME_H
#define BLOSC_EFRAME_H


void* eframe_append_chunk(blosc2_frame *frame, uint8_t *chunk, int32_t nchunk, int64_t cbytes);
int eframe_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free);

int eframe_update_header(blosc2_schunk *schunk);
/*int eframe_new_trailer(blosc2_schunk* schunk);

int eframe_get_header_info(blosc2_schunk *schunk, int32_t *header_len, int64_t *nbytes,
                           int64_t *cbytes, int32_t *chunksize, int32_t *nchunks, int32_t *typesize,
                           uint8_t *compcode, uint8_t *clevel, uint8_t *filters, uint8_t *filters_meta);
*/
#endif //BLOSC_EFRAME_H
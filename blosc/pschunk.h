//
// Created by Marta on 17/11/2020.
//

#include "blosc2.h"

#ifndef BLOSC_PSCHUNK_H
#define BLOSC_PSCHUNK_H

#endif //BLOSC_PSCHUNK_H

// Constants for metadata placement in header
#define PSCHUNK_HEADER_MAGIC 2
#define PSCHUNK_HEADER_LEN (PSCHUNK_HEADER_MAGIC + 8 + 1)  // 11
#define PSCHUNK_LEN (PSCHUNK_HEADER_LEN + 4 + 1)  // 16
#define PSCHUNK_FLAGS (PSCHUNK_LEN + 8 + 1)  // 25
#define PSCHUNK_CODECS (PSCHUNK_FLAGS + 2)  // 27
#define PSCHUNK_NBYTES (PSCHUNK_FLAGS + 4 + 1)  // 30
#define PSCHUNK_CBYTES (PSCHUNK_NBYTES + 8 + 1)  // 39
#define PSCHUNK_TYPESIZE (PSCHUNK_CBYTES + 8 + 1) // 48
#define PSCHUNK_CHUNKSIZE (PSCHUNK_TYPESIZE + 4 + 1)  // 53
#define PSCHUNK_NTHREADS_C (PSCHUNK_CHUNKSIZE + 4 + 1)  // 58
#define PSCHUNK_NTHREADS_D (PSCHUNK_NTHREADS_C + 2 + 1)  // 61
#define PSCHUNK_HAS_USERMETA (PSCHUNK_NTHREADS_D + 2)  // 63
#define PSCHUNK_FILTER_PIPELINE (PSCHUNK_HAS_USERMETA + 1 + 1) // 65
#define PSCHUNK_HEADER_MINLEN (PSCHUNK_FILTER_PIPELINE + 1 + 16)  // 82 <- minimum length
#define PSCHUNK_METALAYERS (PSCHUNK_HEADER_MINLEN)  // 82
#define PSCHUNK_IDX_SIZE (PSCHUNK_METALAYERS + 1 + 1)  // 84

#define PSCHUNK_FILTER_PIPELINE_MAX (8)  // the maximum number of filters that can be stored in header


int pschunk_get_chunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk, bool *needs_free);
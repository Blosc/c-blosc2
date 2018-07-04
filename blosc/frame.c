/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2018-07-04

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "schunk.h"
#include "context.h"

#if defined(_WIN32) && !defined(__MINGW32__)
#include <windows.h>
  #include <malloc.h>

/* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif

#endif  /* _WIN32 */

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#endif



/* Compute the length of a packed super-chunk */
int64_t blosc2_get_frame_length(blosc2_schunk *schunk) {
  int i;
  int64_t length = sizeof(blosc2_schunk);

  if (schunk->metadata_chunk != NULL)
    length += *(int32_t*)(schunk->metadata_chunk + 12);
  if (schunk->userdata_chunk != NULL)
    length += *(int32_t*)(schunk->userdata_chunk + 12);
  if (schunk->data != NULL) {
    for (i = 0; i < schunk->nchunks; i++) {
      length += sizeof(int64_t);
      length += *(int32_t*)(schunk->data[i] + 12);
    }
  }
  return length;
}


/* Create a frame out of a super-chunk */
void* blosc2_make_frame(blosc2_schunk *schunk) {
  int64_t cbytes = sizeof(blosc2_schunk);
  int64_t nbytes = sizeof(blosc2_schunk);
  int64_t nchunks = schunk->nchunks;
  void* packed;
  void* data_chunk;
  int64_t* data_pointers;
  uint64_t data_offsets_len;
  int32_t chunk_cbytes, chunk_nbytes;
  int64_t packed_len;
  int i;

  packed_len = blosc2_get_frame_length(schunk);
  packed = malloc((size_t)packed_len);

  /* Fill the header */
  memcpy(packed, schunk, 40);    /* copy until cbytes */

  /* Finally, setup the data pointers section */
  data_offsets_len = nchunks * sizeof(int64_t);
  data_pointers = (int64_t*)((uint8_t*)packed + packed_len - data_offsets_len);
  *(uint64_t*)((uint8_t*)packed + 72) = packed_len - data_offsets_len;

  /* And fill the actual data chunks */
  if (schunk->data != NULL) {
    for (i = 0; i < nchunks; i++) {
      data_chunk = schunk->data[i];
      chunk_nbytes = *(int32_t*)((uint8_t*)data_chunk + 4);
      chunk_cbytes = *(int32_t*)((uint8_t*)data_chunk + 12);
      memcpy((uint8_t*)packed + cbytes, data_chunk, (size_t)chunk_cbytes);
      data_pointers[i] = cbytes;
      cbytes += chunk_cbytes;
      nbytes += chunk_nbytes;
    }
  }

  /* Add the length for the data chunk offsets */
  cbytes += data_offsets_len;
  nbytes += data_offsets_len;
  assert (cbytes == packed_len);
  *(int64_t*)((uint8_t*)packed + 16) = nchunks;
  *(int64_t*)((uint8_t*)packed + 24) = nbytes;
  *(int64_t*)((uint8_t*)packed + 32) = cbytes;

  return packed;
}


/* Get a super-chunk out of a frame */
blosc2_schunk* blosc2_frame_to_schunk(void* packed) {
  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));
  int64_t nbytes = sizeof(blosc2_schunk);
  int64_t cbytes = sizeof(blosc2_schunk);
  uint8_t* data_chunk;
  void* new_chunk;
  int64_t* data;
  int64_t nchunks;
  int32_t chunk_size;
  int i;

  /* Fill the header */
  memcpy(schunk, packed, 52); /* Copy until cbytes */

  /* Finally, fill the data pointers section */
  data = (int64_t*)(
          (uint8_t*)packed + *(int64_t*)((uint8_t*)packed + 52 + 8 * 4));
  nchunks = *(int64_t*)((uint8_t*)packed + 28);
  schunk->data = malloc(nchunks * sizeof(void*));
  nbytes += nchunks * sizeof(int64_t);
  cbytes += nchunks * sizeof(int64_t);

  /* And create the actual data chunks */
  if (data != NULL) {
    for (i = 0; i < nchunks; i++) {
      data_chunk = (uint8_t*)packed + data[i];
      chunk_size = *(int32_t*)(data_chunk + 12);
      new_chunk = malloc((size_t)chunk_size);
      memcpy(new_chunk, data_chunk, (size_t)chunk_size);
      schunk->data[i] = new_chunk;
      cbytes += chunk_size;
      nbytes += *(int32_t*)(data_chunk + 4);
    }
  }
  schunk->nbytes = nbytes;
  schunk->cbytes = cbytes;

  assert(*(int64_t*)((uint8_t*)packed + 36) == nbytes);
  assert(*(int64_t*)((uint8_t*)packed + 44) == cbytes);

  return schunk;
}


/* Append an existing chunk into a frame. */
void* blosc2_frame_append_chunk(void* packed, void* chunk) {
  int64_t nchunks = *(int64_t*)((uint8_t*)packed + 28);
  int64_t packed_len = *(int64_t*)((uint8_t*)packed + 44);
  int64_t data_offsets = *(int64_t*)((uint8_t*)packed + 52 + 8 * 4);
  uint64_t chunk_offset = packed_len - nchunks * sizeof(int64_t);
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  int32_t nbytes = *(int32_t*)((uint8_t*)chunk + 4);
  int32_t cbytes = *(int32_t*)((uint8_t*)chunk + 12);
  /* The current and new data areas */
  uint8_t* data;
  uint8_t* new_data;

  /* Make space for the new chunk and copy it */
  packed = realloc(packed, packed_len + cbytes + sizeof(int64_t));
  data = (uint8_t*)packed + data_offsets;
  new_data = data + cbytes;
  /* Move the data offsets to the end */
  memmove(new_data, data, (size_t)(nchunks * sizeof(int64_t)));
  ((uint64_t*)new_data)[nchunks] = chunk_offset;
  /* Copy the chunk */
  memcpy((uint8_t*)packed + chunk_offset, chunk, (size_t)cbytes);
  /* Update counters */
  *(int64_t*)((uint8_t*)packed + 28) += 1;
  *(uint64_t*)((uint8_t*)packed + 36) += nbytes + sizeof(uint64_t);
  *(uint64_t*)((uint8_t*)packed + 44) += cbytes + sizeof(uint64_t);
  *(uint64_t*)((uint8_t*)packed + 52 + 8 * 3) += cbytes;
  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n",
          nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */

  return packed;
}


/* Append a data buffer to a frame. */
// TODO: Update for the new filter pipeline support
void* blosc2_frame_append_buffer(void *packed, size_t typesize, size_t nbytes, void *src) {
  int cname = *(int16_t*)((uint8_t*)packed + 4);
  int clevel = *(int16_t*)((uint8_t*)packed + 6);
  void* filters_chunk = (uint8_t*)packed + *(uint64_t*)((uint8_t*)packed + 52);
  uint8_t* filters = (uint8_t*)packed + 12;
  int cbytes;
  void* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);
  void* dest = malloc(nbytes);
  char* compname;
  int doshuffle, dodelta = 0;
  void* new_packed;

  /* Apply filters prior to compress */
  if (filters[0] == BLOSC_DELTA) {
    dodelta = 1;
    doshuffle = filters[1];
  }
  else if (filters[0] == BLOSC_TRUNC_PREC) {
    doshuffle = filters[1];
  }
  else {
    doshuffle = filters[0];
  }

  /* Compress the src buffer using super-chunk defaults */
  blosc_compcode_to_compname(cname, &compname);
  blosc_set_compressor(compname);
  blosc_set_delta(dodelta);
  cbytes = blosc_compress(clevel, doshuffle, typesize, nbytes, src, chunk,
                          nbytes + BLOSC_MAX_OVERHEAD);
  if (cbytes < 0) {
    free(chunk);
    free(dest);
    return NULL;
  }

  free(dest);

  /* Append the chunk and free it */
  new_packed = blosc2_frame_append_chunk(packed, chunk);
  free(chunk);

  return new_packed;
}


/* Decompress and return a chunk that is part of a frame. */
int blosc2_frame_decompress_chunk(void *packed, size_t nchunk, void **dest) {
  int64_t nchunks = *(int64_t*)((uint8_t*)packed + 28);
  int64_t* data = (int64_t*)(
          (uint8_t*)packed + *(int64_t*)((uint8_t*)packed + 52 + 8 * 4));
  void* src;
  int chunksize;
  int32_t nbytes;

  if (nchunk >= nchunks) {
    return -10;
  }

  /* Grab the address of the chunk */
  src = (uint8_t*)packed + data[nchunk];
  /* Create a buffer for destination */
  nbytes = *(int32_t*)((uint8_t*)src + 4);
  *dest = malloc((size_t)nbytes);

  /* And decompress it */
  chunksize = blosc_decompress(src, *dest, (size_t)nbytes);
  if (chunksize < 0) {
    return chunksize;
  }
  if (chunksize != nbytes) {
    return -11;
  }

  return chunksize;
}

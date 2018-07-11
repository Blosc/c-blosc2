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
#include "blosc.h"
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


// Inplace conversion to big/little-endian (it is symmetric).  Sizes supported: 1, 2, 4, 8 bytes.
void* swap_inplace(void *pa, int size) {
  uint8_t* pa_ = (uint8_t*)pa;
  int64_t dest;
  uint8_t* pa2_ = (uint8_t*)&dest;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] == 1) {
    /* little endian */
    switch (size) {
      case 8:
        pa2_[0] = pa_[7];
        pa2_[1] = pa_[6];
        pa2_[2] = pa_[5];
        pa2_[3] = pa_[4];
        pa2_[4] = pa_[3];
        pa2_[5] = pa_[2];
        pa2_[6] = pa_[1];
        pa2_[7] = pa_[0];
        memcpy(pa_, pa2_, 8);
        break;
      case 4:
        pa2_[0] = pa_[3];
        pa2_[1] = pa_[2];
        pa2_[2] = pa_[1];
        pa2_[3] = pa_[0];
        memcpy(pa_, pa2_, 4);
        break;
      case 2:
        pa2_[0] = pa_[1];
        pa2_[1] = pa_[0];
        memcpy(pa_, pa2_, 2);
        break;
      case 1:
        break;
      default:
        fprintf(stderr, "Unhandled size: %d\n", size);
    }
  }
  return pa_;
}


#define FRAME_VERSION 0
#define FRAME_LEN 16
#define FRAME_CHUNKSIZE 53
#define HEADER2_MAXSIZE 64
#define HEADER2_LEN 11

void* new_header2_frame(blosc2_schunk *schunk) {
  uint8_t* h2 = calloc(HEADER2_MAXSIZE, 1);
  uint8_t* h2p = h2;

  // The msgpack header will start as a fix array
  *h2p = 0x90 + 10;  // array with 10 elements
  h2p += 1;

  // Magic number
  *h2p = 0xa0 + 8;  // str with 8 elements
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);
  strncpy((char*)h2p, "b2frame", strlen("b2frame"));
  h2p += 8;

  // Header size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t* h2sizep = (int32_t*)h2p;
  h2p += 4;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Total frame size
  *h2p = 0xcf;  // uint64
  h2p += 1;
  h2p += 8;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Flags
  *h2p = 0xa0 + 4;  // str with 4 elements
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // General flags
  *h2p = 0x4 + FRAME_VERSION;  // frame + version
  *h2p += 0x20;  // 64 bit offsets
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Filter flags
  *h2p = 0x6;  // shuffle + split_blocks
  *h2p += 0;  // same as typesize
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Codec flags
  *h2p = schunk->compcode;
  *h2p += (schunk->clevel) << 4;  // clevel
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Reserved flags
  *h2p = 0;
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Uncompressed size
  *h2p = 0xd3;  // int64
  h2p += 1;
  int64_t nbytes = schunk->nbytes;
  memcpy(h2p, swap_inplace(&nbytes, 8), 8);
  h2p += 8;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Compressed size
  *h2p = 0xd3;  // int64
  h2p += 1;
  int64_t cbytes = schunk->cbytes;
  memcpy(h2p, swap_inplace(&cbytes, 8), 8);
  h2p += 8;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Type size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t typesize = schunk->typesize;
  memcpy(h2p, swap_inplace(&typesize, 4), 4);
  h2p += 4;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Chunk size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t chunksize = schunk->chunksize;
  memcpy(h2p, swap_inplace(&chunksize, 4), 4);
  h2p += 4;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Number of threads
  *h2p = 0xd1;  // int16
  h2p += 1;
  int16_t nthreads = schunk->dctx->nthreads;
  memcpy(h2p, swap_inplace(&nthreads, 2), 2);
  h2p += 2;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Optional meta-blocks
  *h2p = 0x80 + 0;  // map with N keys
  h2p += 1;
  assert(h2p - h2 < HEADER2_MAXSIZE);

  // Finally, set the length now that we know it
  int32_t hsize = (int32_t)(h2p - h2);
  memcpy(h2sizep, swap_inplace(&hsize, 4), 4);

  return h2;
}


/* Create a frame out of a super-chunk */
void* blosc2_new_frame(blosc2_schunk *schunk) {
  int64_t nchunks = schunk->nchunks;
  int64_t cbytes = schunk->cbytes;
  uint64_t coffset = 0;
  uint32_t h2len;
  size_t frame_len;

  void* h2 = new_header2_frame(schunk);
  memcpy(&h2len, h2 + HEADER2_LEN, 4);
  swap_inplace(&h2len, 4);
  printf("header len: %d\n", h2len);
  frame_len = h2len + cbytes;

  // Build the offsets chunk
  int32_t off_cbytes = 0;
  size_t off_nbytes = nchunks * 8;
  void* off_chunk = malloc(off_nbytes + BLOSC_MAX_OVERHEAD);
  uint64_t* data_tmp = malloc(off_nbytes);
  for (int i = 0; i < nchunks; i++) {
    void* data_chunk = schunk->data[i];
    int32_t chunk_cbytes = *(int32_t*)((uint8_t*)data_chunk + 12);
    data_tmp[i] = coffset;
    coffset += chunk_cbytes;
  }
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  off_cbytes = blosc2_compress_ctx(cctx, off_nbytes, data_tmp, off_chunk,
                                   off_nbytes + BLOSC_MAX_OVERHEAD);
  free(data_tmp);
  blosc2_free_ctx(cctx);
  if (off_cbytes < 0) {
    free(off_chunk);
    free(h2);
    return NULL;
  }
  assert (coffset == cbytes);
  frame_len += off_cbytes;
  printf("Total frame length: %ld\n", frame_len);

  // Create the frame and put the header at the beginning
  void* frame = malloc(frame_len);
  memcpy(frame, h2, h2len);
  free(h2);
  // Copy the offsets at the end of the frame
  memcpy(frame + h2len + cbytes, off_chunk, off_cbytes);
  free(off_chunk);
  printf("Offsets compressed from %ld to %d bytes\n", off_nbytes, off_cbytes);

  // Fill the actual data chunks
  int32_t chunksize = 0;
  coffset = 0;
  for (int i = 0; i < nchunks; i++) {
    void* data_chunk = schunk->data[i];
    int32_t chunk_cbytes = *(int32_t*)((uint8_t*)data_chunk + 12);
    memcpy((uint8_t*)frame + h2len + coffset, data_chunk, (size_t)chunk_cbytes);
    coffset += chunk_cbytes;
    int32_t chunksize_ = *(int32_t*)((uint8_t*)data_chunk + 8);
    if (i == 0) {
      chunksize = chunksize_;
    }
    else if (chunksize != chunksize_) {
      // Variable size  TODO: update flags for this (or do not use them at all)
      chunksize = 0;
    }
  }

  // Finally, set the length and the chunksize now that we know them
  uint64_t tbytes = frame_len;
  memcpy(frame + FRAME_LEN, swap_inplace(&tbytes, 8), 8);
  memcpy(frame + FRAME_CHUNKSIZE, swap_inplace(&chunksize, 4), 4);

  return frame;
}


// Get the frame length
uint64_t blosc2_frame_len(const void *frame) {
  uint64_t frame_len;
  memcpy(&frame_len, frame + FRAME_LEN, 8);
  swap_inplace(&frame_len, 8);
  return frame_len;
}


/* Get a super-chunk out of a frame */
blosc2_schunk* blosc2_frame_to_schunk(void* frame) {
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
  memcpy(schunk, frame, 52); /* Copy until cbytes */

  /* Finally, fill the data pointers section */
  data = (int64_t*)(
          (uint8_t*)frame + *(int64_t*)((uint8_t*)frame + 52 + 8 * 4));
  nchunks = *(int64_t*)((uint8_t*)frame + 28);
  schunk->data = malloc(nchunks * sizeof(void*));
  nbytes += nchunks * sizeof(int64_t);
  cbytes += nchunks * sizeof(int64_t);

  /* And create the actual data chunks */
  if (data != NULL) {
    for (i = 0; i < nchunks; i++) {
      data_chunk = (uint8_t*)frame + data[i];
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

  assert(*(int64_t*)((uint8_t*)frame + 36) == nbytes);
  assert(*(int64_t*)((uint8_t*)frame + 44) == cbytes);

  return schunk;
}


/* Append an existing chunk into a frame. */
void* blosc2_frame_append_chunk(void* frame, void* chunk) {
  int64_t nchunks = *(int64_t*)((uint8_t*)frame + 28);
  int64_t frame_len = *(int64_t*)((uint8_t*)frame + 44);
  int64_t data_offsets = *(int64_t*)((uint8_t*)frame + 52 + 8 * 4);
  uint64_t chunk_offset = frame_len - nchunks * sizeof(int64_t);
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  int32_t nbytes = *(int32_t*)((uint8_t*)chunk + 4);
  int32_t cbytes = *(int32_t*)((uint8_t*)chunk + 12);
  /* The current and new data areas */
  uint8_t* data;
  uint8_t* new_data;

  /* Make space for the new chunk and copy it */
  frame = realloc(frame, frame_len + cbytes + sizeof(int64_t));
  data = (uint8_t*)frame_len + data_offsets;
  new_data = data + cbytes;
  /* Move the data offsets to the end */
  memmove(new_data, data, (size_t)(nchunks * sizeof(int64_t)));
  ((uint64_t*)new_data)[nchunks] = chunk_offset;
  /* Copy the chunk */
  memcpy((uint8_t*)frame + chunk_offset, chunk, (size_t)cbytes);
  /* Update counters */
  *(int64_t*)((uint8_t*)frame + 28) += 1;
  *(uint64_t*)((uint8_t*)frame + 36) += nbytes + sizeof(uint64_t);
  *(uint64_t*)((uint8_t*)frame + 44) += cbytes + sizeof(uint64_t);
  *(uint64_t*)((uint8_t*)frame + 52 + 8 * 3) += cbytes;
  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n",
          nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */

  return frame;
}


/* Append a data buffer to a frame. */
// TODO: Update for the new filter pipeline support
void* blosc2_frame_append_buffer(void *frame, size_t typesize, size_t nbytes, void *src) {
  int cname = *(int16_t*)((uint8_t*)frame + 4);
  int clevel = *(int16_t*)((uint8_t*)frame + 6);
  void* filters_chunk = (uint8_t*)frame + *(uint64_t*)((uint8_t*)frame + 52);
  uint8_t* filters = (uint8_t*)frame + 12;
  int cbytes;
  void* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);
  void* dest = malloc(nbytes);
  char* compname;
  int doshuffle, dodelta = 0;
  void* new_frame;

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
  new_frame = blosc2_frame_append_chunk(frame, chunk);
  free(chunk);

  return new_frame;
}


/* Decompress and return a chunk that is part of a frame. */
int blosc2_frame_decompress_chunk(void *frame, size_t nchunk, void **dest) {
  int64_t nchunks = *(int64_t*)((uint8_t*)frame + 28);
  int64_t* data = (int64_t*)(
          (uint8_t*)frame + *(int64_t*)((uint8_t*)frame + 52 + 8 * 4));
  void* src;
  int chunksize;
  int32_t nbytes;

  if (nchunk >= nchunks) {
    return -10;
  }

  /* Grab the address of the chunk */
  src = (uint8_t*)frame + data[nchunk];
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

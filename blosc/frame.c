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
#include <stdbool.h>
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

  #define fseek _fseeki64

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

#define HEADER2_MAGIC 2
#define HEADER2_LEN (HEADER2_MAGIC + 8 + 1)  // 11
#define FRAME_LEN (HEADER2_LEN + 4 + 1)  // 16
#define FRAME_FLAGS (FRAME_LEN + 8 + 1)  // 25
#define FRAME_FILTERS (FRAME_FLAGS + 1)  // 26
#define FRAME_COMPCODE (FRAME_FLAGS + 2)  // 27
#define FRAME_NBYTES (FRAME_FLAGS + 4 + 1)  // 30
#define FRAME_CBYTES (FRAME_NBYTES + 8 + 1)  // 39
#define FRAME_TYPESIZE (FRAME_CBYTES + 8 + 1) // 48
#define FRAME_CHUNKSIZE (FRAME_TYPESIZE + 4 + 1)  // 53
#define FRAME_NTHREADS_C (FRAME_CHUNKSIZE + 4 + 1)  // 58
#define FRAME_NTHREADS_D (FRAME_NTHREADS_C + 2 + 1)  // 61
#define HEADER2_MINLEN (FRAME_NTHREADS_D + 2 + 1)  // 64 <- minimum length
//#define FRAME_ATTRS (HEADER2_MINLEN)  // 64


void* new_header2_frame(blosc2_schunk *schunk) {
  uint8_t* h2 = calloc(HEADER2_MINLEN, 1);
  uint8_t* h2p = h2;

  // The msgpack header will start as a fix array
  *h2p = 0x90 + 12;  // array with 12 elements
  h2p += 1;

  // Magic number
  *h2p = 0xa0 + 8;  // str with 8 elements
  h2p += 1;
  assert(h2p - h2 < HEADER2_MINLEN);
  strncpy((char*)h2p, "b2frame", strlen("b2frame"));
  h2p += 8;

  // Header size
  *h2p = 0xd2;  // int32
  h2p += 1 + 4;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Total frame size
  *h2p = 0xcf;  // uint64
  h2p += 1 + 8;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Flags
  *h2p = 0xa0 + 4;  // str with 4 elements
  h2p += 1;
  assert(h2p - h2 < HEADER2_MINLEN);

  // General flags
  *h2p = 0x4 + FRAME_VERSION;  // frame + version
  *h2p += 0x20;  // 64 bit offsets
  h2p += 1;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Filter flags
  *h2p = 0x6;  // shuffle + split_blocks
  *h2p += 0;  // same as typesize
  h2p += 1;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Codec flags
  *h2p = schunk->compcode;
  *h2p += (schunk->clevel) << 4u;  // clevel
  h2p += 1;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Reserved flags
  *h2p = 0;
  h2p += 1;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Uncompressed size
  *h2p = 0xd3;  // int64
  h2p += 1;
  int64_t nbytes = schunk->nbytes;
  memcpy(h2p, swap_inplace(&nbytes, 8), 8);
  h2p += 8;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Compressed size
  *h2p = 0xd3;  // int64
  h2p += 1;
  int64_t cbytes = schunk->cbytes;
  memcpy(h2p, swap_inplace(&cbytes, 8), 8);
  h2p += 8;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Type size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t typesize = schunk->typesize;
  memcpy(h2p, swap_inplace(&typesize, 4), 4);
  h2p += 4;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Chunk size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t chunksize = schunk->chunksize;
  memcpy(h2p, swap_inplace(&chunksize, 4), 4);
  h2p += 4;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Number of threads for compression
  *h2p = 0xd1;  // int16
  h2p += 1;
  int16_t nthreads = schunk->cctx->nthreads;
  memcpy(h2p, swap_inplace(&nthreads, 2), 2);
  h2p += 2;
  assert(h2p - h2 < HEADER2_MINLEN);

  // Number of threads for decompression
  *h2p = 0xd1;  // int16
  h2p += 1;
  nthreads = schunk->dctx->nthreads;
  memcpy(h2p, swap_inplace(&nthreads, 2), 2);
  h2p += 2;
  assert(h2p - h2 == HEADER2_MINLEN - 1);

  // Make space for an (empty) map
  int32_t hsize = (int32_t)(h2p - h2);
  h2 = realloc(h2, HEADER2_MINLEN + 1 + 2);
  h2p = h2 + hsize;

  // Offsets for optional namespaces of attributes
  int16_t nclients = (schunk->frame == NULL)? (int16_t)0 : schunk->frame->nclients;
  *h2p = 0xde;  // map 16 with N keys
  h2p += 1;
  memcpy(h2p, &nclients, sizeof(nclients));
  h2p += sizeof(nclients);
  int32_t current_header_len = (int32_t)(h2p - h2);
  int32_t *offtooff = malloc(nclients * sizeof(int32_t));
  for (int nclient = 0; nclient < nclients; nclient++) {
    blosc2_frame_attrs *attrs = schunk->frame->attrs[nclient];
    uint8_t nslen = (uint8_t) strlen(attrs->namespace);
    h2 = realloc(h2, (size_t)current_header_len + 1 + nslen + 1 + 4);
    h2p = h2 + current_header_len;
    // Store the namespace
    *h2p = (uint8_t)0xa0 + nslen;  // str
    h2p += 1;
    memcpy(h2p, attrs->namespace, nslen);
    h2p += nslen;
    // Space for storing the offset for the value of this namespace
    *h2p = 0xd2;  // int 32
    h2p += 1;
    offtooff[nclient] = (int32_t)(h2p - h2);
    h2p += 4;
    current_header_len += 1 + nslen + 1 + 4;
  }
  hsize = (int32_t)(h2p - h2);
  assert(hsize == current_header_len);  // sanity check

  // Make space for an (empty) array
  hsize = (int32_t)(h2p - h2);
  h2 = realloc(h2, HEADER2_MINLEN + 1 + 2);
  h2p = h2 + hsize;

  // Now, store the values in an array
  *h2p = 0xdc;  // array 16 with N elements
  h2p += 1;
  memcpy(h2p, &nclients, sizeof(nclients));
  h2p += sizeof(nclients);
  current_header_len = (int32_t)(h2p - h2);
  for (int nclient = 0; nclient < nclients; nclient++) {
    blosc2_frame_attrs *attrs = schunk->frame->attrs[nclient];
    h2 = realloc(h2, (size_t)current_header_len + 1 + 4 + attrs->sattrs_len);
    h2p = h2 + current_header_len;
    // Store the serialized attrs for this namespace
    *h2p = 0xc6;  // bin 32
    h2p += 1;
    memcpy(h2p, &(attrs->sattrs_len), 4);
    h2p += 4;
    memcpy(h2p, attrs->sattrs, attrs->sattrs_len);
    h2p += attrs->sattrs_len;
    // Update the offset now that we know it
    memcpy(h2 + offtooff[nclient], &current_header_len, 4);
    current_header_len += 1 + 4 + attrs->sattrs_len;
  }
  free(offtooff);

  // Set the length of the whole header now that we know it
  hsize = (int32_t)(h2p - h2);
  assert(hsize == current_header_len);  // sanity check
  memcpy(h2 + HEADER2_LEN, swap_inplace(&hsize, 4), 4);

  return h2;
}


/* Create a frame out of a super-chunk. */
int64_t blosc2_schunk_to_frame(blosc2_schunk *schunk, blosc2_frame *frame) {
  int32_t nchunks = schunk->nchunks;
  int64_t cbytes = schunk->cbytes;
  FILE* fp = NULL;

  uint8_t* h2 = new_header2_frame(schunk);
  uint32_t h2len;
  memcpy(&h2len, h2 + HEADER2_LEN, 4);
  swap_inplace(&h2len, 4);

  // Build the offsets chunk
  int32_t chunksize = 0;
  int32_t off_cbytes = 0;
  uint64_t coffset = 0;
  size_t off_nbytes = (size_t)nchunks * 8;
  uint64_t* data_tmp = malloc(off_nbytes);
  for (int i = 0; i < nchunks; i++) {
    uint8_t* data_chunk = schunk->data[i];
    int32_t chunk_cbytes = sw32_(data_chunk + 12);
    data_tmp[i] = coffset;
    coffset += chunk_cbytes;
    int32_t chunksize_ = sw32_(data_chunk + 4);
    if (i == 0) {
      chunksize = chunksize_;
    }
    else if (chunksize != chunksize_) {
      // Variable size  TODO: update flags for this (or do not use them at all)
      chunksize = 0;
    }
  }
  assert ((int64_t)coffset == cbytes);

  // Compress the chunk of offsets
  uint8_t* off_chunk = malloc(off_nbytes + BLOSC_MAX_OVERHEAD);
  blosc2_context* cctx = blosc2_create_cctx(BLOSC_CPARAMS_DEFAULTS);
  cctx->typesize = 8;
  off_cbytes = blosc2_compress_ctx(cctx, off_nbytes, data_tmp, off_chunk,
                                   off_nbytes + BLOSC_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  free(data_tmp);
  if (off_cbytes < 0) {
    free(off_chunk);
    free(h2);
    return -1;
  }

  // Now that we know them, fill the chunksize and frame length in header2
  memcpy(h2 + FRAME_CHUNKSIZE, swap_inplace(&chunksize, 4), 4);
  frame->len = h2len + cbytes + off_cbytes;
  int64_t tbytes = frame->len;
  memcpy(h2 + FRAME_LEN, swap_inplace(&tbytes, sizeof(tbytes)), sizeof(tbytes));

  // Create the frame and put the header at the beginning
  if (frame->fname == NULL) {
    frame->sdata = malloc((size_t)frame->len);
    memcpy(frame->sdata, h2, h2len);
  }
  else {
    fp = fopen(frame->fname, "wb");
    fwrite(h2, h2len, 1, fp);
  }
  free(h2);

  // Fill the frame with the actual data chunks
  coffset = 0;
  for (int i = 0; i < nchunks; i++) {
    uint8_t* data_chunk = schunk->data[i];
    int32_t chunk_cbytes = sw32_(data_chunk + 12);
    if (frame->fname == NULL) {
      memcpy(frame->sdata + h2len + coffset, data_chunk, (size_t)chunk_cbytes);
    } else {
      fwrite(data_chunk, (size_t)chunk_cbytes, 1, fp);
    }
    coffset += chunk_cbytes;
  }
  assert ((int64_t)coffset == cbytes);

  // Copy the offsets chunk at the end of the frame
  if (frame->fname == NULL) {
    memcpy(frame->sdata + h2len + cbytes, off_chunk, off_cbytes);
  }
  else {
    fwrite(off_chunk, (size_t)off_cbytes, 1, fp);
    fclose(fp);
  }
  free(off_chunk);

  return frame->len;
}


/* Write an in-memory frame out to a file. */
int64_t blosc2_frame_to_file(blosc2_frame *frame, char *fname) {
  assert(frame->fname == NULL);  // make sure that we are using an in-memory frame
  FILE* fp = fopen(fname, "wb");
  fwrite(frame->sdata, (size_t)frame->len, 1, fp);
  fclose(fp);
  return frame->len;
}


/* Initialize a frame out of a file */
blosc2_frame* blosc2_frame_from_file(char *fname) {
  blosc2_frame* frame = calloc(1, sizeof(blosc2_frame));
  char* fname_cpy = malloc(strlen(fname) + 1);
  frame->fname = strcpy(fname_cpy, fname);

  uint8_t* header = malloc(HEADER2_MINLEN);
  FILE* fp = fopen(fname, "rb");
  fread(header, HEADER2_MINLEN, 1, fp);
  fclose(fp);

  int64_t frame_len;
  memcpy(&frame_len, header + FRAME_LEN, sizeof(frame_len));
  swap_inplace(&frame_len, sizeof(frame_len));
  frame->len = frame_len;

  free(header);

  return frame;
}


// Get the data pointers section
int32_t get_offsets(blosc2_frame* frame, int64_t frame_len, int32_t header_len,
                    int64_t cbytes, int32_t nchunks, void* offsets) {
  uint8_t* framep = frame->sdata;
  uint8_t* coffsets;

  if (frame->sdata != NULL) {
    coffsets = framep + header_len + cbytes;
  } else {
    int32_t off_cbytes = (int32_t)(frame_len - header_len - cbytes);
    coffsets = malloc((size_t)off_cbytes);
    FILE* fp = fopen(frame->fname, "rb");
    fseek(fp, header_len + cbytes, SEEK_SET);
    size_t rbytes = fread(coffsets, 1, (size_t)off_cbytes, fp);
    if (rbytes != (size_t)off_cbytes) {
      fprintf(stderr, "Cannot read the offsets out of the fileframe.\n");
      fclose(fp);
      return -1;
    };
    fclose(fp);
  }

  int32_t off_cbytes = sw32_(coffsets + 12);
  blosc2_dparams off_dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(off_dparams);
  int32_t off_nbytes = blosc2_decompress_ctx(dctx, coffsets, offsets, (size_t)nchunks * 8);
  blosc2_free_ctx(dctx);
  if (off_nbytes < 0) {
    free(offsets);
    return -1;
  }
  if (frame->sdata == NULL) {
    free(coffsets);
  }

  return off_cbytes;
}


int frame_get_meta(blosc2_frame* frame, int32_t* header_len, int64_t* frame_len,
                   int64_t* nbytes, int64_t* cbytes, int32_t* chunksize, int32_t* nchunks,
                   int32_t* typesize, uint8_t* compcode, uint8_t* filters) {
  uint8_t* framep = frame->sdata;
  uint8_t* header = NULL;

  assert(frame->len > 0);

  if (frame->sdata == NULL) {
    header = malloc(HEADER2_MINLEN);
    FILE* fp = fopen(frame->fname, "rb");
    fread(header, HEADER2_MINLEN, 1, fp);
    framep = header;
    fclose(fp);
  }

  // Fetch some internal lengths
  memcpy(header_len, framep + HEADER2_LEN, sizeof(*header_len));
  swap_inplace(header_len, sizeof(*header_len));

  memcpy(frame_len, framep + FRAME_LEN, sizeof(*frame_len));
  swap_inplace(frame_len, sizeof(*frame_len));

  memcpy(nbytes, framep + FRAME_NBYTES, sizeof(*nbytes));
  swap_inplace(nbytes, sizeof(*nbytes));

  memcpy(cbytes, framep + FRAME_CBYTES, sizeof(*cbytes));
  swap_inplace(cbytes, sizeof(*cbytes));

  memcpy(chunksize, framep + FRAME_CHUNKSIZE, sizeof(*chunksize));
  swap_inplace(chunksize, sizeof(*chunksize));

  if (*nbytes > 0) {
    // We can compute the chunks only when the frame has actual data
    *nchunks = (int32_t) (*nbytes / *chunksize);
    if (*nchunks * *chunksize < *nbytes) {
      *nchunks += 1;
    }
  } else {
    *nchunks = 0;
  }

  memcpy(typesize, framep + FRAME_TYPESIZE, sizeof(*typesize));
  swap_inplace(typesize, sizeof(*typesize));

  // Codec and compression level
  *compcode = framep[FRAME_COMPCODE];

  // Filter.  We don't pay attention to the split flag which is set automatically when compressing.
  *filters = framep[FRAME_FILTERS];

  // TODO: complete other flags


  if (frame->sdata == NULL) {
    free(header);
  }

  return 0;
}


int frame_update_meta(blosc2_frame* frame, int64_t new_frame_len, int64_t new_nbytes,
                      int64_t new_cbytes, int32_t new_chunksize) {
  uint8_t* framep = frame->sdata;
  uint8_t* header = frame->sdata;

  assert(frame->len > 0);

  if (frame->sdata == NULL) {
    header = malloc(HEADER2_MINLEN);
    FILE* fp = fopen(frame->fname, "rb");
    fread(header, HEADER2_MINLEN, 1, fp);
    framep = header;
    fclose(fp);
  }

  int32_t header_len;
  memcpy(&header_len, framep + HEADER2_LEN, sizeof(header_len));
  swap_inplace(&header_len, sizeof(header_len));

  int64_t nbytes;
  memcpy(&nbytes, framep + FRAME_NBYTES, sizeof(nbytes));
  swap_inplace(&nbytes, sizeof(nbytes));

  int32_t chunksize;
  memcpy(&chunksize, framep + FRAME_CHUNKSIZE, sizeof(chunksize));
  swap_inplace(&chunksize, sizeof(chunksize));

  /* Update counters */
  frame->len = new_frame_len;
  memcpy(header + FRAME_LEN, &new_frame_len, sizeof(new_frame_len));
  swap_inplace(header + FRAME_LEN, sizeof(new_frame_len));
  memcpy(header + FRAME_NBYTES, &new_nbytes, sizeof(new_nbytes));
  swap_inplace(header + FRAME_NBYTES, sizeof(new_nbytes));
  memcpy(header + FRAME_CBYTES, &new_cbytes, sizeof(new_cbytes));
  swap_inplace(header + FRAME_CBYTES, sizeof(new_cbytes));
  // Set the chunksize
//  if ((nbytes > 0) && (new_chunksize != chunksize)) {
//    TODO: look into the variable size flag for this condition
//    new_chunksize = 0;   // varlen
//  }
  memcpy(header + FRAME_CHUNKSIZE, &new_chunksize, sizeof(new_chunksize));
  swap_inplace(header + FRAME_CHUNKSIZE, sizeof(new_chunksize));

  if (frame->sdata == NULL) {
    // Write updated counters down to file
    FILE* fp = fopen(frame->fname, "rb+");
    fwrite(header, (size_t)header_len, 1, fp);
    fclose(fp);
    free(header);
  }

  return 0;
}


/* Get a super-chunk out of a frame */
blosc2_schunk* blosc2_schunk_from_frame(blosc2_frame* frame) {
  int32_t header_len;
  int64_t frame_len;
  int64_t nbytes;
  int64_t cbytes;
  int32_t chunksize;
  int32_t nchunks;
  int32_t typesize;
  uint8_t compcode;
  uint8_t filters;
  int ret = frame_get_meta(frame, &header_len, &frame_len, &nbytes, &cbytes,
                           &chunksize, &nchunks, &typesize, &compcode, &filters);
  if (ret < 0) {
    fprintf(stderr, "unable to get meta info from frame");
    return NULL;
  }

  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));
  schunk->nbytes = nbytes;
  schunk->cbytes = cbytes;
  schunk->typesize = typesize;
  schunk->chunksize = chunksize;
  schunk->nchunks = nchunks;
  schunk->clevel = (uint8_t)((compcode & 0xf0u) >> 4u);
  schunk->compcode = (uint8_t)(compcode & 0xfu);
  schunk->filters[BLOSC_MAX_FILTERS - 1] = (uint8_t)((filters & 0xcu) >> 2u);  // filters are in bits 2 and 3

  // Get the offsets
  int64_t* offsets = (int64_t*)calloc((size_t)nchunks * 8, 1);
  int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
  if (off_cbytes < 0) {
    fprintf(stderr, "Cannot get the offsets for the frame\n");
    return NULL;
  }

  // And create the actual data chunks (and, while doing this,
  // get a guess at the blocksize used in this frame)
  schunk->data = malloc(nchunks * sizeof(void*));
  int64_t acc_nbytes = 0;
  int64_t acc_cbytes = 0;
  int32_t blocksize = 0;
  int32_t csize = 0;
  uint8_t* data_chunk = NULL;
  int32_t prev_alloc = BLOSC_MIN_HEADER_LENGTH;
  FILE* fp = NULL;
  if (frame->sdata == NULL) {
    data_chunk = malloc((size_t)prev_alloc);
    fp = fopen(frame->fname, "rb");
  }
  for (int i = 0; i < nchunks; i++) {
    if (frame->sdata != NULL) {
      data_chunk = frame->sdata + header_len + offsets[i];
      csize = sw32_(data_chunk + 12);
    }
    else {
      fseek(fp, header_len + offsets[i], SEEK_SET);
      fread(data_chunk, BLOSC_MIN_HEADER_LENGTH, 1, fp);
      csize = sw32_(data_chunk + 12);
      if (csize > prev_alloc) {
        data_chunk = realloc(data_chunk, (size_t)csize);
        prev_alloc = csize;
      }
      fseek(fp, header_len + offsets[i], SEEK_SET);
      fread(data_chunk, (size_t)csize, 1, fp);
    }
    uint8_t* new_chunk = malloc((size_t)csize);
    memcpy(new_chunk, data_chunk, (size_t)csize);
    schunk->data[i] = new_chunk;
    acc_nbytes += sw32_(data_chunk + 4);
    acc_cbytes += csize;
    int32_t blocksize_ = sw32_(data_chunk + 8);
    if (i == 0) {
      blocksize = blocksize_;
    }
    else if (blocksize != blocksize_) {
      // Blocksize varies
      blocksize = 0;
    }
  }
  schunk->blocksize = blocksize;

  free(offsets);
  if (frame->sdata == NULL) {
    free(data_chunk);
    fclose(fp);
  }
  assert(acc_nbytes == nbytes);
  assert(acc_cbytes == cbytes);
  assert(frame_len == header_len + cbytes + off_cbytes);

  // Compression and decompression contexts
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  for (int i = 0; i < BLOSC_MAX_FILTERS; i++) {
    cparams.filters[i] = schunk->filters[i];
    cparams.filters_meta[i] = schunk->filters_meta[i];
  }
  cparams.compcode = schunk->compcode;
  cparams.clevel = schunk->clevel;
  cparams.typesize = schunk->typesize;
  cparams.blocksize = schunk->blocksize;
  schunk->cctx = blosc2_create_cctx(cparams);

  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;
  schunk->dctx = blosc2_create_dctx(dparams);

  return schunk;
}


/* Return a compressed chunk that is part of a frame in the `chunk` parameter.
 * If the frame is disk-based, a buffer is allocated for the (compressed) chunk,
 * and hence a free is needed.  You can check if the chunk requires a free with the `needs_free`
 * parameter.
 * If the chunk does not need a free, it means that a pointer to the location in frame is returned
 * in the `chunk` parameter.
 *
 * The size of the (compressed) chunk is returned.  If some problem is detected, a negative code
 * is returned instead.
*/
int blosc2_frame_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free) {
  int32_t header_len;
  int64_t frame_len;
  int64_t nbytes;
  int64_t cbytes;
  int32_t chunksize;
  int32_t nchunks;
  int32_t typesize;
  uint8_t compcode;
  uint8_t filters;

  *chunk = NULL;
  *needs_free = false;
  int ret = frame_get_meta(frame, &header_len, &frame_len, &nbytes, &cbytes, &chunksize, &nchunks,
                           &typesize, &compcode, &filters);
  if (ret < 0) {
    fprintf(stderr, "unable to get meta info from frame");
    return -1;
  }

  if (nchunk >= nchunks) {
    fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                    "('%d') in frame\n", nchunk, nchunks);
    return -2;
  }

  // Get the offset to the chunk
  int32_t off_nbytes = nchunks * 8;
  int64_t* offsets = malloc((size_t)off_nbytes);
  int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
  if (off_cbytes < 0) {
    fprintf(stderr, "Cannot get the offset for chunk %d for the frame\n", nchunk);
    return -3;
  }
  int64_t offset = offsets[nchunk];
  free(offsets);

  int32_t chunk_cbytes;
  if (frame->sdata == NULL) {
    FILE* fp = fopen(frame->fname, "rb");
    fseek(fp, header_len + offset + 12, SEEK_SET);
    long rbytes = (long)fread(&chunk_cbytes, 1, sizeof(chunk_cbytes), fp);
    if (rbytes != sizeof(chunk_cbytes)) {
      fprintf(stderr, "Cannot read the cbytes for chunk in the fileframe.\n");
      return -4;
    }
    chunk_cbytes = sw32_(&chunk_cbytes);
    *chunk = malloc((size_t)chunk_cbytes);
    fseek(fp, header_len + offset, SEEK_SET);
    rbytes = (long)fread(*chunk, 1, (size_t)chunk_cbytes, fp);
    if (rbytes != chunk_cbytes) {
      fprintf(stderr, "Cannot read the chunk out of the fileframe.\n");
      return -5;
    }
    fclose(fp);
    *needs_free = true;
  } else {
    *chunk = frame->sdata + header_len + offset;
    chunk_cbytes = sw32_(*chunk + 12);
  }

  return chunk_cbytes;
}


/* Append an existing chunk into a frame. */
void* blosc2_frame_append_chunk(blosc2_frame* frame, void* chunk) {
  int32_t header_len;
  int64_t frame_len;
  int64_t nbytes;
  int64_t cbytes;
  int32_t chunksize;
  int32_t nchunks;
  int32_t typesize;
  uint8_t compcode;
  uint8_t filters;
  int ret = frame_get_meta(frame, &header_len, &frame_len, &nbytes, &cbytes, &chunksize, &nchunks,
                           &typesize, &compcode, &filters);
  if (ret < 0) {
    fprintf(stderr, "unable to get meta info from frame");
    return NULL;
  }

  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  int32_t nbytes_chunk = sw32_((uint8_t*)chunk + 4);
  int64_t new_nbytes = nbytes + nbytes_chunk;
  int32_t cbytes_chunk = sw32_((uint8_t*)chunk + 12);
  int64_t new_cbytes = cbytes + cbytes_chunk;

  if ((nchunks > 0) && (nbytes_chunk > chunksize)) {
    fprintf(stderr, "appending chunks with a larger chunksize than frame is not allowed yet"
                    "%d != %d", nbytes_chunk, chunksize);
    return NULL;
  }

  // Check that we are not appending a small chunk after another small chunk
  if ((nchunks > 0) && (nbytes_chunk < chunksize)) {
    uint8_t* last_chunk;
    bool needs_free;
    int retcode = blosc2_frame_get_chunk(frame, nchunks - 1, &last_chunk, &needs_free);
    if (retcode < 0) {
      fprintf(stderr,
              "cannot get the last chunk (in position %d)", nchunks - 1);
      return NULL;
    }
    int32_t last_nbytes = sw32_(last_chunk + 4);
    if (needs_free) {
      free(last_chunk);
    }
    if ((last_nbytes < chunksize) && (nbytes < chunksize)) {
      fprintf(stderr,
              "appending two consecutive chunks with a chunksize smaller than the frame chunksize"
              "is not allowed yet: "
              "%d != %d", nbytes_chunk, chunksize);
      return NULL;
    }
  }
  // Get the current offsets and add one more
  int32_t off_nbytes = (nchunks + 1) * 8;
  int64_t* offsets = malloc((size_t)off_nbytes);
  int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
  if (off_cbytes < 0) {
    return NULL;
  }
  // Add the new offset
  offsets[nchunks] = cbytes;

  // Re-compress the offsets again
  blosc2_context* cctx = blosc2_create_cctx(BLOSC_CPARAMS_DEFAULTS);
  cctx->typesize = 8;
  void* off_chunk = malloc((size_t)off_nbytes + BLOSC_MAX_OVERHEAD);
  int32_t new_off_cbytes = blosc2_compress_ctx(cctx, (size_t)off_nbytes, offsets,
          off_chunk, (size_t)off_nbytes + BLOSC_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  free(offsets);
  if (new_off_cbytes < 0) {
    free(off_chunk);
    return NULL;
  }

  int64_t new_frame_len = header_len + new_cbytes + new_off_cbytes;

  FILE* fp = NULL;
  if (frame->sdata != NULL) {
    uint8_t* framep = frame->sdata;
    /* Make space for the new chunk and copy it */
    frame->sdata = framep = realloc(framep, (size_t)new_frame_len);
    if (framep == NULL) {
      fprintf(stderr, "cannot realloc space for the frame.");
      return NULL;
    }
    /* Copy the chunk */
    memcpy(framep + header_len + cbytes, chunk, (size_t)cbytes_chunk);
    /* Copy the offsets */
    memcpy(framep + header_len + new_cbytes, off_chunk, (size_t)new_off_cbytes);
  } else {
    // fileframe
    fp = fopen(frame->fname, "rb+");
    fseek(fp, header_len + cbytes, SEEK_SET);
    size_t wbytes = fwrite(chunk, (size_t)cbytes_chunk, 1, fp);  // the new chunk
    if (wbytes != 1) {
      fprintf(stderr, "cannot write the full chunk to fileframe.");
      return NULL;
    }
    wbytes = fwrite(off_chunk, (size_t)new_off_cbytes, 1, fp);  // the new offsets
    if (wbytes != 1) {
      fprintf(stderr, "cannot write the offsets to fileframe.");
      return NULL;
    }
    fclose(fp);
  }
  free(off_chunk);

  /* Update counters */
  frame->len = new_frame_len;
  ret = frame_update_meta(frame, new_frame_len, new_nbytes, new_cbytes, nbytes_chunk);
  if (ret < 0) {
    fprintf(stderr, "unable to update meta info from frame");
    return NULL;
  }

  return frame;
}


/* Decompress and return a chunk that is part of a frame. */
int blosc2_frame_decompress_chunk(blosc2_frame *frame, int nchunk, void *dest, size_t nbytes) {
  uint8_t* src;
  bool needs_free;
  int retcode = blosc2_frame_get_chunk(frame, nchunk, &src, &needs_free);
  if (retcode < 0) {
    fprintf(stderr,
            "cannot get the chunk in position %d", nchunk);
    return -1;
  }

  /* Create a buffer for destination */
  int32_t nbytes_ = sw32_(src + 4);
  if (nbytes_ > (int32_t)nbytes) {
    fprintf(stderr, "Not enough space for decompressing in dest");
    return -1;
  }

  /* And decompress it */
  int32_t chunksize = blosc_decompress(src, dest, nbytes);

  if (needs_free) {
    free(src);
  }
  return (int)chunksize;
}


/* Free all memory from a frame. */
int blosc2_free_frame(blosc2_frame *frame) {

  if (frame->sdata != NULL) {
    free(frame->sdata);
  }
  if (frame->nclients > 0) {
    for (int i = 0; i < frame->nclients; i++) {
      free(frame->attrs[i]->namespace);
      free(frame->attrs[i]->sattrs);
      free(frame->attrs[i]);
    }
  }

  // TODO: make a constructor for frames so that we can handle the contents of the struct
//  if (frame->fname != NULL) {
//    free(frame->fname);
//  }
  free(frame);

  return 0;
}

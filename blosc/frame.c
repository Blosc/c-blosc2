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
#define HEADER2_MAXSIZE 64

#define FRAME_LEN 16
#define FRAME_FILTERS 26
#define FRAME_COMPCODE 27
#define FRAME_NBYTES 30
#define FRAME_CBYTES 39
#define FRAME_TYPESIZE 48
#define FRAME_CHUNKSIZE 53
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


/* Create a frame out of a super-chunk. */
int64_t blosc2_schunk_to_frame(blosc2_schunk *schunk, blosc2_frame *frame) {
  int64_t nchunks = schunk->nchunks;
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
  size_t off_nbytes = nchunks * 8;
  uint64_t* data_tmp = malloc(off_nbytes);
  for (int i = 0; i < nchunks; i++) {
    void* data_chunk = schunk->data[i];
    int32_t chunk_cbytes = *(int32_t*)((uint8_t*)data_chunk + 12);
    data_tmp[i] = coffset;
    coffset += chunk_cbytes;
    int32_t chunksize_ = *(int32_t*)((uint8_t*)data_chunk + 4);
    if (i == 0) {
      chunksize = chunksize_;
    }
    else if (chunksize != chunksize_) {
      // Variable size  TODO: update flags for this (or do not use them at all)
      chunksize = 0;
    }
  }
  assert (coffset == cbytes);

  // Compress the chunk of offsets
  void* off_chunk = malloc(off_nbytes + BLOSC_MAX_OVERHEAD);
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
  uint64_t tbytes = frame->len;
  memcpy(h2 + FRAME_LEN, swap_inplace(&tbytes, 8), 8);

  // Create the frame and put the header at the beginning
  if (frame->fname == NULL) {
    frame->sdata = malloc(frame->len);
    memcpy(frame->sdata, h2, h2len);
  }
  else {
    fp = fopen(frame->fname, "w");
    fwrite(h2, h2len, 1, fp);
  }
  free(h2);

  // Fill the frame with the actual data chunks
  coffset = 0;
  for (int i = 0; i < nchunks; i++) {
    void* data_chunk = schunk->data[i];
    int32_t chunk_cbytes = *(int32_t*)((uint8_t*)data_chunk + 12);
    if (frame->fname == NULL) {
      memcpy((uint8_t *)frame->sdata + h2len + coffset, data_chunk, (size_t) chunk_cbytes);
    } else {
      fwrite(data_chunk, (size_t)chunk_cbytes, 1, fp);
    }
    coffset += chunk_cbytes;
  }
  assert (coffset == cbytes);

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
  FILE* fp = fopen(fname, "w");
  fwrite(frame->sdata, frame->len, 1, fp);
  fclose(fp);
  return frame->len;
}


/* Initialize a frame out of a file */
blosc2_frame* blosc2_frame_from_file(char *fname) {
  blosc2_frame* frame = calloc(1, sizeof(blosc2_frame));
  char* fname_cpy = malloc(strlen(fname) + 1);
  frame->fname = strcpy(fname_cpy, fname);

  uint8_t* header = malloc(HEADER2_MAXSIZE);
  FILE* fp = fopen(fname, "r");
  fread(header, HEADER2_MAXSIZE, 1, fp);
  fclose(fp);

  int64_t frame_len;
  memcpy(&frame_len, header + FRAME_LEN, 8);
  swap_inplace(&frame_len, 8);
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
    FILE* fp = fopen(frame->fname, "r");
    int32_t off_cbytes = (int32_t) (frame_len - header_len - cbytes);
    coffsets = malloc((size_t) off_cbytes);
    fseek(fp, header_len + cbytes, SEEK_SET);
    long rbytes = fread(coffsets, 1, (size_t) off_cbytes, fp);
    if (rbytes != off_cbytes) {
      fprintf(stderr, "Cannot read the offsets out of the fileframe.\n");
      fclose(fp);
      return -1;
    };
    fclose(fp);
  }

  int32_t off_cbytes = *(int32_t *) (coffsets + 12);
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


/* Get a super-chunk out of a frame */
blosc2_schunk* blosc2_schunk_from_frame(blosc2_frame* frame) {
  uint8_t* framep = frame->sdata;
  void* header = NULL;
  FILE* fp = NULL;
  int64_t frame_len = frame->len;

  if (frame->sdata == NULL) {
    header = malloc(HEADER2_MAXSIZE);
    fp = fopen(frame->fname, "r");
    fread(header, HEADER2_MAXSIZE, 1, fp);
    framep = header;
    fclose(fp);
  }
  uint32_t header_len;
  memcpy(&header_len, framep + HEADER2_LEN, 4);
  swap_inplace(&header_len, 4);

  blosc2_schunk* schunk = calloc(1, sizeof(blosc2_schunk));

  // Fetch some internal lengths
  int64_t nbytes;
  memcpy(&nbytes, framep + FRAME_NBYTES, 8);
  swap_inplace(&nbytes, 8);
  schunk->nbytes = nbytes;

  int64_t cbytes;
  memcpy(&cbytes, framep + FRAME_CBYTES, 8);
  swap_inplace(&cbytes, 8);
  schunk->cbytes = cbytes;

  int32_t typesize;
  memcpy(&typesize, framep + FRAME_TYPESIZE, 4);
  swap_inplace(&typesize, 4);
  schunk->typesize = typesize;

  int32_t chunksize;
  memcpy(&chunksize, framep + FRAME_CHUNKSIZE, 4);
  swap_inplace(&chunksize, 4);
  schunk->chunksize = chunksize;

  int32_t nchunks = (int32_t)(nbytes / chunksize);
  if (nchunks * chunksize < nbytes) {
    nchunks += 1;
  }
  schunk->nchunks = nchunks;

  // Fill in the different values in struct
  // Codec
  uint8_t compcode = framep[FRAME_COMPCODE];
  schunk->compcode = (uint8_t)(compcode & 0xf);
  schunk->clevel = (uint8_t)((compcode & 0xf0) >> 4);

  // Filter.  We don't pay attention to the split flag which is set automatically when compressing.
  uint8_t filters = framep[FRAME_FILTERS];
  schunk->filters[BLOSC_MAX_FILTERS - 1] = (filters & 0xc) >> 2;  // filters are in bits 2 and 3

  // TODO: complete other flags

  if (frame->sdata == NULL) {
    free(header);   // we are done with the header
  }

  int64_t* offsets = (int64_t*)calloc(nchunks * 8, 1);
  int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
  if (off_cbytes < 0) {
    fprintf(stderr, "Cannot get the offsets for the frame\n");
    return NULL;
  }

  // And create the actual data chunks (and, while doing this,
  // get a guess of the blocksize used in this frame)
  schunk->data = malloc(nchunks * sizeof(void*));
  int64_t acc_nbytes = 0;
  int64_t acc_cbytes = 0;
  int32_t blocksize = 0;
  int32_t csize = 0;
  uint8_t* data_chunk = NULL;
  int32_t prev_alloc = BLOSC_MIN_HEADER_LENGTH;
  if (frame->sdata == NULL) {
    data_chunk = malloc((size_t)prev_alloc);
    fp = fopen(frame->fname, "r");
  }
  for (int i = 0; i < nchunks; i++) {
    if (frame->sdata != NULL) {
      data_chunk = framep + header_len + offsets[i];
      csize = *(int32_t*)(data_chunk + 12);
    }
    else {
      fseek(fp, header_len + offsets[i], SEEK_SET);
      fread(data_chunk, BLOSC_MIN_HEADER_LENGTH, 1, fp);
      csize = *(int32_t*)(data_chunk + 12);  // TODO: use memcpy for unaligned access
      if (csize > prev_alloc) {
        data_chunk = realloc(data_chunk, (size_t) csize);
        prev_alloc = csize;
      }
      fseek(fp, header_len + offsets[i], SEEK_SET);
      fread(data_chunk, (size_t)csize, 1, fp);
    }
    void* new_chunk = malloc((size_t)csize);
    memcpy(new_chunk, data_chunk, (size_t)csize);
    schunk->data[i] = new_chunk;
    acc_nbytes += *(int32_t*)(data_chunk + 4);
    acc_cbytes += csize;
    int32_t blocksize_ = *(int32_t*)(data_chunk + 8);
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


/* Append an existing chunk into a frame. */
void* blosc2_frame_append_chunk(blosc2_frame* frame, void* chunk) {
  uint8_t* framep = frame->sdata;
  void* header = NULL;
  FILE* fp = NULL;
  size_t wbytes;

  assert(frame->len > 0);

  if (frame->sdata == NULL) {
    header = malloc(HEADER2_MAXSIZE);
    fp = fopen(frame->fname, "r");
    fread(header, HEADER2_MAXSIZE, 1, fp);
    framep = header;
    fclose(fp);
  }

  // Fetch some internal lengths
  int32_t header_len;
  memcpy(&header_len, framep + HEADER2_LEN, sizeof(header_len));
  swap_inplace(&header_len, sizeof(header_len));

  int64_t frame_len;
  memcpy(&frame_len, framep + FRAME_LEN, 8);
  swap_inplace(&frame_len, 8);

  int64_t nbytes;
  memcpy(&nbytes, framep + FRAME_NBYTES, 8);
  swap_inplace(&nbytes, 8);

  int64_t cbytes;
  memcpy(&cbytes, framep + FRAME_CBYTES, 8);
  swap_inplace(&cbytes, 8);

  int32_t chunksize;
  memcpy(&chunksize, framep + FRAME_CHUNKSIZE, 4);
  swap_inplace(&chunksize, 4);

  int32_t nchunks = 0;
  if (nbytes > 0) {
    // We can compute the chunks only when the frame has actual data
    nchunks = (int32_t) (nbytes / chunksize);
    if (nchunks * chunksize < nbytes) {
      nchunks += 1;
    }
  }

  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  int32_t nbytes_chunk = *(int32_t*)((uint8_t*)chunk + 4);
  int64_t new_nbytes = nbytes + nbytes_chunk;
  int32_t cbytes_chunk = *(int32_t*)((uint8_t*)chunk + 12);
  int64_t new_cbytes = cbytes + cbytes_chunk;

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

  if (frame->sdata != NULL) {
    /* Make space for the new chunk and copy it */
    frame->sdata = framep = realloc(framep, (size_t) new_frame_len);
    if (framep == NULL) {
      fprintf(stderr, "cannot realloc space for the frame.");
      return NULL;
    }
    /* Copy the chunk */
    memcpy(framep + header_len + cbytes, chunk, (size_t) cbytes_chunk);
    /* Copy the offsets */
    memcpy(framep + header_len + new_cbytes, off_chunk, (size_t) new_off_cbytes);
  } else {
    // fileframe
    fp = fopen(frame->fname, "r+");
    fseek(fp, header_len + cbytes, SEEK_SET);
    wbytes = fwrite(chunk, (size_t)cbytes_chunk, 1, fp);  // the new chunk
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

  /* Update counters */
  frame->len = new_frame_len;
  memcpy(framep + FRAME_LEN, &new_frame_len, 8);
  swap_inplace(framep + FRAME_LEN, 8);
  memcpy(framep + FRAME_NBYTES, &new_nbytes, 8);
  swap_inplace(framep + FRAME_NBYTES, 8);
  memcpy(framep + FRAME_CBYTES, &new_cbytes, 8);
  swap_inplace(framep + FRAME_CBYTES, 8);
  // Set the chunksize
  if (nbytes == 0) {
    chunksize = nbytes_chunk;
  } else if (chunksize != nbytes_chunk) {
    chunksize = 0;   // varlen
  }
  memcpy(framep + FRAME_CHUNKSIZE, &chunksize, 4);
  swap_inplace(framep + FRAME_CHUNKSIZE, 4);

  if (frame->sdata == NULL) {
    // Write updated counters down to file
    fp = fopen(frame->fname, "r+");
    fwrite(header, (size_t)header_len, 1, fp);
    fclose(fp);
    free(header);
  }

  // Free resources
  free(off_chunk);

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
void* blosc2_frame_get_chunk(blosc2_frame *frame, int nchunk) {
  uint8_t* framep = frame->sdata;
  void* header = NULL;
  FILE* fp = NULL;
  void* chunk;

  assert(frame->len > 0);

  if (frame->sdata == NULL) {
    header = malloc(HEADER2_MAXSIZE);
    fp = fopen(frame->fname, "r");
    fread(header, HEADER2_MAXSIZE, 1, fp);
    framep = header;
    fclose(fp);
  }

  // Fetch some internal lengths
  int32_t header_len;
  memcpy(&header_len, framep + HEADER2_LEN, sizeof(header_len));
  swap_inplace(&header_len, sizeof(header_len));

  int64_t frame_len;
  memcpy(&frame_len, framep + FRAME_LEN, 8);
  swap_inplace(&frame_len, 8);

  int64_t nbytes;
  memcpy(&nbytes, framep + FRAME_NBYTES, 8);
  swap_inplace(&nbytes, 8);

  int64_t cbytes;
  memcpy(&cbytes, framep + FRAME_CBYTES, 8);
  swap_inplace(&cbytes, 8);

  int32_t chunksize;
  memcpy(&chunksize, framep + FRAME_CHUNKSIZE, 4);
  swap_inplace(&chunksize, 4);

  if (frame->sdata == NULL) {
    free(header);
  }

  int32_t nchunks = 0;
  if (nbytes > 0) {
    // We can compute the chunks only when the frame has actual data
    nchunks = (int32_t) (nbytes / chunksize);
    if (nchunks * chunksize < nbytes) {
      nchunks += 1;
    }
  }

  if (nchunk >= nchunks) {
    fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                    "('%d') in frame\n", nchunk, nchunks);
    return NULL;
  }

  // Get the offset to the chunk
  int32_t off_nbytes = nchunks * 8;
  int64_t* offsets = malloc((size_t)off_nbytes);
  int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
  if (off_cbytes < 0) {
    fprintf(stderr, "Cannot get the offsets for the frame\n");
    return NULL;
  }
  int64_t offset = offsets[nchunk];
  free(offsets);

  if (frame->sdata == NULL) {
    fp = fopen(frame->fname, "r");
    fseek(fp, header_len + offset + 12, SEEK_SET);
    int32_t chunk_cbytes;
    long rbytes = fread(&chunk_cbytes, 1, sizeof(chunk_cbytes), fp);
    if (rbytes != sizeof(chunk_cbytes)) {
      fprintf(stderr, "Cannot read the cbytes for chunk of the fileframe.\n");
      return NULL;
    }
    chunk = malloc((size_t)chunk_cbytes);
    fseek(fp, header_len + offset, SEEK_SET);
    rbytes = fread(chunk, 1, (size_t)chunk_cbytes, fp);
    if (rbytes != chunk_cbytes) {
      fprintf(stderr, "Cannot read the chunk out of the fileframe.\n");
      return NULL;
    }
    fclose(fp);
  } else {
    chunk = framep + header_len + offset;
  }

  return chunk;
}


/* Decompress and return a chunk that is part of a frame. */
int blosc2_frame_decompress_chunk(blosc2_frame *frame, int nchunk, void *dest, size_t nbytes) {
  void* src = blosc2_frame_get_chunk(frame, nchunk);

  /* Create a buffer for destination */
  int32_t nbytes_ = *(int32_t*)((uint8_t*)src + 4);
  if (nbytes_ > nbytes) {
    fprintf(stderr, "Not enough space for decompressing in dest");
    return -1;
  }

  /* And decompress it */
  int32_t chunksize = blosc_decompress(src, dest, (size_t)nbytes);
  return (int)chunksize;
}


/* Free all memory from a frame. */
int blosc2_free_frame(blosc2_frame *frame) {

  if (frame->sdata != NULL) {
    free(frame->sdata);
  }
  if (frame->fname != NULL) {
    free(frame->fname);
  }
  return 0;
}

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
#include "blosc2.h"
#include "blosc-private.h"
#include "context.h"
#include "frame.h"

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


// big <-> little-endian and store it in a memory position.  Sizes supported: 1, 2, 4, 8 bytes.
void swap_store(void *dest, const void *pa, int size) {
    uint8_t* pa_ = (uint8_t*)pa;
    uint8_t* pa2_ = malloc((size_t)size);
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
                break;
            case 4:
                pa2_[0] = pa_[3];
                pa2_[1] = pa_[2];
                pa2_[2] = pa_[1];
                pa2_[3] = pa_[0];
                break;
            case 2:
                pa2_[0] = pa_[1];
                pa2_[1] = pa_[0];
                break;
            case 1:
                pa2_[0] = pa_[1];
                break;
            default:
                fprintf(stderr, "Unhandled size: %d\n", size);
        }
    }
    memcpy(dest, pa2_, size);
    free(pa2_);
}


/* Create a new (empty) frame */
blosc2_frame* blosc2_new_frame(char* fname) {
  blosc2_frame* new_frame = malloc(sizeof(blosc2_frame));
  memset(new_frame, 0, sizeof(blosc2_frame));
  if (fname != NULL) {
    char* new_fname = malloc(strlen(fname) + 1);  // + 1 for the trailing NULL
    new_frame->fname = strcpy(new_fname, fname);
  }

  return new_frame;
}


void* new_header2_frame(blosc2_schunk *schunk, blosc2_frame *frame) {
  assert(frame != NULL);
  uint8_t* h2 = calloc(FRAME_HEADER2_MINLEN, 1);
  uint8_t* h2p = h2;

  int16_t nmetalayers = frame->nmetalayers;
  bool has_metalayers = nmetalayers > 0;

  // The msgpack header will start as a fix array
  if (has_metalayers) {
    *h2p = 0x90 + 12;  // array with 12 elements
  }
  else {
    *h2p = 0x90 + 11;  // no metalayers, so just 11 elements
  }
  h2p += 1;

  // Magic number
  *h2p = 0xa0 + 8;  // str with 8 elements
  h2p += 1;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);
  strcpy((char*)h2p, "b2frame");
  h2p += 8;

  // Header size
  *h2p = 0xd2;  // int32
  h2p += 1 + 4;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Total frame size
  *h2p = 0xcf;  // uint64
  // Fill it with frame->len which is known *after* the creation of the frame (e.g. when updating the header)
  int64_t flen = frame->len;
  swap_store(h2 + FRAME_LEN, &flen, sizeof(flen));
  h2p += 1 + 8;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Flags
  *h2p = 0xa0 + 4;  // str with 4 elements
  h2p += 1;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // General flags
  *h2p = 0x4 + FRAME_VERSION;  // frame + version
  *h2p += 0x20;  // 64 bit offsets
  h2p += 1;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Filter flags
  *h2p = 0x6;  // shuffle + split_blocks
  *h2p += 0;  // same as typesize
  h2p += 1;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Codec flags
  *h2p = schunk->compcode;
  *h2p += (schunk->clevel) << 4u;  // clevel
  h2p += 1;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Reserved flags
  *h2p = 0;
  h2p += 1;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Uncompressed size
  *h2p = 0xd3;  // int64
  h2p += 1;
  int64_t nbytes = schunk->nbytes;
  swap_store(h2p, &nbytes, sizeof(nbytes));
  h2p += 8;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Compressed size
  *h2p = 0xd3;  // int64
  h2p += 1;
  int64_t cbytes = schunk->cbytes;
  swap_store(h2p, &cbytes, sizeof(cbytes));
  h2p += 8;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Type size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t typesize = schunk->typesize;
  swap_store(h2p, &typesize, sizeof(typesize));
  h2p += 4;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Chunk size
  *h2p = 0xd2;  // int32
  h2p += 1;
  int32_t chunksize = schunk->chunksize;
  swap_store(h2p, &chunksize, sizeof(chunksize));
  h2p += 4;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Number of threads for compression
  *h2p = 0xd1;  // int16
  h2p += 1;
  int16_t nthreads = (int16_t)schunk->cctx->nthreads;
  swap_store(h2p, &nthreads, sizeof(nthreads));
  h2p += 2;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Number of threads for decompression
  *h2p = 0xd1;  // int16
  h2p += 1;
  nthreads = (int16_t)schunk->dctx->nthreads;
  swap_store(h2p, &nthreads, sizeof(nthreads));
  h2p += 2;
  assert(h2p - h2 < FRAME_HEADER2_MINLEN);

  // Boolean for checking the existence of metalayers
  *h2p = (nmetalayers > 0) ? (uint8_t)0xc3 : (uint8_t)0xc2;  // bool for FRAME_HAS_metalayerS
  h2p += 1;
  assert(h2p - h2 == FRAME_HEADER2_MINLEN);

  int32_t hsize = FRAME_HEADER2_MINLEN;
  if (nmetalayers == 0) {
    goto out;
  }

  // Make space for the header of metalayers (array marker, size, map of offsets)
  h2 = realloc(h2, (size_t)hsize + 1 + 1 + 2 + 1 + 2);
  h2p = h2 + hsize;

  // The msgpack header for the metalayers (array_marker, size, map of offsets, list of values)
  *h2p = 0x90 + 3;  // array with 3 elements
  h2p += 1;

  // Size for the map (index) of offsets, including this uint16 size (to be filled out later on)
  *h2p = 0xcd;  // uint16
  h2p += 1 + 2;

  // Map (index) of offsets for optional metalayers
  *h2p = 0xde;  // map 16 with N keys
  h2p += 1;
  swap_store(h2p, &nmetalayers, sizeof(nmetalayers));
  h2p += sizeof(nmetalayers);
  int32_t current_header_len = (int32_t)(h2p - h2);
  int32_t *offtooff = malloc(nmetalayers * sizeof(int32_t));
  for (int nmetalayer = 0; nmetalayer < nmetalayers; nmetalayer++) {
    assert(frame != NULL);
    blosc2_frame_metalayer *metalayer = frame->metalayers[nmetalayer];
    uint8_t nslen = (uint8_t) strlen(metalayer->name);
    h2 = realloc(h2, (size_t)current_header_len + 1 + nslen + 1 + 4);
    h2p = h2 + current_header_len;
    // Store the metalayer
    assert(nslen < (1U << 5U));  // metalayer strings cannot be longer than 32 bytes
    *h2p = (uint8_t)0xa0 + nslen;  // str
    h2p += 1;
    memcpy(h2p, metalayer->name, nslen);
    h2p += nslen;
    // Space for storing the offset for the value of this metalayer
    *h2p = 0xd2;  // int 32
    h2p += 1;
    offtooff[nmetalayer] = (int32_t)(h2p - h2);
    h2p += 4;
    current_header_len += 1 + nslen + 1 + 4;
  }
  int32_t hsize2 = (int32_t)(h2p - h2);
  assert(hsize2 == current_header_len);  // sanity check

  // Map size + int16 size
  assert((hsize2 - hsize) < (1U << 16U));
  uint16_t map_size = (uint16_t) (hsize2 - hsize);
  swap_store(h2 + FRAME_IDX_SIZE, &map_size, sizeof(map_size));

  // Make space for an (empty) array
  hsize = (int32_t)(h2p - h2);
  h2 = realloc(h2, (size_t)hsize + 2 + 1 + 2);
  h2p = h2 + hsize;

  // Now, store the values in an array
  *h2p = 0xdc;  // array 16 with N elements
  h2p += 1;
  swap_store(h2p, &nmetalayers, sizeof(nmetalayers));
  h2p += sizeof(nmetalayers);
  current_header_len = (int32_t)(h2p - h2);
  for (int nmetalayer = 0; nmetalayer < nmetalayers; nmetalayer++) {
    assert(frame != NULL);
    blosc2_frame_metalayer *metalayer = frame->metalayers[nmetalayer];
    h2 = realloc(h2, (size_t)current_header_len + 1 + 4 + metalayer->content_len);
    h2p = h2 + current_header_len;
    // Store the serialized contents for this metalayer
    *h2p = 0xc6;  // bin 32
    h2p += 1;
    swap_store(h2p, &(metalayer->content_len), sizeof(metalayer->content_len));
    h2p += 4;
    memcpy(h2p, metalayer->content, metalayer->content_len);  // buffer, no need to swap
    h2p += metalayer->content_len;
    // Update the offset now that we know it
    swap_store(h2 + offtooff[nmetalayer], &current_header_len, sizeof(current_header_len));
    current_header_len += 1 + 4 + metalayer->content_len;
  }
  free(offtooff);
  hsize = (int32_t)(h2p - h2);
  assert(hsize == current_header_len);  // sanity check

  out:
  // Set the length of the whole header now that we know it
  swap_store(h2 + FRAME_HEADER2_LEN, &hsize, sizeof(hsize));

  return h2;
}


/* Create a frame out of a super-chunk. */
int64_t blosc2_schunk_to_frame(blosc2_schunk *schunk, blosc2_frame *frame) {
  int32_t nchunks = schunk->nchunks;
  int64_t cbytes = schunk->cbytes;
  FILE* fp = NULL;

  uint8_t* h2 = new_header2_frame(schunk, frame);
  uint32_t h2len;
  swap_store(&h2len, h2 + FRAME_HEADER2_LEN, sizeof(h2len));

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

  uint8_t *off_chunk = NULL;
  if (nchunks > 0) {
    // Compress the chunk of offsets
    off_chunk = malloc(off_nbytes + BLOSC_MAX_OVERHEAD);
    blosc2_context *cctx = blosc2_create_cctx(BLOSC_CPARAMS_DEFAULTS);
    cctx->typesize = 8;
    off_cbytes = blosc2_compress_ctx(cctx, off_nbytes, data_tmp, off_chunk,
                                     off_nbytes + BLOSC_MAX_OVERHEAD);
    blosc2_free_ctx(cctx);
    if (off_cbytes < 0) {
      free(off_chunk);
      free(h2);
      return -1;
    }
  }
  else {
    off_cbytes = 0;
  }
  free(data_tmp);

  // Now that we know them, fill the chunksize and frame length in header2
  swap_store(h2 + FRAME_CHUNKSIZE, &chunksize, sizeof(chunksize));
  frame->len = h2len + cbytes + off_cbytes;
  int64_t tbytes = frame->len;
  swap_store(h2 + FRAME_LEN, &tbytes, sizeof(tbytes));

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
blosc2_frame* blosc2_frame_from_file(const char *fname) {
  blosc2_frame* frame = calloc(1, sizeof(blosc2_frame));
  char* fname_cpy = malloc(strlen(fname) + 1);
  frame->fname = strcpy(fname_cpy, fname);

  uint8_t* header = malloc(FRAME_HEADER2_MINLEN);
  FILE* fp = fopen(fname, "rb");
  size_t rbytes = fread(header, 1, FRAME_HEADER2_MINLEN, fp);
  assert(rbytes == FRAME_HEADER2_MINLEN);

  int64_t frame_len;
  swap_store(&frame_len, header + FRAME_LEN, sizeof(frame_len));
  frame->len = frame_len;

  bool has_metalayers = (header[FRAME_HAS_METALAYERS] == 0xc3) ? true : false;

  free(header);

  if (!has_metalayers) {
    goto out;
  }

  // Get the size for the index of metalayers
  uint16_t idx_size;
  fseek(fp, FRAME_IDX_SIZE, SEEK_SET);
  rbytes = fread(&idx_size, 1, sizeof(uint16_t), fp);
  assert(rbytes == sizeof(uint16_t));

  swap_store(&idx_size, &idx_size, sizeof(idx_size));

  // Read the index of metalayers for metalayers
  uint8_t* metalayers_idx = malloc(idx_size);
  fseek(fp, FRAME_IDX_SIZE + 2, SEEK_SET);
  rbytes = fread(metalayers_idx, 1, idx_size, fp);
  assert(rbytes == idx_size);
  assert(metalayers_idx[0] == 0xde);   // sanity check
  uint8_t* idxp = metalayers_idx + 1;
  uint16_t nmetalayers;
  swap_store(&nmetalayers, idxp, sizeof(uint16_t));
  idxp += 2;
  frame->nmetalayers = nmetalayers;

  // Populate the metalayer and serialized value for each client
  for (int nmetalayer = 0; nmetalayer < nmetalayers; nmetalayer++) {
    assert((*idxp & 0xe0) == 0xa0);   // sanity check
    blosc2_frame_metalayer* metalayer = calloc(sizeof(blosc2_frame_metalayer), 1);
    frame->metalayers[nmetalayer] = metalayer;

    // Populate the metalayer string
    int8_t nslen = *idxp & (uint8_t)0x1f;
    idxp += 1;
    char* ns = malloc((size_t)nslen + 1);
    memcpy(ns, idxp, nslen);
    ns[nslen] = '\0';
    idxp += nslen;
    metalayer->name = ns;

    // Populate the serialized value for this metalayer
    // Get the offset
    assert((*idxp & 0xff) == 0xd2);   // sanity check
    idxp += 1;
    int32_t offset;
    swap_store(&offset, idxp, sizeof(offset));
    idxp += 4;

    // Go to offset and see if we have the correct marker
    uint8_t content_marker;
    fseek(fp, offset, SEEK_SET);
    rbytes = fread(&content_marker, 1, 1, fp);
    assert(rbytes == 1);
    assert(content_marker == 0xc6);

    // Read the size of the content
    int32_t content_len;
    fseek(fp, offset + 1, SEEK_SET);
    rbytes = fread(&content_len, 1, 4, fp);
    assert(rbytes == 4);
    swap_store(&content_len, &content_len, sizeof(content_len));
    metalayer->content_len = content_len;

    // Finally, read the content
    char* content = malloc((size_t)content_len);
    fseek(fp, offset + 1 + 4, SEEK_SET);
    rbytes = fread(content, 1, (size_t)content_len, fp);
    assert(rbytes == (size_t)content_len);
    metalayer->content = (uint8_t*)content;
  }

  free(metalayers_idx);

out:
  fclose(fp);

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
    header = malloc(FRAME_HEADER2_MINLEN);
    FILE* fp = fopen(frame->fname, "rb");
    size_t rbytes = fread(header, 1, FRAME_HEADER2_MINLEN, fp);
    assert(rbytes == FRAME_HEADER2_MINLEN);
    framep = header;
    fclose(fp);
  }

  // Fetch some internal lengths
  swap_store(header_len, framep + FRAME_HEADER2_LEN, sizeof(*header_len));
  swap_store(frame_len, framep + FRAME_LEN, sizeof(*frame_len));
  swap_store(nbytes, framep + FRAME_NBYTES, sizeof(*nbytes));
  swap_store(cbytes, framep + FRAME_CBYTES, sizeof(*cbytes));
  swap_store(chunksize, framep + FRAME_CHUNKSIZE, sizeof(*chunksize));
  swap_store(typesize, framep + FRAME_TYPESIZE, sizeof(*typesize));
  *compcode = framep[FRAME_COMPCODE];
  // Filters: we don't pay attention to the split flag, which is set automatically when compressing.
  *filters = framep[FRAME_FILTERS];
  // TODO: complete other flags

  if (*nbytes > 0) {
    // We can compute the chunks only when the frame has actual data
    *nchunks = (int32_t) (*nbytes / *chunksize);
    if (*nchunks * *chunksize < *nbytes) {
      *nchunks += 1;
    }
  } else {
    *nchunks = 0;
  }

  if (frame->sdata == NULL) {
    free(header);
  }

  return 0;
}


int frame_update_meta(blosc2_frame* frame, blosc2_schunk* schunk) {
  uint8_t* header = frame->sdata;

  assert(frame->len > 0);

  if (frame->sdata == NULL) {
    header = malloc(FRAME_HEADER2_MINLEN);
    FILE* fp = fopen(frame->fname, "rb");
    size_t rbytes = fread(header, 1, FRAME_HEADER2_MINLEN, fp);
    assert(rbytes == FRAME_HEADER2_MINLEN);
    fclose(fp);
  }
  uint32_t prev_h2len;
  swap_store(&prev_h2len, header + FRAME_HEADER2_LEN, sizeof(prev_h2len));

  // Build a new header
  uint8_t* h2 = new_header2_frame(schunk, frame);
  uint32_t h2len;
  swap_store(&h2len, h2 + FRAME_HEADER2_LEN, sizeof(h2len));

  assert(prev_h2len == h2len);  // sanity check: the new header size should equal the previous one

  if (frame->sdata == NULL) {
    // Write updated header down to file
    FILE* fp = fopen(frame->fname, "rb+");
    fwrite(h2, h2len, 1, fp);
    fclose(fp);
    free(header);
  }
  else {
    memcpy(frame->sdata, h2, h2len);
  }
  free(h2);

  return 0;

}


/* Get a super-chunk out of a frame */
blosc2_schunk* blosc2_schunk_from_frame(blosc2_frame* frame, bool sparse) {
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
  schunk->frame = frame;
  schunk->nbytes = nbytes;
  schunk->cbytes = cbytes;
  schunk->typesize = typesize;
  schunk->chunksize = chunksize;
  schunk->nchunks = nchunks;
  schunk->clevel = (uint8_t)((compcode & 0xf0u) >> 4u);
  schunk->compcode = (uint8_t)(compcode & 0xfu);
  schunk->filters[BLOSC_MAX_FILTERS - 1] = (uint8_t)((filters & 0xcu) >> 2u);  // filters are in bits 2 and 3

  // Compression and decompression contexts
  blosc2_cparams *cparams;
  blosc2_schunk_get_cparams(schunk, &cparams);
  schunk->cctx = blosc2_create_cctx(*cparams);
  free(cparams);
  blosc2_dparams *dparams;
  blosc2_schunk_get_dparams(schunk, &dparams);
  schunk->dctx = blosc2_create_dctx(*dparams);
  free(dparams);

  if (!sparse || nchunks == 0) {
    // We are done, so leave here
    return schunk;
  }

  // We are not attached to a frame anymore
  schunk->frame = NULL;

  // Get the offsets
  int64_t *offsets = (int64_t *) calloc((size_t) nchunks * 8, 1);
  int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
  if (off_cbytes < 0) {
    fprintf(stderr, "Cannot get the offsets for the frame\n");
    return NULL;
  }

  // We want the sparse schunk, so create the actual data chunks (and, while doing this,
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
      size_t rbytes = fread(data_chunk, 1, BLOSC_MIN_HEADER_LENGTH, fp);
      assert(rbytes == BLOSC_MIN_HEADER_LENGTH);
      csize = sw32_(data_chunk + 12);
      if (csize > prev_alloc) {
        data_chunk = realloc(data_chunk, (size_t)csize);
        prev_alloc = csize;
      }
      fseek(fp, header_len + offsets[i], SEEK_SET);
      rbytes = fread(data_chunk, 1, (size_t)csize, fp);
      assert(rbytes == (size_t)csize);
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
int frame_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free) {
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
    size_t rbytes = fread(&chunk_cbytes, 1, sizeof(chunk_cbytes), fp);
    if (rbytes != sizeof(chunk_cbytes)) {
      fprintf(stderr, "Cannot read the cbytes for chunk in the fileframe.\n");
      return -4;
    }
    chunk_cbytes = sw32_(&chunk_cbytes);
    *chunk = malloc((size_t)chunk_cbytes);
    fseek(fp, header_len + offset, SEEK_SET);
    rbytes = fread(*chunk, 1, (size_t)chunk_cbytes, fp);
    if (rbytes != (size_t)chunk_cbytes) {
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
void* frame_append_chunk(blosc2_frame* frame, void* chunk, blosc2_schunk* schunk) {
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
    int retcode = frame_get_chunk(frame, nchunks - 1, &last_chunk, &needs_free);
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
  if (nchunks > 0) {
    int32_t off_cbytes = get_offsets(frame, frame_len, header_len, cbytes, nchunks, offsets);
    if (off_cbytes < 0) {
      return NULL;
    }
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
  free(chunk);
  free(off_chunk);

  /* Update header and other metainfo (metalayers) in frame */
  frame->len = new_frame_len;
  ret = frame_update_meta(frame, schunk);
  if (ret < 0) {
    fprintf(stderr, "unable to update meta info from frame");
    return NULL;
  }

  return frame;
}


/* Decompress and return a chunk that is part of a frame. */
int frame_decompress_chunk(blosc2_frame *frame, int nchunk, void *dest, size_t nbytes) {
  uint8_t* src;
  bool needs_free;
  int retcode = frame_get_chunk(frame, nchunk, &src, &needs_free);
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


/* Find whether the frame has a metalayer or not.
 *
 * If successful, return the index of the metalayer.  Else, return a negative value.
 * */
int blosc2_frame_has_metalayer(blosc2_frame *frame, char *name) {
    if (strlen(name) > BLOSC2_METALAYER_NAME_MAXLEN) {
        fprintf(stderr, "metalayers cannot be larger than %d chars\n", BLOSC2_METALAYER_NAME_MAXLEN);
        return -1;
    }

    for (int nmetalayer = 0; nmetalayer < frame->nmetalayers; nmetalayer++) {
        if (strcmp(name, frame->metalayers[nmetalayer]->name) == 0) {
            return nmetalayer;  // Found
        }
    }
    return -1;  // Not found
}


/* Add content into a new metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 * */
int blosc2_frame_add_metalayer(blosc2_frame *frame, char *name, uint8_t *content,
                               uint32_t content_len) {
    int nmetalayer = blosc2_frame_has_metalayer(frame, name);
    if (nmetalayer >= 0) {
        fprintf(stderr, "metalayer \"%s\" already exists", name);
        return -2;
    }

    // Add the metalayer
    blosc2_frame_metalayer *metalayer = malloc(sizeof(blosc2_frame_metalayer));
    char* name_ = malloc(strlen(name) + 1);
    strcpy(name_, name);
    metalayer->name = name_;
    uint8_t* content_buf = malloc((size_t)content_len);
    memcpy(content_buf, content, content_len);
    metalayer->content = content_buf;
    metalayer->content_len = content_len;
    frame->metalayers[frame->nmetalayers] = metalayer;
    frame->nmetalayers += 1;

    return frame->nmetalayers - 1;
}


/* Update the content of an existing metalayer.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 * */
int blosc2_frame_update_metalayer(blosc2_frame *frame, char *name, uint8_t *content,
                                  uint32_t content_len) {
    int nmetalayer = blosc2_frame_has_metalayer(frame, name);
    if (nmetalayer < 0) {
        fprintf(stderr, "metalayer \"%s\" not found\n", name);
        return nmetalayer;
    }

    blosc2_frame_metalayer *metalayer = frame->metalayers[nmetalayer];
    if (content_len > (uint32_t)metalayer->content_len) {
        fprintf(stderr, "`content_len` cannot exceed the existing size of %d bytes", metalayer->content_len);
        return nmetalayer;
    }

    // Update the contents of the metalayer
    memcpy(metalayer->content, content, content_len);
    return nmetalayer;
}


/* Get the content out of a metalayer.
 *
 * The `**content` receives a malloc'ed copy of the content.  The user is responsible of freeing it.
 *
 * If successful, return the index of the new metalayer.  Else, return a negative value.
 * */
int blosc2_frame_get_metalayer(blosc2_frame *frame, char *name, uint8_t **content,
                               uint32_t *content_len) {
    int nmetalayer = blosc2_frame_has_metalayer(frame, name);
    if (nmetalayer < 0) {
        fprintf(stderr, "metalayer \"%s\" not found\n", name);
        return nmetalayer;
    }
    *content_len = (uint32_t)frame->metalayers[nmetalayer]->content_len;
    *content = malloc((size_t)*content_len);
    memcpy(*content, frame->metalayers[nmetalayer]->content, (size_t)*content_len);
    return nmetalayer;
}


/* Free all memory from a frame. */
int blosc2_free_frame(blosc2_frame *frame) {

  if (frame->sdata != NULL) {
    free(frame->sdata);
  }
  if (frame->nmetalayers > 0) {
    for (int i = 0; i < frame->nmetalayers; i++) {
      free(frame->metalayers[i]->name);
      free(frame->metalayers[i]->content);
      free(frame->metalayers[i]);
    }
    frame->nmetalayers = 0;
  }

  if (frame->fname != NULL) {
    free(frame->fname);
  }

  free(frame);

  return 0;
}

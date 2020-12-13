/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>
  Creation date: 2018-07-04

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "blosc2.h"
#include "blosc-private.h"
#include "context.h"
#include "frame.h"
#include "eframe.h"


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


/* Append an existing chunk into an extended frame. */
void* eframe_append_chunk(blosc2_frame *frame, uint8_t *chunk, int32_t nchunk, int64_t cbytes) {
  //get directory/nchunk.chunk
  char chunkname[1024];
  strcpy(chunkname,frame->fname);
  char str_chunk[8 + 6 + 1];
  sprintf(str_chunk,"%08X.chunk",nchunk);
  strcat(chunkname,str_chunk);
  FILE* fpc = fopen(chunkname,"wb");

  size_t wbytes = fwrite(chunk, 1, (size_t)cbytes,fpc);
  if (wbytes != (size_t)cbytes) {
    fprintf(stderr, "cannot write the full chunk.");
    fclose(fpc);
    return NULL;
  }
  fclose(fpc);
}


/*Get chunk from extended frame. */
int eframe_get_chunk(blosc2_frame *frame, int nchunk, uint8_t **chunk, bool *needs_free){
  //get directory/nchunk.chunk
  char chunkname[1024];
  strcpy(chunkname, frame->fname);

  char str_chunk[8 + 6 + 1];
  sprintf(str_chunk,"%08X.chunk", nchunk);
  strcat(chunkname, str_chunk);
  FILE* fpc = fopen(chunkname,"rb");

  fseek(fpc, 0L, SEEK_END);
  int32_t chunk_cbytes = ftell(fpc);
  *chunk = malloc((size_t)chunk_cbytes);

  fseek(fpc, 0L, SEEK_SET);
  size_t rbytes = fread(*chunk, 1, (size_t)chunk_cbytes, fpc);
  if (rbytes != (size_t)chunk_cbytes) {
    fprintf(stderr, "Cannot read the chunk out of the chunkfile.\n");
    fclose(fpc);
    return -1;
  }

  fclose(fpc);
  *needs_free = true;

  return chunk_cbytes;
}


/*Create a header out of a super-chunk. */
int eframe_update_header(blosc2_schunk *schunk) {
  uint8_t* h2 = new_header_frame(schunk, schunk->frame);
  uint32_t hsize;
  swap_store(&hsize, h2 + FRAME_HEADER_LEN, sizeof(hsize));

  free(h2);
  return hsize;
}


int eframe_get_header_info(blosc2_schunk *schunk, int32_t *header_len, int64_t *nbytes,
                           int64_t *cbytes, int32_t *chunksize, int32_t *nchunks, int32_t *typesize,
                           uint8_t *compcode, uint8_t *clevel, uint8_t *filters, uint8_t *filters_meta) {
  uint8_t* headerp;
  uint8_t* header = malloc(FRAME_HEADER_MINLEN);

  char *headername = malloc(sizeof(char) * (strlen(schunk->storage->path) + 6 + 1));
  sprintf(headername, "%sheader", schunk->storage->path);
  FILE *fph = fopen(headername, "rb");
  free(headername);

  size_t rbytes = fread(header, 1, FRAME_HEADER_MINLEN, fph);
  (void) rbytes;
  assert(rbytes == FRAME_HEADER_MINLEN);
  headerp = header;
  fclose(fph);

  // Fetch some internal lengths
  swap_store(header_len, headerp + FRAME_HEADER_LEN, sizeof(*header_len));
  swap_store(nbytes, headerp + FRAME_NBYTES, sizeof(*nbytes));
  swap_store(cbytes, headerp + FRAME_CBYTES, sizeof(*cbytes));
  swap_store(chunksize, headerp + FRAME_CHUNKSIZE, sizeof(*chunksize));
  if (typesize != NULL) {
    swap_store(typesize, headerp + FRAME_TYPESIZE, sizeof(*typesize));
  }

  // Codecs
  uint8_t schunk_codecs = headerp[FRAME_CODECS];
  if (clevel != NULL) {
    *clevel = schunk_codecs >> 4u;
  }
  if (compcode != NULL) {
    *compcode = schunk_codecs & 0xFu;
  }

  // Filters
  if (filters != NULL && filters_meta != NULL) {
    uint8_t nfilters = headerp[FRAME_FILTER_PIPELINE];
    if (nfilters > BLOSC2_MAX_FILTERS) {
      fprintf(stderr, "Error: the number of filters in header are too large for Blosc2");
      return -1;
    }
    uint8_t *filters_ = headerp + FRAME_FILTER_PIPELINE + 1;
    uint8_t *filters_meta_ = headerp + FRAME_FILTER_PIPELINE + 1 + FRAME_FILTER_PIPELINE_MAX;
    for (int i = 0; i < nfilters; i++) {
      filters[i] = filters_[i];
      filters_meta[i] = filters_meta_[i];
    }
  }

  if (*nbytes > 0) {
    // We can compute the number of chunks only when the eframe has actual data
    *nchunks = (int32_t) (*nbytes / *chunksize);
    if (*nbytes % *chunksize > 0) {
      *nchunks += 1;
    }
  } else {
    *nchunks = 0;
  }

  free(header);

  return 0;

}

/*Create a trailer out of a super-schunk. */
int eframe_new_trailer(blosc2_schunk* schunk) {
  return frame_update_trailer(NULL, schunk);
}

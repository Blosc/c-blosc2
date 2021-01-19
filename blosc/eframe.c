/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>
  Creation date: 2018-07-04

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
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
void* eframe_create_chunk(blosc2_frame* frame, uint8_t* chunk, int32_t nchunk, int64_t cbytes) {
  // Get directory/nchunk.chunk with 8 zeros of padding
  char* chunkpath = malloc(strlen(frame->urlpath) + 1 + 8 + strlen(".chunk") + 1);
  sprintf(chunkpath, "%s/%08X.chunk", frame->urlpath, nchunk);
  FILE* fpc = fopen(chunkpath, "wb");
  free(chunkpath);

  size_t wbytes = fwrite(chunk, 1, (size_t)cbytes, fpc);
  fclose(fpc);
  if (wbytes != (size_t)cbytes) {
    BLOSC_TRACE_ERROR("Cannot write the full chunk.");
    return NULL;
  }

  return frame;
}


/*Get chunk from extended frame. */
int eframe_get_chunk(blosc2_frame* frame, int32_t nchunk, uint8_t** chunk, bool* needs_free){
  //get directory/nchunk.chunk
  char* chunkpath = malloc(strlen(frame->urlpath) + 1 + 8 + strlen(".chunk") + 1);
  sprintf(chunkpath, "%s/%08X.chunk", frame->urlpath, nchunk);
  FILE* fpc = fopen(chunkpath, "rb");
  free(chunkpath);
  if(fpc == NULL){
    BLOSC_TRACE_ERROR("Cannot open the chunkfile.");
    return -1;
  }

  fseek(fpc, 0L, SEEK_END);
  int32_t chunk_cbytes = ftell(fpc);
  *chunk = malloc((size_t)chunk_cbytes);

  fseek(fpc, 0L, SEEK_SET);
  size_t rbytes = fread(*chunk, 1, (size_t)chunk_cbytes, fpc);
  fclose(fpc);
  if (rbytes != (size_t)chunk_cbytes) {
    BLOSC_TRACE_ERROR("Cannot read the chunk out of the chunkfile.");
    return -1;
  }
  *needs_free = true;

  return chunk_cbytes;
}

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-07-30

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#if defined(USING_CMAKE)
  #include "config.h"
#endif /*  USING_CMAKE */
#include "blosc.h"

#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>

  /* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif

#else
  #include <stdint.h>
  #include <unistd.h>
  #include <inttypes.h>
#endif  /* _WIN32 */

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
#endif


/* Create a new super-chunk */
schunk_header* blosc2_new_schunk(schunk_params params) {
  schunk_header* sc_header = calloc(1, sizeof(schunk_header));

  sc_header->version = 0x0;  	/* pre-first version */
  sc_header->filters = params.filters;
  sc_header->compressor = params.compressor;
  sc_header->clevel = params.clevel;
  sc_header->data = malloc(0);
  /* The rest of the structure will remain zeroed */

  return sc_header;
}


/* Append an existing chunk to a super-chunk. */
int blosc2_append_chunk(schunk_header* sc_header, void* chunk, int copy) {
  int64_t nchunks = sc_header->nchunks;
  /* The chunksize starts in byte 12 */
  int32_t cbytes = *(int32_t*)(chunk + 12);
  void* chunk_copy;

  /* By copying the chunk we will always be able to free it later on */
  if (copy) {
    chunk_copy = malloc(cbytes);
    memcpy(chunk_copy, chunk, cbytes);
    chunk = chunk_copy;
  }

  /* Make space for appending a new chunk and do it */
  sc_header->data = realloc(sc_header->data, (nchunks + 1) * sizeof(void*));
  sc_header->data[nchunks] = chunk;
  sc_header->nchunks = nchunks + 1;

  return nchunks + 1;
}


/* Append a data buffer to a super-chunk. */
int blosc2_append_buffer(schunk_header* sc_header, size_t typesize,
			 size_t nbytes, void* src) {
  int chunksize;
  void* chunk = malloc(nbytes);

  /* Compress the src buffer using super-chunk defaults */
  chunksize = blosc_compress(sc_header->clevel, sc_header->filters,
			     typesize, nbytes, src, chunk, nbytes);
  if (chunksize < 0) {
    free(chunk);
    return chunksize;
  }

  /* Append the chunk (no copy is required) */
  return blosc2_append_chunk(sc_header, chunk, 0);
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_decompress_chunk(schunk_header* sc_header, int nchunk,
			    void **dest) {
  int64_t nchunks = sc_header->nchunks;
  void* src;
  int chunksize;
  int32_t destsize;

  if (nchunk >= nchunks) {
    return -10;
  }

  /* Grab the address of the chunk */
  src = sc_header->data[nchunk];
  /* Create a buffer for destination */
  destsize = *(int32_t*)(src + 4);
  printf("destsize: %d\n", destsize);
  *dest = malloc(destsize);

  /* And decompress it */
  chunksize = blosc_decompress(src, *dest, destsize);
  if (chunksize < 0) {
    return chunksize;
  }
  if (chunksize != destsize) {
    return -11;
  }

  return chunksize;
}


/* Free all memory from a super-chunk. */
int blosc2_destroy_schunk(schunk_header* sc_header) {
  int i;

  if (sc_header->metadata != NULL)
    free(sc_header->metadata);
  if (sc_header->userdata != NULL)
    free(sc_header->userdata);
  if (sc_header->data != NULL) {
    for (i = 0; i < sc_header->nchunks; i++) {
      if (sc_header->data[i] != NULL) {
	free(sc_header->data[i]);
      }
    }
    free(sc_header->data);
  }
  free(sc_header);
  return 0;
}

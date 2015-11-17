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


/* Encode filters in a 16 bit int type */
uint16_t encode_filters(schunk_params* params) {
  int i;
  int16_t enc_filters = 0;

  /* Encode the BLOSC_MAX_FILTERS filters (3-bit encoded) in 16 bit */
  for (i = 0; i < BLOSC_MAX_FILTERS; i++) {
    enc_filters += params->filters[i] << (i * 3);
  }
  return enc_filters;
}


/* Decode filters.  The returned array must be freed after use.  */
uint8_t* decode_filters(uint16_t enc_filters) {
  int i;
  uint8_t* filters = malloc(BLOSC_MAX_FILTERS);

  /* Decode the BLOSC_MAX_FILTERS filters (3-bit encoded) in 16 bit */
  for (i = 0; i < BLOSC_MAX_FILTERS; i++) {
    filters[i] = enc_filters & 0b11;
    enc_filters >>= 3;
  }
  return filters;
}


/* Create a new super-chunk */
schunk_header* blosc2_new_schunk(schunk_params* params) {
  schunk_header* sc_header = calloc(1, sizeof(schunk_header));

  sc_header->version = 0x0;     /* pre-first version */
  sc_header->filters = encode_filters(params);
  sc_header->filt_info = params->filt_info;
  sc_header->compressor = params->compressor;
  sc_header->clevel = params->clevel;
  sc_header->cbytes = sizeof(*sc_header);
  /* The rest of the structure will remain zeroed */

  return sc_header;
}


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int delta_encoder8(schunk_header* sc_header, int nbytes,
                   uint8_t* src, uint8_t* dest) {
  int i;
  int rbytes = *(int*)(sc_header->data[0] + 4);
  int mbytes;
  uint8_t* dref = (uint8_t*)sc_header->data[0] + BLOSC_MAX_OVERHEAD;

  mbytes = MIN(nbytes, rbytes);

  /* Encode delta */
  for (i = 0; i < mbytes; i++) {
    dest[i] = src[i] - dref[i];
  }

  /* Copy the leftovers */
  if (nbytes > rbytes) {
    for (i = rbytes; i < nbytes; i++) {
      dest[i] = src[i];
    }
  }

  return nbytes;
}


int delta_decoder8(schunk_header* sc_header, int nbytes, uint8_t* src) {
  int i;
  uint8_t* dref = (uint8_t*)sc_header->data[0] + BLOSC_MAX_OVERHEAD;
  int rbytes = *(int*)(sc_header->data[0] + 4);
  int mbytes;

  mbytes = MIN(nbytes, rbytes);

  /* Decode delta */
  for (i = 0; i < (mbytes); i++) {
    src[i] += dref[i];
  }

  /* The leftovers are in-place already */

  return nbytes;
}


/* Append an existing chunk to a super-chunk. */
int blosc2_append_chunk(schunk_header* sc_header, void* chunk, int copy) {
  int64_t nchunks = sc_header->nchunks;
  /* The uncompressed and compressed sizes start at byte 4 and 12 */
  int32_t nbytes = *(int32_t*)(chunk + 4);
  int32_t cbytes = *(int32_t*)(chunk + 12);
  void* chunk_copy;

  /* By copying the chunk we will always be able to free it later on */
  if (copy) {
    chunk_copy = malloc((size_t)cbytes);
    memcpy(chunk_copy, chunk, (size_t)cbytes);
    chunk = chunk_copy;
  }

  /* Make space for appending a new chunk and do it */
  sc_header->data = realloc(sc_header->data, (nchunks + 1) * sizeof(void*));
  sc_header->data[nchunks] = chunk;
  sc_header->nchunks = nchunks + 1;
  if (nchunks > 0) {
    /* Avoid counting the reference as it is uncompressed */
    sc_header->nbytes += nbytes;
    sc_header->cbytes += cbytes;
  }
  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n", */
  /*        nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */

  return nchunks + 1;
}


/* Append a data buffer to a super-chunk. */
int blosc2_append_buffer(schunk_header* sc_header, size_t typesize,
                         size_t nbytes, void* src) {
  int cbytes;
  void* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);
  void* dest = malloc(nbytes);
  int ret;
  uint16_t enc_filters = sc_header->filters;
  uint8_t* filters = decode_filters(enc_filters);
  int clevel = sc_header->clevel;
  char* compname;

  /* Apply filters prior to compress */
  if (filters[0] == BLOSC_DELTA) {
    if (sc_header->nchunks > 0) {
      ret = delta_encoder8(sc_header, (int)nbytes, src, dest);
      /* dest = memcpy(dest, src, nbytes); */
      if (ret < 0) {
        return ret;
      }
      src = dest;
    }
    else {
      /* The reference will not be compressed in-memory */
      clevel = 0;
    }
    enc_filters = enc_filters >> 3;
  }

  /* Compress the src buffer using super-chunk defaults */
  blosc_compcode_to_compname(sc_header->compressor, &compname);
  blosc_set_compressor(compname);
  cbytes = blosc_compress(clevel, enc_filters, typesize, nbytes, src, chunk,
                          nbytes + BLOSC_MAX_OVERHEAD);
  if (cbytes < 0) {
    free(chunk);
    free(dest);
    return cbytes;
  }

  /* We don't need dest anymore */
  free(dest);

  /* Append the chunk (no copy required here) */
  return blosc2_append_chunk(sc_header, chunk, 0);
}


/* Decompress and return a chunk that is part of a super-chunk. */
int blosc2_decompress_chunk(schunk_header* sc_header, int nchunk,
                            void** dest) {
  int64_t nchunks = sc_header->nchunks;
  void* src;
  int chunksize;
  int32_t nbytes;
  uint16_t enc_filters = sc_header->filters;
  uint8_t* filters = decode_filters(enc_filters);

  if (nchunk >= nchunks) {
    return -10;
  }

  /* Grab the address of the chunk */
  src = sc_header->data[nchunk];
  /* Create a buffer for destination */
  nbytes = *(int32_t*)(src + 4);
  *dest = malloc((size_t)nbytes);

  /* And decompress it */
  chunksize = blosc_decompress(src, *dest, (size_t)nbytes);
  if (chunksize < 0) {
    return chunksize;
  }
  if (chunksize != nbytes) {
    return -11;
  }

  /* Apply filters after de-compress */
  if (filters[0] == BLOSC_DELTA && sc_header->nchunks > 0) {
    delta_decoder8(sc_header, nbytes, *dest);
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

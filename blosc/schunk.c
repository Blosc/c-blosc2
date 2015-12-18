/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-07-30

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#endif  /* _WIN32 */

/* If C11 is supported, use it's built-in aligned allocation. */
#if __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
#endif


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int delta_encoder8(schunk_header* sc_header, int nbytes,
                   uint8_t* src, uint8_t* dest) {
  int i;
  int rbytes = *(int*)(sc_header->filters_chunk + 4);
  int mbytes;
  uint8_t* dref = (uint8_t*)sc_header->filters_chunk + BLOSC_MAX_OVERHEAD;

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
  uint8_t* dref = (uint8_t*)sc_header->filters_chunk + BLOSC_MAX_OVERHEAD;
  int rbytes = *(int*)(sc_header->filters_chunk + 4);
  int mbytes;

  mbytes = MIN(nbytes, rbytes);

  /* Decode delta */
  for (i = 0; i < (mbytes); i++) {
    src[i] += dref[i];
  }

  /* The leftovers are in-place already */

  return nbytes;
}


/* Encode filters in a 16 bit int type */
uint16_t encode_filters(schunk_params* params) {
  int i;
  uint16_t enc_filters = 0;

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
    filters[i] = (uint8_t)(enc_filters & 0b11);
    enc_filters >>= 3;
  }
  return filters;
}


/* Create a new super-chunk */
schunk_header* blosc2_new_schunk(schunk_params* params) {
  schunk_header* sc_header = calloc(1, sizeof(schunk_header));

  sc_header->version = 0x0;     /* pre-first version */
  sc_header->filters = encode_filters(params);
  sc_header->filters_meta = params->filters_meta;
  sc_header->compressor = params->compressor;
  sc_header->clevel = params->clevel;
  sc_header->nbytes = sizeof(schunk_header);
  sc_header->cbytes = sizeof(schunk_header);
  /* The rest of the structure will remain zeroed */

  return sc_header;
}


/* Append an existing chunk into a super-chunk. */
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
  /* Update counters */
  sc_header->nchunks = nchunks + 1;
  sc_header->nbytes += nbytes + sizeof(void*);
  sc_header->cbytes += cbytes + sizeof(void*);
  /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n",
          nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */

  return nchunks + 1;
}


/* Append a data buffer to a super-chunk. */
int blosc2_append_buffer(schunk_header* sc_header, size_t typesize,
                         size_t nbytes, void* src) {
  int cbytes;
  void* chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);
  void* dest = malloc(nbytes);
  void* filters_chunk;
  int ret;
  uint8_t* dec_filters = decode_filters(sc_header->filters);
  int clevel = sc_header->clevel;
  char* compname;
  int doshuffle;

  /* Apply filters prior to compress */
  if (dec_filters[0] == BLOSC_DELTA) {
    doshuffle = dec_filters[1];
    if (sc_header->filters_chunk == NULL) {
      /* The reference chunk will not be compressed */
      filters_chunk = malloc(nbytes + BLOSC_MAX_OVERHEAD);
      cbytes = blosc_compress(0, 0, typesize, nbytes, src, filters_chunk,
                              nbytes + BLOSC_MAX_OVERHEAD);
      sc_header->filters_chunk = filters_chunk;
      if (cbytes < 0) {
        free(filters_chunk);
        free(dest);
        return cbytes;
      }
      sc_header->nbytes += nbytes;
      sc_header->cbytes += cbytes;
    }
    ret = delta_encoder8(sc_header, (int)nbytes, src, dest);
    /* dest = memcpy(dest, src, nbytes); */
    if (ret < 0) {
      return ret;
    }
    src = dest;
  }
  else {
    doshuffle = dec_filters[0];
  }

  /* Compress the src buffer using super-chunk defaults */
  blosc_compcode_to_compname(sc_header->compressor, &compname);
  blosc_set_compressor(compname);
  cbytes = blosc_compress(clevel, doshuffle, typesize, nbytes, src, chunk,
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
int blosc2_decompress_chunk(schunk_header* sc_header, int nchunk, void** dest) {
  int64_t nchunks = sc_header->nchunks;
  void* src;
  int chunksize;
  int32_t nbytes;
  uint8_t* filters = decode_filters(sc_header->filters);

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


/* Compute the final length of a packed super-chunk */
int64_t blosc2_get_packed_length(schunk_header* sc_header) {
  int i;
  int64_t length = BLOSC_HEADER_PACKED_LENGTH;

  if (sc_header->filters_chunk != NULL)
    length += *(int32_t*)(sc_header->filters_chunk + 12);
  if (sc_header->codec_chunk != NULL)
    length += *(int32_t*)(sc_header->codec_chunk + 12);
  if (sc_header->metadata_chunk != NULL)
    length += *(int32_t*)(sc_header->metadata_chunk + 12);
  if (sc_header->userdata_chunk != NULL)
    length += *(int32_t*)(sc_header->userdata_chunk + 12);
  if (sc_header->data != NULL) {
    for (i = 0; i < sc_header->nchunks; i++) {
      length += sizeof(int64_t);
      length += *(int32_t*)(sc_header->data[i] + 12);
    }
  }
  return length;
}

/* Copy a chunk into a packed super-chunk */
void pack_copy_chunk(void* chunk, void* packed, int offset, int64_t* cbytes, int64_t* nbytes) {
  int32_t cbytes_, nbytes_;

  if (chunk != NULL) {
    nbytes_ = *(int32_t*)(chunk + 4);
    cbytes_ = *(int32_t*)(chunk + 12);
    memcpy(packed + (size_t)*cbytes, chunk, (size_t)cbytes_);
    *(int64_t*)(packed + offset) = *cbytes;
    *nbytes += nbytes_;
    *cbytes += cbytes_;
  }
  else {
    /* No data in chunk */
    *(int64_t*)(packed + offset) = 0;
  }
}


/* Create a packed super-chunk */
void* blosc2_pack_schunk(schunk_header* sc_header) {
  int64_t cbytes = BLOSC_HEADER_PACKED_LENGTH;
  int64_t nbytes = BLOSC_HEADER_PACKED_LENGTH;
  int64_t nchunks = sc_header->nchunks;
  void* packed;
  void* data_chunk;
  int64_t* data_pointers;
  size_t data_offsets_len;
  int32_t chunk_cbytes, chunk_nbytes;
  int64_t packed_len;
  int i;

  packed_len = blosc2_get_packed_length(sc_header);
  packed = malloc((size_t)packed_len);

  /* Fill the header */
  memcpy(packed, sc_header, 40);    /* copy until cbytes */

  /* Fill the ancillary chunks info */
  pack_copy_chunk(sc_header->filters_chunk,  packed, 40, &cbytes, &nbytes);
  pack_copy_chunk(sc_header->codec_chunk,    packed, 48, &cbytes, &nbytes);
  pack_copy_chunk(sc_header->metadata_chunk, packed, 56, &cbytes, &nbytes);
  pack_copy_chunk(sc_header->userdata_chunk, packed, 64, &cbytes, &nbytes);

  /* Finally, setup the data pointers section */
  data_pointers = packed + cbytes;
  *(int64_t*)(packed + 72) = cbytes;
  data_offsets_len = (size_t)nchunks * sizeof(int64_t);
  cbytes += data_offsets_len;
  nbytes += data_offsets_len;
  /* Bytes from 80 to 96 in header are reserved */

  /* And fill the actual data chunks */
  if (sc_header->data != NULL) {
    for (i = 0; i < nchunks; i++) {
      data_chunk = sc_header->data[i];
      chunk_nbytes = *(int32_t*)(data_chunk + 4);
      chunk_cbytes = *(int32_t*)(data_chunk + 12);
      memcpy(packed + cbytes, data_chunk, (size_t)chunk_cbytes);
      data_pointers[i] = cbytes;
      cbytes += chunk_cbytes;
      nbytes += chunk_nbytes;
    }
  }

  /* printf("nchunks, nbytes, cbytes, packed_len: %d, %d, %d, %d\n", nchunks, nbytes, cbytes, packed_len); */
  assert (cbytes == packed_len);
  *(int64_t*)(packed + 16) = nchunks;
  *(int64_t*)(packed + 24) = nbytes;
  *(int64_t*)(packed + 32) = cbytes;

  return packed;
}


/* Copy a chunk into a packed super-chunk */
void* unpack_copy_chunk(void* packed, int offset, schunk_header* sc_header,
                        int64_t *nbytes, int64_t *cbytes) {
  int32_t nbytes_, cbytes_;
  void *chunk, *dst_chunk = NULL;

  if (*(int64_t*)(packed + offset) != 0) {
    chunk = packed + *(int64_t*)(packed + offset);
    nbytes_ = *(int32_t*)(chunk + 4);
    cbytes_ = *(int32_t*)(chunk + 12);
    /* Create a copy of the chunk */
    dst_chunk = malloc((size_t)cbytes_);
    memcpy(dst_chunk, chunk, (size_t)cbytes_);
    /* Update counters */
    sc_header->nbytes += nbytes_;
    sc_header->cbytes += cbytes_;
    *cbytes += cbytes_;
    *nbytes += nbytes_;
  }
  return dst_chunk;
}


/* Unpack a packed super-chunk */
schunk_header* blosc2_unpack_schunk(void* packed) {
  schunk_header* sc_header = calloc(1, sizeof(schunk_header));
  int64_t nbytes = BLOSC_HEADER_PACKED_LENGTH;
  int64_t cbytes = BLOSC_HEADER_PACKED_LENGTH;
  void* data_chunk;
  void* new_chunk;
  int64_t* data;
  int64_t nchunks;
  int32_t chunk_size;
  int i;

  /* Fill the header */
  memcpy(sc_header, packed, 40); /* Copy until cbytes */

  /* Fill the ancillary chunks info */
  sc_header->filters_chunk = unpack_copy_chunk(packed, 40, sc_header, &nbytes, &cbytes);
  sc_header->codec_chunk = unpack_copy_chunk(packed, 48, sc_header, &nbytes, &cbytes);
  sc_header->metadata_chunk = unpack_copy_chunk(packed, 56, sc_header, &nbytes, &cbytes);
  sc_header->userdata_chunk = unpack_copy_chunk(packed, 64, sc_header, &nbytes, &cbytes);

  /* Finally, fill the data pointers section */
  data = packed + *(int64_t*)(packed + 72);
  nchunks = *(int64_t*)(packed + 16);
  sc_header->data = malloc(nchunks * sizeof(void*));
  nbytes += nchunks * sizeof(int64_t);
  cbytes += nchunks * sizeof(int64_t);

  /* And create the actual data chunks */
  if (data != NULL) {
    for (i = 0; i < nchunks; i++) {
      data_chunk = packed + data[i];
      chunk_size = *(int32_t*)(data_chunk + 12);
      new_chunk = malloc((size_t)chunk_size);
      memcpy(new_chunk, data_chunk, (size_t)chunk_size);
      sc_header->data[i] = new_chunk;
      cbytes += chunk_size;
      nbytes += *(int32_t*)(data_chunk + 4);
    }
  }
  sc_header->nbytes = nbytes;
  sc_header->cbytes = cbytes;

  /* printf("nbytes: %d, %d\n", *(int64_t*)(packed + 24), nbytes); */
  /* printf("cbytes: %d, %d\n", *(int64_t*)(packed + 32), cbytes); */
  assert (*(int64_t*)(packed + 24) == nbytes);
  assert (*(int64_t*)(packed + 32) == cbytes);

  return sc_header;
}


/* Free all memory from a super-chunk. */
int blosc2_destroy_schunk(schunk_header* sc_header) {
  int i;

  if (sc_header->filters_chunk != NULL)
    free(sc_header->metadata_chunk);
  if (sc_header->codec_chunk != NULL)
    free(sc_header->metadata_chunk);
  if (sc_header->metadata_chunk != NULL)
    free(sc_header->metadata_chunk);
  if (sc_header->userdata_chunk != NULL)
    free(sc_header->userdata_chunk);
  if (sc_header->data != NULL) {
    for (i = 0; i < sc_header->nchunks; i++) {
      free(sc_header->data[i]);
    }
    free(sc_header->data);
  }
  free(sc_header);
  return 0;
}

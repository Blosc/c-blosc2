//
// Created by Marta on 17/11/2020.
//

#include <stdio.h>
#include <assert.h>


#include "blosc2.h"
#include "blosc-private.h"
#include "context.h"
#include "frame.h"
#include "pschunk.h"



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

//
/* Append an existing chunk into a sparse schunk. */
//
int schunk_sparse_append_chunk(blosc2_schunk *schunk, uint8_t *chunk) {
    int32_t nchunks = schunk->nchunks;
    /* The uncompressed and compressed sizes start at byte 4 and 12 */
    int32_t nbytes = sw32_(chunk + 4);
    int32_t cbytes = sw32_(chunk + 12);

    if (schunk->chunksize == -1) {
        schunk->chunksize = nbytes;  // The super-chunk is initialized now
    }

    if (nbytes > schunk->chunksize) {
        fprintf(stderr, "Appending chunks that have different lengths in the same schunk is not supported yet: "
                        "%d > %d", nbytes, schunk->chunksize);
        return -1;
    }

    /* Update counters */
    schunk->nchunks = nchunks + 1;
    schunk->nbytes += nbytes;
    schunk->cbytes += cbytes;

    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (nbytes < schunk->chunksize)) {
        //get the size of the last chunk
        uint8_t *last_chunk = malloc(sizeof(uint8_t) * (nchunks / 10 + 1 + 6 + 1));
        sprintf(last_chunk, "%d",nchunks - 1);
        strcat(last_chunk, ".chunk");
        FILE *fplast = fopen(last_chunk, "r");
        fseek(fplast, 4, SEEK_SET);
        int32_t last_nbytes;
        fread(last_nbytes, sizeof(int32_t), 1, fplast);
        fclose(fplast);
        free(last_chunk);

        if ((last_nbytes < schunk->chunksize) && (nbytes < schunk->chunksize)) {
            fprintf(stderr,
                    "appending two consecutive chunks with a chunksize smaller than the schunk chunksize"
                    "is not allowed yet: "
                    "%d != %d", nbytes, schunk->chunksize);
            return -1;
        }
    }


    //add chunkfile to chunks.txt
    uint8_t *filename = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path)) + 10 +1);
    strcpy(filename, schunk->storage->path);
    strcat(filename,"chunks.txt");
    FILE *fpcs = fopen(filename,"w+");
    free(filename);

    //chunk name
    uint8_t *chunkname = malloc(sizeof(uint8_t) * (nchunks / 10 + 1 + 6 + 1));
    sprintf(chunkname, "%d",nchunks);
    strcat(chunkname, ".chunk");

    fputs(chunkname, fpcs);
    fputc('\n', fpcs);
    fclose(fpcs);

    //create chunk file
    FILE *fpc = fopen(chunkname, "wb");
    size_t wbytes = fwrite(chunk, 1, (size_t)cbytes,fpc);
    if (wbytes != (size_t)cbytes) {
        fprintf(stderr, "cannot write the full chunk.");
        return NULL;
    }
    fclose(fpc);
    free(chunkname);

    //update header
    int64_t header_len = blosc2_sparse_new_header(schunk);


    /* printf("Compression chunk #%lld: %d -> %d (%.1fx)\n", */
    /*         nchunks, nbytes, cbytes, (1.*nbytes) / cbytes); */
    return schunk->nchunks;
}



//get chunk from persistent schunk
int pschunk_get_chunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk, bool *needs_free){
    int32_t nchunks = schunk->nchunks;

    *chunk = NULL;
    *needs_free = true;

    if (nchunk >= schunk->nchunks) {
        fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                        "('%d') in the schunk\n", nchunk, nchunks);
        return -2;
    }

    uint8_t *chunkname = malloc(sizeof(uint8_t) * (nchunk / 10 + 1 + 6 + 1));
    sprintf(chunkname, "%d",nchunk);
    strcat(chunkname, ".chunk");
    FILE *fpc = fopen(chunkname, "r");
    free(chunkname);

    fseek(fpc, 0L, SEEK_END);
    int32_t chunk_cbytes = ftell(fpc);
    *chunk = malloc((size_t)chunk_cbytes);

    fseek(fpc, 0L, SEEK_SET);
    size_t rbytes = fread(*chunk, 1, (size_t)chunk_cbytes, fpc);
    if (rbytes != (size_t)chunk_cbytes) {
        fprintf(stderr, "Cannot read the chunk out of the chunkfile.\n");
        return -6;
    }

    fclose(fpc);

    return chunk_cbytes;
}


/*Create a header out of a super-chunk. */

int64_t blosc2_sparse_new_header(blosc2_schunk *schunk) {
    uint8_t* h2 = calloc(PSCHUNK_HEADER_MINLEN, 1);
    uint8_t* h2p = h2;

    // The msgpack header starts here
    *h2p = 0x90;  // fixarray...
    *h2p += 13;   // ...with 13 elements
    h2p += 1;

    // Magic number
    *h2p = 0xa0 + 8;  // str with 8 elements
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);
    strcpy((char*)h2p, "b2frame");
    h2p += 8;

    // Header size
    *h2p = 0xd2;  // int32
    h2p += 1 + 4;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Total frame size
    *h2p = 0xcf;  // uint64
    //framesize = 0
    // Fill it with frame->len which is known *after* the creation of the frame (e.g. when updating the header)
    //int64_t flen = frame->len;
    //swap_store(h2 + FRAME_LEN, &flen, sizeof(flen));
    h2p += 1 + 8;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Flags
    *h2p = 0xa0 + 4;  // str with 4 elements
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // General flags
    *h2p = BLOSC2_VERSION_FRAME_FORMAT;  // version
    *h2p += 0x20;  // 64-bit offsets
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Reserved flags
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Codec flags
    *h2p = schunk->compcode;
    *h2p += (schunk->clevel) << 4u;  // clevel
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Reserved flags
    *h2p = 0;
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Uncompressed size
    *h2p = 0xd3;  // int64
    h2p += 1;
    int64_t nbytes = schunk->nbytes;
    swap_store(h2p, &nbytes, sizeof(nbytes));
    h2p += 8;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Compressed size
    *h2p = 0xd3;  // int64
    h2p += 1;
    int64_t cbytes = schunk->cbytes;
    swap_store(h2p, &cbytes, sizeof(cbytes));
    h2p += 8;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Type size
    *h2p = 0xd2;  // int32
    h2p += 1;
    int32_t typesize = schunk->typesize;
    swap_store(h2p, &typesize, sizeof(typesize));
    h2p += 4;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Chunk size
    *h2p = 0xd2;  // int32
    h2p += 1;
    int32_t chunksize = schunk->chunksize;
    swap_store(h2p, &chunksize, sizeof(chunksize));
    h2p += 4;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Number of threads for compression
    *h2p = 0xd1;  // int16
    h2p += 1;
    int16_t nthreads = (int16_t)schunk->cctx->nthreads;
    swap_store(h2p, &nthreads, sizeof(nthreads));
    h2p += 2;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // Number of threads for decompression
    *h2p = 0xd1;  // int16
    h2p += 1;
    nthreads = (int16_t)schunk->dctx->nthreads;
    swap_store(h2p, &nthreads, sizeof(nthreads));
    h2p += 2;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);

    // The boolean for FRAME_HAS_USERMETA
    *h2p = (schunk->usermeta_len > 0) ? (uint8_t)0xc3 : (uint8_t)0xc2;
    h2p += 1;
    assert(h2p - h2 < PSCHUNK_HEADER_MINLEN);


    // The space for FRAME_FILTER_PIPELINE
    *h2p = 0xd8;  //  fixext 16
    h2p += 1;
    assert(BLOSC2_MAX_FILTERS <= PSCHUNK_FILTER_PIPELINE_MAX);
    // Store the filter pipeline in header
    uint8_t* mp_filters = h2 + PSCHUNK_FILTER_PIPELINE + 1;
    uint8_t* mp_meta = h2 + PSCHUNK_FILTER_PIPELINE + 1 + PSCHUNK_FILTER_PIPELINE_MAX;
    int nfilters = 0;
    for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
        if (schunk->filters[i] != BLOSC_NOFILTER) {
            mp_filters[nfilters] = schunk->filters[i];
            mp_meta[nfilters] = schunk->filters_meta[i];
            nfilters++;
        }
    }
    *h2p = (uint8_t)nfilters;
    h2p += 1;
    h2p += 16;
    assert(h2p - h2 == PSCHUNK_HEADER_MINLEN);

    int32_t hsize = PSCHUNK_HEADER_MINLEN;

    // Now, deal with metalayers
    int16_t nmetalayers = schunk->nmetalayers;

    // Make space for the header of metalayers (array marker, size, map of offsets)
    h2 = realloc(h2, (size_t)hsize + 1 + 1 + 2 + 1 + 2);
    h2p = h2 + hsize;

    // The msgpack header for the metalayers (array_marker, size, map of offsets, list of metalayers)
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
        blosc2_metalayer *metalayer = schunk->metalayers[nmetalayer];
        uint8_t namelen = (uint8_t) strlen(metalayer->name);
        h2 = realloc(h2, (size_t)current_header_len + 1 + namelen + 1 + 4);
        h2p = h2 + current_header_len;
        // Store the metalayer
        assert(namelen < (1U << 5U));  // metalayer strings cannot be longer than 32 bytes
        *h2p = (uint8_t)0xa0 + namelen;  // str
        h2p += 1;
        memcpy(h2p, metalayer->name, namelen);
        h2p += namelen;
        // Space for storing the offset for the value of this metalayer
        *h2p = 0xd2;  // int32
        h2p += 1;
        offtooff[nmetalayer] = (int32_t)(h2p - h2);
        h2p += 4;
        current_header_len += 1 + namelen + 1 + 4;
    }
    int32_t hsize2 = (int32_t)(h2p - h2);
    assert(hsize2 == current_header_len);  // sanity check

    // Map size + int16 size
    assert((uint32_t) (hsize2 - hsize) < (1U << 16U));
    uint16_t map_size = (uint16_t) (hsize2 - hsize);
    swap_store(h2 + PSCHUNK_IDX_SIZE, &map_size, sizeof(map_size));

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
        blosc2_metalayer *metalayer = schunk->metalayers[nmetalayer];
        h2 = realloc(h2, (size_t)current_header_len + 1 + 4 + metalayer->content_len);
        h2p = h2 + current_header_len;
        // Store the serialized contents for this metalayer
        *h2p = 0xc6;  // bin32
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

    // Set the length of the whole header now that we know it
    swap_store(h2 + PSCHUNK_HEADER_LEN, &hsize, sizeof(hsize));



    //Create the header file
    char headername = malloc(sizeof(uint8_t) * strlen(schunk->storage->path) + 6 + 1);
    strcpy(headername, schunk->storage->path);
    strcat(headername,"header");
    FILE *fph;
    fph = fopen(headername, "wb");
    fwrite(h2, 1,  hsize, fph);
    free(h2);
    fclose(fph);
    free(headername);

    return hsize;

}

int sparse_new_trailer(blosc2_schunk* schunk) {
    // Create the trailer in msgpack (see the frame format document)
    uint32_t trailer_len = FRAME_TRAILER_MINLEN + schunk->usermeta_len;
    uint8_t* trailer = calloc((size_t)trailer_len, 1);
    uint8_t* ptrailer = trailer;
    *ptrailer = 0x90 + 4;  // fixarray with 4 elements
    ptrailer += 1;
    // Trailer format version
    *ptrailer = FRAME_TRAILER_VERSION;
    ptrailer += 1;
    // usermeta
    *ptrailer = 0xc6;     // bin32
    ptrailer += 1;
    swap_store(ptrailer, &(schunk->usermeta_len), 4);
    ptrailer += 4;
    memcpy(ptrailer, schunk->usermeta, schunk->usermeta_len);
    ptrailer += schunk->usermeta_len;
    // Trailer length
    *ptrailer = 0xce;  // uint32
    ptrailer += 1;
    swap_store(ptrailer, &(trailer_len), sizeof(uint32_t));
    ptrailer += sizeof(uint32_t);
    // Up to 16 bytes for frame fingerprint (using XXH3 included in https://github.com/Cyan4973/xxHash)
    // Maybe someone would need 256-bit in the future, but for the time being 128-bit seems like a good tradeoff
    *ptrailer = 0xd8;  // fixext 16
    ptrailer += 1;
    *ptrailer = 0;  // fingerprint type: 0 -> no fp; 1 -> 32-bit; 2 -> 64-bit; 3 -> 128-bit
    ptrailer += 1;
    // Uncomment this when we compute an actual fingerprint
    // memcpy(ptrailer, xxh3_fingerprint, sizeof(xxh3_fingerprint));
    ptrailer += 16;
    // Sanity check
    assert(ptrailer - trailer == trailer_len);

    char *trailername = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path)) + 7 +1);
    strcpy(trailername, schunk->storage->path);
    strcat(trailername,"trailer");
    FILE *fpt = fopen(trailername,"wb");
    fwrite(trailer, 1, trailer_len, fpt);
    fclose(fpt);
    free(trailername);


    // Update the trailer.  As there are no internal offsets to the trailer section,
    // and it is always at the end of the frame, we can just write (or overwrite) it
    // at the end of the frame.

    return 1;
}

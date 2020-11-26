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


/* Append an existing chunk into a sparse schunk. */
void* pschunk_append_chunk(blosc2_schunk *schunk, uint8_t *chunk) {
    int32_t nchunks = schunk->nchunks -1;
    /* The uncompressed and compressed sizes start at byte 4 and 12 */
    int32_t nbytes = sw32_(chunk + 4);
    int32_t cbytes = sw32_(chunk + 12);

    // Check that we are not appending a small chunk after another small chunk
    if ((schunk->nchunks > 0) && (nbytes < schunk->chunksize)) {
        //get the size of the last chunk
        uint8_t *last_chunk = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path) +
                                    (nchunks / 10 + 1 + 6 + 1)));
        strcpy(last_chunk, schunk->storage->path);
        uint8_t *n = malloc(sizeof(uint8_t) * (nchunks / 10 + 1 + 1));
        sprintf(n, "%d", nchunks-1);
        strcat(last_chunk, n);
        free(n);
        strcat(last_chunk, ".chunk");
        FILE *fplast = fopen(last_chunk, "r");
        free(last_chunk);
        fseek(fplast, 4, SEEK_SET);
        int32_t last_nbytes;
        size_t rbytes = fread(last_nbytes, sizeof(int32_t), 1, fplast);
        if (rbytes != sizeof(int32_t)) {
            fprintf(stderr, "Error: cannot read from file '%s'\n", last_chunk);
            return NULL;
        }
        fclose(fplast);

        if ((last_nbytes < schunk->chunksize) && (nbytes < schunk->chunksize)) {
            fprintf(stderr,
                    "appending two consecutive chunks with a chunksize smaller than the schunk chunksize"
                    "is not allowed yet: "
                    "%d != %d", nbytes, schunk->chunksize);
            return NULL;
        }
    }


    //add chunkfile to chunks.txt
    uint8_t *filename = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path) + 10 + 1));
    strcpy(filename, schunk->storage->path);
    strcat(filename,"chunks.txt");
    FILE *fpcs = fopen(filename,"a");
    free(filename);

    //chunk name
    uint8_t *chunkname = malloc(sizeof(uint8_t) * (nchunks / 10 + 1 + 6 + 1));
    sprintf(chunkname, "%d",nchunks);
    strcat(chunkname, ".chunk");

    fputs(chunkname, fpcs);
    fputc('\n', fpcs);
    fclose(fpcs);

    //create chunk file
    uint8_t *chunkfile = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path) +
                                strlen(chunkname) + 1));
    strcpy(chunkfile, schunk->storage->path);
    strcat(chunkfile, chunkname);

    FILE *fpc = fopen(chunkfile, "wb");
    size_t wbytes = fwrite(chunk, 1, (size_t)cbytes,fpc);
    if (wbytes != (size_t)cbytes) {
        fprintf(stderr, "cannot write the full chunk.");
        return NULL;
    }
    fclose(fpc);
    free(chunkname);
    free(chunkfile);

    //update header
    int header_len = pschunk_update_header(schunk);
    if (header_len < 0){
        fprintf(stderr, "cannot update the header.");
        return NULL;
    }

}


/*Get chunk from persistent schunk. */
int pschunk_get_chunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk, bool *needs_free){
    int32_t nchunks = schunk->nchunks;

    *chunk = NULL;
    *needs_free = false;

    if (nchunk >= schunk->nchunks) {
        fprintf(stderr, "nchunk ('%d') exceeds the number of chunks "
                        "('%d') in the schunk\n", nchunk, nchunks);
        return -2;
    }
    uint8_t *chunkname = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path) +
                                (nchunk / 10 + 1 + 6 + 1)));
    strcpy(chunkname, schunk->storage->path);
    uint8_t *n = malloc(sizeof(uint8_t) * (nchunk / 10 + 1 + 1));;
    sprintf(n,"%d",nchunk);
    strcat(chunkname, n);
    free(n);
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
        return -1;
    }

    fclose(fpc);
    *needs_free = true;

    return chunk_cbytes;
}


/*Create a header out of a super-chunk. */
int pschunk_update_header(blosc2_schunk *schunk) {
    uint8_t* h2 = new_header_frame(schunk, NULL);
    uint32_t hsize;
    swap_store(&hsize, h2 + FRAME_HEADER_LEN, sizeof(hsize));

    //Create the header file
    uint8_t *headername = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path) + 6 + 1));
    strcpy(headername, schunk->storage->path);
    strcat(headername,"header");
    FILE *fph;
    fph = fopen(headername, "wb");
    size_t wbytes = fwrite(h2, 1,  hsize, fph);
    if (wbytes != hsize) {
        fprintf(stderr, "cannot write header to header file.");
        return -1;
    }
    free(h2);
    fclose(fph);
    free(headername);

    return hsize;

}


int pschunk_get_header_info(blosc2_schunk *schunk, int32_t *header_len, int64_t *nbytes,
                    int64_t *cbytes, int32_t *chunksize, int32_t *nchunks, int32_t *typesize,
                    uint8_t *compcode, uint8_t *clevel, uint8_t *filters, uint8_t *filters_meta) {
    uint8_t* headerp;
    uint8_t* header;

    header = malloc(FRAME_HEADER_MINLEN);
    uint8_t *headername = malloc(sizeof(uint8_t) * (strlen(schunk->storage->path) + 6 + 1));
    strcpy(headername, schunk->storage->path);
    strcat(headername,"header");
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
        // We can compute the number of chunks only when the pschunk has actual data
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
int pschunk_new_trailer(blosc2_schunk* schunk) {
    return frame_update_trailer(NULL, schunk);
}

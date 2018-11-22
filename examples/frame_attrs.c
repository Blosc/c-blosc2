/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc frame_attrs.c -o frame_attrs -lblosc

  To run:

  $ ./frame_attrs

 */

#include <stdio.h>
#include <assert.h>
#include <blosc.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 1
#define NTHREADS 4


int main() {
    static int32_t data[CHUNKSIZE];
    size_t isize = CHUNKSIZE * sizeof(int32_t);
    int64_t nbytes, cbytes;
    int i, nchunk;
    int nchunks;
    blosc_timestamp_t last, current;
    double ttotal;

    printf("Blosc version info: %s (%s)\n",
           BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

    /* Create a super-chunk container */
    blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
    cparams.typesize = sizeof(int32_t);
    cparams.compcode = BLOSC_LZ4;
    cparams.clevel = 9;
    cparams.nthreads = NTHREADS;
    blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
    dparams.nthreads = NTHREADS;
    blosc2_schunk* schunk = blosc2_new_schunk(cparams, dparams, NULL);

    blosc_set_timestamp(&last);
    for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
        for (i = 0; i < CHUNKSIZE; i++) {
            data[i] = i * nchunk;
        }
        nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
        assert(nchunks == nchunk + 1);
    }
    /* Gather some info */
    nbytes = schunk->nbytes;
    cbytes = schunk->cbytes;
    blosc_set_timestamp(&current);
    ttotal = blosc_elapsed_secs(last, current);
    printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
           nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
    printf("Compression time: %.3g s, %.1f MB/s\n",
           ttotal, nbytes / (ttotal * MB));

    // super-chunk -> frame1 (in-memory)
    blosc_set_timestamp(&last);
    blosc2_frame frame1 = BLOSC_EMPTY_FRAME;

    // Add some attrs
    blosc2_frame_attrs *attrs = malloc(sizeof(blosc2_frame_attrs));
    attrs->namespace = strdup("myattrs_namespace");
    attrs->sattrs = (uint8_t*)strdup("myattrs_value");
    attrs->sattrs_len = (int32_t)strlen((char*)attrs->sattrs);
    frame1.attrs[0] = attrs;
    frame1.nclients++;
    int64_t frame_len = blosc2_schunk_to_frame(schunk, &frame1);
    blosc_set_timestamp(&current);
    ttotal = blosc_elapsed_secs(last, current);
    printf("Time for schunk -> frame: %.3g s, %.1f GB/s\n",
           ttotal, nbytes / (ttotal * GB));
    printf("Frame length in memory: %lld bytes\n", frame_len);

    // frame1 (in-memory) -> fileframe (on-disk)
    blosc_set_timestamp(&last);
    frame_len = blosc2_frame_to_file(&frame1, "frame_attrs.b2frame");
    printf("Frame length on disk: %lld bytes\n", frame_len);
    blosc_set_timestamp(&current);
    ttotal = blosc_elapsed_secs(last, current);
    printf("Time for frame -> fileframe (simple_frame.b2frame): %.3g s, %.1f GB/s\n",
           ttotal, nbytes / (ttotal * GB));

    // fileframe (file) -> frame2 (on-disk frame)
    blosc_set_timestamp(&last);
    blosc2_frame* frame2 = blosc2_frame_from_file("frame_attrs.b2frame");
    blosc_set_timestamp(&current);
    ttotal = blosc_elapsed_secs(last, current);
    printf("Time for fileframe (%s) -> frame : %.3g s, %.1f GB/s\n",
           frame2->fname, ttotal, nbytes / (ttotal * GB));

    // frame2 (on-disk) -> schunk
    blosc_set_timestamp(&last);
    blosc2_schunk* schunk2 = blosc2_new_schunk(cparams, dparams, frame2);
    if (schunk2 == NULL) {
        printf("Bad conversion frame2 -> schunk2!\n");
        return -1;
    }
    blosc_set_timestamp(&current);
    ttotal = blosc_elapsed_secs(last, current);
    printf("Time for fileframe -> schunk: %.3g s, %.1f GB/s\n",
           ttotal, nbytes / (ttotal * GB));

    /* Free resources */
    blosc2_free_schunk(schunk);

    return 0;
}

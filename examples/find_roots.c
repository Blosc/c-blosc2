/*
  Copyright (C) 2017  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Example program demonstrating how the different compression params affects
  the performance of root finding.

  To compile this program:

  $ gcc -O3 find_roots.c -o find_roots -lblosc

  To run:

  $ ./find_roots
  ...

*/

#include <stdio.h>
#include <assert.h>
#include "blosc.h"
#include <time.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)


#define NCHUNKS 500
#define CHUNKSIZE 200*1000  // a chunksize that fits well in modern L3 caches

/* The type of timestamp used on this system. */
#define blosc_timestamp_t struct timespec

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
    clock_gettime(CLOCK_MONOTONIC, timestamp);
}

/* Given two timestamp values, return the difference in microseconds. */
double blosc_elapsed_usecs(blosc_timestamp_t start_time, blosc_timestamp_t end_time) {
    return (1e6 * (end_time.tv_sec - start_time.tv_sec))
           + (1e-3 * (end_time.tv_nsec - start_time.tv_nsec));
}

/* Given two timeval stamps, return the difference in seconds */
double getseconds(blosc_timestamp_t last, blosc_timestamp_t current) {
    return 1e-6 * blosc_elapsed_usecs(last, current);
}

/* Given two timeval stamps, return the time per chunk in usec */
double get_usec_chunk(blosc_timestamp_t last, blosc_timestamp_t current, int niter, size_t nchunks) {
    double elapsed_usecs = (double)blosc_elapsed_usecs(last, current);
    return elapsed_usecs / (double)(niter * nchunks);
}


inline void fill_buffer(double *x, int nchunk) {
    double incx = 10. / (NCHUNKS * CHUNKSIZE);

    for (int i = 0; i < CHUNKSIZE; i++) {
        x[i] = incx * (nchunk * CHUNKSIZE + i);
    }
}

inline void process_data(const double *x, double *y) {

    for (int i = 0; i < CHUNKSIZE; i++) {
        double xi = x[i];
        //y[i] = ((.25 * xi + .75) * xi - 1.5) * xi - 2;
        y[i] = (xi - 1.35) * (xi - 4.45) * (xi - 8.5);
    }
}

inline void find_root(const double *x, const double *y,
                      const double prev_value) {
    double pv = prev_value;
    int last_root_idx = -1;

    for (int i = 0; i < CHUNKSIZE; i++) {
        double yi = y[i];
        if (((yi > 0) - (yi < 0)) != ((pv > 0) - (pv < 0))) {
            if (last_root_idx != (i - 1)) {
                printf("%.16g, ", x[i]);
                last_root_idx = i;  // avoid the last point (ULP effects)
            }
        }
        pv = yi;
    }
}


int compute_vectors(void) {
    static double buffer_x[CHUNKSIZE];
    static double buffer_y[CHUNKSIZE];
    const int isize = CHUNKSIZE * sizeof(double);
    const int osize = CHUNKSIZE * sizeof(double);
    int dsize, csize;
    long nbytes = 0;
    blosc2_sparams sparams;
    blosc2_sheader *sc_x, *sc_y;
    int i, j, nchunks;
    blosc_timestamp_t last, current;
    double ttotal;
    double prev_value;

    /* Create a super-chunk container for input (X values) */
    sparams = BLOSC_SPARAMS_DEFAULTS;
    sparams.compressor = BLOSC_LZ4;
    sparams.clevel = 9;
    sparams.filters[0] = BLOSC_DELTA;
    sparams.filters[1] = BLOSC_SHUFFLE;
    sc_x = blosc2_new_schunk(&sparams);

    /* Create a super-chunk container for output (Y values) */
    sparams = BLOSC_SPARAMS_DEFAULTS;
    sparams.compressor = BLOSC_LZ4;
    sparams.clevel = 9;
    sparams.filters[0] = BLOSC_DELTA;
    sparams.filters[1] = BLOSC_SHUFFLE;
    sc_y = blosc2_new_schunk(&sparams);

    /* Now fill the buffer with even values between 0 and 10 */
    blosc_set_timestamp(&last);
    for (i = 0; i < NCHUNKS; i++) {
        fill_buffer(buffer_x, i);
        nchunks = blosc2_append_buffer(sc_x, sizeof(double), isize,
                                       buffer_x);
        nbytes += isize;
    }
    blosc_set_timestamp(&current);
    ttotal = (double) getseconds(last, current);
    printf("Creation time for X values: %.3g s, %.1f MB/s\n",
           ttotal, nbytes / (ttotal * MB));
    printf("Compression for X values: %.1f MB -> %.1f MB (%.1fx)\n",
           sc_x->nbytes / MB, sc_x->cbytes / MB,
           (1. * sc_x->nbytes) / sc_x->cbytes);

    /* Retrieve the chunks and compute the polynomial in another super-chunk */
    blosc_set_timestamp(&last);
    for (i = 0; i < NCHUNKS; i++) {
        dsize = blosc2_decompress_chunk(sc_x, i, (void *) buffer_x, isize);
        if (dsize < 0) {
            printf("Decompression error.  Error code: %d\n", dsize);
            return dsize;
        }
        process_data(buffer_x, buffer_y);
        nchunks = blosc2_append_buffer(sc_y, sizeof(double), isize,
                                       buffer_y);
    }
    blosc_set_timestamp(&current);
    ttotal = (double) getseconds(last, current);
    printf("Computing Y polynomial: %.3g s, %.1f MB/s\n",
           ttotal,
           2. * nbytes / (ttotal * MB));    // 2 super-chunks involved
    printf("Compression for Y values: %.1f MB -> %.1f MB (%.1fx)\n",
           sc_y->nbytes / MB, sc_y->cbytes / MB,
           (1. * sc_y->nbytes) / sc_y->cbytes);

    /* Find the roots of the polynomial */
    printf("Roots found at: ");
    blosc_set_timestamp(&last);
    for (i = 0; i < NCHUNKS; i++) {
        dsize = blosc2_decompress_chunk(sc_y, i, (void *) buffer_y, isize);
        if (i == 0) {
            prev_value = buffer_y[0];
        }
        if (dsize < 0) {
            printf("Decompression error.  Error code: %d\n", dsize);
            return dsize;
        }
        dsize = blosc2_decompress_chunk(sc_x, i, (void *) buffer_x, isize);
        if (dsize < 0) {
            printf("Decompression error.  Error code: %d\n", dsize);
            return dsize;
        }
        find_root(buffer_x, buffer_y, prev_value);
        prev_value = buffer_y[CHUNKSIZE - 1];
    }
    blosc_set_timestamp(&current);
    ttotal = (double) getseconds(last, current);
    printf("\n");
    printf("Find root time:  %.3g s, %.1f MB/s\n",
           ttotal, 2. * nbytes / (ttotal * MB));    // 2 super-chunks involved

    /* Free resources */
    /* Destroy the super-chunk */
    blosc2_destroy_schunk(sc_x);
    blosc2_destroy_schunk(sc_y);
    return 0;
}


int main() {
    printf("Blosc version info: %s (%s)\n",
           BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

    /* Initialize the Blosc compressor */
    blosc_init();

    blosc_set_nthreads(4);

    compute_vectors();

    /* Destroy the Blosc environment */
    blosc_destroy();

    return 0;
}

/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark for reading individual coordinates from a 1-D b2nd array.

  To run from the repository root:

  $ ./build/bench/read-coords bench/sin-1d.b2nd [ncoords]
*/

/*
Use:

import numpy as np

import blosc2

N = 24_000_000
a = blosc2.linspace(0., 1., N, dtype=np.float32)
b = blosc2.sin(a).compute(urlpath="sin-1d.b2nd", mode="w")
print(b[:])

for generating the data file.

*/
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <b2nd.h>
#include <blosc2.h>

#define DEFAULT_NCOORDS 100
#define NPRINT 10
#define NTHREADS 4

static void fill_random_coords(int64_t *coords, int64_t ncoords, int64_t nitems) {
  uint64_t state = UINT64_C(0x20260522);

  for (int64_t i = 0; i < ncoords; ++i) {
    /* xorshift64*: deterministic and good enough for benchmark coordinates. */
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    uint64_t r = state * UINT64_C(2685821657736338717);
    coords[i] = (int64_t)(r % (uint64_t)nitems);
  }
}

static int open_array(const char **urlpath, bool use_fallbacks, b2nd_array_t **array) {
  int rc = b2nd_open(*urlpath, array);
  if (rc >= 0 || *array != NULL || !use_fallbacks) {
    if (rc < 0) {
      fprintf(stderr, "Cannot open %s (error %d)\n", *urlpath, rc);
    }
    return rc;
  }

  const char *fallbacks[] = {"../bench/sin-1d.b2nd", "../../bench/sin-1d.b2nd"};
  for (size_t i = 0; i < sizeof(fallbacks) / sizeof(fallbacks[0]); ++i) {
    *urlpath = fallbacks[i];
    rc = b2nd_open(*urlpath, array);
    if (rc >= 0) {
      return rc;
    }
  }

  fprintf(stderr, "Cannot open sin-1d.b2nd (last error %d)\n", rc);
  return rc;
}

static int getitem_sparse(const b2nd_array_t *array, const int64_t *coords, int64_t ncoords, float *data) {
  int64_t chunk_nitems = array->sc->chunksize / array->sc->typesize;

  for (int64_t i = 0; i < ncoords; ++i) {
    int64_t coord = coords[i];
    int64_t nchunk = coord / chunk_nitems;
    int start = (int)(coord % chunk_nitems);
    uint8_t *chunk = NULL;
    bool needs_free = false;

    int cbytes = blosc2_schunk_get_lazychunk(array->sc, nchunk, &chunk, &needs_free);
    if (cbytes <= 0) {
      fprintf(stderr, "Cannot get chunk %" PRId64 " for coordinate %" PRId64 " (error %d)\n",
              nchunk, coord, cbytes);
      return cbytes < 0 ? cbytes : -1;
    }

    int nbytes = blosc2_getitem_ctx(array->sc->dctx, chunk, cbytes, start, 1, &data[i], sizeof(float));
    if (needs_free) {
      free(chunk);
    }
    if (nbytes != (int)sizeof(float)) {
      fprintf(stderr, "Cannot get coordinate %" PRId64 " from chunk %" PRId64 " (error %d)\n",
              coord, nchunk, nbytes);
      return nbytes < 0 ? nbytes : -1;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  const char *urlpath = (argc > 1) ? argv[1] : "bench/sin-1d.b2nd";
  int64_t ncoords = (argc > 2) ? strtoll(argv[2], NULL, 10) : DEFAULT_NCOORDS;
  bool new_first = (argc > 3) && (strcmp(argv[3], "new-first") == 0);
  bool getitem_only = (argc > 3) && (strcmp(argv[3], "getitem-only") == 0);
  bool new_only = (argc > 3) && (strcmp(argv[3], "new-only") == 0);
  b2nd_array_t *array = NULL;
  int64_t *coords = NULL;
  float *data_getitem = NULL;
  float *data_new = NULL;
  blosc_timestamp_t t0, t1;
  double getitem_time;
  double new_time;

  blosc2_init();
  blosc2_set_nthreads(NTHREADS);

  if (ncoords <= 0) {
    fprintf(stderr, "ncoords must be positive\n");
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (open_array(&urlpath, argc == 1, &array) < 0) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (array->ndim != 1) {
    fprintf(stderr, "Expected a 1-D array, got %d dimensions\n", array->ndim);
    b2nd_free(array);
    blosc2_destroy();
    return EXIT_FAILURE;
  }
  if (array->sc->typesize != (int32_t)sizeof(float)) {
    fprintf(stderr, "Expected float elements (typesize %zu), got typesize %d\n",
            sizeof(float), array->sc->typesize);
    b2nd_free(array);
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  coords = malloc((size_t)ncoords * sizeof(int64_t));
  data_getitem = malloc((size_t)ncoords * sizeof(float));
  data_new = malloc((size_t)ncoords * sizeof(float));
  if (coords == NULL || data_getitem == NULL || data_new == NULL) {
    fprintf(stderr, "Cannot allocate benchmark buffers\n");
    free(coords);
    free(data_getitem);
    free(data_new);
    b2nd_free(array);
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  fill_random_coords(coords, ncoords, array->shape[0]);

  if (new_first || new_only) {
    blosc_set_timestamp(&t0);
    int rc = blosc2_schunk_get_sparse(array->sc, ncoords, coords, data_new);
    blosc_set_timestamp(&t1);
    new_time = blosc_elapsed_secs(t0, t1);
    if (rc < 0) {
      fprintf(stderr, "blosc2_schunk_get_sparse failed (error %d)\n", rc);
      free(coords);
      free(data_getitem);
      free(data_new);
      b2nd_free(array);
      blosc2_destroy();
      return EXIT_FAILURE;
    }
  }
  if (!new_only) {
    blosc_set_timestamp(&t0);
    if (getitem_sparse(array, coords, ncoords, data_getitem) < 0) {
      free(coords);
      free(data_getitem);
      free(data_new);
      b2nd_free(array);
      blosc2_destroy();
      return EXIT_FAILURE;
    }
    blosc_set_timestamp(&t1);
    getitem_time = blosc_elapsed_secs(t0, t1);
  }
  if (!new_first && !getitem_only && !new_only) {
    blosc_set_timestamp(&t0);
    int rc = blosc2_schunk_get_sparse(array->sc, ncoords, coords, data_new);
    blosc_set_timestamp(&t1);
    new_time = blosc_elapsed_secs(t0, t1);
    if (rc < 0) {
      fprintf(stderr, "blosc2_schunk_get_sparse failed (error %d)\n", rc);
      free(coords);
      free(data_getitem);
      free(data_new);
      b2nd_free(array);
      blosc2_destroy();
      return EXIT_FAILURE;
    }
  }

  if (!getitem_only && !new_only) {
  for (int64_t i = 0; i < ncoords; ++i) {
    if (data_getitem[i] != data_new[i]) {
      fprintf(stderr, "Mismatched result at %" PRId64 ": getitem_sparse=%.9g, new_sparse=%.9g\n",
              i, data_getitem[i], data_new[i]);
      free(coords);
      free(data_getitem);
      free(data_new);
      b2nd_free(array);
      blosc2_destroy();
      return EXIT_FAILURE;
    }
  }
  }

  printf("Read %" PRId64 " random coordinates from %s (%d threads)\n", ncoords, urlpath, NTHREADS);
  int64_t nprint = ncoords < NPRINT ? ncoords : NPRINT;
  printf("First %" PRId64 " retrieved elements:\n", nprint);
  for (int64_t i = 0; i < nprint; ++i) {
    printf("  coord[%" PRId64 "] = %" PRId64 ", value = %.9g\n", i, coords[i], data_new[i]);
  }
  if (!new_only) {
    printf("getitem_sparse: %.9f s\n", getitem_time);
  }
  if (!getitem_only) {
    printf("new_sparse:     %.9f s\n", new_time);
  }

  free(coords);
  free(data_getitem);
  free(data_new);
  b2nd_free(array);
  blosc2_destroy();

  return EXIT_SUCCESS;
}

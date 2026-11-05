/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Read-ahead (blosc2_set_prefetch()) must not change what slices of frames on disk return (b2nd_get_slice_cbuffer(),
   b2nd_get_slice(), b2nd_get_orthogonal_selection(), blosc2_schunk_get_slice_buffer()): with the default and the mmap
   I/O; with chunks that are special values; with blocks stored in order, out of order (several compression threads)
   and as plain copies (clevel 0), as slices read only the blocks they need of the chunks they cover in part; with a
   read-ahead budget smaller than a chunk; with small and large pieces; on an empty array; after fork(); and with
   several threads of a process reading at once (their slices share the process's read-ahead pool). */

#include "test_common.h"
#include "blosc2/blosc2-stdio.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define URLPATH "test_b2nd_prefetch.b2nd"
#define NDIM 3

static const int64_t shape[NDIM] = {40, 50, 61};
static const int32_t chunkshape[NDIM] = {10, 20, 16};
static const int32_t blockshape[NDIM] = {5, 8, 8};

/* Slices: whole array, inside one chunk, across chunks, a single item, one that also covers the zero chunks */
static const int64_t starts[][NDIM] = {{0, 0, 0}, {11, 21, 17}, {3, 7, 9}, {39, 49, 60}, {0, 30, 0}};
static const int64_t stops[][NDIM] = {{40, 50, 61}, {19, 39, 31}, {37, 44, 58}, {40, 50, 61}, {40, 50, 30}};
#define NSLICES ((int) (sizeof(starts) / sizeof(starts[0])))
/* Each slice is read four ways: b2nd_get_slice_cbuffer(), b2nd_get_slice(), b2nd_get_orthogonal_selection() (with
   every index of the slice), and, last, blosc2_schunk_get_slice_buffer() (a range of items of the super-chunk) */
#define NWAYS 4

typedef struct {
  int32_t *data;
  int64_t nbytes;
} result_t;

#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAILED: %s (line %d)\n", msg, __LINE__); \
                                              return 1; } } while (0)


static int create_array(int clevel, int nthreads) {
  blosc2_remove_urlpath(URLPATH);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = clevel;
  cparams.nthreads = nthreads;
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  storage.contiguous = true;
  storage.urlpath = URLPATH;
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, NDIM, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  CHECK(ctx != NULL, "create ctx");
  b2nd_array_t *array;
  CHECK(b2nd_zeros(ctx, &array) == 0, "zeros");
  // Data in the first 30 rows of axis 1 only: the chunks beyond stay special values (zeros) in the index
  int64_t box_start[NDIM] = {0, 0, 0};
  int64_t box_stop[NDIM] = {40, 30, 61};
  int64_t box_shape[NDIM] = {40, 30, 61};
  int64_t n = 40 * 30 * 61;
  int32_t *data = malloc(n * sizeof(int32_t));
  for (int64_t i = 0; i < n; ++i) {
    data[i] = (int32_t) ((i * 2654435761u) >> 7);  // varied, so that chunks compress to different sizes
  }
  CHECK(b2nd_set_slice_cbuffer(data, box_shape, n * (int64_t) sizeof(int32_t), box_start, box_stop, array) == 0,
        "set slice");
  free(data);
  b2nd_free(array);
  b2nd_free_ctx(ctx);
  return 0;
}


static int open_array(bool mmap_io, b2nd_array_t **array, blosc2_stdio_mmap *mmap_file) {
  if (!mmap_io) {
    return b2nd_open(URLPATH, array);
  }
  *mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file->mode = "r";
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = mmap_file};
  blosc2_schunk *schunk = blosc2_schunk_open_udio(URLPATH, &io);
  if (schunk == NULL) {
    return -1;
  }
  return b2nd_from_schunk(schunk, array);
}


static int64_t slice_nitems(int s) {
  int64_t n = 1;
  for (int i = 0; i < NDIM; ++i) {
    n *= stops[s][i] - starts[s][i];
  }
  return n;
}


/* Drop the file from the page cache, so that read-ahead has something to read (where the OS lets us) */
static void evict(void) {
#if defined(POSIX_FADV_DONTNEED)
  int fd = open(URLPATH, O_RDONLY);
  if (fd >= 0) {
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);
  }
#endif
}


static int read_slice(b2nd_array_t *array, int s, result_t *out) {
  int64_t sshape[NDIM];
  int64_t *selection[NDIM];
  for (int i = 0; i < NDIM; ++i) {
    sshape[i] = stops[s][i] - starts[s][i];
    selection[i] = malloc(sshape[i] * sizeof(int64_t));
    for (int64_t j = 0; j < sshape[i]; ++j) {
      selection[i][j] = starts[s][i] + j;
    }
  }
  int64_t nbytes = slice_nitems(s) * (int64_t) sizeof(int32_t);
  for (int w = 0; w < NWAYS - 1; ++w) {
    out[w].nbytes = nbytes;
    out[w].data = malloc(nbytes);
  }
  CHECK(b2nd_get_slice_cbuffer(array, starts[s], stops[s], out[0].data, sshape, nbytes) == 0, "get slice");

  // Into an array in memory with small chunks, so that it takes several source chunks per chunk
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  const int32_t small_chunks[NDIM] = {4, 8, 8};
  const int32_t small_blocks[NDIM] = {2, 4, 4};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, NDIM, sshape, small_chunks, small_blocks, NULL, 0, NULL, 0);
  b2nd_array_t *sliced;
  CHECK(ctx != NULL && b2nd_get_slice(ctx, &sliced, array, starts[s], stops[s]) == 0, "get slice as array");
  CHECK(b2nd_to_cbuffer(sliced, out[1].data, nbytes) == 0, "array to buffer");
  b2nd_free(sliced);
  b2nd_free_ctx(ctx);

  CHECK(b2nd_get_orthogonal_selection(array, selection, sshape, out[2].data, sshape, nbytes) == 0,
        "orthogonal selection");
  for (int i = 0; i < NDIM; ++i) {
    free(selection[i]);
  }

  int64_t nitems = array->sc->nbytes / array->sc->typesize;
  int64_t first = (s * 7919) % nitems;
  int64_t count = nitems - first < 20000 ? nitems - first : 20000;
  out[3].nbytes = count * (int64_t) sizeof(int32_t);
  out[3].data = malloc(out[3].nbytes);
  CHECK(blosc2_schunk_get_slice_buffer(array->sc, first, first + count, out[3].data) == 0, "schunk slice");
  return 0;
}


/* Read all slices (with the current read-ahead setting), each in all ways */
static int read_slices(bool mmap_io, result_t out[][NWAYS]) {
  b2nd_array_t *array;
  blosc2_stdio_mmap mmap_file;
  CHECK(open_array(mmap_io, &array, &mmap_file) == 0, "open");
  for (int s = 0; s < NSLICES; ++s) {
    CHECK(read_slice(array, s, out[s]) == 0, "read slice");
  }
  b2nd_free(array);
  return 0;
}


static int compare_slices(result_t a[][NWAYS], result_t b[][NWAYS]) {
  for (int s = 0; s < NSLICES; ++s) {
    for (int w = 0; w < NWAYS; ++w) {
      CHECK(a[s][w].nbytes == b[s][w].nbytes && memcmp(a[s][w].data, b[s][w].data, a[s][w].nbytes) == 0,
            "slice differs with read-ahead");
    }
  }
  return 0;
}


static void free_slices(result_t slices[][NWAYS]) {
  for (int s = 0; s < NSLICES; ++s) {
    for (int w = 0; w < NWAYS; ++w) {
      free(slices[s][w].data);
      slices[s][w].data = NULL;
    }
  }
}


/* Read the slices cold (file evicted) and warm, and compare them with the expected ones */
static int check_reads(bool mmap_io, result_t expected[][NWAYS]) {
  result_t got[NSLICES][NWAYS];
  int rc = 0;
  for (int warm = 0; warm < 2 && rc == 0; ++warm) {
    if (!warm) {
      evict();
    }
    rc |= read_slices(mmap_io, got);
    rc |= compare_slices(expected, got);
    free_slices(got);
  }
  return rc;
}


#if !defined(_WIN32)
#define NREADERS 6

typedef struct {
  bool mmap_io;
  result_t (*expected)[NWAYS];
  int rc;
} reader_t;


static void* concurrent_reader(void *arg) {
  reader_t *reader = arg;
  reader->rc = check_reads(reader->mmap_io, reader->expected);
  return NULL;
}


/* NREADERS threads read the slices at the same time, cold and warm, each with its own handle */
static int check_concurrent_reads(bool mmap_io, result_t expected[][NWAYS]) {
  pthread_t threads[NREADERS];
  reader_t readers[NREADERS];
  int nstarted = 0;
  while (nstarted < NREADERS) {
    readers[nstarted].mmap_io = mmap_io;
    readers[nstarted].expected = expected;
    readers[nstarted].rc = 1;
    if (pthread_create(&threads[nstarted], NULL, concurrent_reader, &readers[nstarted]) != 0) {
      break;
    }
    nstarted++;
  }
  int rc = nstarted == NREADERS ? 0 : 1;
  for (int i = 0; i < nstarted; ++i) {
    pthread_join(threads[i], NULL);
    rc |= readers[i].rc;
  }
  return rc;
}
#endif


/* An array on disk with an empty axis (chunk shape 0 there): nothing to read ahead, and no division by zero */
static int check_empty(void) {
  const char *urlpath = "test_b2nd_prefetch_empty.b2nd";
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  storage.contiguous = true;
  storage.urlpath = (char *) urlpath;
  const int64_t eshape[NDIM] = {5, 0, 3};
  const int32_t echunks[NDIM] = {5, 0, 3};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, NDIM, eshape, echunks, echunks, NULL, 0, NULL, 0);
  b2nd_array_t *array;
  CHECK(ctx != NULL && b2nd_zeros(ctx, &array) == 0, "empty array");
  int64_t sel0[] = {0, 4};
  int64_t sel1[1] = {0};
  int64_t sel2[] = {1};
  int64_t *selection[NDIM] = {sel0, sel1, sel2};
  int64_t selection_size[NDIM] = {2, 0, 1};
  int32_t buffer[1];
  CHECK(b2nd_get_orthogonal_selection(array, selection, selection_size, buffer, selection_size, 0) == 0,
        "orthogonal selection of an empty array");
  b2nd_free(array);
  b2nd_free_ctx(ctx);
  blosc2_remove_urlpath(urlpath);
  return 0;
}


typedef struct {
  int clevel;
  int nthreads;  // compression threads: with several, blocks are stored in the order they are done
} layout_t;

typedef struct {
  int nthreads;
  int64_t max_ahead;
  int64_t piece_size;
} prefetch_t;


CUTEST_TEST_SETUP(prefetch) {
  blosc2_init();
  CUTEST_PARAMETRIZE(layout, layout_t, CUTEST_DATA(
      {5, 1},  // blocks in order
      {5, 4},  // blocks out of order
      {0, 1},  // blocks stored as plain copies
  ));
  CUTEST_PARAMETRIZE(setting, prefetch_t, CUTEST_DATA(
      {4, 0, 0},
      {1, 1, 0},  // a budget smaller than any chunk: each chunk is only requested when the slice waits for it
      {3, 0, BLOSC2_PREFETCH_MIN_PIECE_SIZE},
      {2, 0, 4 * 1024 * 1024},  // pieces larger than the file
  ));
  CUTEST_PARAMETRIZE(mmap_io, bool, CUTEST_DATA(false, true));
}


CUTEST_TEST_TEST(prefetch) {
  CUTEST_GET_PARAMETER(layout, layout_t);
  CUTEST_GET_PARAMETER(setting, prefetch_t);
  CUTEST_GET_PARAMETER(mmap_io, bool);

  CUTEST_ASSERT("Cannot create the array", create_array(layout.clevel, layout.nthreads) == 0);

  result_t expected[NSLICES][NWAYS];
  blosc2_set_prefetch(0, 0, 0);
  CUTEST_ASSERT("Cannot read the slices", read_slices(mmap_io, expected) == 0);
  for (int s = 0; s < NSLICES; ++s) {
    for (int w = 1; w < NWAYS - 1; ++w) {
      CUTEST_ASSERT("Ways of reading a slice differ",
                    memcmp(expected[s][0].data, expected[s][w].data, expected[s][0].nbytes) == 0);
    }
  }

  CUTEST_ASSERT("Cannot set read-ahead",
                blosc2_set_prefetch(setting.nthreads, setting.max_ahead, setting.piece_size) >= 0);
  CUTEST_ASSERT("Slices differ with read-ahead", check_reads(mmap_io, expected) == 0);
  CUTEST_ASSERT("Empty array", check_empty() == 0);
#if !defined(_WIN32)
  // The threads run in this process now (the cold reads above started them); a child must start its own
  fflush(NULL);
  pid_t pid = fork();
  if (pid == 0) {
    _exit(check_reads(mmap_io, expected));
  }
  int status;
  CUTEST_ASSERT("Cannot fork", pid > 0 && waitpid(pid, &status, 0) == pid);
  CUTEST_ASSERT("Slices differ with read-ahead in a forked child", WIFEXITED(status) && WEXITSTATUS(status) == 0);
  CUTEST_ASSERT("Slices differ with read-ahead in threads reading at once",
                check_concurrent_reads(mmap_io, expected) == 0);
#endif
  free_slices(expected);

  CUTEST_ASSERT("Negative nthreads accepted", blosc2_set_prefetch(-1, 0, 0) < 0);
  CUTEST_ASSERT("Negative max_ahead accepted", blosc2_set_prefetch(1, -1, 0) < 0);
  CUTEST_ASSERT("Piece size below the minimum accepted",
                blosc2_set_prefetch(1, 0, BLOSC2_PREFETCH_MIN_PIECE_SIZE - 1) < 0);
  blosc2_set_prefetch(0, 0, 0);
  blosc2_remove_urlpath(URLPATH);
  return 0;
}


CUTEST_TEST_TEARDOWN(prefetch) {
  blosc2_destroy();
}


int main(void) {
  CUTEST_TEST_RUN(prefetch);
}

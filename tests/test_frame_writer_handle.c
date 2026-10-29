/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  In-place updates of an on-disk contiguous frame go through a writable handle
  that the frame keeps open between calls, and the trailer update only truncates
  the file when the frame got shorter.  Check that the file still ends exactly
  at the frame end after every kind of update (append, update to a smaller and a
  larger chunk, insert, delete), after reopening, and when the frame file is
  replaced underneath the open handle; that a write to a removed frame file
  fails instead of getting lost; and that with locking on no writer stays open
  once the lock is released.

  Run with "nocache" to set the handle budget to zero, so that every call falls
  back to a private per-call handle, and with "budget1" to allow a single cached
  handle, so that a frame can end up with a cached writer but no cached reader.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <fcntl.h>
#endif
#include "test_common.h"
#include "frame.h"

#define CHUNKSIZE (200 * 1000)  /* int32 items */
#define NCHUNKS 6
#define REPLACED_BASE 1000000

int tests_run = 0;

/* Named after the run's mode, as ctest runs the modes in parallel */
static char URLPATH[64];
static char URLPATH2[64];

static int32_t* data;
static int32_t* rdata;
static blosc2_cparams cparams;
static blosc2_dparams dparams;


/* A chunk whose values identify it; noisy ones compress much worse than flat ones */
static void fill(int32_t base, bool noisy) {
  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = noisy ? base + (int32_t)((i * 2654435761u) >> 8) : base;
  }
}


#if !defined(_WIN32)
static int count_open_fds(void) {
  int n = 0;
  for (int fd = 0; fd < 1024; fd++) {
    if (fcntl(fd, F_GETFD) != -1) {
      n++;
    }
  }
  return n;
}
#endif


static int64_t file_size(const char* urlpath) {
  struct stat st;
  if (stat(urlpath, &st) != 0) {
    return -1;
  }
  return (int64_t)st.st_size;
}


static char* check_file_ends_at_frame_end(blosc2_schunk* schunk, const char* urlpath) {
  /* The on-disk length; blosc2_schunk_to_buffer() would serialize a compacted copy */
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  int64_t frame_end = frame->file_offset + frame->len;
  int64_t size = file_size(urlpath);
  if (size != frame_end) {
    fprintf(stderr, "file size %lld != frame end %lld\n", (long long)size, (long long)frame_end);
  }
  mu_assert("The file does not end at the frame end", size == frame_end);
  return EXIT_SUCCESS;
}


/* Compress data[] into chunk with the schunk's own context */
static int compress_data(blosc2_schunk* schunk, uint8_t* chunk) {
  return blosc2_compress_ctx(schunk->cctx, data, CHUNKSIZE * sizeof(int32_t), chunk,
                             CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
}


/* expected[n] is the base of chunk n, noisy[n] how it was filled */
static char* check_contents(blosc2_schunk* schunk, const int32_t* expected, const bool* noisy, int nchunks) {
  mu_assert("Wrong number of chunks", schunk->nchunks == nchunks);
  for (int n = 0; n < nchunks; n++) {
    int32_t nbytes = blosc2_schunk_decompress_chunk(schunk, n, rdata, CHUNKSIZE * sizeof(int32_t));
    mu_assert("Cannot decompress chunk", nbytes == (int32_t)(CHUNKSIZE * sizeof(int32_t)));
    fill(expected[n], noisy[n]);
    mu_assert("Wrong chunk contents", memcmp(data, rdata, CHUNKSIZE * sizeof(int32_t)) == 0);
  }
  return EXIT_SUCCESS;
}


static char* test_updates(void) {
  int32_t expected[NCHUNKS + 2];
  bool noisy[NCHUNKS + 2];
  int nchunks = 0;

  blosc2_remove_urlpath(URLPATH);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH,
                            .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create the schunk", schunk != NULL);
  mu_assert("A new frame must end at its end", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);

  /* Grow */
  for (int n = 0; n < NCHUNKS; n++) {
    expected[n] = n * CHUNKSIZE;
    noisy[n] = true;
    fill(expected[n], noisy[n]);
    mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
    nchunks++;
    mu_assert("append", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  }

  /* Update a middle chunk to a much smaller one (the tail moves down)... */
  int nupdate = NCHUNKS / 2;
  expected[nupdate] = 42;
  noisy[nupdate] = false;
  fill(expected[nupdate], noisy[nupdate]);
  uint8_t* chunk = malloc(CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
  mu_assert("Cannot compress", compress_data(schunk, chunk) > 0);
  mu_assert("Cannot update", blosc2_schunk_update_chunk(schunk, nupdate, chunk, true) >= 0);
  mu_assert("update (shrink)", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  mu_assert("update (shrink)", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);

  /* ...and back to a bigger one (the tail moves up) */
  expected[nupdate] = 7 * CHUNKSIZE;
  noisy[nupdate] = true;
  fill(expected[nupdate], noisy[nupdate]);
  mu_assert("Cannot compress", compress_data(schunk, chunk) > 0);
  mu_assert("Cannot update", blosc2_schunk_update_chunk(schunk, nupdate, chunk, true) >= 0);
  mu_assert("update (grow)", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  mu_assert("update (grow)", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);

  /* Insert at the front */
  fill(-1, true);
  mu_assert("Cannot compress", compress_data(schunk, chunk) > 0);
  mu_assert("Cannot insert", blosc2_schunk_insert_chunk(schunk, 0, chunk, true) >= 0);
  memmove(expected + 1, expected, nchunks * sizeof(int32_t));
  memmove(noisy + 1, noisy, nchunks * sizeof(bool));
  expected[0] = -1;
  noisy[0] = true;
  nchunks++;
  mu_assert("insert", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  mu_assert("insert", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);

  /* Shrink: delete the last two chunks, so the file must be cut */
  for (int i = 0; i < 2; i++) {
    mu_assert("Cannot delete", blosc2_schunk_delete_chunk(schunk, nchunks - 1) >= 0);
    nchunks--;
    mu_assert("delete", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  }
  mu_assert("delete", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);

  /* Reopen (the file size is unknown to the new handle) and grow, then shrink again */
  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot reopen", schunk != NULL);
  mu_assert("reopen", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);
  expected[nchunks] = 9 * CHUNKSIZE;
  noisy[nchunks] = true;
  fill(expected[nchunks], true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  nchunks++;
  mu_assert("append after reopen", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  mu_assert("Cannot delete", blosc2_schunk_delete_chunk(schunk, 0) >= 0);
  memmove(expected, expected + 1, (nchunks - 1) * sizeof(int32_t));
  memmove(noisy, noisy + 1, (nchunks - 1) * sizeof(bool));
  nchunks--;
  mu_assert("delete after reopen", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  mu_assert("after reopen", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);

  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot reopen", schunk != NULL);
  mu_assert("final reopen", check_contents(schunk, expected, noisy, nchunks) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);
  free(chunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* Replace the frame file underneath an open schunk that has already written
   through its cached handle: the next write must land in the replacement (as
   it did when every write reopened the path), not in the unlinked old inode. */
static char* test_replaced_file(void) {
  blosc2_remove_urlpath(URLPATH);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH,
                            .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create the schunk", schunk != NULL);
  for (int n = 0; n < 2; n++) {
    fill(n * CHUNKSIZE, true);
    mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  }

  blosc2_remove_urlpath(URLPATH2);
  blosc2_storage storage2 = {.contiguous=true, .urlpath=URLPATH2,
                             .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* replacement = blosc2_schunk_new(&storage2);
  mu_assert("Cannot create the replacement", replacement != NULL);
  for (int n = 0; n < 3; n++) {
    fill(REPLACED_BASE + n * CHUNKSIZE, true);
    mu_assert("Cannot append", blosc2_schunk_append_buffer(replacement, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  }
  blosc2_schunk_free(replacement);
  blosc2_remove_urlpath(URLPATH);
  mu_assert("Cannot move the replacement in place", rename(URLPATH2, URLPATH) == 0);

  /* Reading first lets the cached reader notice the replacement */
  int32_t nbytes = blosc2_schunk_decompress_chunk(schunk, 0, rdata, CHUNKSIZE * sizeof(int32_t));
  mu_assert("Cannot decompress", nbytes == (int32_t)(CHUNKSIZE * sizeof(int32_t)));
  fill(REPLACED_BASE, true);
  mu_assert("Reads must see the replacement", memcmp(data, rdata, CHUNKSIZE * sizeof(int32_t)) == 0);

  fill(REPLACED_BASE + 3 * CHUNKSIZE, true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  mu_assert("replaced", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);

  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot reopen", schunk != NULL);
  int32_t expected[4] = {REPLACED_BASE, REPLACED_BASE + CHUNKSIZE, REPLACED_BASE + 2 * CHUNKSIZE,
                         REPLACED_BASE + 3 * CHUNKSIZE};
  bool noisy[4] = {true, true, true, true};
  mu_assert("The append must land in the replacement",
            check_contents(schunk, expected, noisy, 4) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* A frame can end up with a cached writer but no cached reader: the handle
   budget was full when it first read, and a slot came free before it first
   wrote.  Replacing its file must still redirect the writer.  Only the
   "budget1" run gets there; the others take the usual paths. */
static char* test_replaced_file_writer_only(void) {
  blosc2_storage storage = {.contiguous=true, .cparams=&cparams, .dparams=&dparams};
  char* urlpaths[2] = {URLPATH, URLPATH2};
  for (int i = 0; i < 2; i++) {
    blosc2_remove_urlpath(urlpaths[i]);
    storage.urlpath = urlpaths[i];
    blosc2_schunk* schunk = blosc2_schunk_new(&storage);
    mu_assert("Cannot create the schunk", schunk != NULL);
    for (int n = 0; n < 2 + i; n++) {
      fill(i * REPLACED_BASE + n * CHUNKSIZE, true);
      mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
    }
    blosc2_schunk_free(schunk);
  }

  /* Another schunk holds the only slot while this one first reads... */
  blosc2_schunk* other = blosc2_schunk_open(URLPATH2);
  mu_assert("Cannot open the other schunk", other != NULL);
  mu_assert("Cannot decompress",
            blosc2_schunk_decompress_chunk(other, 0, rdata, CHUNKSIZE * sizeof(int32_t)) > 0);
  blosc2_schunk* schunk = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot open the schunk", schunk != NULL);
  mu_assert("Cannot decompress",
            blosc2_schunk_decompress_chunk(schunk, 0, rdata, CHUNKSIZE * sizeof(int32_t)) > 0);
  /* ...and gives it up before it first writes */
  blosc2_schunk_free(other);
  fill(2 * CHUNKSIZE, true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);

  blosc2_remove_urlpath(URLPATH);
  mu_assert("Cannot move the replacement in place", rename(URLPATH2, URLPATH) == 0);
  int32_t nbytes = blosc2_schunk_decompress_chunk(schunk, 0, rdata, CHUNKSIZE * sizeof(int32_t));
  mu_assert("Cannot decompress", nbytes == (int32_t)(CHUNKSIZE * sizeof(int32_t)));
  fill(REPLACED_BASE, true);
  mu_assert("Reads must see the replacement", memcmp(data, rdata, CHUNKSIZE * sizeof(int32_t)) == 0);
  fill(REPLACED_BASE + 3 * CHUNKSIZE, true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  mu_assert("replaced (writer only)", check_file_ends_at_frame_end(schunk, URLPATH) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);

  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot reopen", schunk != NULL);
  int32_t expected[4] = {REPLACED_BASE, REPLACED_BASE + CHUNKSIZE, REPLACED_BASE + 2 * CHUNKSIZE,
                         REPLACED_BASE + 3 * CHUNKSIZE};
  bool noisy[4] = {true, true, true, true};
  mu_assert("The append must land in the replacement",
            check_contents(schunk, expected, noisy, 4) == EXIT_SUCCESS);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* Remove the frame file underneath an open schunk that has already written
   through its cached handle: the next write must fail, as it did when every
   write reopened the path, instead of going to the unlinked inode. */
static char* test_removed_file(void) {
  blosc2_remove_urlpath(URLPATH);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH,
                            .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create the schunk", schunk != NULL);
  fill(0, true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);

  blosc2_remove_urlpath(URLPATH);
  mu_assert("An append to a removed file must fail",
            blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) < 0);
  mu_assert("The failed append must not recreate the file", file_size(URLPATH) < 0);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}


/* With locking on, each lock holder must see the writes of the one before, so
   no write may stay behind in a cached writer once the lock is let go: the
   unlock closes the writer, and a write outside the lock (blosc2_meta_update
   takes none) must not cache one in the first place.  POSIX only, as handles
   are never cached on Windows. */
static char* test_locking(void) {
#if !defined(_WIN32)
  blosc2_stdio_params ioparams = {.locking = true};
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &ioparams};
  blosc2_remove_urlpath(URLPATH);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH,
                            .cparams=&cparams, .dparams=&dparams, .io=&io};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create the schunk", schunk != NULL);
  uint8_t meta[8] = {0};
  mu_assert("Cannot add a metalayer", blosc2_meta_add(schunk, "writer", meta, sizeof(meta)) >= 0);
  fill(0, true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  /* The cached reader and the lock sidecar stay open from here on */
  int nfds = count_open_fds();

  fill(CHUNKSIZE, true);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) > 0);
  mu_assert("The unlock must close the writer", count_open_fds() == nfds);
  meta[0] = 42;
  mu_assert("Cannot update the metalayer", blosc2_meta_update(schunk, "writer", meta, sizeof(meta)) >= 0);
  mu_assert("A write outside the lock must not keep a writer open", count_open_fds() == nfds);
  blosc2_schunk_free(schunk);

  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot reopen", schunk != NULL);
  int32_t expected[2] = {0, CHUNKSIZE};
  bool noisy[2] = {true, true};
  mu_assert("locking", check_contents(schunk, expected, noisy, 2) == EXIT_SUCCESS);
  uint8_t* content;
  int32_t content_len;
  mu_assert("Cannot get the metalayer", blosc2_meta_get(schunk, "writer", &content, &content_len) >= 0);
  mu_assert("The metalayer update is lost", content_len == sizeof(meta) && content[0] == 42);
  free(content);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
#endif
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  mu_run_test(test_updates);
  mu_run_test(test_replaced_file);
  mu_run_test(test_replaced_file_writer_only);
  mu_run_test(test_removed_file);
  mu_run_test(test_locking);
  return EXIT_SUCCESS;
}


int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "cached";
  const char* budget = NULL;
  if (strcmp(mode, "nocache") == 0) {
    budget = "0";
  }
  else if (strcmp(mode, "budget1") == 0) {
    budget = "1";
  }
  else if (strcmp(mode, "cached") != 0) {
    printf("Unknown mode: %s\n", mode);
    return EXIT_FAILURE;
  }
  snprintf(URLPATH, sizeof(URLPATH), "test_frame_writer_handle_%s.b2frame", mode);
  snprintf(URLPATH2, sizeof(URLPATH2), "test_frame_writer_handle_%s_2.b2frame", mode);
  if (budget != NULL) {
    /* Read once per process, on the first cached open */
#if defined(_WIN32)
    _putenv_s("BLOSC_MAX_CACHED_READERS", budget);
#else
    setenv("BLOSC_MAX_CACHED_READERS", budget, 1);
#endif
  }
  blosc2_init();
  cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  cparams.nthreads = 2;
  dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 2;
  data = malloc(CHUNKSIZE * sizeof(int32_t));
  rdata = malloc(CHUNKSIZE * sizeof(int32_t));

  char* result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  free(data);
  free(rdata);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}

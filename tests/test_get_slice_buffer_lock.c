/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  A slice read of a super-chunk reads several chunks.  With locking on, each
  chunk access used to take the shared frame lock on its own, and the blocks
  of a lazy chunk were read after it was let go, so a writer could rewrite the
  frame in the middle of the read, although it held the exclusive lock across
  each rewrite.  blosc2_schunk_get_slice_buffer() now holds the shared lock
  from the refresh to the last chunk read.  A writer child rewrites all chunks
  with a new version (dropping the last chunk and appending it again, so the
  frame layout changes too) under one exclusive bracket per version, while the
  parent reads a slice across all chunks: every read must succeed and see one
  version only.  POSIX-only, like the other fork-based locking tests.
*/

#include <stdio.h>
#include "test_common.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#define NCHUNKS (8)
#define CHUNKITEMS (16 * 1024)  /* int64 items */
#define URLPATH "test_get_slice_buffer_lock.b2frame"

/* Global vars */
int tests_run = 0;


#if !defined(_WIN32)
static blosc2_io* locking_io(void) {
  static blosc2_stdio_params ioparams = {.locking = true};
  static blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &ioparams};
  return &io;
}


/* The items of version `version`: the version in the high bits, plus noise
   whose range (and so the compressed size) varies with the version */
static void fill_version(int64_t *data, int64_t version) {
  uint32_t x = 12345u + (uint32_t) version;
  for (int64_t i = 0; i < CHUNKITEMS; i++) {
    x = x * 1103515245u + 12345u;
    data[i] = (version << 32) + (int64_t) ((x >> 8) % (uint32_t) (1 + (version % 5) * 1000));
  }
}


static int rewrite(blosc2_schunk *schunk, int64_t version, int64_t *data, uint8_t *chunk,
                   int32_t chunk_size) {
  fill_version(data, version);
  int csize = blosc2_compress_ctx(schunk->cctx, data, CHUNKITEMS * (int32_t) sizeof(int64_t),
                                  chunk, chunk_size);
  if (csize <= 0) {
    return -1;
  }
  if (blosc2_schunk_delete_chunk(schunk, NCHUNKS - 1) != NCHUNKS - 1) {
    return -1;
  }
  for (int64_t n = 0; n < NCHUNKS - 1; n++) {
    if (blosc2_schunk_update_chunk(schunk, n, chunk, true) != NCHUNKS - 1) {
      return -1;
    }
  }
  return blosc2_schunk_append_chunk(schunk, chunk, true) == NCHUNKS ? 0 : -1;
}


static char* test_fork_rewriting_writer(void) {
  const int writer_iters = 300;
  const int32_t chunk_size = CHUNKITEMS * (int32_t) sizeof(int64_t) + BLOSC2_MAX_OVERHEAD;
  int64_t *data = malloc(CHUNKITEMS * sizeof(int64_t));
  uint8_t *chunk = malloc(chunk_size);
  mu_assert("ERROR: cannot allocate", data != NULL && chunk != NULL);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.nthreads = 1;
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams,
                            .io = locking_io()};
  blosc2_remove_urlpath(URLPATH);
  blosc2_schunk *creator = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create the frame", creator != NULL);
  fill_version(data, 0);
  for (int n = 0; n < NCHUNKS; n++) {
    mu_assert("ERROR: cannot fill the frame",
              blosc2_schunk_append_buffer(creator, data, CHUNKITEMS * sizeof(int64_t)) == n + 1);
  }
  blosc2_schunk_free(creator);

  pid_t pid = fork();
  mu_assert("ERROR: cannot fork the writer", pid >= 0);
  if (pid == 0) {
    blosc2_schunk *w = blosc2_schunk_open_udio(URLPATH, locking_io());
    if (w == NULL) {
      _exit(1);
    }
    for (int64_t version = 1; version <= writer_iters; version++) {
      if (blosc2_schunk_lock(w) != 0) {
        _exit(2);
      }
      int rc = rewrite(w, version, data, chunk, chunk_size);
      if (blosc2_schunk_unlock(w) != 0 || rc < 0) {
        _exit(3);
      }
    }
    blosc2_schunk_free(w);
    _exit(0);
  }

  blosc2_schunk *r = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the reader handle", r != NULL);
  // From the middle of the first chunk to the middle of the last one
  int64_t start = CHUNKITEMS / 2;
  int64_t stop = NCHUNKS * CHUNKITEMS - CHUNKITEMS / 2;
  int64_t *rbuffer = malloc((stop - start) * sizeof(int64_t));
  mu_assert("ERROR: cannot allocate the read buffer", rbuffer != NULL);

  long nreads = 0, nfailed = 0, ntorn = 0;
  int status = 0;
  bool done = false;
  while (!done) {
    done = waitpid(pid, &status, WNOHANG) == pid;  // one last read after the writer is gone
    if (blosc2_schunk_get_slice_buffer(r, start, stop, rbuffer) < 0) {
      nfailed++;
      continue;
    }
    nreads++;
    for (int64_t i = 1; i < stop - start; i++) {
      if (rbuffer[i] >> 32 != rbuffer[0] >> 32) {
        ntorn++;
        break;
      }
    }
  }
  free(rbuffer);
  free(chunk);
  free(data);
  printf("(%ld reads, %ld failed, %ld torn) ", nreads + nfailed, nfailed, ntorn);
  mu_assert("ERROR: a locked read failed while the writer rewrote the frame", nfailed == 0);
  mu_assert("ERROR: a locked read mixed chunks of several versions", ntorn == 0);
  mu_assert("ERROR: the writer child failed", WIFEXITED(status) && WEXITSTATUS(status) == 0);

  blosc2_schunk_free(r);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}
#endif  /* !_WIN32 */


static char *all_tests(void) {
#if !defined(_WIN32)
  mu_run_test(test_fork_rewriting_writer);
#endif
  return EXIT_SUCCESS;
}


int main(void) {
  char* result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != EXIT_SUCCESS;
}

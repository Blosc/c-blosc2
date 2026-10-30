/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  A slice read refreshes the shape and then reads the chunks of that shape.
  With locking on, each chunk access used to take the shared frame lock on
  its own, so a writer could shrink the array between the refresh and the
  chunk reads and the read failed ("nchunk exceeds the number of chunks"),
  although the writer held the exclusive lock across its whole resize + fill.
  b2nd_get_slice_cbuffer() now holds the shared lock from the refresh to the
  last chunk read.  A writer child resizes the array up and down (filling the
  rows it adds with their row number) under one exclusive bracket per resize,
  while the parent reads the whole array with b2nd_to_cbuffer(): every read
  must succeed and see rows that are either filled (row i == i) or zero.
  POSIX-only, like the other fork-based locking tests.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#define MAXROWS (64)
#define NCOLS (16)
#define CHUNKROWS (4)
#define URLPATH "test_b2nd_get_slice_lock.b2nd"

/* Global vars */
int tests_run = 0;


#if !defined(_WIN32)
static b2nd_array_t* create_array(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.nthreads = 1;
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
  blosc2_remove_urlpath(URLPATH);

  int64_t shape[] = {MAXROWS, NCOLS};
  int32_t chunkshape[] = {CHUNKROWS, NCOLS};
  int32_t blockshape[] = {CHUNKROWS / 2, NCOLS};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                        NULL, 0, NULL, 0);
  if (ctx == NULL) {
    return NULL;
  }
  b2nd_array_t *array = NULL;
  if (b2nd_zeros(ctx, &array) < 0) {
    array = NULL;
  }
  b2nd_free_ctx(ctx);
  return array;
}


static int fill_rows(b2nd_array_t *array, int64_t row0, int64_t row1) {
  int64_t n = (row1 - row0) * NCOLS;
  int64_t *buffer = malloc(n * sizeof(int64_t));
  if (buffer == NULL) {
    return -1;
  }
  for (int64_t i = 0; i < n; i++) {
    buffer[i] = row0 + i / NCOLS;
  }
  int64_t start[] = {row0, 0};
  int64_t stop[] = {row1, NCOLS};
  int64_t bshape[] = {row1 - row0, NCOLS};
  int rc = b2nd_set_slice_cbuffer(buffer, bshape, n * (int64_t)sizeof(int64_t), start, stop, array);
  free(buffer);
  return rc;
}


static char* test_fork_shrinking_writer(void) {
  const int writer_iters = 400;

  /* b2nd_open() has no udio variant to pass a per-handle locking io, so drive
     locking through the environment, which fork() passes on */
  setenv("BLOSC_LOCKING", "1", 1);
  b2nd_array_t *creator = create_array();
  mu_assert("ERROR: cannot create the array", creator != NULL);
  mu_assert("ERROR: cannot fill the array", fill_rows(creator, 0, MAXROWS) >= 0);
  b2nd_free(creator);

  pid_t pid = fork();
  mu_assert("ERROR: cannot fork the writer", pid >= 0);
  if (pid == 0) {
    b2nd_array_t *w = NULL;
    if (b2nd_open(URLPATH, &w) < 0) {
      _exit(1);
    }
    unsigned seed = 1;
    for (int i = 0; i < writer_iters; i++) {
      seed = seed * 1103515245u + 12345u;
      int64_t nrows = 1 + (int64_t)((seed >> 16) % MAXROWS);
      int64_t old_rows = w->shape[0];
      int64_t new_shape[] = {nrows, NCOLS};
      if (blosc2_schunk_lock(w->sc) != 0) {
        _exit(2);
      }
      int rc = b2nd_resize(w, new_shape, NULL);
      if (rc >= 0 && nrows > old_rows) {
        rc = fill_rows(w, old_rows, nrows);
      }
      if (blosc2_schunk_unlock(w->sc) != 0 || rc < 0) {
        _exit(3);
      }
    }
    b2nd_free(w);
    _exit(0);
  }

  b2nd_array_t *r = NULL;
  mu_assert("ERROR: cannot open the reader handle", b2nd_open(URLPATH, &r) >= 0);
  int64_t *rbuffer = malloc(MAXROWS * NCOLS * sizeof(int64_t));
  mu_assert("ERROR: cannot allocate the read buffer", rbuffer != NULL);

  long nreads = 0, nfailed = 0, nbad = 0;
  int status = 0;
  bool done = false;
  while (!done) {
    done = waitpid(pid, &status, WNOHANG) == pid;  // one last read after the writer is gone
    if (b2nd_to_cbuffer(r, rbuffer, MAXROWS * NCOLS * (int64_t)sizeof(int64_t)) < 0) {
      nfailed++;
      continue;
    }
    nreads++;
    for (int64_t i = 0; i < r->shape[0] * NCOLS; i++) {
      if (rbuffer[i] != i / NCOLS && rbuffer[i] != 0) {
        nbad++;
        break;
      }
    }
  }
  free(rbuffer);
  printf("(%ld reads, %ld failed) ", nreads + nfailed, nfailed);
  mu_assert("ERROR: a locked read failed while the writer resized", nfailed == 0);
  mu_assert("ERROR: a locked read returned rows of another shape", nbad == 0);
  mu_assert("ERROR: the writer child failed", WIFEXITED(status) && WEXITSTATUS(status) == 0);

  b2nd_free(r);
  blosc2_remove_urlpath(URLPATH);
  setenv("BLOSC_LOCKING", "", 1);  // "" means off, like unset
  return EXIT_SUCCESS;
}
#endif  /* !_WIN32 */


static char *all_tests(void) {
#if !defined(_WIN32)
  mu_run_test(test_fork_shrinking_writer);
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

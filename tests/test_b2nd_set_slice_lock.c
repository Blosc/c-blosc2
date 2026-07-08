/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  A slice write spanning multiple chunks used to be N independently-locked
  chunk updates: two writers on overlapping slices could interleave at chunk
  granularity, so a locked reader could observe a half-applied write (a
  mixed result, not corruption). b2nd_set_slice_cbuffer() now holds the
  exclusive frame lock across the whole multi-chunk write, like b2nd_resize()
  already does (plans/todo-locking-swmr.md item 2; python-blosc2's
  todo/locking-mwmr.md item 2). The fork-based hammer below is one writer vs.
  readers only; this pins the two-*writer* case specifically. POSIX-only,
  like the other fork-based locking tests.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#define NROWS (200)
#define NCOLS (50)
#define URLPATH "test_b2nd_set_slice_lock.b2nd"

/* Global vars */
int tests_run = 0;


#if !defined(_WIN32)
/* b2nd_open() has no udio variant to pass a per-handle locking io, so this
   test drives locking through the BLOSC_LOCKING env var instead (like
   test_frame_lock.c's test_env_locking); fork() copies it to both children. */
static void enable_locking(void) {
  setenv("BLOSC_LOCKING", "1", 1);
}


static b2nd_array_t* create_array(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
  blosc2_remove_urlpath(URLPATH);

  int64_t shape[] = {NROWS, NCOLS};
  int32_t chunkshape[] = {NROWS / 4, NCOLS};
  int32_t blockshape[] = {NROWS / 4, NCOLS};
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


/* Two writer processes repeatedly overwrite the *whole* (multi-chunk) array
   with their own distinguishable constant value, through their own locked
   handle. Meanwhile the parent repeatedly reads the whole array under its
   own exclusive bracket (blosc2_schunk_lock()/unlock(), which nests the
   per-chunk reads into one atomic read) and asserts every single element is
   the same value -- i.e. it only ever sees one writer's complete pass,
   never a mix of both. */
static char* test_fork_two_writers_set_slice(void) {
  const int child_iters = 150;

  enable_locking();
  b2nd_array_t *creator = create_array();
  mu_assert("ERROR: cannot create the array", creator != NULL);
  b2nd_free(creator);

  pid_t pids[2];
  for (int child = 0; child < 2; child++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork a writer", pid >= 0);
    if (pid == 0) {
      b2nd_array_t *w = NULL;
      if (b2nd_open(URLPATH, &w) < 0) {
        _exit(1);
      }
      int32_t value = child == 0 ? 1 : 2;
      int32_t *buffer = malloc(NROWS * NCOLS * sizeof(int32_t));
      if (buffer == NULL) {
        _exit(2);
      }
      for (int i = 0; i < NROWS * NCOLS; i++) {
        buffer[i] = value;
      }
      int64_t start[] = {0, 0};
      int64_t stop[] = {NROWS, NCOLS};
      int64_t bshape[] = {NROWS, NCOLS};
      for (int i = 0; i < child_iters; i++) {
        if (b2nd_set_slice_cbuffer(buffer, bshape, NROWS * NCOLS * (int64_t)sizeof(int32_t),
                                   start, stop, w) < 0) {
          _exit(3);
        }
      }
      free(buffer);
      b2nd_free(w);
      _exit(0);
    }
    pids[child] = pid;
  }

  b2nd_array_t *r = NULL;
  mu_assert("ERROR: cannot open the reader handle", b2nd_open(URLPATH, &r) >= 0);

  int32_t *rbuffer = malloc(NROWS * NCOLS * sizeof(int32_t));
  mu_assert("ERROR: cannot allocate the read buffer", rbuffer != NULL);
  int64_t start[] = {0, 0};
  int64_t stop[] = {NROWS, NCOLS};
  int64_t bshape[] = {NROWS, NCOLS};

  bool mixed = false;
  bool children_done[2] = {false, false};
  int statuses[2] = {0, 0};
  long nreads = 0;
  while (!children_done[0] || !children_done[1]) {
    for (int child = 0; child < 2; child++) {
      if (!children_done[child] && waitpid(pids[child], &statuses[child], WNOHANG) == pids[child]) {
        children_done[child] = true;  // do one final read below before checking status
      }
    }
    mu_assert("ERROR: cannot take the reader bracket", blosc2_schunk_lock(r->sc) == 0);
    int rc = b2nd_get_slice_cbuffer(r, start, stop, rbuffer, bshape,
                                    NROWS * NCOLS * (int64_t)sizeof(int32_t));
    mu_assert("ERROR: cannot release the reader bracket", blosc2_schunk_unlock(r->sc) == 0);
    if (rc < 0) {
      mixed = true;  // treat a read failure as a hard failure too
      break;
    }
    int32_t first = rbuffer[0];
    if (first != 0 && first != 1 && first != 2) {
      mixed = true;
      break;
    }
    for (int i = 1; i < NROWS * NCOLS; i++) {
      if (rbuffer[i] != first) {
        mixed = true;
        break;
      }
    }
    nreads++;
    if (mixed) {
      break;
    }
  }
  free(rbuffer);
  printf("(%ld whole-array reads) ", nreads);
  mu_assert("ERROR: a locked reader observed a mixed (half-applied) slice write", !mixed);

  bool children_ok = true;
  for (int child = 0; child < 2; child++) {
    if (!children_done[child]) {
      waitpid(pids[child], &statuses[child], 0);
    }
    if (!WIFEXITED(statuses[child]) || WEXITSTATUS(statuses[child]) != 0) {
      children_ok = false;
    }
  }
  mu_assert("ERROR: a writer child failed", children_ok);

  b2nd_free(r);
  blosc2_remove_urlpath(URLPATH);
  setenv("BLOSC_LOCKING", "", 1);  // "" means off, like unset
  return EXIT_SUCCESS;
}
#endif  /* !_WIN32 */


static char *all_tests(void) {
#if !defined(_WIN32)
  mu_run_test(test_fork_two_writers_set_slice);
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

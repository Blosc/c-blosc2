/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Threads do not survive fork(): a child has only the thread that forked.  A
  child of a process that compressed or decompressed with several threads must
  still work: with new contexts, with the contexts and the global settings it
  inherited, after blosc2_set_nthreads(), and after blosc2_destroy().  Each
  scenario runs in its own child under a watchdog (alarm()), so a hang fails
  the test instead of blocking it; the parent keeps working afterwards.
  POSIX-only, like the other fork-based tests.
*/

#include <stdio.h>
#include "test_common.h"
#include "../blosc/context.h"

#if !defined(_WIN32)
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define NITEMS (64 * 1024)   /* 256 KiB of int32: 16 blocks */
#define BLOCKSIZE (16 * 1024)
#define NTHREADS 4
#define CHILD_TIMEOUT 30     /* seconds */
#define NBUSY_FORKS 16

/* Global vars */
int tests_run = 0;


#if !defined(_WIN32)
static int32_t src[NITEMS];
static uint8_t cdata[NITEMS * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD];
static int cbytes;
/* Used with NTHREADS threads by the parent, inherited by the children */
static blosc2_context *cctx;
static blosc2_context *dctx;


static blosc2_context *new_cctx(int16_t nthreads) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.blocksize = BLOCKSIZE;
  cparams.nthreads = nthreads;
  return blosc2_create_cctx(cparams);
}

static blosc2_context *new_dctx(int16_t nthreads) {
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  return blosc2_create_dctx(dparams);
}


/* 0 if @p chunk decompresses (with @p ctx, or the global context if NULL) to src */
static int check_decompress(blosc2_context *ctx, const uint8_t *chunk, int nbytes) {
  static int32_t dest[NITEMS];
  memset(dest, 0, sizeof(dest));
  int dsize = ctx != NULL ? blosc2_decompress_ctx(ctx, chunk, nbytes, dest, sizeof(dest)) :
                            blosc2_decompress(chunk, nbytes, dest, sizeof(dest));
  return dsize == (int)sizeof(dest) && memcmp(src, dest, sizeof(dest)) == 0 ? 0 : -1;
}


/* 0 if compressing src with @p ctx_c and decompressing it with @p ctx_d works (the global context where NULL) */
static int check_roundtrip(blosc2_context *ctx_c, blosc2_context *ctx_d) {
  static uint8_t chunk[sizeof(cdata)];
  int nbytes = ctx_c != NULL ? blosc2_compress_ctx(ctx_c, src, sizeof(src), chunk, sizeof(chunk)) :
                               blosc2_compress(5, BLOSC_SHUFFLE, sizeof(int32_t), src, sizeof(src),
                                               chunk, sizeof(chunk));
  return nbytes > 0 ? check_decompress(ctx_d, chunk, nbytes) : -1;
}


/* 0 if new contexts with @p nthreads threads compress and decompress src */
static int check_new_contexts(int16_t nthreads) {
  blosc2_context *c = new_cctx(nthreads);
  blosc2_context *d = new_dctx(nthreads);
  int rc = c != NULL && d != NULL ? check_roundtrip(c, d) : -1;
  if (c != NULL) blosc2_free_ctx(c);
  if (d != NULL) blosc2_free_ctx(d);
  return rc;
}


/* Run @p fn in a forked child under a watchdog, and blosc2_destroy() after it there; 0 if both worked */
static int run_in_child(int (*fn)(void), const char *name) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == 0) {
    alarm(CHILD_TIMEOUT);
    int rc = fn();
    if (rc == 0) {
      blosc2_destroy();
    }
    exit(rc);
  }
  int status;
  if (pid < 0 || waitpid(pid, &status, 0) != pid) {
    fprintf(stderr, "%s: fork() or waitpid() failed\n", name);
    return -1;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return 0;
  }
  if (WIFSIGNALED(status)) {
    fprintf(stderr, "%s: the child got signal %d%s\n", name, WTERMSIG(status),
            WTERMSIG(status) == SIGALRM ? " (it hung)" : "");
  }
  else {
    fprintf(stderr, "%s: the child failed in step %d\n", name, WEXITSTATUS(status));
  }
  return -1;
}


/* The children; the exit code is the step that failed */

static int child_roundtrip(void) {
  return check_roundtrip(NULL, NULL) < 0 ? 1 : 0;
}

static int child_new_contexts(void) {
  // NTHREADS and 3 threads: the parent had a pool for each
  if (check_new_contexts(NTHREADS) < 0) return 1;
  if (check_new_contexts(3) < 0) return 2;
  // A grandchild of a process that started its own pools
  blosc2_context *d = new_dctx(NTHREADS);
  if (d == NULL || check_decompress(d, cdata, cbytes) < 0) return 3;
  if (run_in_child(child_roundtrip, "grandchild") < 0) return 4;
  if (check_decompress(d, cdata, cbytes) < 0) return 5;
  blosc2_free_ctx(d);
  return 0;
}

static int child_inherited_contexts(void) {
  if (check_decompress(dctx, cdata, cbytes) < 0) return 1;
  if (check_roundtrip(cctx, dctx) < 0) return 2;
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return 0;
}

static int child_global(void) {
  if (blosc2_get_nthreads() != NTHREADS) return 1;
  if (check_decompress(NULL, cdata, cbytes) < 0) return 2;
  if (check_roundtrip(NULL, NULL) < 0) return 3;
  return 0;
}

static int child_set_nthreads(void) {
  // The global context holds the last reference to the parent's pool now
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  if (blosc2_set_nthreads(1) != NTHREADS) return 1;
  if (check_decompress(NULL, cdata, cbytes) < 0) return 2;
  blosc2_context *d = new_dctx(1);
  if (d == NULL || check_decompress(d, cdata, cbytes) < 0) return 3;
  blosc2_free_ctx(d);
  if (blosc2_set_nthreads(NTHREADS) != 1) return 4;
  if (check_roundtrip(NULL, NULL) < 0) return 5;
  return 0;
}

static int child_destroy(void) {
  blosc2_destroy();
  blosc2_init();
  blosc2_set_nthreads(NTHREADS);
  blosc1_set_blocksize(BLOCKSIZE);
  if (check_roundtrip(NULL, NULL) < 0) return 1;
  if (check_new_contexts(NTHREADS) < 0) return 2;
  return 0;
}


static char *test_parent_threads(void) {
  for (int i = 0; i < NITEMS; i++) {
    src[i] = i % 1000 + (i / 1000) * 7;
  }
  blosc2_set_nthreads(NTHREADS);
  blosc1_set_blocksize(BLOCKSIZE);
  cctx = new_cctx(NTHREADS);
  dctx = new_dctx(NTHREADS);
  mu_assert("ERROR: cannot create the contexts", cctx != NULL && dctx != NULL);
  cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), cdata, sizeof(cdata));
  mu_assert("ERROR: cannot compress", cbytes > 0);
  mu_assert("ERROR: cannot decompress", check_decompress(dctx, cdata, cbytes) == 0);
  mu_assert("ERROR: the decompression threads did not start", dctx->thread_pool != NULL);
  // The global context with NTHREADS threads, and a pool with 3 threads
  mu_assert("ERROR: global roundtrip failed", check_roundtrip(NULL, NULL) == 0);
  blosc2_context *c3 = new_cctx(3);
  mu_assert("ERROR: 3-thread roundtrip failed", c3 != NULL && check_roundtrip(c3, NULL) == 0);
  mu_assert("ERROR: the 3 threads did not start", c3->thread_pool != NULL);
  blosc2_free_ctx(c3);
  return EXIT_SUCCESS;
}


/* Each child forks from the parent as test_parent_threads() left it */
static char *test_children(void) {
  static const struct {
    int (*fn)(void);
    const char *name;
  } children[] = {
    {child_new_contexts, "new contexts"},
    {child_inherited_contexts, "inherited contexts"},
    {child_global, "global context"},
    {child_set_nthreads, "set_nthreads"},
    {child_destroy, "destroy"},
  };
  int failed = 0;
  for (size_t i = 0; i < ARRAY_SIZE(children); i++) {
    failed += run_in_child(children[i].fn, children[i].name) < 0;
  }
  mu_assert("ERROR: forked children failed (see above)", failed == 0);
  return EXIT_SUCCESS;
}


/* Another thread changes the global thread count and compresses while this one forks */
static pthread_mutex_t busy_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool busy_stop = false;

static bool busy_stopped(void) {
  pthread_mutex_lock(&busy_mutex);
  bool stop = busy_stop;
  pthread_mutex_unlock(&busy_mutex);
  return stop;
}

static void *busy_worker(void *arg) {
  BLOSC_UNUSED_PARAM(arg);
  for (int i = 0; !busy_stopped(); i++) {
    blosc2_set_nthreads(i % 2 ? 2 : NTHREADS);
    if (check_roundtrip(NULL, NULL) < 0) {
      return (void *)1;
    }
  }
  return NULL;
}

static char *test_fork_while_busy(void) {
  pthread_t thread;
  mu_assert("ERROR: cannot start the busy thread", pthread_create(&thread, NULL, busy_worker, NULL) == 0);
  alarm(2 * CHILD_TIMEOUT);  // a fork() that does not return fails the test as well
  int failed = 0;
  for (int i = 0; i < NBUSY_FORKS && !failed; i++) {
    failed = run_in_child(child_roundtrip, "fork while busy") < 0;
  }
  pthread_mutex_lock(&busy_mutex);
  busy_stop = true;
  pthread_mutex_unlock(&busy_mutex);
  void *busy_rc;
  pthread_join(thread, &busy_rc);
  alarm(0);
  mu_assert("ERROR: child forked while another thread used blosc2", !failed);
  mu_assert("ERROR: the busy thread failed", busy_rc == NULL);
  blosc2_set_nthreads(NTHREADS);
  return EXIT_SUCCESS;
}


/* The parent still works with its threads and contexts */
static char *test_parent_after_fork(void) {
  mu_assert("ERROR: parent decompression", check_decompress(dctx, cdata, cbytes) == 0);
  mu_assert("ERROR: parent roundtrip", check_roundtrip(cctx, dctx) == 0);
  mu_assert("ERROR: parent global roundtrip", check_roundtrip(NULL, NULL) == 0);
  blosc2_set_nthreads(1);
  mu_assert("ERROR: parent serial roundtrip", check_roundtrip(NULL, NULL) == 0);
  blosc2_set_nthreads(NTHREADS);
  mu_assert("ERROR: parent global roundtrip again", check_roundtrip(NULL, NULL) == 0);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}
#endif  /* !_WIN32 */


static char *all_tests(void) {
#if !defined(_WIN32)
  mu_run_test(test_parent_threads);
  mu_run_test(test_children);
  mu_run_test(test_fork_while_busy);
  mu_run_test(test_parent_after_fork);
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

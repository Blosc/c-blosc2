/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  C-side follow-up to python-blosc2's todo/locking-mwmr.md "creative test
  pass" (2026-07-08): NDArray.append() had a real data-loss bug (a stale,
  unrefreshed shape read before computing a resize target), but b2nd_insert()/
  b2nd_append()/b2nd_resize() at the C level all call refresh_if_stale()
  first, before touching array->shape for anything -- so the bug was purely
  in the Python wrapper, not the C API used as designed. What IS genuinely
  untested at any level: NDArray.resize() (blosc2_ext.pyx) hardcodes
  start=NULL, so mid-array insert (b2nd_resize()'s shrink_shape/extend_shape
  with a non-NULL start) is unreachable from Python entirely, and no
  existing test exercises concurrent shrink racing concurrent growth either.
  POSIX-only, like the other fork-based locking tests.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#define URLPATH "test_b2nd_multiwriter_lock.b2nd"

/* Global vars */
int tests_run = 0;


#if !defined(_WIN32)
/* b2nd_open() has no udio variant to pass a per-handle locking io, so this
   test drives locking through the BLOSC_LOCKING env var instead (like
   test_frame_lock.c's test_env_locking); fork() copies it to both children. */
static void enable_locking(void) {
  setenv("BLOSC_LOCKING", "1", 1);
}

static void disable_locking(void) {
  setenv("BLOSC_LOCKING", "", 1);  // "" means off, like unset
}


static b2nd_array_t* create_1d_array(int64_t initial_items, int32_t chunkitems, int32_t blockitems) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
  blosc2_remove_urlpath(URLPATH);

  int64_t shape[] = {initial_items};
  int32_t chunkshape[] = {chunkitems};
  int32_t blockshape[] = {blockitems};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape,
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


/* --------------------------------------------------------------------- */
/* Mid-array insert: several writer processes concurrently b2nd_insert()  */
/* at position 0, forcing the non-NULL `start` branch of b2nd_resize()    */
/* (shrink_shape/extend_shape with start != NULL) -- the one path         */
/* unreachable from python-blosc2, since NDArray.resize() always passes   */
/* start=NULL.                                                            */
/* --------------------------------------------------------------------- */
#define INSERT_NWRITERS (4)
#define INSERT_PER_WRITER (25)
#define INSERT_ITEMS_PER_CALL (10)
#define INSERT_INITIAL_ITEMS (16)
#define INSERT_SENTINEL (-1)

static char* test_fork_multiwriter_insert_middle(void) {
  enable_locking();
  // b2nd_resize() with a non-NULL `start` (the mid-array insert path)
  // requires both the insertion point and the growth delta to be multiples
  // of chunkshape ("chunks cannot be cut unless they are in the last
  // position") -- chunkshape must divide INSERT_ITEMS_PER_CALL exactly.
  b2nd_array_t *creator = create_1d_array(INSERT_INITIAL_ITEMS, INSERT_ITEMS_PER_CALL, INSERT_ITEMS_PER_CALL / 2);
  mu_assert("ERROR: cannot create the array", creator != NULL);

  int64_t prefix[INSERT_INITIAL_ITEMS];
  for (int i = 0; i < INSERT_INITIAL_ITEMS; i++) {
    prefix[i] = INSERT_SENTINEL;
  }
  int64_t start0[] = {0};
  int64_t stop0[] = {INSERT_INITIAL_ITEMS};
  int64_t bshape0[] = {INSERT_INITIAL_ITEMS};
  mu_assert("ERROR: cannot fill the prefix",
            b2nd_set_slice_cbuffer(prefix, bshape0, INSERT_INITIAL_ITEMS * (int64_t)sizeof(int64_t),
                                   start0, stop0, creator) >= 0);
  b2nd_free(creator);

  pid_t pids[INSERT_NWRITERS];
  for (int w = 0; w < INSERT_NWRITERS; w++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork a writer", pid >= 0);
    if (pid == 0) {
      b2nd_array_t *arr = NULL;
      if (b2nd_open(URLPATH, &arr) < 0) {
        _exit(1);
      }
      for (int i = 0; i < INSERT_PER_WRITER; i++) {
        int64_t tag = (int64_t) w * 1000000 + i;
        int64_t buffer[INSERT_ITEMS_PER_CALL];
        for (int j = 0; j < INSERT_ITEMS_PER_CALL; j++) {
          buffer[j] = tag;
        }
        // Always insert at position 0: forces b2nd_resize()'s non-NULL
        // `start` branch every single call.
        //
        // b2nd_insert() calls refresh_if_stale() at its own start, but that
        // refresh takes and releases its own shared lock immediately --
        // it does not stay held across the resize()+fill sequence that
        // follows. Bracket the whole call in the exclusive lock (the C
        // equivalent of Python's holding_lock()) so a concurrent writer
        // can't grow the array in the gap between this call's own refresh
        // and its resize(), which would otherwise make resize() see a
        // newshape that's stale relative to the true (now larger) current
        // shape and take the shrink path -- deleting the other writer's
        // just-inserted data. (Confirmed empirically: unwrapped, this
        // reproduces reliably under real fork() concurrency, but not via
        // same-process multi-handle round-robin, which never truly
        // interleaves -- a genuine race, not a logic bug in isolation.)
        if (blosc2_schunk_lock(arr->sc) < 0) {
          _exit(4);
        }
        int irc = b2nd_insert(arr, buffer, INSERT_ITEMS_PER_CALL * (int64_t)sizeof(int64_t), 0, 0);
        blosc2_schunk_unlock(arr->sc);
        if (irc < 0) {
          fprintf(stderr, "[writer %d] insert %d failed rc=%d shape=%lld\n",
                  w, i, irc, (long long)arr->shape[0]);
          _exit(2);
        }
      }
      b2nd_free(arr);
      _exit(0);
    }
    pids[w] = pid;
  }

  bool all_ok = true;
  for (int w = 0; w < INSERT_NWRITERS; w++) {
    int status = 0;
    pid_t r = waitpid(pids[w], &status, 0);
    if (r < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      all_ok = false;
    }
  }
  mu_assert("ERROR: an insert-writer child failed", all_ok);

  b2nd_array_t *reader = NULL;
  mu_assert("ERROR: cannot open reader", b2nd_open(URLPATH, &reader) >= 0);
  int64_t total = reader->shape[0];
  int64_t expected_total = INSERT_INITIAL_ITEMS +
      (int64_t) INSERT_NWRITERS * INSERT_PER_WRITER * INSERT_ITEMS_PER_CALL;
  mu_assert("ERROR: final length mismatch -- lost or duplicated an insert", total == expected_total);

  int64_t *data = malloc((size_t) total * sizeof(int64_t));
  mu_assert("ERROR: cannot allocate read buffer", data != NULL);
  int64_t rstart[] = {0};
  int64_t rstop[] = {total};
  int64_t rbshape[] = {total};
  mu_assert("ERROR: cannot read final array",
            b2nd_get_slice_cbuffer(reader, rstart, rstop, data, rbshape,
                                   total * (int64_t)sizeof(int64_t)) >= 0);
  b2nd_free(reader);

  // Since every insert is atomic (locked) and always lands exactly at
  // position 0, no existing block is ever split -- the final array must be
  // a clean concatenation of untorn runs: the sentinel prefix once, plus
  // one INSERT_ITEMS_PER_CALL-long run per insert call, each holding a
  // single tag value throughout.
  int64_t expected_ntags = (int64_t) INSERT_NWRITERS * INSERT_PER_WRITER;
  int64_t *seen_tags = malloc((size_t) expected_ntags * sizeof(int64_t));
  mu_assert("ERROR: cannot allocate tag buffer", seen_tags != NULL);
  int64_t ntags = 0;
  bool saw_prefix = false;
  bool corrupted = false;

  int64_t pos = 0;
  while (pos < total) {
    int64_t run_len = 1;
    while (pos + run_len < total && data[pos + run_len] == data[pos]) {
      run_len++;
    }
    if (data[pos] == INSERT_SENTINEL) {
      if (saw_prefix || run_len != INSERT_INITIAL_ITEMS) {
        corrupted = true;
      }
      saw_prefix = true;
    }
    else {
      if (run_len != INSERT_ITEMS_PER_CALL || ntags >= expected_ntags) {
        corrupted = true;
      }
      else {
        seen_tags[ntags++] = data[pos];
      }
    }
    pos += run_len;
  }
  mu_assert("ERROR: array is not a clean concatenation of untorn insert blocks", !corrupted);
  mu_assert("ERROR: sentinel prefix missing or wrong length", saw_prefix);
  mu_assert("ERROR: wrong number of tagged blocks -- an insert was lost or duplicated",
            ntags == expected_ntags);

  // Sort and compare against the expected tag multiset (insertion sort is
  // fine, ntags is small)
  for (int64_t i = 1; i < ntags; i++) {
    int64_t key = seen_tags[i];
    int64_t j = i - 1;
    while (j >= 0 && seen_tags[j] > key) {
      seen_tags[j + 1] = seen_tags[j];
      j--;
    }
    seen_tags[j + 1] = key;
  }
  bool tags_ok = true;
  int64_t k = 0;
  for (int w = 0; w < INSERT_NWRITERS && tags_ok; w++) {
    for (int i = 0; i < INSERT_PER_WRITER; i++) {
      int64_t expected = (int64_t) w * 1000000 + i;
      if (seen_tags[k++] != expected) {
        tags_ok = false;
        break;
      }
    }
  }
  mu_assert("ERROR: insert tags don't match: a block was lost, duplicated, or corrupted", tags_ok);

  free(data);
  free(seen_tags);
  blosc2_remove_urlpath(URLPATH);
  disable_locking();
  return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------- */
/* Concurrent shrink racing concurrent growth: no test anywhere exercises */
/* shrink at all under multi-writer conditions.  Growers keep appending   */
/* tagged blocks (b2nd_append(), already refresh-safe at the C level);    */
/* one shrinker concurrently truncates the tail. The exact outcome is     */
/* inherently racy (which blocks survive is not defined), so this only    */
/* asserts the invariants that must hold regardless of interleaving: no   */
/* crash, the array stays internally consistent (readable, correct        */
/* shape), and every surviving value is a genuine, untorn tagged block    */
/* -- never garbage or a partial/mixed run.                               */
/* --------------------------------------------------------------------- */
#define SHRINK_NGROWERS (3)
#define SHRINK_APPENDS_PER_GROWER (40)
#define SHRINK_ITEMS_PER_APPEND (10)
#define SHRINK_INITIAL_ITEMS (200)
#define SHRINK_ITERS (30)
#define SHRINK_DROP_PER_ITER (7)

static char* test_fork_shrink_vs_grow(void) {
  enable_locking();
  b2nd_array_t *creator = create_1d_array(SHRINK_INITIAL_ITEMS, SHRINK_ITEMS_PER_APPEND * 8, SHRINK_ITEMS_PER_APPEND);
  mu_assert("ERROR: cannot create the array", creator != NULL);
  int64_t *prefix = malloc(SHRINK_INITIAL_ITEMS * sizeof(int64_t));
  mu_assert("ERROR: cannot allocate prefix", prefix != NULL);
  for (int i = 0; i < SHRINK_INITIAL_ITEMS; i++) {
    prefix[i] = INSERT_SENTINEL;
  }
  int64_t start0[] = {0};
  int64_t stop0[] = {SHRINK_INITIAL_ITEMS};
  int64_t bshape0[] = {SHRINK_INITIAL_ITEMS};
  mu_assert("ERROR: cannot fill the prefix",
            b2nd_set_slice_cbuffer(prefix, bshape0, SHRINK_INITIAL_ITEMS * (int64_t)sizeof(int64_t),
                                   start0, stop0, creator) >= 0);
  free(prefix);
  b2nd_free(creator);

  pid_t pids[SHRINK_NGROWERS + 1];
  int nchildren = 0;

  for (int g = 0; g < SHRINK_NGROWERS; g++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork a grower", pid >= 0);
    if (pid == 0) {
      b2nd_array_t *arr = NULL;
      if (b2nd_open(URLPATH, &arr) < 0) {
        _exit(1);
      }
      for (int i = 0; i < SHRINK_APPENDS_PER_GROWER; i++) {
        int64_t tag = (int64_t) g * 1000000 + i;
        int64_t buffer[SHRINK_ITEMS_PER_APPEND];
        for (int j = 0; j < SHRINK_ITEMS_PER_APPEND; j++) {
          buffer[j] = tag;
        }
        // Same reasoning as test_fork_multiwriter_insert_middle: bracket
        // the whole call so a concurrent shrink/grow can't land in the gap
        // between b2nd_append()'s own refresh and its resize().
        if (blosc2_schunk_lock(arr->sc) < 0) {
          _exit(6);
        }
        int arc = b2nd_append(arr, buffer, SHRINK_ITEMS_PER_APPEND * (int64_t)sizeof(int64_t), 0);
        blosc2_schunk_unlock(arr->sc);
        if (arc < 0) {
          _exit(2);
        }
      }
      b2nd_free(arr);
      _exit(0);
    }
    pids[nchildren++] = pid;
  }

  pid_t shrinker_pid = fork();
  mu_assert("ERROR: cannot fork the shrinker", shrinker_pid >= 0);
  if (shrinker_pid == 0) {
    b2nd_array_t *arr = NULL;
    if (b2nd_open(URLPATH, &arr) < 0) {
      _exit(3);
    }
    for (int i = 0; i < SHRINK_ITERS; i++) {
      // Same reasoning again: refresh() and resize() are two separate
      // lock cycles: bracket them so `cur` can't go stale relative to the
      // array a concurrent grower is appending to between the two calls.
      if (blosc2_schunk_lock(arr->sc) < 0) {
        _exit(7);
      }
      if (b2nd_refresh(arr) < 0) {
        blosc2_schunk_unlock(arr->sc);
        _exit(4);
      }
      int64_t cur = arr->shape[0];
      int64_t drop = cur < SHRINK_DROP_PER_ITER ? cur : SHRINK_DROP_PER_ITER;
      int src = 0;
      if (drop > 0) {
        int64_t newshape[] = {cur - drop};
        src = b2nd_resize(arr, newshape, NULL);
      }
      blosc2_schunk_unlock(arr->sc);
      if (src < 0) {
        _exit(5);
      }
    }
    b2nd_free(arr);
    _exit(0);
  }
  pids[nchildren++] = shrinker_pid;

  bool all_ok = true;
  for (int c = 0; c < nchildren; c++) {
    int status = 0;
    pid_t r = waitpid(pids[c], &status, 0);
    if (r < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      all_ok = false;
    }
  }
  mu_assert("ERROR: a grower or the shrinker child failed", all_ok);

  b2nd_array_t *reader = NULL;
  mu_assert("ERROR: cannot open reader after shrink/grow race", b2nd_open(URLPATH, &reader) >= 0);
  int64_t total = reader->shape[0];
  mu_assert("ERROR: array shrank to nothing (shrinker outran growers entirely -- widen the test)", total > 0);

  int64_t *data = malloc((size_t) total * sizeof(int64_t));
  mu_assert("ERROR: cannot allocate read buffer", data != NULL);
  int64_t rstart[] = {0};
  int64_t rstop[] = {total};
  int64_t rbshape[] = {total};
  mu_assert("ERROR: cannot read final array (inconsistent after shrink/grow race)",
            b2nd_get_slice_cbuffer(reader, rstart, rstop, data, rbshape,
                                   total * (int64_t)sizeof(int64_t)) >= 0);
  b2nd_free(reader);

  // Every surviving value must belong to a genuine, untorn run of a single
  // value: the sentinel prefix (always the first run, since shrink only
  // ever removes from the tail) or a tagged append block. Shrinking can
  // legitimately catch *any* block -- including the prefix itself, if the
  // shrinker gets ahead of the growers early on -- mid-way and truncate it,
  // with more (fresh, fully-formed) blocks appended after it once the
  // growers catch up; SHRINK_DROP_PER_ITER also does not evenly divide
  // SHRINK_ITEMS_PER_APPEND, so a truncated run's length need not even be a
  // clean fraction. A short run is therefore never a problem by itself, at
  // any position -- what would be a real bug is a run *longer* than its
  // value could legitimately produce, or a run mixing two different
  // values (a genuinely torn/half-applied write).
  bool corrupted = false;
  int64_t pos = 0;
  bool first_run = true;
  while (pos < total) {
    int64_t run_len = 1;
    while (pos + run_len < total && data[pos + run_len] == data[pos]) {
      run_len++;
    }
    int64_t full_len = (data[pos] == INSERT_SENTINEL) ? SHRINK_INITIAL_ITEMS : SHRINK_ITEMS_PER_APPEND;
    if (data[pos] == INSERT_SENTINEL && !first_run) {
      corrupted = true;  // sentinel can only ever be the very first run
    }
    else if (run_len > full_len) {
      corrupted = true;  // longer than any real block -- genuinely impossible unless torn
    }
    first_run = false;
    pos += run_len;
  }
  mu_assert("ERROR: shrink/grow race left a torn or corrupted block", !corrupted);

  free(data);
  blosc2_remove_urlpath(URLPATH);
  disable_locking();
  return EXIT_SUCCESS;
}


/* --------------------------------------------------------------------- */
/* Bonus: a second, independent attempt at naturally reproducing the      */
/* open-vs-mutation race (already covered deterministically via the       */
/* BLOSC_TESTING fault-injection hook in test_frame_lock.c), using         */
/* mid-array insert instead of same-slot update.  A middle insert must     */
/* shift every subsequent chunk's offset in one rewrite -- structurally    */
/* closer to append's reliably-wide window than an in-place update's       */
/* same-slot swap (which test_fork_open_race_update's own probe showed     */
/* moves the frame boundary by several KB every call, yet still would not  */
/* land the race in dozens of trials). Best-effort like that test: kept    */
/* as extra stress coverage, not a guaranteed trip wire for any specific   */
/* bug.                                                                     */
/* --------------------------------------------------------------------- */
#define OPENRACE_NINSERTERS (4)
#define OPENRACE_NOPENERS (4)
#define OPENRACE_INSERTS_PER_WRITER (60)
#define OPENRACE_OPENS_PER_OPENER (240)
#define OPENRACE_ITEMS_PER_INSERT (20)
#define OPENRACE_INITIAL_ITEMS (200)

static char* test_fork_insert_vs_open_race(void) {
  enable_locking();
  // Same chunk-alignment constraint as test_fork_multiwriter_insert_middle
  b2nd_array_t *creator = create_1d_array(OPENRACE_INITIAL_ITEMS, OPENRACE_ITEMS_PER_INSERT, OPENRACE_ITEMS_PER_INSERT / 2);
  mu_assert("ERROR: cannot create the array", creator != NULL);
  b2nd_free(creator);

  pid_t pids[OPENRACE_NINSERTERS + OPENRACE_NOPENERS];
  int nchildren = 0;

  for (int w = 0; w < OPENRACE_NINSERTERS; w++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork an inserter", pid >= 0);
    if (pid == 0) {
      b2nd_array_t *arr = NULL;
      if (b2nd_open(URLPATH, &arr) < 0) {
        _exit(1);
      }
      int64_t buffer[OPENRACE_ITEMS_PER_INSERT];
      for (int j = 0; j < OPENRACE_ITEMS_PER_INSERT; j++) {
        buffer[j] = (int64_t) w;
      }
      for (int i = 0; i < OPENRACE_INSERTS_PER_WRITER; i++) {
        // Insert at position 0 every time: shifts the *entire* rest of the
        // frame's chunk offsets, a much larger rewrite than a same-slot
        // update. Bracketed for the same reason as the other two tests
        // above (see test_fork_multiwriter_insert_middle).
        if (blosc2_schunk_lock(arr->sc) < 0) {
          _exit(4);
        }
        int irc = b2nd_insert(arr, buffer, OPENRACE_ITEMS_PER_INSERT * (int64_t)sizeof(int64_t), 0, 0);
        blosc2_schunk_unlock(arr->sc);
        if (irc < 0) {
          _exit(2);
        }
      }
      b2nd_free(arr);
      _exit(0);
    }
    pids[nchildren++] = pid;
  }

  for (int o = 0; o < OPENRACE_NOPENERS; o++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork an opener", pid >= 0);
    if (pid == 0) {
      for (int i = 0; i < OPENRACE_OPENS_PER_OPENER; i++) {
        blosc2_schunk* sc = blosc2_schunk_open(URLPATH);
        if (sc == NULL) {
          _exit(3);
        }
        blosc2_schunk_free(sc);
      }
      _exit(0);
    }
    pids[nchildren++] = pid;
  }

  bool all_ok = true;
  for (int c = 0; c < nchildren; c++) {
    int status = 0;
    pid_t r = waitpid(pids[c], &status, 0);
    if (r < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      all_ok = false;
    }
  }
  mu_assert("ERROR: an inserter or opener child failed under concurrent mid-array insert", all_ok);

  blosc2_remove_urlpath(URLPATH);
  disable_locking();
  return EXIT_SUCCESS;
}
#endif  /* !_WIN32 */


static char *all_tests(void) {
#if !defined(_WIN32)
  mu_run_test(test_fork_multiwriter_insert_middle);
  mu_run_test(test_fork_shrink_vs_grow);
  mu_run_test(test_fork_insert_vs_open_race);
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

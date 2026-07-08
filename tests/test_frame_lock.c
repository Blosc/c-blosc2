/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Test the opt-in cross-process locking for disk-based frames (sidecar lock
  file via flock/LockFileEx; see blosc2_stdio_params.locking).  The fork-based
  reader-vs-writer hammer only runs on POSIX systems.
*/

#include <stdio.h>
#include "test_common.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHUNKSIZE (1000)    /* items per chunk (int32_t) */
#define NCHUNKS (20)
#define URLPATH "test_frame_lock.b2frame"

/* Global vars */
int tests_run = 0;


static bool path_exists(const char* path) {
  FILE* f = fopen(path, "rb");
  if (f != NULL) {
    fclose(f);
    return true;
  }
  return false;
}


static blosc2_io* locking_io(void) {
  static blosc2_stdio_params ioparams = {.locking = true};
  static blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &ioparams};
  return &io;
}


static int64_t fill_frame(bool contiguous, bool locking) {
  static int32_t data[CHUNKSIZE];
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous = contiguous, .urlpath = URLPATH, .cparams = &cparams};
  if (locking) {
    storage.io = locking_io();
  }
  blosc2_remove_urlpath(URLPATH);

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return -1;
  }
  int64_t nchunks = 0;
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t));
  }
  blosc2_schunk_free(schunk);
  return nchunks;
}


static int evict_chunk(blosc2_schunk* schunk, int64_t nchunk) {
  uint8_t uninit_chunk[BLOSC_EXTENDED_HEADER_LENGTH];
  int csize = blosc2_chunk_uninit(*schunk->storage->cparams, CHUNKSIZE * (int32_t)sizeof(int32_t),
                                  uninit_chunk, sizeof(uninit_chunk));
  if (csize < 0) {
    return csize;
  }
  int64_t nchunks_ = blosc2_schunk_update_chunk(schunk, nchunk, uninit_chunk, true);
  return nchunks_ < 0 ? (int)nchunks_ : 0;
}


/* With locking disabled (the default), no sidecar must ever appear. */
static char* test_locking_off(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(false, false) == NCHUNKS);
  blosc2_schunk* schunk = blosc2_schunk_open(URLPATH);
  mu_assert("ERROR: cannot open the schunk", schunk != NULL);

  uint8_t* chunk;
  bool needs_free;
  int csize = blosc2_schunk_get_chunk(schunk, 0, &chunk, &needs_free);
  mu_assert("ERROR: cannot read a chunk", csize > 0);
  if (needs_free) {
    free(chunk);
  }
  mu_assert("ERROR: cannot evict a chunk", evict_chunk(schunk, 0) == 0);
  mu_assert("ERROR: a sidecar lock file appeared with locking off",
            !path_exists(URLPATH ".b2lock"));

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* Two locked handles in one process: the stale-handle re-sync must keep
   working under locking, and the sidecar must appear. */
static char* test_two_handles(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(false, true) == NCHUNKS);

  blosc2_schunk* h1 = blosc2_schunk_open_udio(URLPATH, locking_io());
  blosc2_schunk* h2 = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk twice", h1 != NULL && h2 != NULL);

  // Warm h2's cached view
  uint8_t* chunk;
  bool needs_free;
  int old_size = blosc2_schunk_get_chunk(h2, 0, &chunk, &needs_free);
  mu_assert("ERROR: cannot read chunk 0 before the mutation", old_size > 0);
  if (needs_free) {
    free(chunk);
  }

  // The sidecar shows up after the first locked operation (sframe: inside the dir)
  mu_assert("ERROR: the sidecar lock file did not appear",
            path_exists(URLPATH "/.b2lock"));

  mu_assert("ERROR: cannot evict chunk 0", evict_chunk(h1, 0) == 0);

  int gsize = blosc2_schunk_get_chunk(h2, 0, &chunk, &needs_free);
  mu_assert("ERROR: stale handle did not re-sync under locking",
            gsize == BLOSC_EXTENDED_HEADER_LENGTH);
  if (needs_free) {
    free(chunk);
  }
  int lsize = blosc2_schunk_get_lazychunk(h2, 0, &chunk, &needs_free);
  mu_assert("ERROR: get_lazychunk disagrees with get_chunk on the stale handle",
            lsize == BLOSC_EXTENDED_HEADER_LENGTH);
  if (needs_free) {
    free(chunk);
  }

  blosc2_schunk_free(h1);
  blosc2_schunk_free(h2);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* A handle whose lock acquisition flags staleness must resync its cached
   nbytes/cbytes/nchunks *before* applying an append/insert delta to them, or
   it persists wrong header counters (plans/todo-locking-swmr.md item 1).
   h1 appends, then h2 (stale at that point) appends; the on-disk counters,
   re-read from a fresh open, must equal the sum of both appends. */
static char* test_stale_append_resync(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);

  blosc2_schunk* h1 = blosc2_schunk_open_udio(URLPATH, locking_io());
  blosc2_schunk* h2 = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk twice", h1 != NULL && h2 != NULL);

  static int32_t data[CHUNKSIZE];
  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  // h1 appends first, bumping the lock generation counter that h2 has not seen yet
  int64_t nchunks1 = blosc2_schunk_append_buffer(h1, data, CHUNKSIZE * sizeof(int32_t));
  mu_assert("ERROR: h1 append failed", nchunks1 == NCHUNKS + 1);

  // h2's handle is stale now; its append must resync counters before applying its delta
  int64_t nchunks2 = blosc2_schunk_append_buffer(h2, data, CHUNKSIZE * sizeof(int32_t));
  mu_assert("ERROR: h2 append failed", nchunks2 == NCHUNKS + 2);

  // h1 inserts; then h2 (stale again) inserts too -- insert must resync as well
  uint8_t* chunk1 = malloc(CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
  int csize1 = blosc2_compress_ctx(h1->cctx, data, CHUNKSIZE * sizeof(int32_t),
                                   chunk1, CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cannot compress chunk for h1 insert", csize1 > 0);
  int64_t nchunks3 = blosc2_schunk_insert_chunk(h1, 0, chunk1, false);
  mu_assert("ERROR: h1 insert failed", nchunks3 == NCHUNKS + 3);

  uint8_t* chunk2 = malloc(CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
  int csize2 = blosc2_compress_ctx(h2->cctx, data, CHUNKSIZE * sizeof(int32_t),
                                   chunk2, CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cannot compress chunk for h2 insert", csize2 > 0);
  int64_t nchunks4 = blosc2_schunk_insert_chunk(h2, 0, chunk2, false);
  mu_assert("ERROR: h2 insert failed", nchunks4 == NCHUNKS + 4);

  blosc2_schunk_free(h1);
  blosc2_schunk_free(h2);

  // Reopen fresh and check the on-disk header counters agree with reality
  blosc2_schunk* sc = blosc2_schunk_open(URLPATH);
  mu_assert("ERROR: cannot reopen the schunk", sc != NULL);
  mu_assert("ERROR: nchunks mismatch after stale-handle append/insert",
            sc->nchunks == NCHUNKS + 4);
  int64_t expected_nbytes = (int64_t)(NCHUNKS + 4) * CHUNKSIZE * sizeof(int32_t);
  mu_assert("ERROR: nbytes mismatch after stale-handle append/insert",
            sc->nbytes == expected_nbytes);
  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* blosc2_vlmeta_exists() must poll staleness: a vlmetalayer added or deleted
   through another locked handle is reflected without any data access. */
static char* test_vlmeta_exists_stale(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);
  blosc2_schunk* h1 = blosc2_schunk_open_udio(URLPATH, locking_io());
  blosc2_schunk* h2 = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk twice", h1 != NULL && h2 != NULL);

  mu_assert("ERROR: vlmetalayer must not exist yet",
            blosc2_vlmeta_exists(h2, "foo") == BLOSC2_ERROR_NOT_FOUND);

  uint8_t content[4] = {1, 2, 3, 4};
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  mu_assert("ERROR: cannot add the vlmetalayer",
            blosc2_vlmeta_add(h1, "foo", content, sizeof(content), &cparams) >= 0);
  mu_assert("ERROR: the added vlmetalayer is not seen by the other handle",
            blosc2_vlmeta_exists(h2, "foo") >= 0);

  mu_assert("ERROR: cannot delete the vlmetalayer",
            blosc2_vlmeta_delete(h1, "foo") >= 0);
  mu_assert("ERROR: the deleted vlmetalayer is still seen by the other handle",
            blosc2_vlmeta_exists(h2, "foo") == BLOSC2_ERROR_NOT_FOUND);

  blosc2_schunk_free(h1);
  blosc2_schunk_free(h2);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


static void set_env(const char* name, const char* value) {
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}


/* The BLOSC_LOCKING env var must enable locking globally, with no locking
   set in the io params; "0" must leave it off. */
static char* test_env_locking(void) {
  set_env("BLOSC_LOCKING", "1");
  int64_t nchunks_on = fill_frame(true, false);
  bool sidecar_on = path_exists(URLPATH ".b2lock");

  set_env("BLOSC_LOCKING", "0");
  int64_t nchunks_off = fill_frame(true, false);
  bool sidecar_off = path_exists(URLPATH ".b2lock");

  // Clear the env var before asserting, so a failure cannot leak it into
  // the remaining tests ("" means off, like unset)
  set_env("BLOSC_LOCKING", "");
  blosc2_remove_urlpath(URLPATH);

  mu_assert("ERROR: cannot create the frames",
            nchunks_on == NCHUNKS && nchunks_off == NCHUNKS);
  mu_assert("ERROR: BLOSC_LOCKING=1 did not enable locking", sidecar_on);
  mu_assert("ERROR: BLOSC_LOCKING=0 did not disable locking", !sidecar_off);
  return EXIT_SUCCESS;
}


/* The sidecar of a cframe must be removed along with the frame. */
static char* test_sidecar_cleanup(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);

  blosc2_schunk* schunk = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk", schunk != NULL);
  mu_assert("ERROR: cannot evict a chunk", evict_chunk(schunk, 0) == 0);
  mu_assert("ERROR: the sidecar lock file did not appear",
            path_exists(URLPATH ".b2lock"));
  blosc2_schunk_free(schunk);

  blosc2_remove_urlpath(URLPATH);
  mu_assert("ERROR: the frame was not removed", !path_exists(URLPATH));
  mu_assert("ERROR: the sidecar lock file was not removed",
            !path_exists(URLPATH ".b2lock"));
  return EXIT_SUCCESS;
}


/* blosc2_schunk_lock()/unlock() bracket: operations inside nest on the held
   lock, everything keeps working after the unlock, and both calls are no-ops
   on a handle without locking. */
static char* test_lock_bracket(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);
  blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk", sc != NULL);

  mu_assert("ERROR: cannot take the lock bracket", blosc2_schunk_lock(sc) == 0);
  // Mutations and reads inside the bracket nest instead of deadlocking
  mu_assert("ERROR: cannot evict inside the bracket", evict_chunk(sc, 0) == 0);
  uint8_t* chunk;
  bool needs_free;
  int csize = blosc2_schunk_get_chunk(sc, 0, &chunk, &needs_free);
  mu_assert("ERROR: cannot read inside the bracket",
            csize == BLOSC_EXTENDED_HEADER_LENGTH);
  if (needs_free) {
    free(chunk);
  }
  mu_assert("ERROR: cannot release the lock bracket", blosc2_schunk_unlock(sc) == 0);

  // The lock is really released: operations after the bracket work normally
  mu_assert("ERROR: cannot evict after the bracket", evict_chunk(sc, 1) == 0);
  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);

  // No-op on a handle without locking (and on in-memory schunks)
  mu_assert("ERROR: cannot create the frame", fill_frame(true, false) == NCHUNKS);
  sc = blosc2_schunk_open(URLPATH);
  mu_assert("ERROR: cannot open the schunk", sc != NULL);
  mu_assert("ERROR: lock bracket not a no-op without locking",
            blosc2_schunk_lock(sc) == 0 && blosc2_schunk_unlock(sc) == 0);
  mu_assert("ERROR: no sidecar must appear without locking",
            !path_exists(URLPATH ".b2lock"));
  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


#if !defined(_WIN32)
/* A bracketed multi-operation mutation must be atomic for other processes: a
   child that reads while the parent holds the bracket blocks until the unlock
   and then sees both mutations, never just the first one. */
static char* test_bracket_atomic(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(false, true) == NCHUNKS);

  int pipefd[2];
  mu_assert("ERROR: cannot create a pipe", pipe(pipefd) == 0);
  pid_t pid = fork();
  mu_assert("ERROR: cannot fork", pid >= 0);

  if (pid == 0) {
    /* Child: wait until the parent holds the bracket, then read both mutated
       chunks; the shared lock is only granted after the parent's unlock, so
       both must already be evicted (32-byte special chunks). */
    close(pipefd[1]);
    char byte;
    if (read(pipefd[0], &byte, 1) != 1) {
      _exit(1);
    }
    blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
    if (sc == NULL) {
      _exit(2);
    }
    // Chunk 1 is mutated *last* by the parent: seeing it evicted while
    // chunk 0 is not would mean the bracket leaked intermediate state
    uint8_t* chunk;
    bool needs_free;
    int csize1 = blosc2_schunk_get_chunk(sc, 1, &chunk, &needs_free);
    if (needs_free) {
      free(chunk);
    }
    int csize0 = blosc2_schunk_get_chunk(sc, 0, &chunk, &needs_free);
    if (needs_free) {
      free(chunk);
    }
    blosc2_schunk_free(sc);
    if (csize1 != BLOSC_EXTENDED_HEADER_LENGTH || csize0 != BLOSC_EXTENDED_HEADER_LENGTH) {
      fprintf(stderr, "[child] csize1=%d csize0=%d (expected %d)\n",
              csize1, csize0, BLOSC_EXTENDED_HEADER_LENGTH);
      _exit(3);
    }
    _exit(0);
  }

  /* Parent: bracket around two evictions, with a pause in between that would
     let the child slip in if each operation released the lock. */
  close(pipefd[0]);
  blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk in the parent", sc != NULL);
  mu_assert("ERROR: cannot take the lock bracket", blosc2_schunk_lock(sc) == 0);
  mu_assert("ERROR: cannot signal the child", write(pipefd[1], "x", 1) == 1);
  mu_assert("ERROR: cannot evict chunk 0", evict_chunk(sc, 0) == 0);
  usleep(200 * 1000);  // give the child time to block on the lock
  mu_assert("ERROR: cannot evict chunk 1", evict_chunk(sc, 1) == 0);
  mu_assert("ERROR: cannot release the lock bracket", blosc2_schunk_unlock(sc) == 0);
  close(pipefd[1]);

  int status = 0;
  mu_assert("ERROR: cannot wait for the child", waitpid(pid, &status, 0) == pid);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "[parent] child status: exited=%d code=%d signaled=%d sig=%d\n",
            WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : -1,
            WIFSIGNALED(status), WIFSIGNALED(status) ? WTERMSIG(status) : -1);
  }
  mu_assert("ERROR: the child observed intermediate bracket state",
            WIFEXITED(status) && WEXITSTATUS(status) == 0);

  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* Two children append through their own handle concurrently under locking.
   This is the cross-process pin for test_stale_append_resync's counter fix
   (plans/todo-locking-swmr.md item 1; python-blosc2's todo/locking-mwmr.md
   item 1): each child's append re-syncs the cached nbytes/cbytes/nchunks
   before applying its delta, so interleaved appends from two processes must
   not lose or corrupt either side's chunks. Chunk values are tagged by
   parity (child 0 -> even, child 1 -> odd) so ownership and any torn/mixed
   content are both checkable from a fresh open. */
static char* test_fork_two_appenders(void) {
  static int32_t data[CHUNKSIZE];
  const int child_iters = 200;

  mu_assert("ERROR: cannot create the frame", fill_frame(false, true) == NCHUNKS);

  for (int child = 0; child < 2; child++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork", pid >= 0);
    if (pid == 0) {
      blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
      if (sc == NULL) {
        _exit(1);
      }
      for (int i = 0; i < child_iters; i++) {
        for (int j = 0; j < CHUNKSIZE; j++) {
          data[j] = (i * CHUNKSIZE + j) * 2 + child;
        }
        int64_t nchunks_ = blosc2_schunk_append_buffer(sc, data, CHUNKSIZE * sizeof(int32_t));
        if (nchunks_ < 0) {
          _exit(2);
        }
      }
      blosc2_schunk_free(sc);
      _exit(0);
    }
  }

  bool children_ok = true;
  for (int child = 0; child < 2; child++) {
    int status = 0;
    pid_t w = wait(&status);
    if (w < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      children_ok = false;
    }
  }
  mu_assert("ERROR: a child appender failed", children_ok);

  blosc2_schunk* sc = blosc2_schunk_open(URLPATH);
  mu_assert("ERROR: cannot reopen the schunk", sc != NULL);
  int64_t expected_nchunks = NCHUNKS + 2 * (int64_t)child_iters;
  mu_assert("ERROR: nchunks mismatch after concurrent appenders",
            sc->nchunks == expected_nchunks);
  int64_t expected_nbytes = expected_nchunks * CHUNKSIZE * (int64_t)sizeof(int32_t);
  mu_assert("ERROR: nbytes mismatch after concurrent appenders",
            sc->nbytes == expected_nbytes);

  // Every appended chunk must be internally consistent (single parity, i.e.
  // not a mix of both children's writes), and both children's chunks present
  static int32_t dest[CHUNKSIZE];
  bool content_ok = true, seen_even = false, seen_odd = false;
  for (int64_t nchunk = NCHUNKS; nchunk < sc->nchunks; nchunk++) {
    int dsize = blosc2_schunk_decompress_chunk(sc, nchunk, dest, sizeof(dest));
    if (dsize < 0) {
      content_ok = false;
      break;
    }
    int parity = ((dest[0] % 2) + 2) % 2;
    for (int j = 1; j < CHUNKSIZE; j++) {
      if ((((dest[j] % 2) + 2) % 2) != parity) {
        content_ok = false;
        break;
      }
    }
    if (parity == 0) {
      seen_even = true;
    }
    else {
      seen_odd = true;
    }
  }
  mu_assert("ERROR: a chunk mixed values from both appenders", content_ok);
  mu_assert("ERROR: did not see appended chunks from both children", seen_even && seen_odd);

  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* The real test: a child process keeps rewriting chunks while the parent
   reads all of them; with locking on, no read may ever fail (without it,
   torn reads intermittently yield BLOSC2_ERROR_FILE_READ). */
static char* test_fork_hammer(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  const int child_iters = 300;

  mu_assert("ERROR: cannot create the frame", fill_frame(false, true) == NCHUNKS);

  pid_t pid = fork();
  mu_assert("ERROR: cannot fork", pid >= 0);

  if (pid == 0) {
    /* Child: the evictor/writer.  Open our own handle (a handle inherited
       through fork would share the parent's lock). */
    blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
    if (sc == NULL) {
      _exit(1);
    }
    for (int i = 0; i < child_iters; i++) {
      int64_t nchunk = i % NCHUNKS;
      if (i % 2 == 0) {
        // Evict: replace with an UNINIT special chunk (truncates the chunk file)
        if (evict_chunk(sc, nchunk) != 0) {
          _exit(2);
        }
      }
      else {
        // Re-fill: replace with a freshly compressed regular chunk
        for (int j = 0; j < CHUNKSIZE; j++) {
          data[j] = i + j;
        }
        uint8_t* chunk = malloc(CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
        int csize = blosc2_compress_ctx(sc->cctx, data, CHUNKSIZE * sizeof(int32_t),
                                        chunk, CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
        if (csize < 0) {
          _exit(3);
        }
        int64_t nchunks_ = blosc2_schunk_update_chunk(sc, nchunk, chunk, false);
        if (nchunks_ < 0) {
          _exit(4);
        }
      }
    }
    blosc2_schunk_free(sc);
    _exit(0);
  }

  /* Parent: the reader.  Sweep all chunks through all three read entry points
     until the child finishes; every single read must succeed. */
  blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
  mu_assert("ERROR: cannot open the schunk in the parent", sc != NULL);

  int status = 0;
  bool child_done = false;
  long nreads = 0;
  while (!child_done) {
    if (waitpid(pid, &status, WNOHANG) == pid) {
      child_done = true;  // do one final sweep below before checking status
    }
    for (int64_t nchunk = 0; nchunk < NCHUNKS; nchunk++) {
      uint8_t* chunk;
      bool needs_free;
      int csize = blosc2_schunk_get_chunk(sc, nchunk, &chunk, &needs_free);
      mu_assert("ERROR: get_chunk failed during concurrent writes", csize > 0);
      if (needs_free) {
        free(chunk);
      }
      csize = blosc2_schunk_get_lazychunk(sc, nchunk, &chunk, &needs_free);
      mu_assert("ERROR: get_lazychunk failed during concurrent writes", csize > 0);
      if (needs_free) {
        free(chunk);
      }
      int dsize = blosc2_schunk_decompress_chunk(sc, nchunk, data_dest, sizeof(data_dest));
      mu_assert("ERROR: decompress_chunk failed during concurrent writes", dsize >= 0);
      nreads += 3;
    }
  }
  printf("(%ld reads against %d writes) ", nreads, child_iters);
  mu_assert("ERROR: the writer child failed",
            WIFEXITED(status) && WEXITSTATUS(status) == 0);

  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


#if defined(BLOSC_TESTING)
/* Deterministic version of the open-race bug: rather than racing real
   concurrent writers/openers and hoping to land in a microseconds-wide
   window (see test_fork_open_race_update() below, which tries exactly that
   and mostly fails to land it even with a confirmed several-KB frame_len
   swing on every write), directly simulate the transient state a race
   would produce via the frame.c-internal blosc2_test_arm_open_race() fault
   injection hook (BLOSC_TESTING-only, see blosc/frame.c).

   fill_frame() performs many locked appends, which bump the sidecar lock's
   generation counter; a freshly-opened handle's default lock_seq is behind
   that counter, so its very first open already takes the force_refresh
   path (frame.c: "this also triggers on every open of a previously-mutated
   frame") -- no extra setup mutation is needed. That open makes exactly two
   calls to frame_from_file_offset(): the bootstrap read (call #1) and the
   force_refresh re-read (call #2). Arming the hook for call #2 forces that
   specific call to see a transient "frame length exceeds file boundary"
   the same way a real race would, then self-restores before the retry's
   next attempt -- exercising precisely the code path this bug lived in. */
extern void blosc2_test_arm_open_race(int arm_at);

static char* test_open_race_deterministic(void) {
  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);

  blosc2_test_arm_open_race(2);
  blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
  blosc2_test_arm_open_race(0);  // belt-and-suspenders; the hook also self-disarms
  mu_assert("ERROR: open failed on a simulated transient race in the "
            "force_refresh re-read -- the retry should have absorbed it",
            sc != NULL);

  blosc2_schunk_free(sc);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}
#endif  /* BLOSC_TESTING */


/* A concurrent opener must not see spurious failures while a writer grows the
   frame. frame_from_file_offset()'s bootstrap read (stat() for the file size,
   then the header/trailer) happens before any lock is taken; a writer that
   grows the frame between the stat() and the header read can leave the
   opener with a header advertising a frame_len larger than the now-stale
   file_size snapshot, which frame_from_file_offset() treated as a hard
   "frame length exceeds file boundary" error. Under locking this is a
   transient race, not a real error -- blosc2_schunk_open_offset_udio() should
   retry rather than propagate it.

   A single appender vs. a single serial opener rarely lands in the window;
   several concurrent appenders racing several concurrent openers from the
   very first append reproduces it reliably (this is how it was originally
   found, via python-blosc2's cross-process NDArray multi-writer hammer
   test). */
#define OPEN_RACE_NAPPENDERS (4)
#define OPEN_RACE_NOPENERS (4)
static char* test_fork_open_race(void) {
  const int appender_iters = 100;
  const int opener_iters = appender_iters * 4;

  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);

  pid_t pids[OPEN_RACE_NAPPENDERS + OPEN_RACE_NOPENERS];
  int nchildren = 0;

  for (int a = 0; a < OPEN_RACE_NAPPENDERS; a++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork an appender", pid >= 0);
    if (pid == 0) {
      static int32_t data[CHUNKSIZE];
      blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
      if (sc == NULL) {
        _exit(1);
      }
      for (int i = 0; i < appender_iters; i++) {
        for (int j = 0; j < CHUNKSIZE; j++) {
          data[j] = i * CHUNKSIZE + j;
        }
        if (blosc2_schunk_append_buffer(sc, data, CHUNKSIZE * sizeof(int32_t)) < 0) {
          _exit(2);
        }
      }
      blosc2_schunk_free(sc);
      _exit(0);
    }
    pids[nchildren++] = pid;
  }

  for (int o = 0; o < OPEN_RACE_NOPENERS; o++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork an opener", pid >= 0);
    if (pid == 0) {
      /* Opens a *fresh* handle repeatedly, exercising the bootstrap read
         racing the appenders' growth; every single open must succeed. */
      for (int i = 0; i < opener_iters; i++) {
        blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
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
  for (int k = 0; k < nchildren; k++) {
    int status = 0;
    pid_t w = waitpid(pids[k], &status, 0);
    if (w < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      all_ok = false;
    }
  }
  mu_assert("ERROR: an appender or opener child failed under concurrent frame growth", all_ok);

  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* Same race as test_fork_open_race() above, but the frame never grows in
   the sense of gaining chunks: chunk count is fixed throughout, only
   in-place updates happen. An update whose new compressed size does not fit
   in the space the old chunk occupied still forces c-blosc2 to relocate the
   chunk to the end of the frame and rewrite the trailer, changing frame_len
   exactly like an append does -- so the opener can still catch the frame
   mid-rewrite. To make that relocation happen on (almost) every single
   update -- and not just the first one per chunk -- each cycle evicts the
   chunk down to a near-zero UNINIT special before refilling it with
   incompressible random data at full size, so the chunk's footprint swings
   from ~0 back to ~full on every refill.

   This specifically targets the second, force_refresh re-read inside
   blosc2_schunk_open_offset_udio() (the one done under the freshly-acquired
   shared lock, after the bootstrap read already succeeded): that re-read
   used to run without the bounded retry the bootstrap read has, so it
   propagated the same transient "frame length exceeds file boundary" race
   as a hard failure on nearly every open (force_refresh is set on
   essentially every open of a previously-mutated frame).

   Unlike test_fork_open_race() above, this one is *not* a reliable trip
   wire: a stat+header probe confirms the on-disk frame length really does
   swing by several KB on every single evict/refill cycle here, so the
   transient window this test is trying to land in genuinely exists, but on
   fast local storage that window is apparently narrow enough that this
   fork-based harness did not manage to land in it in dozens of manual
   trials, with or without the fix. Kept as stress coverage for the
   concurrent open+update path in general (and it may well catch this race
   reliably on slower/loaded machines or filesystems), not as a guaranteed
   regression test for this specific bug -- the bug itself was confirmed via
   direct reproduction through python-blosc2's multiprocessing-based hammer
   test instead (see python-blosc2 tests/test_locking.py,
   test_cross_process_open_race_under_update, similarly best-effort). */
#define OPEN_RACE_NUPDATERS (4)
#define OPEN_RACE_NOPENERS2 (4)
static char* test_fork_open_race_update(void) {
  const int updater_iters = 150;
  const int opener_iters = updater_iters * 4;

  mu_assert("ERROR: cannot create the frame", fill_frame(true, true) == NCHUNKS);

  pid_t pids[OPEN_RACE_NUPDATERS + OPEN_RACE_NOPENERS2];
  int nchildren = 0;

  for (int u = 0; u < OPEN_RACE_NUPDATERS; u++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork an updater", pid >= 0);
    if (pid == 0) {
      static int32_t data[CHUNKSIZE];
      unsigned int seed = (unsigned int)(getpid() + u);
      blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
      if (sc == NULL) {
        _exit(1);
      }
      int64_t nchunk = u % sc->nchunks;
      for (int i = 0; i < updater_iters; i++) {
        if (evict_chunk(sc, nchunk) != 0) {
          _exit(2);
        }
        for (int j = 0; j < CHUNKSIZE; j++) {
          data[j] = (int32_t)rand_r(&seed);
        }
        uint8_t* chunk = malloc(CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
        int csize = blosc2_compress_ctx(sc->cctx, data, CHUNKSIZE * sizeof(int32_t),
                                        chunk, CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD);
        if (csize < 0) {
          free(chunk);
          _exit(3);
        }
        if (blosc2_schunk_update_chunk(sc, nchunk, chunk, false) < 0) {
          _exit(4);
        }
      }
      blosc2_schunk_free(sc);
      _exit(0);
    }
    pids[nchildren++] = pid;
  }

  for (int o = 0; o < OPEN_RACE_NOPENERS2; o++) {
    pid_t pid = fork();
    mu_assert("ERROR: cannot fork an opener", pid >= 0);
    if (pid == 0) {
      /* Opens a *fresh* handle repeatedly, exercising both the bootstrap
         read and the force_refresh re-read racing the updaters; every
         single open must succeed. */
      for (int i = 0; i < opener_iters; i++) {
        blosc2_schunk* sc = blosc2_schunk_open_udio(URLPATH, locking_io());
        if (sc == NULL) {
          _exit(5);
        }
        blosc2_schunk_free(sc);
      }
      _exit(0);
    }
    pids[nchildren++] = pid;
  }

  bool all_ok = true;
  for (int k = 0; k < nchildren; k++) {
    int status = 0;
    pid_t w = waitpid(pids[k], &status, 0);
    if (w < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      all_ok = false;
    }
  }
  mu_assert("ERROR: an updater or opener child failed under concurrent in-place updates", all_ok);

  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}
#endif  /* !_WIN32 */


static char *all_tests(void) {
  mu_run_test(test_locking_off);
  mu_run_test(test_two_handles);
  mu_run_test(test_stale_append_resync);
  mu_run_test(test_env_locking);
  mu_run_test(test_sidecar_cleanup);
  mu_run_test(test_vlmeta_exists_stale);
  mu_run_test(test_lock_bracket);
#if defined(BLOSC_TESTING)
  mu_run_test(test_open_race_deterministic);
#endif
#if !defined(_WIN32)
  mu_run_test(test_bracket_atomic);
  mu_run_test(test_fork_hammer);
  mu_run_test(test_fork_two_appenders);
  mu_run_test(test_fork_open_race);
  mu_run_test(test_fork_open_race_update);
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

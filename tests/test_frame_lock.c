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


#if !defined(_WIN32)
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
#endif  /* !_WIN32 */


static char *all_tests(void) {
  mu_run_test(test_locking_off);
  mu_run_test(test_two_handles);
  mu_run_test(test_sidecar_cleanup);
#if !defined(_WIN32)
  mu_run_test(test_fork_hammer);
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

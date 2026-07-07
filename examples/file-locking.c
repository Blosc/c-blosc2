/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating the opt-in file locking for disk-based frames.

  When several handles — typically in different processes — operate on the same
  on-disk frame (e.g. one process evicting chunks from a cache while others
  read it), enable `locking` in the I/O parameters of every handle.  Blosc2
  then serializes the accesses through a small sidecar lock file next to the
  frame (`<dir>/.b2lock` for sparse frames, `<file>.b2lock` for contiguous
  ones): readers share the lock, mutating operations take it exclusively, and
  a stale handle re-syncs its view of the frame automatically.

  Things to know:

  - Locking is *advisory*: it only protects the frame if every handle on it
    enables it.  Handles opened without `locking` bypass the lock entirely.
  - It is off by default, and only supported for the default filesystem I/O
    (BLOSC2_IO_FILESYSTEM); not on network filesystems (NFS).
  - The lock is held per operation, and it is crash-safe: the OS releases it
    when a process dies.
  - Lock failures surface as BLOSC2_ERROR_LOCK.

  To compile this program:

  $ gcc file-locking.c -o file-locking -lblosc2

  To run:

  $ ./file-locking
  Both handles enabled locking on: file-locking.b2frame
  Handle 2 read chunk 0 (before eviction): 371 bytes
  Handle 1 evicted chunk 0 (replaced with an UNINIT special chunk)
  Handle 2 read chunk 0 (after eviction): 32 bytes (special chunk, re-synced)
  Success!

*/

#include <stdio.h>
#include <stdlib.h>
#include <blosc2.h>

#define CHUNKSIZE (1000)    /* items per chunk (int64_t) */
#define NCHUNKS (10)
#define URLPATH "file-locking.b2frame"


int main(void) {
  blosc2_init();

  // Opt in to locking: pass blosc2_stdio_params with `locking` set through
  // the `params` member of the (default filesystem) blosc2_io struct.
  // These structs must outlive the super-chunks opened with them.
  blosc2_stdio_params ioparams = {.locking = true};
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &ioparams};

  // Create a sparse frame on-disk, with locking enabled from the start
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous = false, .urlpath = URLPATH,
                            .cparams = &cparams, .io = &io};
  blosc2_remove_urlpath(URLPATH);
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    printf("Cannot create the super-chunk!\n");
    return -1;
  }
  int64_t data[CHUNKSIZE];
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) < 0) {
      printf("Cannot append data to the super-chunk!\n");
      return -1;
    }
  }
  blosc2_schunk_free(schunk);

  // Two independent handles on the same frame, as two cooperating processes
  // would have (here in a single process for the sake of the example; the
  // behavior across processes is the same).  Every handle enables locking.
  blosc2_schunk *h1 = blosc2_schunk_open_udio(URLPATH, &io);
  blosc2_schunk *h2 = blosc2_schunk_open_udio(URLPATH, &io);
  if (h1 == NULL || h2 == NULL) {
    printf("Cannot open the super-chunk!\n");
    return -1;
  }
  printf("Both handles enabled locking on: %s\n", URLPATH);

  // h2 reads chunk 0 (and caches its view of the frame)
  uint8_t *chunk;
  bool needs_free;
  int csize = blosc2_schunk_get_chunk(h2, 0, &chunk, &needs_free);
  if (csize < 0) {
    printf("Cannot read a chunk: %s\n", blosc2_error_string(csize));
    return -1;
  }
  if (needs_free) {
    free(chunk);
  }
  printf("Handle 2 read chunk 0 (before eviction): %d bytes\n", csize);

  // h1 mutates the frame behind h2's back: replace chunk 0 with an UNINIT
  // special chunk (the typical cache-eviction primitive, as this reclaims
  // the chunk storage on-disk for sparse frames)
  uint8_t uninit[BLOSC_EXTENDED_HEADER_LENGTH];
  csize = blosc2_chunk_uninit(*h1->storage->cparams, CHUNKSIZE * sizeof(int64_t),
                              uninit, sizeof(uninit));
  if (csize < 0 || blosc2_schunk_update_chunk(h1, 0, uninit, true) < 0) {
    printf("Cannot evict the chunk!\n");
    return -1;
  }
  printf("Handle 1 evicted chunk 0 (replaced with an UNINIT special chunk)\n");

  // h2 reads again: the operation takes the shared lock (so it cannot land in
  // the middle of h1's rewrite) and re-syncs its now-stale view of the frame
  csize = blosc2_schunk_get_chunk(h2, 0, &chunk, &needs_free);
  if (csize < 0) {
    printf("Cannot read the mutated chunk: %s\n", blosc2_error_string(csize));
    return -1;
  }
  if (needs_free) {
    free(chunk);
  }
  printf("Handle 2 read chunk 0 (after eviction): %d bytes (special chunk, re-synced)\n",
         csize);
  if (csize != BLOSC_EXTENDED_HEADER_LENGTH) {
    printf("Unexpected chunk size!\n");
    return -1;
  }

  printf("Success!\n");
  blosc2_schunk_free(h1);
  blosc2_schunk_free(h2);
  blosc2_remove_urlpath(URLPATH);
  blosc2_destroy();
  return 0;
}

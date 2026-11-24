/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program showing how to write the chunks of a super-chunk straight
  from uncompressed buffers:

    blosc2_schunk_append_buffer()   compress a buffer and add it as the last chunk
    blosc2_schunk_insert_buffer()   compress a buffer and add it as chunk number n
    blosc2_schunk_update_buffer()   compress a buffer and replace chunk number n

  All three compress with the super-chunk's own compression context (codec,
  filters, number of threads, prefilter), so the caller never handles
  compressed chunks.

  To compile this program:

  $ gcc schunk_buffer.c -o schunk_buffer -lblosc2

  To run:

  $ ./schunk_buffer
  Blosc version info: 3.5.1.dev ($Date:: 2026-11-04 #$)
  Appended 4 chunks:        [ 0][ 1][ 2][ 3]
  Inserted a chunk at 2:    [ 0][ 1][10][ 2][ 3]
  Updated chunk 0:          [20][ 1][10][ 2][ 3]
  Added 1 to every item:    [20][ 1][10][ 2][ 3]
  Reopened the file:        [20][ 1][10][ 2][ 3]
  Successful roundtrip data <-> schunk!

  Each [nn] is a chunk; nn is the tag it was filled with (see fill_chunk()).


  What the *_buffer() functions save you
  --------------------------------------

  Without them, inserting or replacing a chunk from uncompressed data takes a
  compression buffer per chunk:

    int32_t dest_nbytes = nbytes + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(dest_nbytes);
    blosc2_compress_ctx(schunk->cctx, src, nbytes, chunk, dest_nbytes);
    blosc2_schunk_update_chunk(schunk, nchunk, chunk, true);
    free(chunk);

  blosc2_schunk_update_buffer(schunk, nchunk, src, nbytes) does all of that
  in one call (and blosc2_schunk_insert_buffer() for an insert).  When the
  super-chunk lives in a frame (contiguous or sparse, in memory or on disk),
  the frame writes its own copy of the compressed chunk, so the compressed
  chunk is only needed until then.  The functions therefore compress into a
  buffer that the calling thread keeps for its next chunk:

                 compress with               the frame
                 schunk->cctx                writes a copy
    +--------+                +--------+                 +---------------------+
    |  src   | -------------> | chunk  | --------------> | ... | chunk n | ... |  frame
    +--------+                +--------+                 +---------------------+
    uncompressed,             compressed, in a buffer
    nbytes                    that the calling thread reuses

                        by hand                          with *_buffer()
    chunk 0:   malloc, compress, store, free     compress into a new buffer, store
    chunk 1:   malloc, compress, store, free     compress into the same buffer, store
    chunk 2:   malloc, compress, store, free     compress into the same buffer, store
      ...

  With glibc, a malloc() of a few MB is usually a fresh mmap(): the kernel
  zeroes its pages while the compression threads first write to them, and
  free() hands them back with munmap().  Reusing one buffer avoids both.
  (The buffer is kept for chunks of 64 KiB to 32 MiB, and not on Windows.
  A plain super-chunk, without a frame, keeps the compressed chunk itself, so
  there every call allocates one.)

  Also, unlike blosc2_compress_ctx() above, the *_buffer() functions set
  schunk->current_nchunk first, so a prefilter gets the number of the chunk
  being written in its params->nchunk.
*/

#include <stdio.h>
#include <stdlib.h>

#include "blosc2.h"

#define CHUNK_ITEMS (256 * 1024)  // 1 MiB of int32_t per chunk
#define NAPPEND 4
#define NTHREADS 4

// Item i of a chunk is tag * TAG_STRIDE + i, so its first item tells which chunk it is
#define TAG_STRIDE 1000000
#define TAG_INSERTED 10
#define TAG_UPDATED 20

static void fill_chunk(int32_t *buf, int32_t tag) {
  for (int32_t i = 0; i < CHUNK_ITEMS; i++) {
    buf[i] = tag * TAG_STRIDE + i;
  }
}

// Print the tag of every chunk, e.g. "[ 0][ 1][10][ 2][ 3]"
static int print_chunks(const char *msg, blosc2_schunk *schunk, int32_t *buf, int32_t nbytes) {
  printf("%-25s ", msg);
  for (int64_t nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    int dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, buf, nbytes);
    if (dsize != nbytes) {
      printf("\nCannot decompress chunk %lld.  Error code: %d\n", (long long)nchunk, dsize);
      return -1;
    }
    printf("[%2d]", buf[0] / TAG_STRIDE);
  }
  printf("\n");
  return 0;
}

int main(void) {
  const char *urlpath = "schunk_buffer.b2frame";
  const int32_t nbytes = (int32_t)(CHUNK_ITEMS * sizeof(int32_t));
  int rc = -1;
  blosc2_schunk *schunk = NULL;
  int64_t nchunks;

  blosc2_init();
  printf("Blosc version info: %s (%s)\n", blosc2_get_version_string(), BLOSC2_VERSION_DATE);

  int32_t *buf = malloc(nbytes);
  if (buf == NULL) {
    printf("Cannot allocate the buffer\n");
    goto cleanup;
  }

  // A contiguous frame on disk; the calls are the same for any other storage
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.contiguous=true, .urlpath=(char *)urlpath,
                            .cparams=&cparams, .dparams=&dparams};
  blosc2_remove_urlpath(urlpath);
  schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    printf("Cannot create the super-chunk\n");
    goto cleanup;
  }

  /* Step 1: append chunks
   *
   *   [ 0][ 1][ 2][ 3]
   */
  for (int32_t tag = 0; tag < NAPPEND; tag++) {
    fill_chunk(buf, tag);
    nchunks = blosc2_schunk_append_buffer(schunk, buf, nbytes);
    if (nchunks != tag + 1) {
      printf("Cannot append chunk %d.  Error code: %lld\n", tag, (long long)nchunks);
      goto cleanup;
    }
  }
  if (print_chunks("Appended 4 chunks:", schunk, buf, nbytes) < 0) {
    goto cleanup;
  }

  /* Step 2: insert a chunk at position 2; the chunks from there on move one up
   *
   *   before:  [ 0][ 1][ 2][ 3]
   *                     |    \    \
   *   after:   [ 0][ 1][10][ 2][ 3]
   *                     ^ new chunk 2
   *
   * Any position from 0 (before the first chunk) to nchunks (after the last
   * one, like an append) works.
   */
  fill_chunk(buf, TAG_INSERTED);
  nchunks = blosc2_schunk_insert_buffer(schunk, 2, buf, nbytes);
  if (nchunks != NAPPEND + 1) {
    printf("Cannot insert the chunk.  Error code: %lld\n", (long long)nchunks);
    goto cleanup;
  }
  if (print_chunks("Inserted a chunk at 2:", schunk, buf, nbytes) < 0) {
    goto cleanup;
  }

  /* Step 3: replace chunk 0; the number of chunks stays the same
   *
   *   before:  [ 0][ 1][10][ 2][ 3]
   *   after:   [20][ 1][10][ 2][ 3]
   *             ^ replaced
   */
  fill_chunk(buf, TAG_UPDATED);
  nchunks = blosc2_schunk_update_buffer(schunk, 0, buf, nbytes);
  if (nchunks != NAPPEND + 1) {
    printf("Cannot update chunk 0.  Error code: %lld\n", (long long)nchunks);
    goto cleanup;
  }
  if (print_chunks("Updated chunk 0:", schunk, buf, nbytes) < 0) {
    goto cleanup;
  }

  /* Step 4: change every chunk in place, one at a time.  This is where reusing
   * the compression buffer pays off: one chunk-sized buffer for the whole loop.
   *
   *              frame                                  buf (uncompressed)
   *   +-------------------------+  decompress_chunk()  +-----------+
   *   | ... | chunk n | ...     | -------------------> |   items   |
   *   +-------------------------+                      +-----------+
   *                ^                                         |
   *                |                                    items += 1
   *                |                                         |
   *                +-----------------------------------------+
   *                             update_buffer()
   */
  for (int64_t nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    int dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, buf, nbytes);
    if (dsize != nbytes) {
      printf("Cannot decompress chunk %lld.  Error code: %d\n", (long long)nchunk, dsize);
      goto cleanup;
    }
    for (int32_t i = 0; i < CHUNK_ITEMS; i++) {
      buf[i] += 1;
    }
    nchunks = blosc2_schunk_update_buffer(schunk, nchunk, buf, nbytes);
    if (nchunks < 0) {
      printf("Cannot update chunk %lld.  Error code: %lld\n", (long long)nchunk, (long long)nchunks);
      goto cleanup;
    }
  }
  if (print_chunks("Added 1 to every item:", schunk, buf, nbytes) < 0) {
    goto cleanup;
  }

  // Everything is in the file: open it again and check every item
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open(urlpath);
  if (schunk == NULL) {
    printf("Cannot open %s\n", urlpath);
    goto cleanup;
  }
  if (print_chunks("Reopened the file:", schunk, buf, nbytes) < 0) {
    goto cleanup;
  }
  const int32_t expected_tags[NAPPEND + 1] = {TAG_UPDATED, 1, TAG_INSERTED, 2, 3};
  if (schunk->nchunks != NAPPEND + 1) {
    printf("Unexpected number of chunks: %lld\n", (long long)schunk->nchunks);
    goto cleanup;
  }
  for (int64_t nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    int dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, buf, nbytes);
    if (dsize != nbytes) {
      printf("Cannot decompress chunk %lld.  Error code: %d\n", (long long)nchunk, dsize);
      goto cleanup;
    }
    for (int32_t i = 0; i < CHUNK_ITEMS; i++) {
      int32_t expected = expected_tags[nchunk] * TAG_STRIDE + i + 1;
      if (buf[i] != expected) {
        printf("Chunk %lld, item %d: %d instead of %d\n", (long long)nchunk, i, buf[i], expected);
        goto cleanup;
      }
    }
  }
  printf("Successful roundtrip data <-> schunk!\n");
  rc = 0;

cleanup:
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }
  free(buf);
  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}

/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Lazy chunk reads through the mmap io, which hands out pointers into its
  mapping instead of filling the caller's buffer, and NULL for a read past the
  end of the file.

  - A damaged chunk header that makes frame_get_lazychunk() read its block
    offsets past the end of the file: the read returned NULL and was memcpy'ed
    before its length was checked (segfault instead of an error).  Damaged block
    offsets (with either io) overflowed int32 while being sorted.
  - Single VL-block reads of a lazy chunk (blosc2_vldecompress_block_ctx(),
    blosc2_vlblock_get_nbytes_ctx()) peek at the block's size prefix on disk
    and free()d the pointer the read returned, which is not theirs to free:
    any valid VL-block frame opened with the mmap io crashed.
*/

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define NITEMS (256)

int tests_run = 0;


static blosc2_schunk *open_mmap(const char *urlpath, blosc2_stdio_mmap *mmap_file) {
  *mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file->mode = "r";
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = mmap_file};
  return blosc2_schunk_open_udio(urlpath, &io);
}


/* Write a frame of two chunks (4 blocks each) to a file, with `damage` applied
   to the second chunk, then check that reading that chunk fails with either io
   while the first one still reads */
static char *check_damaged_chunk1(const char *urlpath, const int32_t *data,
                                  void (*damage)(uint8_t *chunk1)) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.blocksize = NITEMS;
  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);
  for (int c = 0; c < 2; c++) {
    mu_assert("Cannot append chunk", blosc2_schunk_append_buffer(schunk, data, NITEMS * sizeof(int32_t)) == c + 1);
  }
  // The in-memory frame itself: chunks follow the header in append order
  uint8_t *frame;
  bool needs_free;
  int64_t len = blosc2_schunk_to_buffer(schunk, &frame, &needs_free);
  mu_assert("Cannot get the frame", len > 0 && !needs_free);
  int32_t header_len;
  from_big(&header_len, frame + FRAME_HEADER_LEN, sizeof(header_len));
  damage(frame + header_len + sw32_(frame + header_len + BLOSC2_CHUNK_CBYTES));
  blosc2_remove_urlpath(urlpath);
  mu_assert("Cannot write the damaged frame", blosc2_schunk_to_file(schunk, urlpath) == len);
  blosc2_schunk_free(schunk);

  for (int mmap_io = 0; mmap_io < 2; mmap_io++) {
    blosc2_stdio_mmap mmap_file;
    schunk = mmap_io ? open_mmap(urlpath, &mmap_file) : blosc2_schunk_open(urlpath);
    mu_assert("Cannot open the damaged frame", schunk != NULL);

    uint8_t *chunk = NULL;
    needs_free = false;
    int cbytes = blosc2_schunk_get_lazychunk(schunk, 1, &chunk, &needs_free);
    mu_assert("A lazy read of the damaged chunk must fail", cbytes < 0);
    if (needs_free) {
      free(chunk);
    }
    int32_t dest[NITEMS];
    mu_assert("Decompressing the damaged chunk must fail",
              blosc2_schunk_decompress_chunk(schunk, 1, dest, sizeof(dest)) < 0);
    // The undamaged chunk still reads
    mu_assert("Cannot decompress the undamaged chunk",
              blosc2_schunk_decompress_chunk(schunk, 0, dest, sizeof(dest)) == (int)sizeof(dest));
    mu_assert("Wrong data in the undamaged chunk", memcmp(dest, data, sizeof(dest)) == 0);
    blosc2_schunk_free(schunk);
  }
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* About a million blocks (and the room for their offsets): a lazy read asks
   for 4 MB of block offsets out of a 2 KB file */
static void claim_many_blocks(uint8_t *chunk1) {
  _sw32(chunk1 + BLOSC2_CHUNK_NBYTES, sw32_(chunk1 + BLOSC2_CHUNK_BLOCKSIZE) * (1 << 20));
  _sw32(chunk1 + BLOSC2_CHUNK_CBYTES, INT32_MAX);
}


/* Block starts far outside the chunk: their differences overflowed int32 */
static void scramble_block_starts(uint8_t *chunk1) {
  _sw32(chunk1 + BLOSC_EXTENDED_HEADER_LENGTH, -2000000000);
  _sw32(chunk1 + BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t), 2000000000);
}


static char *test_damaged_block_offsets(void) {
  int32_t data[NITEMS];
  for (int i = 0; i < NITEMS; i++) {
    data[i] = i;
  }
  blosc2_init();
  char *msg = check_damaged_chunk1("test_frame_lazychunk_io_damaged.b2frame", data, claim_many_blocks);
  if (msg == EXIT_SUCCESS) {
    msg = check_damaged_chunk1("test_frame_lazychunk_io_damaged.b2frame", data, scramble_block_starts);
  }
  blosc2_destroy();
  return msg;
}


static char *test_vlblocks_of_lazy_chunk(void) {
  const char *urlpath = "test_frame_lazychunk_io_vl.b2frame";
  static char b0[5000], b1[7000], b2[3000];
  memset(b0, 'a', sizeof(b0));
  memset(b1, 'b', sizeof(b1));
  memset(b2, 'c', sizeof(b2));
  const void *srcs[3] = {b0, b1, b2};
  int32_t srcsizes[3] = {sizeof(b0), sizeof(b1), sizeof(b2)};

  blosc2_init();
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("Cannot create cctx", cctx != NULL);
  int32_t destsize = sizeof(b0) + sizeof(b1) + sizeof(b2) + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t *vlchunk = malloc((size_t)destsize);
  int cbytes = blosc2_vlcompress_ctx(cctx, srcs, srcsizes, 3, vlchunk, destsize);
  blosc2_free_ctx(cctx);
  mu_assert("Cannot create a VL-block chunk", cbytes > 0);
  blosc2_storage storage = {.contiguous = true, .urlpath = (char *)urlpath, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);
  mu_assert("Cannot append chunk", blosc2_schunk_append_chunk(schunk, vlchunk, true) == 1);
  free(vlchunk);
  blosc2_schunk_free(schunk);

  blosc2_stdio_mmap mmap_file;
  schunk = open_mmap(urlpath, &mmap_file);
  mu_assert("Cannot open the frame with the mmap io", schunk != NULL);
  uint8_t *chunk = NULL;
  bool needs_free = false;
  int lazy_cbytes = blosc2_schunk_get_lazychunk(schunk, 0, &chunk, &needs_free);
  mu_assert("Cannot get the lazy chunk", lazy_cbytes > 0);
  for (int i = 0; i < 3; i++) {
    int32_t nbytes = 0;
    mu_assert("Cannot get the block size",
              blosc2_vlblock_get_nbytes_ctx(schunk->dctx, chunk, lazy_cbytes, i, &nbytes) == 0);
    mu_assert("Wrong block size", nbytes == srcsizes[i]);
    uint8_t *block = NULL;
    int32_t blocksize = 0;
    int rc = blosc2_vldecompress_block_ctx(schunk->dctx, chunk, lazy_cbytes, i, &block, &blocksize);
    mu_assert("Cannot decompress the block", rc == srcsizes[i] && blocksize == srcsizes[i]);
    mu_assert("Wrong block content", memcmp(block, srcs[i], (size_t)srcsizes[i]) == 0);
    free(block);
  }
  if (needs_free) {
    free(chunk);
  }
  blosc2_schunk_free(schunk);

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_damaged_block_offsets);
  mu_run_test(test_vlblocks_of_lazy_chunk);
  return EXIT_SUCCESS;
}


int main(void) {
  char *result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  return result != EXIT_SUCCESS;
}

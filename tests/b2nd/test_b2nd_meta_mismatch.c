/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* b2nd_from_schunk() (so b2nd_open(), b2nd_open_offset(), b2nd_from_cframe())
   rejects a super-chunk that contradicts its b2nd metalayer, as damaged frames
   do: fewer chunks than the shape covers (readers then ask for chunks that are
   not there, or allocate results for a huge shape), or chunks of another size
   than the chunk shape times the typesize (a damaged typesize made
   python-blosc2's orthogonal selections write past their result).  Everything
   c-blosc2 and python-blosc2 write must keep opening, whatever the dtype: e.g.
   the "|u1" that c-blosc2 2.6 to 2.16 stored for arrays created without a
   dtype, whatever their typesize. */

#include "test_common.h"
#include "blosc-private.h"
#include "frame.h"

#define HUGE_DIM 137438953481  // a shape[0] that bit flips left in a fuzzed frame of 25 chunks

/* A contiguous in-memory schunk with a b2nd metalayer for `shape`/`chunks` and
   `nchunks` stored chunks of `typesize`-byte items, or of `chunkbytes` bytes
   when that is > 0. */
static blosc2_schunk *crafted_schunk(int8_t ndim, const int64_t *shape, const int32_t *chunks, const char *dtype,
                              int32_t typesize, int64_t nchunks, int32_t chunkbytes) {
  uint8_t *smeta = NULL;
  int32_t smeta_len = b2nd_serialize_meta(ndim, shape, chunks, chunks, dtype, DTYPE_NUMPY_FORMAT, &smeta);
  if (smeta_len < 0) {
    return NULL;
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage storage = {.cparams = &cparams, .contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  int rc = schunk == NULL ? BLOSC2_ERROR_FAILURE : blosc2_meta_add(schunk, "b2nd", smeta, smeta_len);
  free(smeta);
  if (rc < 0) {
    if (schunk != NULL) {
      blosc2_schunk_free(schunk);
    }
    return NULL;
  }
  if (chunkbytes <= 0) {
    chunkbytes = typesize;
    for (int i = 0; i < ndim; i++) {
      chunkbytes *= chunks[i];
    }
  }
  // Chunks compressed apart: their size need not be a multiple of the typesize
  blosc2_cparams bytes_cparams = BLOSC2_CPARAMS_DEFAULTS;
  bytes_cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(bytes_cparams);
  uint8_t *buf = malloc((size_t)chunkbytes);
  uint8_t *chunk = malloc((size_t)chunkbytes + BLOSC2_MAX_OVERHEAD);
  for (int32_t i = 0; i < chunkbytes; i++) {
    buf[i] = (uint8_t)(i * 7);
  }
  int cbytes = blosc2_compress_ctx(cctx, buf, chunkbytes, chunk, chunkbytes + BLOSC2_MAX_OVERHEAD);
  for (int64_t i = 0; i < nchunks; i++) {
    if (cbytes <= 0 || blosc2_schunk_append_chunk(schunk, chunk, true) < 0) {
      blosc2_schunk_free(schunk);
      schunk = NULL;
      break;
    }
  }
  free(chunk);
  free(buf);
  blosc2_free_ctx(cctx);
  return schunk;
}

/* b2nd_from_schunk() on a crafted schunk: 0 if it built an array, else its error code */
static int from_crafted(int8_t ndim, const int64_t *shape, const int32_t *chunks, const char *dtype,
                        int32_t typesize, int64_t nchunks, int32_t chunkbytes) {
  blosc2_schunk *schunk = crafted_schunk(ndim, shape, chunks, dtype, typesize, nchunks, chunkbytes);
  if (schunk == NULL) {
    return INT32_MIN;
  }
  b2nd_array_t *array = NULL;
  int rc = b2nd_from_schunk(schunk, &array);
  if (rc < 0) {
    // On failure the schunk stays with the caller, and no array comes back
    if (array != NULL) {
      return INT32_MIN;
    }
    blosc2_schunk_free(schunk);
    return rc;
  }
  b2nd_free(array);
  return 0;
}


#define META_SHAPE0 (-1)  // the frame offset that open_damaged() finds for the metalayer's shape[0]

/* Write a valid (11, 16) array to a file, grown, reopened and shrunk to (4, 16) to cover resizes, check that it
   opens, then store `value` big-endian in its `len` bytes at frame offset `pos` (or at META_SHAPE0) and check
   that every way to open it as an array fails while the super-chunk itself still opens. */
static bool open_damaged(const char *dtype, int32_t typesize, int64_t pos, int64_t value, int32_t len) {
  const char *urlpath = "test_b2nd_meta_mismatch.b2nd";
  int64_t shape[2] = {11, 16};
  int32_t chunks[2] = {3, 16};
  int32_t blocks[2] = {3, 8};
  int64_t grown[2] = {22, 16};
  int64_t shrunk[2] = {4, 16};
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage storage = {.cparams = &cparams, .contiguous = true, .urlpath = (char *)urlpath};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunks, blocks, dtype, DTYPE_NUMPY_FORMAT, NULL, 0);
  // Real data: a frame whose first chunk is a special value does not open with some damaged typesizes
  int64_t nbytes = shape[0] * shape[1] * typesize;
  uint8_t *data = malloc((size_t)nbytes);
  for (int64_t i = 0; i < nbytes; i++) {
    data[i] = (uint8_t)(i % 251);
  }
  b2nd_array_t *array;
  bool ok = ctx != NULL && b2nd_from_cbuffer(ctx, &array, data, nbytes) >= 0;
  free(data);
  ok = ok && b2nd_resize(array, grown, NULL) >= 0 && b2nd_free(array) >= 0;
  b2nd_free_ctx(ctx);
  ok = ok && b2nd_open(urlpath, &array) >= 0;
  ok = ok && b2nd_resize(array, shrunk, NULL) >= 0 && b2nd_free(array) >= 0;
  ok = ok && b2nd_open(urlpath, &array) >= 0;
  if (!ok || array->shape[0] != shrunk[0] || b2nd_free(array) < 0) {
    return false;
  }

  FILE *f = fopen(urlpath, "rb");
  if (f == NULL) {
    return false;
  }
  fseek(f, 0, SEEK_END);
  int64_t frame_len = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *frame = malloc((size_t)frame_len);
  ok = frame != NULL && fread(frame, 1, (size_t)frame_len, f) == (size_t)frame_len;
  fclose(f);
  if (!ok) {
    free(frame);
    return false;
  }
  if (pos == META_SHAPE0) {
    // The metalayer is stored verbatim in the header; shape[0] follows its array
    // header, version, ndim, shape array header and int64 marker
    uint8_t *smeta;
    int32_t smeta_len = b2nd_serialize_meta(2, shrunk, chunks, blocks, dtype, DTYPE_NUMPY_FORMAT, &smeta);
    for (int64_t i = 0; i + smeta_len <= frame_len && pos < 0; i++) {
      if (memcmp(frame + i, smeta, (size_t)smeta_len) == 0) {
        pos = i + 5;
      }
    }
    free(smeta);
  }
  ok = ok && pos >= 0;
  if (ok) {
    if (len == (int32_t)sizeof(int32_t)) {
      int32_t value32 = (int32_t)value;
      to_big(frame + pos, &value32, len);
    }
    else {
      to_big(frame + pos, &value, len);
    }
  }
  f = fopen(urlpath, "wb");
  ok = ok && f != NULL && fwrite(frame, 1, (size_t)frame_len, f) == (size_t)frame_len;
  if (f != NULL) {
    fclose(f);
  }

  array = NULL;
  ok = ok && b2nd_open(urlpath, &array) < 0 && array == NULL;
  ok = ok && b2nd_open_offset(urlpath, &array, 0) < 0 && array == NULL;
  ok = ok && b2nd_from_cframe(frame, frame_len, true, &array) < 0 && array == NULL;
  ok = ok && b2nd_from_cframe(frame, frame_len, false, &array) < 0 && array == NULL;
  blosc2_stdio_mmap mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file.mode = "r";
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file};
  blosc2_schunk *schunk = blosc2_schunk_open_udio(urlpath, &io);
  ok = ok && schunk != NULL;
  ok = ok && b2nd_from_schunk(schunk, &array) == BLOSC2_ERROR_INVALID_HEADER && array == NULL;
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }
  free(frame);
  blosc2_remove_urlpath(urlpath);
  return ok;
}


CUTEST_TEST_SETUP(meta_mismatch) {
  blosc2_init();
}


CUTEST_TEST_TEST(meta_mismatch) {
  int64_t shape[1] = {10};
  int32_t chunks[1] = {5};

  /* stored chunks against the shape */
  CUTEST_ASSERT("all chunks stored must open", from_crafted(1, shape, chunks, "<i4", 4, 2, 0) == 0);
  CUTEST_ASSERT("more chunks than the shape covers must open", from_crafted(1, shape, chunks, "<i4", 4, 3, 0) == 0);
  CUTEST_ASSERT("a missing chunk must be rejected",
                from_crafted(1, shape, chunks, "<i4", 4, 1, 0) == BLOSC2_ERROR_INVALID_HEADER);
  int64_t big[2] = {HUGE_DIM, 193};
  int32_t chunks2[2] = {16, 193};
  CUTEST_ASSERT("a huge shape over 25 chunks must be rejected",
                from_crafted(2, big, chunks2, "<i2", 2, 25, 0) == BLOSC2_ERROR_INVALID_HEADER);
  int64_t empty[2] = {0, 5};
  int32_t chunks_empty[2] = {4, 5};
  CUTEST_ASSERT("an empty array without chunks must open", from_crafted(2, empty, chunks_empty, "<i4", 4, 0, 0) == 0);
  CUTEST_ASSERT("a 0-d array with its chunk must open", from_crafted(0, NULL, NULL, "<f8", 8, 1, 0) == 0);
  CUTEST_ASSERT("a 0-d array without its chunk must be rejected",
                from_crafted(0, NULL, NULL, "<f8", 8, 0, 0) == BLOSC2_ERROR_INVALID_HEADER);
  /* a shape whose (unused) chunk_array_strides product overflowed int64 while the array was built */
  int64_t wide[3] = {2, (int64_t)1 << 40, 1 << 20};
  int32_t chunks3[3] = {1, 1 << 20, 1 << 10};
  CUTEST_ASSERT("a shape with overflowing chunk strides must be rejected cleanly",
                from_crafted(3, wide, chunks3, "|u1", 1, 1, 8) == BLOSC2_ERROR_INVALID_HEADER);

  /* the chunk size against the chunk shape and the typesize */
  CUTEST_ASSERT("chunks of another size must be rejected",
                from_crafted(1, shape, chunks, "<f8", 24, 2, 5 * 8) == BLOSC2_ERROR_INVALID_HEADER);
  CUTEST_ASSERT("chunks of another size must be rejected whatever the dtype",
                from_crafted(1, shape, chunks, "|u1", 4, 2, 5 * 1) == BLOSC2_ERROR_INVALID_HEADER);

  /* the dtype is not interpreted */
  CUTEST_ASSERT("legacy |u1 with typesize 8 must open", from_crafted(1, shape, chunks, "|u1", 8, 2, 0) == 0);
  CUTEST_ASSERT("legacy |u1 with typesize 300 must open", from_crafted(1, shape, chunks, "|u1", 300, 2, 0) == 0);
  CUTEST_ASSERT("<U10 with typesize 40 must open", from_crafted(1, shape, chunks, "<U10", 40, 2, 0) == 0);
  CUTEST_ASSERT("a structured dtype must open",
                from_crafted(1, shape, chunks, "[('a', '<i4'), ('b', '<f8')]", 12, 2, 0) == 0);

  /* The same through files, as bit flips in a frame header leave them, and through every way to open one */
  CUTEST_ASSERT("a damaged typesize must be rejected", open_damaged("<f8", 8, FRAME_TYPESIZE, 24, 4));
  CUTEST_ASSERT("a damaged typesize of a legacy |u1 array must be rejected",
                open_damaged("|u1", 4, FRAME_TYPESIZE, 4 + 65536, 4));
  CUTEST_ASSERT("a damaged shape must be rejected", open_damaged("<f8", 8, META_SHAPE0, HUGE_DIM, 8));

  return 0;
}


CUTEST_TEST_TEARDOWN(meta_mismatch) {
  blosc2_destroy();
}


int main(void) {
  CUTEST_TEST_RUN(meta_mismatch);
}

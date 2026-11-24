/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Chunk updates between blosc2_schunk_begin_updates() and blosc2_schunk_end_updates(): a resized chunk
  leaves a hole instead of moving the payload behind it, and the end closes all holes in one pass.  The
  frame must hold the right data at every point (also as seen by another handle), and be exactly as
  compact afterwards as one written without holes.
*/

#include <inttypes.h>

#include "test_common.h"
#include "frame.h"
#include "b2nd.h"

#define URLPATH "test_deferred_updates.b2frame"
#define NITEMS 2048
#define NCHUNKS 24
#define CHUNK_NBYTES (NITEMS * (int32_t)sizeof(int64_t))

int tests_run = 0;

/* How a chunk is made: its compressed size ranges from 0 (special, in the offsets) to more than its data */
enum { KIND_CONST, KIND_RANDOM, KIND_HALF, KIND_3QUARTERS, KIND_BYTES, KIND_ZEROS, KIND_VALUE, NKINDS };

typedef struct {
  int kind;
  uint64_t seed;
} chunk_model;

static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;

static uint64_t rng(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return rng_state;
}

static void fill_data(chunk_model m, int64_t* data) {
  uint64_t s = m.seed | 1;
  for (int i = 0; i < NITEMS; i++) {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    switch (m.kind) {
      case KIND_CONST:
      case KIND_VALUE:
        data[i] = (int64_t)m.seed;
        break;
      case KIND_RANDOM:
        data[i] = (int64_t)s;
        break;
      case KIND_HALF:
        data[i] = i < NITEMS / 2 ? (int64_t)s : (int64_t)m.seed;
        break;
      case KIND_3QUARTERS:
        data[i] = i < 3 * NITEMS / 4 ? (int64_t)s : (int64_t)m.seed;
        break;
      case KIND_BYTES:
        data[i] = (int64_t)(s & 0xff);
        break;
      default:
        data[i] = 0;
    }
  }
}

/* A new chunk (malloc'ed) for the model */
static uint8_t* make_chunk(blosc2_schunk* schunk, chunk_model m) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  int32_t maxlen = CHUNK_NBYTES + BLOSC2_MAX_OVERHEAD;
  uint8_t* chunk = malloc((size_t)maxlen);
  int rc;
  if (m.kind == KIND_ZEROS) {
    rc = blosc2_chunk_zeros(cparams, CHUNK_NBYTES, chunk, maxlen);
  }
  else if (m.kind == KIND_VALUE) {
    int64_t value = (int64_t)m.seed;
    rc = blosc2_chunk_repeatval(cparams, CHUNK_NBYTES, chunk, maxlen, &value);
  }
  else {
    int64_t* data = malloc(CHUNK_NBYTES);
    fill_data(m, data);
    rc = blosc2_compress_ctx(schunk->cctx, data, CHUNK_NBYTES, chunk, maxlen);
    free(data);
  }
  if (rc < 0) {
    free(chunk);
    return NULL;
  }
  return chunk;
}

static chunk_model random_model(void) {
  chunk_model m = {.kind = (int)(rng() % NKINDS), .seed = rng()};
  return m;
}

/* Every chunk holds the data of its model */
static bool check_data(blosc2_schunk* schunk, const chunk_model* model, int64_t nchunks) {
  if (schunk->nchunks != nchunks) {
    return false;
  }
  int64_t expected[NITEMS];
  int64_t data[NITEMS];
  for (int64_t i = 0; i < nchunks; i++) {
    fill_data(model[i], expected);
    if (blosc2_schunk_decompress_chunk(schunk, i, data, CHUNK_NBYTES) != CHUNK_NBYTES ||
        memcmp(data, expected, CHUNK_NBYTES) != 0) {
      fprintf(stderr, "chunk %" PRId64 " (kind %d) is wrong\n", i, model[i].kind);
      return false;
    }
  }
  return true;
}

static int64_t file_size(const char* urlpath) {
  FILE* f = fopen(urlpath, "rb");
  if (f == NULL) {
    return -1;
  }
  fseek(f, 0, SEEK_END);
  int64_t size = (int64_t)ftell(f);
  fclose(f);
  return size;
}


static int compare_int64(const void* a, const void* b) {
  int64_t x = *(const int64_t*)a;
  int64_t y = *(const int64_t*)b;
  return (x > y) - (x < y);
}

/* The stored chunks lie back to back from the start of the payload, which ends right behind them, and a
   frame on disk is as long as its file (if check_file_len; the mmap I/O on Windows cuts the file only when
   it closes it) */
static bool check_compact_file(blosc2_schunk* schunk, bool check_file_len) {
  int64_t* offsets = blosc2_frame_get_offsets(schunk);
  if (offsets == NULL) {
    return false;
  }
  int64_t* sorted = malloc((size_t)schunk->nchunks * 2 * sizeof(int64_t));
  int64_t nstored = 0;
  for (int64_t i = 0; i < schunk->nchunks; i++) {
    if (offsets[i] < 0) {
      continue;
    }
    uint8_t* chunk;
    bool needs_free;
    int32_t cbytes = blosc2_schunk_get_chunk(schunk, i, &chunk, &needs_free);
    if (needs_free) {
      free(chunk);
    }
    sorted[2 * nstored] = offsets[i];
    sorted[2 * nstored + 1] = cbytes;
    nstored++;
  }
  qsort(sorted, (size_t)nstored, 2 * sizeof(int64_t), compare_int64);
  bool ok = true;
  int64_t pos = 0;
  for (int64_t k = 0; k < nstored; k++) {
    ok = ok && sorted[2 * k] == pos;
    pos += sorted[2 * k + 1];
  }
  ok = ok && schunk->cbytes == pos;
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  if (frame->urlpath != NULL && check_file_len) {
    ok = ok && file_size(frame->urlpath) == frame->len;
  }
  if (!ok) {
    fprintf(stderr, "frame not compact: cbytes %" PRId64 ", stored %" PRId64 "\n", schunk->cbytes, pos);
  }
  free(sorted);
  free(offsets);
  return ok;
}

static bool check_compact(blosc2_schunk* schunk) {
  return check_compact_file(schunk, true);
}


static blosc2_schunk* reopen(const char* urlpath, bool mmap_io) {
  if (!mmap_io) {
    return blosc2_schunk_open(urlpath);
  }
  static blosc2_stdio_mmap mmap_params;
  mmap_params = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_params.mode = "r";
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_params};
  return blosc2_schunk_open_udio(urlpath, &io);
}


/* Random sequences of updates, inserts, appends and deletes (sequential full overwrites, random ones,
   backwards) in nested update brackets, checked against a model inside the brackets and from a second
   handle; after each bracket the data must be right, and the payload exactly as long as that of a twin
   super-chunk that got the same calls without brackets (compact, unless chunks were deleted) */
static char* random_sequences(const char* urlpath, int io_kind) {
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.nthreads = 2;
  blosc2_storage storage = {.contiguous = true, .urlpath = (char*)urlpath, .cparams = &cparams};
  static blosc2_stdio_params lock_params = {.locking = true};
  blosc2_io lock_io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &lock_params};
  blosc2_stdio_mmap mmap_params = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_params.mode = "w+";
  mmap_params.initial_mapping_size = 64 * 1024;  // remaps while the frame grows
  blosc2_io mmap_io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_params};
  if (io_kind == 1) {
    storage.io = &lock_io;
  }
  else if (io_kind == 2) {
    storage.io = &mmap_io;
  }
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create the super-chunk", schunk != NULL);
  blosc2_storage twin_storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk* twin = blosc2_schunk_new(&twin_storage);

  int64_t cap = 4 * NCHUNKS;
  chunk_model* model = malloc((size_t)cap * sizeof(chunk_model));
  int64_t nchunks = 0;
  for (; nchunks < NCHUNKS; nchunks++) {
    model[nchunks] = random_model();
    uint8_t* chunk = make_chunk(schunk, model[nchunks]);
    mu_assert("ERROR: cannot append", blosc2_schunk_append_chunk(twin, chunk, true) == nchunks + 1);
    mu_assert("ERROR: cannot append", blosc2_schunk_append_chunk(schunk, chunk, false) == nchunks + 1);
  }
  bool deleted = false;

  for (int round = 0; round < 40; round++) {
    mu_assert("ERROR: begin_updates failed", blosc2_schunk_begin_updates(schunk) == 0);
    int nupdates = 1 + (int)(rng() % (uint64_t)(2 * nchunks));
    int order = round % 3;
    for (int u = 0; u < nupdates; u++) {
      bool nested = rng() % 5 == 0;
      if (nested) {
        mu_assert("ERROR: nested begin_updates failed", blosc2_schunk_begin_updates(schunk) == 0);
      }
      uint64_t op = rng() % 32;
      chunk_model m = random_model();
      uint8_t* chunk = make_chunk(schunk, m);
      mu_assert("ERROR: cannot make a chunk", chunk != NULL);
      if (op == 0 && nchunks < cap) {
        mu_assert("ERROR: append failed", blosc2_schunk_append_chunk(twin, chunk, true) == nchunks + 1);
        mu_assert("ERROR: append failed", blosc2_schunk_append_chunk(schunk, chunk, false) == nchunks + 1);
        model[nchunks++] = m;
      }
      else if (op == 1 && nchunks < cap) {
        int64_t pos = (int64_t)(rng() % (uint64_t)(nchunks + 1));
        mu_assert("ERROR: insert failed", blosc2_schunk_insert_chunk(twin, pos, chunk, true) == nchunks + 1);
        mu_assert("ERROR: insert failed", blosc2_schunk_insert_chunk(schunk, pos, chunk, false) == nchunks + 1);
        memmove(model + pos + 1, model + pos, (size_t)(nchunks - pos) * sizeof(chunk_model));
        model[pos] = m;
        nchunks++;
      }
      else if (op == 2 && nchunks > 2 && round % 5 == 4) {
        int64_t pos = (int64_t)(rng() % (uint64_t)nchunks);
        mu_assert("ERROR: delete failed", blosc2_schunk_delete_chunk(twin, pos) == nchunks - 1);
        mu_assert("ERROR: delete failed", blosc2_schunk_delete_chunk(schunk, pos) == nchunks - 1);
        free(chunk);
        memmove(model + pos, model + pos + 1, (size_t)(nchunks - pos - 1) * sizeof(chunk_model));
        nchunks--;
        deleted = true;
      }
      else {
        int64_t nchunk = order == 0 ? u % nchunks : order == 1 ? (int64_t)(rng() % (uint64_t)nchunks)
                                                                : nchunks - 1 - u % nchunks;
        mu_assert("ERROR: update failed", blosc2_schunk_update_chunk(twin, nchunk, chunk, true) == nchunks);
        mu_assert("ERROR: update failed", blosc2_schunk_update_chunk(schunk, nchunk, chunk, true) == nchunks);
        free(chunk);
        model[nchunk] = m;
      }
      if (nested) {
        mu_assert("ERROR: nested end_updates failed", blosc2_schunk_end_updates(schunk) == 0);
      }
      if (rng() % 8 == 0) {
        mu_assert("ERROR: wrong data inside the bracket", check_data(schunk, model, nchunks));
      }
    }
    if (io_kind != 1 && round % 4 == 0 && urlpath != NULL) {
      // The frame on disk is valid in the middle of the bracket (a locking handle would wait for the lock)
      blosc2_schunk* other = reopen(urlpath, io_kind == 2);
      mu_assert("ERROR: cannot open the frame inside the bracket", other != NULL);
      mu_assert("ERROR: wrong data in a second handle", check_data(other, model, nchunks));
      blosc2_schunk_free(other);
    }
    mu_assert("ERROR: end_updates failed", blosc2_schunk_end_updates(schunk) == 0);
    mu_assert("ERROR: wrong data after the bracket", check_data(schunk, model, nchunks));
    mu_assert("ERROR: payload not as long as without brackets", schunk->cbytes == twin->cbytes);
#if defined(_WIN32)
    bool check_file_len = io_kind != 2;
#else
    bool check_file_len = true;
#endif
    mu_assert("ERROR: frame not compact after the bracket", deleted || check_compact_file(schunk, check_file_len));
    if (urlpath != NULL && round % 8 == 7) {
      blosc2_schunk_free(schunk);
      schunk = io_kind == 1 ? blosc2_schunk_open_udio(urlpath, &lock_io) : reopen(urlpath, false);
      mu_assert("ERROR: cannot reopen", schunk != NULL);
      mu_assert("ERROR: wrong data after reopening", check_data(schunk, model, nchunks));
      mu_assert("ERROR: payload not as long as without brackets after reopening", schunk->cbytes == twin->cbytes);
      mu_assert("ERROR: frame not compact after reopening", deleted || check_compact(schunk));
    }
  }
  mu_assert("ERROR: the twin is wrong", check_data(twin, model, nchunks));
  free(model);
  blosc2_schunk_free(twin);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}

static char* test_random_in_memory(void) {
  return random_sequences(NULL, 0);
}

static char* test_random_on_disk(void) {
  return random_sequences(URLPATH, 0);
}

static char* test_random_locking(void) {
  return random_sequences(URLPATH, 1);
}

static char* test_random_mmap(void) {
  return random_sequences(URLPATH, 2);
}


/* Where resized chunks go inside a bracket: nothing else moves */
static char* placement(const char* urlpath) {
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous = true, .urlpath = (char*)urlpath, .cparams = &cparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  chunk_model model[6];
  for (int i = 0; i < 6; i++) {
    model[i] = (chunk_model){.kind = KIND_HALF, .seed = (uint64_t)i + 1};
    blosc2_schunk_append_chunk(schunk, make_chunk(schunk, model[i]), false);
  }
  int64_t* before = blosc2_frame_get_offsets(schunk);
  int64_t payload = schunk->cbytes;
  int32_t half = (int32_t)(payload / 6);

  mu_assert("ERROR: begin_updates failed", blosc2_schunk_begin_updates(schunk) == 0);
  // A smaller chunk 1 stays in place and leaves a hole behind it
  model[1] = (chunk_model){.kind = KIND_BYTES, .seed = 11};
  uint8_t* chunk = make_chunk(schunk, model[1]);
  int32_t small_cbytes;
  blosc2_cbuffer_sizes(chunk, NULL, &small_cbytes, NULL);
  mu_assert("ERROR: the test needs a smaller chunk", small_cbytes < half);
  blosc2_schunk_update_chunk(schunk, 1, chunk, false);
  int64_t* offsets = blosc2_frame_get_offsets(schunk);
  for (int i = 0; i < 6; i++) {
    mu_assert("ERROR: a chunk moved", offsets[i] == before[i]);
  }
  mu_assert("ERROR: the payload changed its length", schunk->cbytes == payload);
  free(offsets);

  // A larger chunk 2 takes that hole and its own place
  model[2] = (chunk_model){.kind = KIND_3QUARTERS, .seed = 12};
  chunk = make_chunk(schunk, model[2]);
  int32_t large_cbytes;
  blosc2_cbuffer_sizes(chunk, NULL, &large_cbytes, NULL);
  mu_assert("ERROR: the test needs a chunk that fits there",
            large_cbytes > half && large_cbytes <= 2 * half - small_cbytes);
  blosc2_schunk_update_chunk(schunk, 2, chunk, false);
  offsets = blosc2_frame_get_offsets(schunk);
  mu_assert("ERROR: chunk 2 not slid into the hole", offsets[2] == before[1] + small_cbytes);
  mu_assert("ERROR: a chunk moved", offsets[3] == before[3] && offsets[4] == before[4]);
  free(offsets);

  // A larger chunk 0 does not fit (no hole next to it): it goes to the end
  model[0] = (chunk_model){.kind = KIND_3QUARTERS, .seed = 10};
  blosc2_schunk_update_chunk(schunk, 0, make_chunk(schunk, model[0]), false);
  offsets = blosc2_frame_get_offsets(schunk);
  mu_assert("ERROR: chunk 0 not at the end", offsets[0] == payload);
  free(offsets);
  int64_t end = schunk->cbytes;

  // The last chunk grows in place
  model[0] = (chunk_model){.kind = KIND_RANDOM, .seed = 20};
  blosc2_schunk_update_chunk(schunk, 0, make_chunk(schunk, model[0]), false);
  offsets = blosc2_frame_get_offsets(schunk);
  mu_assert("ERROR: the last chunk moved", offsets[0] == payload && schunk->cbytes > end);
  free(offsets);
  end = schunk->cbytes;

  // A chunk becomes special: its place is a hole; a special chunk becomes regular: at the end
  model[4] = (chunk_model){.kind = KIND_ZEROS};
  blosc2_schunk_update_chunk(schunk, 4, make_chunk(schunk, model[4]), false);
  model[4] = (chunk_model){.kind = KIND_HALF, .seed = 14};
  blosc2_schunk_update_chunk(schunk, 4, make_chunk(schunk, model[4]), false);
  offsets = blosc2_frame_get_offsets(schunk);
  mu_assert("ERROR: chunk 4 not at the end", offsets[4] == end);
  free(offsets);
  mu_assert("ERROR: wrong data inside the bracket", check_data(schunk, model, 6));

  mu_assert("ERROR: end_updates failed", blosc2_schunk_end_updates(schunk) == 0);
  mu_assert("ERROR: wrong data after the bracket", check_data(schunk, model, 6));
  mu_assert("ERROR: frame not compact", check_compact(schunk));
  // The chunks keep their order in the payload
  offsets = blosc2_frame_get_offsets(schunk);
  mu_assert("ERROR: order changed", offsets[1] < offsets[2] && offsets[2] < offsets[3] &&
                                    offsets[3] < offsets[5] && offsets[5] < offsets[0] && offsets[0] < offsets[4]);
  free(offsets);
  free(before);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}

static char* test_placement_in_memory(void) {
  return placement(NULL);
}

static char* test_placement_on_disk(void) {
  return placement(URLPATH);
}


/* The space of a chunk deleted inside a bracket stays, as without brackets; the holes around it close */
static char* test_delete_inside_bracket(void) {
  blosc2_remove_urlpath(URLPATH);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  blosc2_storage twin_storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk* twin = blosc2_schunk_new(&twin_storage);
  chunk_model model[8];
  for (int i = 0; i < 8; i++) {
    model[i] = (chunk_model){.kind = KIND_HALF, .seed = (uint64_t)i + 1};
    uint8_t* chunk = make_chunk(schunk, model[i]);
    blosc2_schunk_append_chunk(twin, chunk, true);
    blosc2_schunk_append_chunk(schunk, chunk, false);
  }

  mu_assert("ERROR: begin_updates failed", blosc2_schunk_begin_updates(schunk) == 0);
  int64_t nchunks = 8;
  const int64_t ops[][2] = {{4, KIND_BYTES}, {5, -1}, {0, KIND_CONST}, {6, KIND_RANDOM}, {2, -1}, {1, KIND_ZEROS}};
  for (int k = 0; k < 6; k++) {
    int64_t nchunk = ops[k][0];
    if (ops[k][1] < 0) {
      mu_assert("ERROR: delete failed", blosc2_schunk_delete_chunk(twin, nchunk) == nchunks - 1);
      mu_assert("ERROR: delete failed", blosc2_schunk_delete_chunk(schunk, nchunk) == nchunks - 1);
      memmove(model + nchunk, model + nchunk + 1, (size_t)(nchunks - nchunk - 1) * sizeof(chunk_model));
      nchunks--;
    }
    else {
      model[nchunk] = (chunk_model){.kind = (int)ops[k][1], .seed = 50 + (uint64_t)k};
      uint8_t* chunk = make_chunk(schunk, model[nchunk]);
      blosc2_schunk_update_chunk(twin, nchunk, chunk, true);
      blosc2_schunk_update_chunk(schunk, nchunk, chunk, false);
    }
  }
  mu_assert("ERROR: end_updates failed", blosc2_schunk_end_updates(schunk) == 0);
  mu_assert("ERROR: wrong data", check_data(schunk, model, nchunks));
  mu_assert("ERROR: payload not as long as without brackets", schunk->cbytes == twin->cbytes);
  blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
  mu_assert("ERROR: file not as long as the frame", file_size(URLPATH) == frame->len);
  blosc2_schunk_free(twin);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


/* b2nd writes on an existing array: the result is as compact as the same data written into a new array */
static char* b2nd_overwrite(const char* urlpath) {
  blosc2_remove_urlpath(urlpath);
  int64_t shape[3] = {40, 48, 40};
  int32_t chunkshape[3] = {8, 24, 20};
  int32_t blockshape[3] = {4, 12, 10};
  int64_t nitems = shape[0] * shape[1] * shape[2];
  int64_t nbytes = nitems * (int64_t)sizeof(int32_t);
  int32_t* data = malloc((size_t)nbytes);
  int32_t* back = malloc((size_t)nbytes);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 2;
  blosc2_storage storage = {.contiguous = true, .urlpath = (char*)urlpath, .cparams = &cparams};
  b2nd_context_t* ctx = b2nd_create_ctx(&storage, 3, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  b2nd_array_t* array;
  mu_assert("ERROR: cannot create the array", b2nd_zeros(ctx, &array) == 0);

  int64_t start[3] = {0, 0, 0};
  for (int pass = 0; pass < 8; pass++) {
    // Full writes and slabs, with data whose compressibility differs from chunk to chunk and pass to pass
    int64_t lo[3] = {0, 0, 0};
    int64_t hi[3] = {shape[0], shape[1], shape[2]};
    if (pass % 2 == 1) {
      lo[0] = 5 + pass;
      hi[0] = 30;
      lo[2] = 3;
    }
    int64_t slab_shape[3] = {hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]};
    int64_t slab_nitems = slab_shape[0] * slab_shape[1] * slab_shape[2];
    int32_t* slab = malloc((size_t)slab_nitems * sizeof(int32_t));
    for (int64_t i = 0; i < slab_nitems; i++) {
      int64_t z = i / (slab_shape[1] * slab_shape[2]);
      int64_t chunk_kind = ((z + lo[0]) / chunkshape[0] + pass) % 3;
      slab[i] = chunk_kind == 0 ? pass : chunk_kind == 1 ? (int32_t)(rng() & 0xffff) : (int32_t)(rng());
    }
    mu_assert("ERROR: set_slice failed", b2nd_set_slice_cbuffer(slab, slab_shape, slab_nitems * 4, lo, hi,
                                                                 array) == 0);
    for (int64_t i = 0; i < slab_nitems; i++) {
      int64_t z = i / (slab_shape[1] * slab_shape[2]);
      int64_t y = (i / slab_shape[2]) % slab_shape[1];
      int64_t x = i % slab_shape[2];
      data[((z + lo[0]) * shape[1] + y + lo[1]) * shape[2] + x + lo[2]] = slab[i];
    }
    free(slab);

    mu_assert("ERROR: get_slice failed", b2nd_get_slice_cbuffer(array, start, shape, back, shape, nbytes) == 0);
    mu_assert("ERROR: wrong data", memcmp(back, data, (size_t)nbytes) == 0);
    mu_assert("ERROR: frame not compact", check_compact(array->sc));
    // The same data, written into a new in-memory array at once
    blosc2_storage fresh_storage = {.contiguous = true, .cparams = &cparams};
    b2nd_context_t* fresh_ctx = b2nd_create_ctx(&fresh_storage, 3, shape, chunkshape, blockshape, NULL, 0,
                                                NULL, 0);
    b2nd_array_t* fresh;
    mu_assert("ERROR: from_cbuffer failed", b2nd_from_cbuffer(fresh_ctx, &fresh, data, nbytes) == 0);
    mu_assert("ERROR: not as compact as a new array", fresh->sc->cbytes == array->sc->cbytes);
    b2nd_free(fresh);
    b2nd_free_ctx(fresh_ctx);
  }

  // An orthogonal selection over several chunks
  int64_t sel0[] = {1, 9, 17, 33};
  int64_t sel1[] = {0, 30, 47};
  int64_t sel2[] = {2, 25};
  int64_t* selection[3] = {sel0, sel1, sel2};
  int64_t selection_size[3] = {4, 3, 2};
  int32_t values[24];
  for (int i = 0; i < 24; i++) {
    values[i] = (int32_t)rng();
  }
  int64_t values_shape[3] = {4, 3, 2};
  mu_assert("ERROR: set_orthogonal_selection failed",
            b2nd_set_orthogonal_selection(array, selection, selection_size, values, values_shape, sizeof(values)) == 0);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 2; k++) {
        data[(sel0[i] * shape[1] + sel1[j]) * shape[2] + sel2[k]] = values[(i * 3 + j) * 2 + k];
      }
    }
  }
  mu_assert("ERROR: get_slice failed", b2nd_get_slice_cbuffer(array, start, shape, back, shape, nbytes) == 0);
  mu_assert("ERROR: wrong data after the selection", memcmp(back, data, (size_t)nbytes) == 0);
  mu_assert("ERROR: frame not compact after the selection", check_compact(array->sc));

  b2nd_free(array);
  b2nd_free_ctx(ctx);
  free(data);
  free(back);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}

static char* test_b2nd_in_memory(void) {
  return b2nd_overwrite(NULL);
}

static char* test_b2nd_on_disk(void) {
  return b2nd_overwrite(URLPATH);
}


/* blosc2_schunk_set_slice_buffer() over many chunks */
static char* test_schunk_set_slice(void) {
  blosc2_remove_urlpath(URLPATH);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  chunk_model model[NCHUNKS];
  for (int i = 0; i < NCHUNKS; i++) {
    model[i] = (chunk_model){.kind = KIND_CONST, .seed = (uint64_t)i};
    blosc2_schunk_append_chunk(schunk, make_chunk(schunk, model[i]), false);
  }
  int64_t* buffer = malloc((size_t)NCHUNKS * CHUNK_NBYTES);
  for (int i = 0; i < NCHUNKS; i++) {
    model[i] = (chunk_model){.kind = i % 2 ? KIND_RANDOM : KIND_BYTES, .seed = 100 + (uint64_t)i};
    fill_data(model[i], buffer + (int64_t)i * NITEMS);
  }
  mu_assert("ERROR: set_slice_buffer failed",
            blosc2_schunk_set_slice_buffer(schunk, 0, (int64_t)NCHUNKS * NITEMS, buffer) == 0);
  mu_assert("ERROR: wrong data", check_data(schunk, model, NCHUNKS));
  mu_assert("ERROR: frame not compact", check_compact(schunk));
  free(buffer);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  mu_run_test(test_placement_in_memory);
  mu_run_test(test_placement_on_disk);
  mu_run_test(test_delete_inside_bracket);
  mu_run_test(test_random_in_memory);
  mu_run_test(test_random_on_disk);
  mu_run_test(test_random_mmap);
  mu_run_test(test_random_locking);
  mu_run_test(test_b2nd_in_memory);
  mu_run_test(test_b2nd_on_disk);
  mu_run_test(test_schunk_set_slice);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char* result = all_tests();
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

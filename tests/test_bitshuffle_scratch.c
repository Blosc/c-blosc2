/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  The bitshuffle routines take their scratch buffer from the caller: the
  compression and decompression pipelines pass the thread_context's tmp4
  (which blosc_d() also reads the blocks of lazy chunks into), the public
  blosc2_bitshuffle()/blosc2_bitunshuffle() allocate one per call.  Checks
  every bitshuffle routine that is built (CMake does not build the NEON one)
  and supported by the CPU against the generic one with a scratch of exactly
  size * elem_size bytes followed by guard bytes and with guard bytes after
  the output (element counts leave every remainder of the vector loops), the
  internal bitshuffle()/bitunshuffle() with a scratch against the public API,
  that the decompression pipeline uses tmp4, and round trips through the pipelines
  with one large block per chunk and with partial blocks: serial and
  threaded, getitem, filter combinations, a prefilter and a postfilter, and
  lazy chunks of a file frame read through stdio and mmap.
*/

#include "test_common.h"
#include "shuffle.h"
#include "bitshuffle-generic.h"
#if defined(SHUFFLE_SSE2_ENABLED)
#include "bitshuffle-sse2.h"
#endif
#if defined(SHUFFLE_AVX2_ENABLED)
#include "bitshuffle-avx2.h"
#endif
#if defined(SHUFFLE_AVX512_ENABLED)
#include "bitshuffle-avx512.h"
#endif
#if defined(SHUFFLE_ALTIVEC_ENABLED)
#include "bitshuffle-altivec.h"
#endif

/* Run-time CPU checks for the x86 routines; elsewhere only the generic and
   AltiVec ones (the latter is only compiled for CPUs that have it) run */
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__)) && \
    (defined(__GNUC__) || defined(__clang__))
#define CPU_HAS(feature) __builtin_cpu_supports(feature)
#else
#define CPU_HAS(feature) 0
#endif

#define GUARD 64
#define FILL 0xa5

int tests_run = 0;

/* Input for all checks: compressible, but without runs */
#define DATA_NBYTES (2 * 1024 * 1024)
static uint8_t* data;


typedef int64_t (*bshuf_func)(const void*, void*, const size_t, const size_t, void*);

typedef struct {
  const char* name;
  bshuf_func trans;
  bshuf_func untrans;
} bshuf_variant;

/* The generic routines first */
static int get_variants(bshuf_variant* variants) {
  int n = 0;
  variants[n++] = (bshuf_variant){"generic", bshuf_trans_bit_elem_scal, bshuf_untrans_bit_elem_scal};
#if defined(SHUFFLE_SSE2_ENABLED)
  if (is_bshuf_SSE && CPU_HAS("sse2")) {
    variants[n++] = (bshuf_variant){"sse2", bshuf_trans_bit_elem_SSE, bshuf_untrans_bit_elem_SSE};
  }
#endif
#if defined(SHUFFLE_AVX2_ENABLED)
  if (is_bshuf_AVX && CPU_HAS("avx2")) {
    variants[n++] = (bshuf_variant){"avx2", bshuf_trans_bit_elem_AVX, bshuf_untrans_bit_elem_AVX};
  }
#endif
#if defined(SHUFFLE_AVX512_ENABLED)
  if (is_bshuf_AVX512 && CPU_HAS("avx512f") && CPU_HAS("avx512bw")) {
    variants[n++] = (bshuf_variant){"avx512", bshuf_trans_bit_elem_AVX512, bshuf_untrans_bit_elem_AVX512};
  }
#endif
#if defined(SHUFFLE_ALTIVEC_ENABLED)
  if (is_bshuf_altivec) {
    variants[n++] = (bshuf_variant){"altivec", bshuf_trans_bit_elem_altivec, bshuf_untrans_bit_elem_altivec};
  }
#endif
  return n;
}


static bool guard_intact(const uint8_t* scratch, size_t nbytes) {
  for (size_t i = nbytes; i < nbytes + GUARD; i++) {
    if (scratch[i] != FILL) {
      return false;
    }
  }
  return true;
}


/* Every routine with a scratch of exactly size * elem_size bytes gives the
   generic result and writes nothing past the scratch */
static char* test_routines(void) {
  // Check the two-byte inverse's intermediate layout independently of SIMD:
  // each 16-byte (128-bit) lane contains low then high bytes of eight
  // elements.  Lane-local unpacks are correct for AVX2 AND AVX-512.
  uint8_t input[64], shuffled[64], intermediate[64], scratch_layout[64], restored[64];
  for (int i = 0; i < 32; ++i) {
    input[2 * i] = (uint8_t)(i * 7 + 3);
    input[2 * i + 1] = (uint8_t)(i * 11 + 129);
  }
  bshuf_trans_bit_elem_scal(input, shuffled, 32, 2, scratch_layout);
  bshuf_trans_byte_bitrow_scal(shuffled, intermediate, 32, 2);
  for (int lane = 0; lane < 4; ++lane) {
    uint8_t lo[8] = {0}, hi[8] = {0};
    for (int i = 0; i < 8; ++i) {
      for (int bit = 0; bit < 8; ++bit) {
        lo[i] |= ((intermediate[16 * lane + bit] >> i) & 1) << bit;
        hi[i] |= ((intermediate[16 * lane + 8 + bit] >> i) & 1) << bit;
      }
    }
    for (int i = 0; i < 8; ++i) {
      restored[16 * lane + 2 * i] = lo[i];
      restored[16 * lane + 2 * i + 1] = hi[i];
    }
  }
  mu_assert("ERROR: two-byte inverse does not have lane-local low/high groups",
            memcmp(restored, input, sizeof(input)) == 0);

  bshuf_variant variants[8];
  int nvariants = get_variants(variants);
  printf("[bitshuffle routines:");
  for (int v = 0; v < nvariants; v++) {
    printf(" %s", variants[v].name);
  }
  printf("] ");
  const size_t elem_sizes[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 16, 17, 24, 32, 64};
  // 8-64 elements (and 1032-1088, after whole vectors) leave every possible
  // remainder of the 16, 32 and 64-byte loops; with 1024 the bit rows are a
  // multiple of 32 bytes
  const size_t sizes[] = {8, 16, 24, 32, 40, 48, 56, 64, 136, 1024,
                          1032, 1040, 1048, 1056, 1064, 1072, 1080, 1088, 16392};
  size_t max_nbytes = 64 * 16392;
  uint8_t* ref = malloc(max_nbytes + GUARD);
  uint8_t* out = malloc(max_nbytes + GUARD);
  uint8_t* scratch = malloc(max_nbytes + GUARD);
  for (size_t e = 0; e < ARRAY_SIZE(elem_sizes); e++) {
    for (size_t s = 0; s < ARRAY_SIZE(sizes); s++) {
      size_t elem_size = elem_sizes[e], size = sizes[s], nbytes = size * elem_size;
      mu_assert("ERROR: the buffers are too small", nbytes <= max_nbytes);
      const uint8_t* in = data + e * 64 + s;  // not always aligned
      for (int v = 0; v < nvariants; v++) {
        // the generic routine gives the reference
        uint8_t* shuffled = (v == 0) ? ref : out;
        memset(scratch, FILL, nbytes + GUARD);
        memset(shuffled, FILL, nbytes + GUARD);
        int64_t rc = variants[v].trans(in, shuffled, size, elem_size, scratch);
        bool ok = rc >= 0 && guard_intact(scratch, nbytes) && guard_intact(shuffled, nbytes) &&
                  memcmp(shuffled, ref, nbytes) == 0;
        memset(scratch, FILL, nbytes + GUARD);
        memset(out, FILL, nbytes + GUARD);
        rc = variants[v].untrans(ref, out, size, elem_size, scratch);
        ok = ok && rc >= 0 && guard_intact(scratch, nbytes) && guard_intact(out, nbytes) &&
             memcmp(out, in, nbytes) == 0;
        if (!ok) {
          printf("\n%s routines, elem_size %zu, size %zu\n", variants[v].name, elem_size, size);
          mu_assert("ERROR: bit(un)shuffle differs from the generic one or writes past its scratch or output", false);
        }
      }
    }
  }
  free(ref);
  free(out);
  free(scratch);
  return EXIT_SUCCESS;
}


/* bitshuffle()/bitunshuffle() with a scratch agree with the public API (which
   allocates one), also for leftover elements and bytes */
static char* test_entry_points(void) {
  const int32_t typesizes[] = {1, 2, 3, 4, 8, 16};
  const int32_t blocksizes[] = {7, 64, 1000, 4099, 32768, 32771, 786432};
  int32_t max_blocksize = 786432;
  uint8_t* ref = malloc(max_blocksize);
  uint8_t* out = malloc(max_blocksize);
  uint8_t* scratch = malloc((size_t)max_blocksize + GUARD);
  for (size_t t = 0; t < ARRAY_SIZE(typesizes); t++) {
    for (size_t b = 0; b < ARRAY_SIZE(blocksizes); b++) {
      int32_t typesize = typesizes[t], blocksize = blocksizes[b];
      const uint8_t* in = data + t * 10 + b;
      mu_assert("ERROR: blosc2_bitshuffle failed",
                blosc2_bitshuffle(typesize, blocksize, in, ref) == blocksize);
      memset(scratch, FILL, (size_t)blocksize + GUARD);
      mu_assert("ERROR: bitshuffle with a scratch failed",
                bitshuffle(typesize, blocksize, in, out, scratch) == blocksize);
      mu_assert("ERROR: bitshuffle with a scratch differs from blosc2_bitshuffle",
                memcmp(out, ref, blocksize) == 0 && guard_intact(scratch, blocksize));
      memset(scratch, FILL, (size_t)blocksize + GUARD);
      mu_assert("ERROR: bitunshuffle with a scratch failed",
                bitunshuffle(typesize, blocksize, ref, out, scratch, BLOSC2_VERSION_FORMAT) == blocksize);
      mu_assert("ERROR: bitunshuffle with a scratch does not restore the data",
                memcmp(out, in, blocksize) == 0 && guard_intact(scratch, blocksize));
      memset(out, 0, blocksize);
      mu_assert("ERROR: blosc2_bitunshuffle failed",
                blosc2_bitunshuffle(typesize, blocksize, ref, out) == blocksize);
      mu_assert("ERROR: blosc2_bitunshuffle does not restore the data", memcmp(out, in, blocksize) == 0);
    }
  }
  free(ref);
  free(out);
  free(scratch);
  return EXIT_SUCCESS;
}


/* The pipelines */

typedef struct {
  int32_t blocksize;
  int32_t nblocks;      // full blocks per chunk
  int32_t leftover;     // elements in the partial last block
} chunk_shape;

/* One block of 768 KB per chunk (the case that made every read page-fault),
   blocks of 256 KB (and 256 KB - 1 for odd typesizes) with a partial last
   block whose elements are not a multiple of 8, small blocks */
static const chunk_shape shapes[] = {
    {768 * 1024, 1, 0},
    {256 * 1024, 3, 8 * 1000 + 5},
    {32 * 1024, 6, 8 * 100 + 3},
};
/* Lazy chunks are checked with the two large-block shapes */
#define NLAZY_SHAPES 2

static const int32_t typesizes[] = {1, 2, 3, 4, 8, 16};

static const uint8_t filter_sets[][BLOSC2_MAX_FILTERS] = {
    {0, 0, 0, 0, 0, BLOSC_BITSHUFFLE},
    {0, 0, 0, 0, BLOSC_DELTA, BLOSC_BITSHUFFLE},
    {0, 0, 0, 0, BLOSC_BITSHUFFLE, BLOSC_SHUFFLE},
};
#define NFILTER_SETS ((int)ARRAY_SIZE(filter_sets))

static const int16_t nthreads_list[] = {1, 4};


static void shape_sizes(const chunk_shape* shape, int32_t typesize, int32_t* blocksize, int32_t* nbytes) {
  *blocksize = shape->blocksize / typesize * typesize;
  *nbytes = shape->nblocks * *blocksize + shape->leftover * typesize;
}


static int copy_prefilter(blosc2_prefilter_params* params) {
  memcpy(params->output, params->input, params->output_size);
  return 0;
}


static int copy_postfilter(blosc2_postfilter_params* params) {
  memcpy(params->output, params->input, params->size);
  return 0;
}


static blosc2_cparams make_cparams(int32_t typesize, int32_t blocksize, int filter_set,
                                   int16_t nthreads) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.blocksize = blocksize;
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 5;
  cparams.nthreads = nthreads;
  memcpy(cparams.filters, filter_sets[filter_set], BLOSC2_MAX_FILTERS);
  return cparams;
}


static int compress_chunk(blosc2_cparams cparams, int32_t nbytes, uint8_t* chunk) {
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  int cbytes = blosc2_compress_ctx(cctx, data, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  return cbytes;
}


/* Decompress `chunk` with `dctx` and compare with `src`; with `getitem`,
   also from the middle of the second block (getitem works serially) */
static char* check_decompression(blosc2_context* dctx, const uint8_t* chunk, int32_t cbytes,
                                 const uint8_t* src, int32_t nbytes, int32_t typesize,
                                 int32_t blocksize, bool getitem, uint8_t* dest) {
  memset(dest, 0, nbytes);
  int dsize = blosc2_decompress_ctx(dctx, chunk, cbytes, dest, nbytes);
  mu_assert("ERROR: decompression failed", dsize == nbytes);
  mu_assert("ERROR: decompression does not restore the data", memcmp(dest, src, nbytes) == 0);
  if (!getitem) {
    return EXIT_SUCCESS;
  }
  int32_t start = (blocksize + blocksize / 2) / typesize;
  if (start * typesize >= nbytes) {
    start = nbytes / typesize / 3;
  }
  int32_t nitems = nbytes / typesize - start;
  memset(dest, 0, nbytes);
  dsize = blosc2_getitem_ctx(dctx, chunk, cbytes, start, nitems, dest, nbytes);
  mu_assert("ERROR: getitem failed", dsize == nitems * typesize);
  mu_assert("ERROR: getitem does not restore the data",
            memcmp(dest, src + start * typesize, (size_t)nitems * typesize) == 0);
  return EXIT_SUCCESS;
}


/* Compress `data` with filter set `f` (or, for f == NFILTER_SETS, the first
   one with a prefilter and a postfilter) and decompress it again */
static char* round_trip(int32_t typesize, int32_t blocksize, int32_t nbytes, int f, int16_t nthreads,
                        uint8_t* chunk, uint8_t* dest) {
  bool callbacks = f == NFILTER_SETS;
  blosc2_cparams cparams = make_cparams(typesize, blocksize, callbacks ? 0 : f, nthreads);
  blosc2_prefilter_params preparams = {0};
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_postfilter_params postparams = {0};
  if (callbacks) {
    cparams.prefilter = copy_prefilter;
    cparams.preparams = &preparams;
    dparams.postfilter = copy_postfilter;
    dparams.postparams = &postparams;
  }
  int cbytes = compress_chunk(cparams, nbytes, chunk);
  mu_assert("ERROR: compression failed", cbytes > 0);
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  char* msg = check_decompression(dctx, chunk, cbytes, data, nbytes, typesize, blocksize, nthreads == 1, dest);
  blosc2_free_ctx(dctx);
  return msg;
}


static char* test_pipelines(void) {
  uint8_t* chunk = malloc(DATA_NBYTES + BLOSC2_MAX_OVERHEAD);
  uint8_t* dest = malloc(DATA_NBYTES);
  for (size_t sh = 0; sh < ARRAY_SIZE(shapes); sh++) {
    for (size_t t = 0; t < ARRAY_SIZE(typesizes); t++) {
      int32_t typesize = typesizes[t], blocksize, nbytes;
      shape_sizes(&shapes[sh], typesize, &blocksize, &nbytes);
      for (int f = 0; f <= NFILTER_SETS; f++) {
        for (size_t n = 0; n < ARRAY_SIZE(nthreads_list); n++) {
          // chunks of one block are always (de)compressed serially
          if (nthreads_list[n] > 1 && shapes[sh].nblocks == 1) {
            continue;
          }
          char* msg = round_trip(typesize, blocksize, nbytes, f, nthreads_list[n], chunk, dest);
          if (msg != EXIT_SUCCESS) {
            printf("\ntypesize %d, blocksize %d, filters %d, nthreads %d\n", typesize, blocksize, f,
                   nthreads_list[n]);
            return msg;
          }
        }
      }
    }
  }
  free(chunk);
  free(dest);
  return EXIT_SUCCESS;
}


/* The decompression pipeline gives bitunshuffle the last of the four block
   temporaries of the thread_context (tmp4) instead of allocating one: a
   postfilter (which runs after the filters of each block and may use the
   whole temporary area, `ttmp`) fills that quarter with a pattern, and the
   bitunshuffle of the next block overwrites it */
typedef struct {
  int nblocks;
  int overwritten;
} scratch_probe;

static int probe_postfilter(blosc2_postfilter_params* params) {
  scratch_probe* probe = params->user_data;
  memcpy(params->output, params->input, params->size);
  size_t quarter = params->ttmp_nbytes / 4;
  uint8_t* tmp4 = params->ttmp + 3 * quarter;
  if (probe->nblocks > 0) {
    for (size_t i = 0; i < quarter; i++) {
      if (tmp4[i] != FILL) {
        probe->overwritten++;
        break;
      }
    }
  }
  memset(tmp4, FILL, quarter);
  probe->nblocks++;
  return 0;
}

static char* test_scratch_in_thread_context(void) {
  int32_t typesize = 4, blocksize = 64 * 1024, nbytes = 8 * blocksize;
  uint8_t* chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  uint8_t* dest = malloc(nbytes);
  int cbytes = compress_chunk(make_cparams(typesize, blocksize, 0, 1), nbytes, chunk);
  mu_assert("ERROR: compression failed", cbytes > 0);

  scratch_probe probe = {0};
  blosc2_postfilter_params postparams = {0};
  postparams.user_data = &probe;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  dparams.postfilter = probe_postfilter;
  dparams.postparams = &postparams;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  int dsize = blosc2_decompress_ctx(dctx, chunk, cbytes, dest, nbytes);
  blosc2_free_ctx(dctx);
  mu_assert("ERROR: decompression failed", dsize == nbytes && memcmp(dest, data, nbytes) == 0);
  mu_assert("ERROR: the postfilter did not run for every block", probe.nblocks == nbytes / blocksize);
  mu_assert("ERROR: bitunshuffle did not use the thread_context's tmp4 as its scratch",
            probe.overwritten == probe.nblocks - 1);
  free(chunk);
  free(dest);
  return EXIT_SUCCESS;
}


/* Lazy chunks: blosc_d() reads their blocks into tmp4 (stdio) or points into
   the mapping (mmap) */
#define URLPATH "test_bitshuffle_scratch.b2frame"
#define NCHUNKS 2

static char* check_lazy_chunks(blosc2_schunk* schunk, int32_t nbytes, int32_t typesize,
                               int32_t blocksize, int32_t nblocks, uint8_t* dest) {
  for (size_t n = 0; n < ARRAY_SIZE(nthreads_list); n++) {
    if (nthreads_list[n] > 1 && nblocks == 1) {
      continue;
    }
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = nthreads_list[n];
    dparams.schunk = schunk;
    blosc2_context* dctx = blosc2_create_dctx(dparams);
    char* msg = EXIT_SUCCESS;
    for (int64_t nchunk = 0; nchunk < NCHUNKS && msg == EXIT_SUCCESS; nchunk++) {
      uint8_t* chunk;
      bool needs_free;
      int cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &chunk, &needs_free);
      if (cbytes <= 0) {
        msg = "ERROR: cannot get the lazy chunk";
        break;
      }
      msg = check_decompression(dctx, chunk, cbytes, data + nchunk * nbytes, nbytes, typesize, blocksize,
                                nthreads_list[n] == 1, dest);
      if (needs_free) {
        free(chunk);
      }
    }
    blosc2_free_ctx(dctx);
    if (msg != EXIT_SUCCESS) {
      printf("\ntypesize %d, blocksize %d, nthreads %d\n", typesize, blocksize, nthreads_list[n]);
      return msg;
    }
  }
  return EXIT_SUCCESS;
}


static char* test_lazy_chunks(void) {
  uint8_t* dest = malloc(DATA_NBYTES);
  for (size_t sh = 0; sh < NLAZY_SHAPES; sh++) {
    for (size_t t = 0; t < ARRAY_SIZE(typesizes); t++) {
      int32_t typesize = typesizes[t], blocksize, nbytes;
      shape_sizes(&shapes[sh], typesize, &blocksize, &nbytes);
      blosc2_cparams cparams = make_cparams(typesize, blocksize, 2, 4);
      blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
      blosc2_remove_urlpath(URLPATH);
      blosc2_schunk* schunk = blosc2_schunk_new(&storage);
      mu_assert("ERROR: cannot create the frame", schunk != NULL);
      for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
        mu_assert("ERROR: cannot append a chunk",
                  blosc2_schunk_append_buffer(schunk, data + (size_t)nchunk * nbytes, nbytes) == nchunk + 1);
      }
      blosc2_schunk_free(schunk);

      // freeing an mmap schunk resets its parameters
      blosc2_stdio_mmap mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
      mmap_file.mode = "r";
      const blosc2_io ios[] = {
          BLOSC2_IO_DEFAULTS,
          {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file},
      };
      for (size_t io = 0; io < ARRAY_SIZE(ios); io++) {
        schunk = blosc2_schunk_open_udio(URLPATH, &ios[io]);
        mu_assert("ERROR: cannot open the frame", schunk != NULL);
        char* msg = check_lazy_chunks(schunk, nbytes, typesize, blocksize, shapes[sh].nblocks, dest);
        blosc2_schunk_free(schunk);
        mu_assert(msg, msg == EXIT_SUCCESS);
      }
      blosc2_remove_urlpath(URLPATH);
    }
  }
  free(dest);
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  mu_run_test(test_routines);
  mu_run_test(test_entry_points);
  mu_run_test(test_pipelines);
  mu_run_test(test_scratch_in_thread_context);
  mu_run_test(test_lazy_chunks);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  data = malloc(DATA_NBYTES);
  uint32_t x = 1;
  for (size_t i = 0; i < DATA_NBYTES; i++) {
    x = x * 1103515245u + 12345u;
    data[i] = (uint8_t)((i / 64) + ((x >> 16) & 0x0f));
  }
  char* result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  free(data);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}

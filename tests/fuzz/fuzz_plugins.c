#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <blosc2.h>
#include <b2nd.h>
#include <blosc2/codecs-registry.h>
#include <blosc2/filters-registry.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Option-matrix fuzzer for the compiled-in codec and filter plugins.
 *
 * Threat model: the NDLZ and ZFP codecs and the NDCELL/NDMEAN/BYTEDELTA/
 * INT_TRUNC filters ship inside libblosc2 and are auto-registered, so a
 * codec/filter id in an untrusted chunk or frame header (plus the meta byte
 * that accompanies it) selects them on the decompression path.  The plugins
 * derive per-block geometry from the "b2nd" metalayer, a separate
 * attacker-controlled field (this is exactly the CVE-2024-3204 class of bug
 * NDLZ was hardened against), so both the compress side (driven here through
 * the b2nd API with a valid metalayer) and the decompress side (driven here
 * through b2nd_from_cframe on untrusted bytes) are in scope.
 *
 * One mode sweeps codec x compcode_meta x filter x filters_meta x typesize
 * through create -> read-back -> serialize -> parse; the other mode feeds the
 * whole input to b2nd_from_cframe so crafted frames reach the plugin
 * decompressors.  Fixed-order tail bytes carry the control plane; allocations
 * are bounded.
 */

#define FUZZ_MAX_DIM 4         /* ZFP_MAX_DIM */
#define FUZZ_MAX_EXTENT 8      /* per-dimension bound */
#define FUZZ_MAX_ITEMS 4096    /* cap on elements of any allocated buffer */

typedef struct {
  const uint8_t *data;
  size_t size;
} tail_reader;

static uint8_t tail_u8(tail_reader *r) {
  if (r->size == 0) {
    return 0;
  }
  r->size--;
  return r->data[r->size];
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  tail_reader r = {data, size};

  if (size < 16) {
    return 0;
  }

  uint8_t mode = (uint8_t)(tail_u8(&r) % 2);
  uint8_t typesize_sel = (uint8_t)(tail_u8(&r) % 4);

  blosc2_init();
  blosc2_set_nthreads(1);

  if (mode == 1) {
    /* Parser mode: untrusted b2nd frame; registered codecs and filters are
     * selected by the frame's own header fields. */
    b2nd_array_t *parsed = NULL;
    if (b2nd_from_cframe((uint8_t *)data, (int64_t)size, false, &parsed) >= 0 &&
        parsed != NULL) {
      uint8_t *buf = (uint8_t *)malloc(FUZZ_MAX_ITEMS * 8);
      if (buf != NULL) {
        b2nd_to_cbuffer(parsed, buf, FUZZ_MAX_ITEMS * 8);
        free(buf);
      }
      b2nd_free(parsed);
    }
    blosc2_destroy();
    return 0;
  }

  static const uint8_t typesizes[4] = {4, 8, 1, 2};
  static const uint8_t compcodes[7] = {
      BLOSC_BLOSCLZ, BLOSC_LZ4, BLOSC_ZSTD, BLOSC_CODEC_NDLZ,
      BLOSC_CODEC_ZFP_FIXED_ACCURACY, BLOSC_CODEC_ZFP_FIXED_PRECISION,
      BLOSC_CODEC_ZFP_FIXED_RATE};
  static const uint8_t filters[9] = {
      BLOSC_SHUFFLE, BLOSC_BITSHUFFLE, BLOSC_DELTA, BLOSC_TRUNC_PREC,
      BLOSC_FILTER_NDCELL, BLOSC_FILTER_NDMEAN, BLOSC_FILTER_BYTEDELTA,
      BLOSC_FILTER_BYTEDELTA_BUGGY, BLOSC_FILTER_INT_TRUNC};

  uint8_t comp_sel = (uint8_t)(tail_u8(&r) % 7);
  uint8_t raw_meta = tail_u8(&r);
  uint8_t filter_sel = (uint8_t)(tail_u8(&r) % 9);
  uint8_t filter_meta = tail_u8(&r);
  uint8_t clevel = (uint8_t)(tail_u8(&r) % 10);
  int8_t ndim = (int8_t)(tail_u8(&r) % FUZZ_MAX_DIM + 1);
  uint8_t typesize = typesizes[typesize_sel];

  uint8_t compcode = compcodes[comp_sel];
  uint8_t compcode_meta = raw_meta;
  if (compcode == BLOSC_CODEC_NDLZ) {
    compcode_meta = (uint8_t)((raw_meta % 2) ? 8 : 4);
  } else if (compcode == BLOSC_CODEC_ZFP_FIXED_PRECISION) {
    compcode_meta = (uint8_t)(raw_meta % (typesize * 8) + 1);
  } else if (compcode == BLOSC_CODEC_ZFP_FIXED_RATE) {
    compcode_meta = (uint8_t)(raw_meta % 32 + 1);
  } else if (compcode == BLOSC_CODEC_ZFP_FIXED_ACCURACY) {
    compcode_meta = (uint8_t)(raw_meta % 16);
  }

  int64_t shape[FUZZ_MAX_DIM] = {0};
  int32_t chunkshape[FUZZ_MAX_DIM] = {0};
  int32_t blockshape[FUZZ_MAX_DIM] = {0};
  int64_t total = 1;
  for (int8_t d = 0; d < ndim; d++) {
    int64_t ext = (int64_t)(tail_u8(&r) % FUZZ_MAX_EXTENT + 1);
    if (total * ext > FUZZ_MAX_ITEMS) {
      ext = 1;
    }
    shape[d] = ext;
    chunkshape[d] = (int32_t)(ext > 4 ? 4 : ext);
    blockshape[d] = (int32_t)(ext > 2 ? 2 : ext);
    total *= ext;
  }

  const uint8_t *payload = data;
  size_t payload_len = r.size;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = clevel;
  cparams.compcode = compcode;
  cparams.compcode_meta = compcode_meta;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filters[filter_sel];
  cparams.filters_meta[BLOSC2_MAX_FILTERS - 1] =
      (filter_sel == 6 || filter_sel == 7) ? typesize : filter_meta;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                            .contiguous = true};

  b2nd_context_t *ctx = b2nd_create_ctx(&storage, ndim, shape, chunkshape,
                                        blockshape, NULL, 0, NULL, 0);
  if (ctx == NULL) {
    blosc2_destroy();
    return 0;
  }

  b2nd_array_t *arr = NULL;
  int rc = b2nd_from_cbuffer(ctx, &arr, payload, (int64_t)payload_len);
  if (rc >= 0 && arr != NULL) {
    uint8_t *buf = (uint8_t *)malloc((size_t)FUZZ_MAX_ITEMS * 8);
    if (buf != NULL) {
      b2nd_to_cbuffer(arr, buf, (int64_t)FUZZ_MAX_ITEMS * 8);

      uint8_t *cframe = NULL;
      int64_t len = 0;
      bool needs_free = false;
      if (b2nd_to_cframe(arr, &cframe, &len, &needs_free) >= 0 && len > 0 &&
          cframe != NULL) {
        b2nd_array_t *again = NULL;
        if (b2nd_from_cframe(cframe, len, false, &again) >= 0 &&
            again != NULL) {
          b2nd_to_cbuffer(again, buf, (int64_t)FUZZ_MAX_ITEMS * 8);
          b2nd_free(again);
        }
        if (needs_free) {
          free(cframe);
        }
      }
      free(buf);
    }
  }
  /* b2nd_empty inside b2nd_from_cbuffer may have assigned *array even on
   * error; free it whenever it is set. */
  if (arr != NULL) {
    b2nd_free(arr);
  }

  b2nd_free_ctx(ctx);
  blosc2_destroy();
  return 0;
}

#ifdef __cplusplus
}
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <blosc2.h>
#include <b2nd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fuzzer for the b2nd N-dimensional array API.
 *
 * Threat model: b2nd arrays are serialized to (and parsed from) b2frame
 * buffers that arrive from untrusted sources (files, network).  The attacker
 * controls both the frame bytes (parser mode below) and, through applications,
 * the geometry (shapes/chunks/blocks) and slice coordinates.  A real bug here
 * is any memory-safety violation reachable from the public b2nd entry points:
 * OOB reads/writes in slice index math, int64 offset overflows, and
 * use-of-uninitialized data in freshly created arrays.
 *
 * Input layout is consumed in a fixed order so crashes reproduce: control
 * bytes are taken from the tail of the input (innermost byte first), the
 * remaining head bytes are the data payload.  Sizes are bounded so that the
 * fuzzer explores geometry, not the OOM killer.
 */

#define FUZZ_MAX_DIM 16        /* B2ND_MAX_DIM */
#define FUZZ_MAX_EXTENT 64     /* per-dimension bound */
#define FUZZ_MAX_ITEMS 8192    /* cap on elements of any allocated buffer */

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

static int64_t tail_dim(tail_reader *r) {
  return (int64_t)(tail_u8(r) % (FUZZ_MAX_EXTENT + 1));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  tail_reader r = {data, size};

  if (size < 16) {
    return 0;
  }

  uint8_t mode = (uint8_t)(tail_u8(&r) % 5);
  int8_t ndim = (int8_t)(tail_u8(&r) % FUZZ_MAX_DIM + 1);
  uint8_t typesize = (uint8_t)(tail_u8(&r) % 8 + 1);

  int64_t shape[FUZZ_MAX_DIM] = {0};
  int32_t chunkshape[FUZZ_MAX_DIM] = {0};
  int32_t blockshape[FUZZ_MAX_DIM] = {0};
  int64_t start[FUZZ_MAX_DIM] = {0};
  int64_t stop[FUZZ_MAX_DIM] = {0};
  int64_t diff[FUZZ_MAX_DIM] = {0};
  int64_t shape_items = 1;
  int64_t chunk_items = 1;
  int64_t nchunks_est = 1;
  int64_t req = 1;
  int8_t d;

  for (d = 0; d < ndim; d++) {
    int64_t ext = tail_dim(&r);
    if (ext > 0 && shape_items * ext > FUZZ_MAX_ITEMS) {
      ext = 1;
    }
    shape[d] = ext;
    if (ext > 0) {
      shape_items *= ext;
    }

    int32_t c_ext = (int32_t)(tail_u8(&r) % FUZZ_MAX_EXTENT + 1);
    if (chunk_items * c_ext > FUZZ_MAX_ITEMS) {
      c_ext = 1;
    }
    int64_t dim_chunks = ext > 0 ? (ext + c_ext - 1) / c_ext : 1;
    if (nchunks_est * dim_chunks > 64 && ext > 0) {
      c_ext = (int32_t)ext;
      dim_chunks = 1;
    }
    chunkshape[d] = c_ext;
    chunk_items *= c_ext;
    nchunks_est *= dim_chunks;

    blockshape[d] = (int32_t)(tail_u8(&r) % c_ext + 1);
  }
  for (d = 0; d < ndim; d++) {
    start[d] = tail_dim(&r);
    stop[d] = tail_dim(&r);
    diff[d] = stop[d] - start[d];
    if (diff[d] < 0) {
      diff[d] = 0;
      stop[d] = start[d];
    }
    if (diff[d] > 0 && req * diff[d] > FUZZ_MAX_ITEMS) {
      diff[d] = 1;
      stop[d] = start[d] + 1;
    }
    if (diff[d] > 0) {
      req *= diff[d];
    }
  }
  for (d = 0; d < ndim; d++) {
    if (diff[d] == 0) {
      req = 0;
      break;
    }
  }

  /* The head bytes that were not consumed as control bytes are the payload. */
  const uint8_t *payload = data;
  size_t payload_len = r.size;

  blosc2_init();
  blosc2_set_nthreads(1);

  if (mode == 4) {
    /* Parser mode: the whole input is an untrusted b2nd frame. */
    b2nd_array_t *parsed = NULL;
    if (b2nd_from_cframe((uint8_t *)data, (int64_t)size, false, &parsed) >= 0 &&
        parsed != NULL) {
      if (parsed->nitems >= 0 && parsed->nitems <= FUZZ_MAX_ITEMS &&
          parsed->extchunknitems >= 0 && parsed->extchunknitems <= FUZZ_MAX_ITEMS * 8 &&
          parsed->sc != NULL && parsed->sc->typesize > 0 && parsed->sc->typesize <= 8) {
        uint8_t *buf = (uint8_t *)malloc(FUZZ_MAX_ITEMS * 8);
        if (buf != NULL) {
          b2nd_to_cbuffer(parsed, buf, FUZZ_MAX_ITEMS * 8);
          free(buf);
        }
      }
      b2nd_free(parsed);
    }
    blosc2_destroy();
    return 0;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = tail_u8(&r) % 10;
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
  int rc = BLOSC2_ERROR_DATA;
  switch (mode) {
    case 0:
      rc = b2nd_from_cbuffer(ctx, &arr, payload, (int64_t)payload_len);
      break;
    case 1:
      rc = b2nd_zeros(ctx, &arr);
      break;
    case 2:
      rc = b2nd_empty(ctx, &arr);
      break;
    default: {
      uint8_t fill_value[8] = {0};
      if (payload_len > 0) {
        memcpy(fill_value, payload,
               payload_len < typesize ? payload_len : typesize);
      }
      rc = b2nd_full(ctx, &arr, fill_value);
      break;
    }
  }
  if (rc < 0 || arr == NULL) {
    /* b2nd_empty inside b2nd_from_cbuffer may have assigned *array even on
     * error; free it whenever it is set. */
    if (arr != NULL) {
      b2nd_free(arr);
    }
    b2nd_free_ctx(ctx);
    blosc2_destroy();
    return 0;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)FUZZ_MAX_ITEMS * 8);
  if (buf != NULL) {
    /* Get a slice into a plain buffer. */
    b2nd_get_slice_cbuffer(arr, start, stop, buf, diff, req * typesize);

    /* Get a slice as a new array and read it back. */
    b2nd_array_t *slice = NULL;
    if (b2nd_get_slice(ctx, &slice, arr, start, stop) >= 0 && slice != NULL) {
      b2nd_to_cbuffer(slice, buf, req * typesize);
      b2nd_free(slice);
    }

    /* Set a slice from a payload-derived buffer (zero-padded to the full
     * requested extent so the buffer honestly owns `buffersize` bytes). */
    memset(buf, 0, (size_t)req * typesize);
    memcpy(buf, payload,
           payload_len < (size_t)req * typesize ? payload_len
                                                : (size_t)req * typesize);
    b2nd_set_slice_cbuffer(buf, diff, (int64_t)req * typesize, start, stop, arr);

    /* Read the whole array back. */
    b2nd_to_cbuffer(arr, buf, (int64_t)FUZZ_MAX_ITEMS * 8);

    /* Grow along axis 0 and insert at the front (bounded by payload & chunk cap). */
    size_t max_grow = (arr->sc != NULL && arr->sc->chunksize > 0)
                          ? (size_t)arr->sc->chunksize * 4
                          : 4096;
    if (max_grow > 4096) {
      max_grow = 4096;
    }
    size_t grow_len = payload_len < max_grow ? payload_len : max_grow;
    if (grow_len > 0) {
      b2nd_append(arr, payload, (int64_t)grow_len, 0);
      b2nd_insert(arr, payload, (int64_t)grow_len, 0, 0);
    }

    /* Resize up (bounded by both items and chunk count). */
    int64_t new_shape[FUZZ_MAX_DIM];
    int64_t new_total = 1;
    int64_t new_nchunks = 1;
    for (d = 0; d < ndim; d++) {
      int64_t cand = arr->shape[d] < FUZZ_MAX_EXTENT ? arr->shape[d] + 1 : arr->shape[d];
      int64_t cand_chunks = (cand > 0 && arr->chunkshape[d] > 0)
                                ? (cand + arr->chunkshape[d] - 1) / arr->chunkshape[d]
                                : 1;
      if (cand > 0 && (new_total * cand > 2 * FUZZ_MAX_ITEMS || new_nchunks * cand_chunks > 64)) {
        cand = arr->shape[d];
        cand_chunks = (cand > 0 && arr->chunkshape[d] > 0)
                          ? (cand + arr->chunkshape[d] - 1) / arr->chunkshape[d]
                          : 1;
      }
      new_shape[d] = cand;
      if (cand > 0) {
        new_total *= cand;
      }
      new_nchunks *= cand_chunks;
    }
    if (new_total <= 2 * FUZZ_MAX_ITEMS && new_nchunks <= 64) {
      b2nd_resize(arr, new_shape, start);
    }

    /* Structural views. */
    b2nd_array_t *view = NULL;
    if (b2nd_squeeze(arr, &view) >= 0 && view != NULL) {
      b2nd_free(view);
    }
    bool squeeze_index[FUZZ_MAX_DIM];
    for (d = 0; d < ndim; d++) {
      squeeze_index[d] = (shape[d] == 1);
    }
    view = NULL;
    if (b2nd_squeeze_index(arr, &view, squeeze_index) >= 0 && view != NULL) {
      b2nd_free(view);
    }
    if (ndim + 2 <= FUZZ_MAX_DIM) {
      bool axis[FUZZ_MAX_DIM] = {false};
      axis[0] = true;
      axis[ndim] = true;
      view = NULL;
      if (b2nd_expand_dims(arr, &view, axis, (uint8_t)(ndim + 2)) >= 0 &&
          view != NULL) {
        b2nd_free(view);
      }
    }

    /* Serialize and parse again (round trip through the frame format). */
    uint8_t *cframe = NULL;
    int64_t len = 0;
    bool needs_free = false;
    if (b2nd_to_cframe(arr, &cframe, &len, &needs_free) >= 0 && len > 0 &&
        cframe != NULL) {
      b2nd_array_t *again = NULL;
      if (b2nd_from_cframe(cframe, len, false, &again) >= 0 && again != NULL) {
        b2nd_to_cbuffer(again, buf, (int64_t)FUZZ_MAX_ITEMS * 8);
        b2nd_free(again);
      }
      if (needs_free) {
        free(cframe);
      }
    }

    free(buf);
  }

  b2nd_free(arr);
  b2nd_free_ctx(ctx);
  blosc2_destroy();
  return 0;
}

#ifdef __cplusplus
}
#endif

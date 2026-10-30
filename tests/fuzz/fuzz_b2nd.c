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
  int64_t req = 1;
  int8_t d;

  for (d = 0; d < ndim; d++) {
    int64_t ext = tail_dim(&r);
    if (req * ext > FUZZ_MAX_ITEMS) {
      ext = 1;
    }
    shape[d] = ext;
    req *= ext;
    chunkshape[d] = (int32_t)(tail_u8(&r) % FUZZ_MAX_EXTENT + 1);
    blockshape[d] = (int32_t)(tail_u8(&r) % FUZZ_MAX_EXTENT + 1);
  }
  for (d = 0; d < ndim; d++) {
    start[d] = tail_dim(&r);
    stop[d] = tail_dim(&r);
    diff[d] = stop[d] - start[d];
    if (diff[d] < 0) {
      diff[d] = 0;
    }
    if (req * diff[d] > FUZZ_MAX_ITEMS) {
      diff[d] = 1;
    }
    req *= diff[d];
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

    /* Grow along axis 0 and insert at the front (bounded by payload cap). */
    size_t grow_len = payload_len < 4096 ? payload_len : 4096;
    if (grow_len > 0) {
      b2nd_append(arr, payload, (int64_t)grow_len, 0);
      b2nd_insert(arr, payload, (int64_t)grow_len, 0, 0);
    }

    /* Resize one dimension up (bounded). */
    int64_t new_shape[FUZZ_MAX_DIM];
    int64_t new_total = 1;
    for (d = 0; d < ndim; d++) {
      new_shape[d] = shape[d] < FUZZ_MAX_EXTENT ? shape[d] + 1 : shape[d];
      new_total *= new_shape[d];
    }
    if (new_total <= 2 * FUZZ_MAX_ITEMS) {
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

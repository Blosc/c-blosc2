#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <blosc2.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stateful fuzzer for the blosc2_schunk API over untrusted frames and chunks.
 *
 * Threat model: super-chunks are serialized as frames (b2frame) that arrive
 * from untrusted sources, and individual chunks are byte blobs whose layout
 * is entirely attacker-controlled once a frame/chunk stream is intercepted.
 * The harness builds (or parses) a super-chunk and then drives a bounded
 * sequence of mutating and reading API calls: append/update/insert/delete
 * chunk, decompress, get_chunk, slice reads and metalayer manipulation.
 *
 * Chunks handed to append/update/insert are validated with the public
 * blosc1_cbuffer_sizes helper first: the caller contract requires a complete
 * chunk, so the harness only passes chunks whose header length fits the
 * remaining bytes.  Everything else (frame parsing, chunk interpretation,
 * offset handling) is left to the library, where a violation is a real bug.
 *
 * Fixed-order tail bytes (innermost byte first) carry the control plane, the
 * head bytes are the payload; allocations are bounded.
 */

#define FUZZ_MAX_OPS 24
#define FUZZ_BUF_CAP (1u << 20)   /* 1 MiB cap for scratch buffers */

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

static int64_t clamp_index(int64_t index, int64_t count) {
  int64_t n = count > 0 ? count : 1;
  return index % n;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  tail_reader r = {data, size};

  if (size < 32) {
    return 0;
  }

  uint8_t srcmode = (uint8_t)(tail_u8(&r) % 3);
  uint8_t comp_idx = tail_u8(&r);
  uint8_t clevel = (uint8_t)(tail_u8(&r) % 10);
  uint8_t typesize = (uint8_t)(tail_u8(&r) % 8 + 1);
  uint8_t filter = (uint8_t)(tail_u8(&r) % (BLOSC_TRUNC_PREC + 1));
  uint8_t nops = (uint8_t)(tail_u8(&r) % (FUZZ_MAX_OPS + 1));

  const int compcodes[5] = {BLOSC_BLOSCLZ, BLOSC_LZ4, BLOSC_LZ4HC, BLOSC_ZLIB,
                            BLOSC_ZSTD};

  const uint8_t *payload = data;
  size_t payload_len = r.size;
  size_t cursor = 0;

  blosc2_init();
  blosc2_set_nthreads(1);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = clevel;
  cparams.compcode = (uint8_t)compcodes[comp_idx % 5];
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                            .contiguous = (srcmode != 1)};

  blosc2_schunk *schunk = NULL;
  if (srcmode == 2) {
    /* Parse an untrusted frame, copying it so it can be mutated below. */
    schunk = blosc2_schunk_from_buffer((uint8_t *)payload,
                                       (int64_t)payload_len, true);
  } else {
    schunk = blosc2_schunk_new(&storage);
  }
  if (schunk == NULL) {
    blosc2_destroy();
    return 0;
  }
  for (uint8_t op_i = 0; op_i < nops; op_i++) {
    uint8_t op = tail_u8(&r);
    uint8_t arg = tail_u8(&r);
    int64_t nchunk = clamp_index(arg, schunk->nchunks);

    switch (op % 10) {
      case 0: {
        /* Append a plain data buffer. */
        size_t avail = payload_len - cursor;
        if (avail == 0) {
          break;
        }
        size_t len = 1 + (arg % 512);
        if (len > avail) {
          len = avail;
        }
        blosc2_schunk_append_buffer(schunk, payload + cursor, (int32_t)len);
        cursor += len;
        break;
      }
      case 1:
      case 2:
      case 3: {
        /* Raw chunk bytes: only pass chunks that are complete per their own
         * header, which is what the API contract requires from callers. */
        size_t avail = payload_len - cursor;
        if (avail < BLOSC_MIN_HEADER_LENGTH) {
          break;
        }
        size_t nbytes = 0, cbytes = 0, blocksize = 0;
        blosc1_cbuffer_sizes(payload + cursor, &nbytes, &cbytes, &blocksize);
        if (cbytes < BLOSC_MIN_HEADER_LENGTH || cbytes > avail) {
          cursor += 1;
          break;
        }
        uint8_t *chunk = (uint8_t *)payload + cursor;
        if (op % 10 == 1) {
          blosc2_schunk_append_chunk(schunk, chunk, true);
        } else if (op % 10 == 2) {
          blosc2_schunk_update_chunk(schunk, nchunk, chunk, true);
        } else {
          blosc2_schunk_insert_chunk(schunk, nchunk, chunk, true);
        }
        cursor += cbytes;
        break;
      }
      case 4:
        blosc2_schunk_delete_chunk(schunk, nchunk);
        break;
      case 5: {
        uint8_t dest[4096];
        blosc2_schunk_decompress_chunk(schunk, nchunk, dest, sizeof(dest));
        break;
      }
      case 6: {
        uint8_t *chunk = NULL;
        bool needs_free = false;
        blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
        if (needs_free && chunk != NULL) {
          free(chunk);
        }
        break;
      }
      case 7: {
        /* Slice read; bounded by the scratch cap. */
        if (schunk->chunksize <= 0 ||
            schunk->chunksize > FUZZ_BUF_CAP / 8 ||
            schunk->nbytes > FUZZ_BUF_CAP) {
          break;
        }
        int64_t start = clamp_index(arg, schunk->nchunks + 1);
        int64_t stop = start + 1 + (tail_u8(&r) % 8);
        if (stop > schunk->nchunks) {
          stop = schunk->nchunks;
        }
        if (stop <= start) {
          break;
        }
        size_t slice_len = (size_t)(stop - start) * (size_t)schunk->chunksize;
        uint8_t *slice_buf = (uint8_t *)malloc(slice_len);
        if (slice_buf != NULL) {
          blosc2_schunk_get_slice_buffer(schunk, start, stop, slice_buf);
          free(slice_buf);
        }
        break;
      }
      case 8: {
        /* Metalayer add / update / get. */
        const char *name = (arg % 2) ? "fz" : "b2nd";
        size_t avail = payload_len - cursor;
        size_t mlen = 1 + (arg % 64);
        if (mlen > avail) {
          mlen = avail;
        }
        if (blosc2_meta_exists(schunk, name) >= 0) {
          uint8_t *content = NULL;
          int32_t content_len = 0;
          blosc2_meta_get(schunk, name, &content, &content_len);
          if (mlen > 0) {
            blosc2_meta_update(schunk, name, (uint8_t *)payload + cursor,
                               (int32_t)mlen);
            cursor += mlen;
          }
        } else if (mlen > 0) {
          blosc2_meta_add(schunk, name, (uint8_t *)payload + cursor,
                          (int32_t)mlen);
          cursor += mlen;
        }
        break;
      }
      default: {
        uint8_t *cframe = NULL;
        bool needs_free = false;
        blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
        if (needs_free && cframe != NULL) {
          free(cframe);
        }
        break;
      }
    }
  }

  blosc2_schunk_free(schunk);
  blosc2_destroy();
  return 0;
}

#ifdef __cplusplus
}
#endif

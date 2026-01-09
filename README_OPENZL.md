# OpenZL in Blosc2 (C API)

This document describes how to use and configure OpenZL with the Blosc2 C API.
OpenZL is available only through the Blosc2 API (not the Blosc1-compatible API).

## Basic Usage

OpenZL is selected via `compcode = BLOSC_OPENZL` and a profile via
`compcode_meta`. The `compcode_meta` is an 8-bit integer corresponding to meta information relating to
codecs, filters and other nodes for the compression graph. Starting from the Least-Significant-Bit (LSB),
setting the bits tells OpenZL how to build the graph:
  CODEC | SHUFFLE | DELTA | SPLIT | CRC | x | x | x |

  - CODEC - If set, use LZ4. Else ZSTD.
  - SHUFFLE - If set, use shuffle (outputs a stream for every byte of input data typesize)
  - DELTA - If set, apply a bytedelta (to all streams if necessary)
  - SPLIT - If set, do not recombine the bytestreams
  - CRC - If set, store a checksum during compression and check it during decompression
  
The remaining bits may be used in the future.


Minimal example:

```c
#include "blosc2.h"

int main(void) {
  blosc2_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = BLOSC_OPENZL;
  cparams.compcode_meta = 0;  /* OpenZL profile for ZSTD*/
  cparams.typesize = 4;
  cparams.clevel = 3;

  blosc2_context* cctx = blosc2_create_cctx(cparams);
  /* ... use blosc2_compress_ctx(...) ... */
  blosc2_free_ctx(cctx);

  blosc2_destroy();
  return 0;
}
```

## Environment Variables

OpenZL behavior can be tuned at runtime using these environment variables:

- `BLOSC_OPENZL_LOGLEVEL=<int>`
  - Sets OpenZL log level.
- `BLOSC_OPENZL_REFLECT=1`
  - Prints a reflection summary of the last OpenZL frame (debug aid).

## Notes and Caveats

- OpenZL is available only via the Blosc2 API. The Blosc1-compatible API
  rejects `openzl` as a compressor name.
- Checksum verification is opt-in for decompression. Compressed frames still
  include checksums; they are just not verified unless enabled.

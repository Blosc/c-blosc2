# OpenZL in Blosc2 (C API)

This document describes how to use and configure OpenZL with the Blosc2 C API.
OpenZL is available only through the Blosc2 API (not the Blosc1-compatible API).

## Basic Usage

OpenZL is selected via `compcode = BLOSC_OPENZL` and a profile via
`compcode_meta`. The `compcode_meta` values correspond to profile names used in
Blosc2 tools.

Minimal example:

```c
#include "blosc2.h"

int main(void) {
  blosc2_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = BLOSC_OPENZL;
  cparams.compcode_meta = BLOSC_SH_ZSTD;  /* OpenZL profile */
  cparams.typesize = 4;
  cparams.clevel = 3;

  blosc2_context* cctx = blosc2_create_cctx(cparams);
  /* ... use blosc2_compress_ctx(...) ... */
  blosc2_free_ctx(cctx);

  blosc2_destroy();
  return 0;
}
```

## Profiles (compcode_meta)

Supported OpenZL profiles in Blosc2:

- `BLOSC_SH_BD_LZ4`
- `BLOSC_SH_BD_ZSTD`
- `BLOSC_SH_LZ4`
- `BLOSC_SH_ZSTD`
- `BLOSC_BD_SH_LZ4`
- `BLOSC_BD_SH_ZSTD`
- `BLOSC_LZ4`
- `BLOSC_ZSTD`

Notes:
- `SH_*` profiles include a shuffle-like transpose stage.
- `BD_SH_*` and `SH_BD_*` include delta + shuffle stages in different orders.
- `LZ4`/`ZSTD` are backend-only profiles (no shuffle).

## Environment Variables

OpenZL behavior can be tuned at runtime using these environment variables:

- `BLOSC_OPENZL_CACHE=0`
  - Disables caching of the OpenZL compressor/graph in Blosc2 contexts.
  - Caching is enabled by default for OpenZL.
  - This affects compression (OpenZL graph creation), not decompression.
- `BLOSC_OPENZL_LOGLEVEL=<int>`
  - Sets OpenZL log level.
- `BLOSC_OPENZL_REFLECT=1`
  - Prints a reflection summary of the last OpenZL frame (debug aid).
- `BLOSC_OPENZL_ENABLE_CHECKSUM=1`
  - Enables checksum verification during OpenZL decompression.
  - By default, Blosc2 disables OpenZL checksum verification for speed.

## Notes and Caveats

- OpenZL is available only via the Blosc2 API. The Blosc1-compatible API
  rejects `openzl` as a compressor name.
- Checksum verification is opt-in for decompression. Compressed frames still
  include checksums; they are just not verified unless enabled.

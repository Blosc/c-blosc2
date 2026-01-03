# OpenZL in Blosc2

This document describes how to use the OpenZL codec in Blosc2, the available
profiles, and the environment variables that control OpenZL behavior.

## Quick Start

OpenZL is exposed via the Blosc2 API (not the Blosc1-compatible API).

Examples:

```bash
# Synthetic bench (OpenZL profile)
./build/bench/b2bench_openzl SH_ZSTD

# b2nd stack bench (OpenZL profile)
./build/bench/b2nd/b2nd_bench_stack_append_openzl openzl SH_ZSTD -n 10000 -t 1 -s 1000x1000 -l 3 -r
```

## Profiles

Blosc2 maps OpenZL profiles via `compcode_meta` (used with `compcode=BLOSC_OPENZL`).
Supported profile names in Blosc2 tools:

- `SH_BD_LZ4`
- `SH_BD_ZSTD`
- `SH_LZ4`
- `SH_ZSTD`
- `BD_SH_LZ4`
- `BD_SH_ZSTD`
- `LZ4`
- `ZSTD`

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

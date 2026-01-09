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

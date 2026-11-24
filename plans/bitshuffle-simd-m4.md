# Bitshuffle SIMD optimization measurements

## Scope and implementation

Baseline: `6307c47f7b09d80e1f9359191f7acf91271bca68`.
Host: Apple M4 Pro, 24 GiB, macOS, Apple Clang 21.

- Specialize the full forward/inverse pipelines for 1/2/4/8/16-byte elements.
- Forward: byte-shuffle into scratch, then emit each byte plane's bit rows directly
  to the final destination. This removes the full-buffer bitrow reorder and skips
  the initial copy for one-byte elements.
- Inverse: fuse bitrow gathering with independent 8x8 bit transposes in vector
  registers, producing byte planes, then use the existing SIMD byte unshuffle.
  One-byte elements need neither scratch materialization nor byte unshuffle.
- NEON: replace scalar movemask emulation with vector bit transposes; process
  128-byte forward tiles with vector stores instead of short scalar stores.
- AVX2 retains a 128-bit byte-shuffle stage and uses AVX2 for forward bit packing;
  inverse bit reconstruction shares the fused SSE2 helper, followed by AVX2 byte
  unshuffle. A tested 256-bit inverse reconstruction was much slower under Rosetta
  and was not retained; native x86 tuning remains necessary.
- Enable NEON bitshuffle dispatch on AArch64. Previously it was disabled and the
  scalar implementation was selected. ARMv7 and non-specialized NEON element sizes
  retain their scalar implementation. No format or public API changes.

## Native NEON kernel results

128,000-byte, cache-resident buffers, one thread. Times are microseconds per
complete forward/inverse kernel call, including scratch-buffer traffic but not
allocation or codecs. Each time is the best of five batches of 4,000 calls.
**The baseline is the previously dispatched scalar kernel**, not the slower,
disabled old NEON implementation. Scalar and NEON are timed in the same binary.

| Bytes/type | Shuffle before | After | Speedup | Unshuffle before | After | Speedup |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 40.792 | 13.252 | 3.08x | 39.286 | 12.970 | 3.03x |
| 2 | 41.064 | 15.576 | 2.64x | 39.698 | 15.016 | 2.64x |
| 4 | 41.048 | 15.726 | 2.61x | 41.362 | 15.220 | 2.72x |
| 8 | 40.828 | 15.605 | 2.62x | 44.594 | 15.254 | 2.92x |
| 16 | 40.716 | 16.613 | 2.45x | 53.306 | 16.032 | 3.32x |

## SSE2 and AVX2 under Rosetta

**These are translated x86 measurements, not native Intel/AMD results.** They
validate correctness and provide a local comparison only. In particular, large
AVX2 inverse improvements reflect Rosetta's translation costs and should not be
extrapolated to native AVX2 CPUs. Same timing protocol as NEON. The SSE2 object is
compiled with `-msse2 -mno-avx -mno-ssse3`; only AVX2 objects use `-mavx2`.

| ISA | Bytes/type | Shuffle before | After | Speedup | Unshuffle before | After | Speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| SSE2 | 1 | 38.037 | 32.526 | 1.17x | 14.007 | 12.896 | 1.09x |
| SSE2 | 2 | 37.281 | 35.032 | 1.06x | 15.845 | 15.096 | 1.05x |
| SSE2 | 4 | 60.579 | 37.801 | 1.60x | 37.593 | 15.224 | 2.47x |
| SSE2 | 8 | 43.287 | 35.875 | 1.21x | 41.668 | 15.168 | 2.75x |
| SSE2 | 16 | 51.134 | 36.839 | 1.39x | 40.337 | 17.572 | 2.30x |
| AVX2 | 1 | 44.950 | 38.968 | 1.15x | 67.657 | 12.801 | 5.29x |
| AVX2 | 2 | 43.958 | 40.995 | 1.07x | 69.783 | 23.190 | 3.01x |
| AVX2 | 4 | 67.714 | 43.110 | 1.57x | 55.061 | 19.639 | 2.80x |
| AVX2 | 8 | 50.787 | 41.812 | 1.21x | 55.712 | 21.481 | 2.59x |
| AVX2 | 16 | 56.999 | 42.040 | 1.36x | 56.355 | 32.033 | 1.76x |

## Billion-element Python constructor and full read

`blosc2.arange(1e9)`, float64, 12 threads, chunks `(2000000,)`, blocks `(16000,)`,
BITSHUFFLE, clevel 1. These match the layout and compressed sizes in the original
example. Exact baseline and candidate native libraries were built into separate
Python extensions, using the same Python source and miniexpr source; the shared
editable installation was not replaced. The conda `blosc2` interpreter was used
with `-S` and explicit isolated package/dependency paths.

Median wall times of five measured repetitions after one warm-up per codec.
The full read is `array[:]`, including allocation/page faults for the 8 GB NumPy
destination, codec decompression, and unbitshuffle. It is substantially noisier
than the direct-kernel benchmark, and its speedup is not an isolated unbitshuffle
speedup.

| Codec | Create before | After | Speedup | Full read before | After | Speedup | Compressed bytes (unchanged) |
|---|---:|---:|---:|---:|---:|---:|---:|
| BLOSCLZ | 405.9 ms | 221.6 ms | 1.83x | 849.9 ms | 779.1 ms | 1.09x | 45,542,129 |
| LZ4 | 410.4 ms | 221.2 ms | 1.86x | 777.9 ms | 642.6 ms | 1.21x | 50,330,803 |
| ZSTD | 486.9 ms | 296.1 ms | 1.64x | 925.3 ms | 819.3 ms | 1.13x | 24,870,021 |

## Validation and reproduction

- All 1,735 native CTest cases pass, including 147 bitshuffle-related cases with guarded scratch,
  compatibility fixtures, partial blocks, and filter pipelines. The guarded
  native test now includes NEON directly.
- Direct NEON/SSE2/AVX2 checks compare shuffled bytes against the scalar wire
  format and decode scalar-produced bytes. They exercise 1/2/3/4/8/16/24-byte
  elements, element counts 8..264 in steps of eight, and offsets 0..3 for input,
  output, and scratch. The benchmark also checks each timed block's roundtrip.
- The direct ARM64 checks pass AddressSanitizer.
- Billion-element checks verify representative output values after each read and
  identical compressed byte counts for all three codecs.

Run from this repository, using the `blosc2` conda environment. `reference-src`
below is an untouched snapshot of the baseline commit. The x86 commands require
an AVX2-capable CPU or Rosetta version supporting AVX2.

```sh
conda run -n blosc2 bash bench/build_bitshuffle_simd.sh . build/bitshuffle-after arm64
conda run -n blosc2 bash bench/build_bitshuffle_simd.sh reference-src build/bitshuffle-before-x86 x86_64
conda run -n blosc2 bash bench/build_bitshuffle_simd.sh . build/bitshuffle-after-x86 x86_64
conda run -n blosc2 build/bitshuffle-after
conda run -n blosc2 build/bitshuffle-before-x86
conda run -n blosc2 build/bitshuffle-after-x86
conda run -n blosc2 cmake --build build --parallel 8
conda run -n blosc2 ctest --test-dir build -R 'bitshuffle|bitunshuffle' --output-on-failure -j 8
conda run -n blosc2 python -S bench/bench_bitshuffle_constructor.py --package PATH_TO_ISOLATED_PACKAGE --output results.json --repeat 5
```

Raw local CSV/JSON files and build/test logs are in `build/bitshuffle-simd/`.
Native x86 performance and cross-platform CI are still outstanding.

# Review of SHUFFLE, BITSHUFFLE, and SHUFFLE+BYTEDELTA fast paths

## Baseline and scope

Baseline: `f1256e391fc1649a6d77e337370c9583c603a670`. Apple M4 Pro,
24 GiB RAM, Apple Clang, native ARM64. Python-Blosc2 uses the same source and
miniexpr dependency in both isolated packages. This review builds on the
recently enabled NEON bitshuffle, rather than comparing with the old scalar
dispatch.

The initial production candidate changes only NEON forward bitshuffle. SHUFFLE,
SHUFFLE+BYTEDELTA, codecs, partitions, inverse kernels, Python sources, and
dependency pins are identical across the paired measurements.

## Review findings

### SHUFFLE

The 2-byte NEON path already batches four tiles, the 4-byte path uses LD4, and
the 8-byte path uses a register transpose. The 16-byte path processes eight
planes at a time to avoid too many simultaneous output streams. The inverse
8-byte path explicitly uses full-width stores to avoid a known Clang codegen
regression. These are already short, load/store-heavy kernels.

The earlier standalone-shuffle investigation established modest constructor
improvements. The earlier fused-transpose investigation tested structured-load
replacement, ZIP butterflies, delta-before-transpose, unrolling, and alternative
stores. A roughly 7% isolated four-byte kernel gain failed end-to-end
qualification. Repeating those changes without new evidence is not justified.

SHUFFLE is a control in the new constructor measurements. No further
SHUFFLE-specific speedup is established by this review.

### SHUFFLE+BYTEDELTA

The new pair already eliminates the whole intermediate write/read between
transpose and delta, and compile-time plane counts avoid register spills.
Fusion requires adjacent filters with matching group metadata and preserves
the legacy/fallback cases. Native support is currently for 2/4/8-byte groups.

There is limited headroom in the forward 4/8-byte NEON pair compared with
standalone SHUFFLE: the delta adds only an extract and subtraction per vector.
Remaining constructor time includes source generation, codec work, per-chunk
synchronization, and allocation. The pair is also a control in the new runs.

Possible follow-ups, requiring separate qualification:

- A native AVX2 fused implementation: x86 fusion currently uses SSSE3 even on
  AVX2-capable CPUs.
- A 16-byte fused implementation, if representative complex/structured-array
  workloads justify it; float32/float64 ramps do not test that group size.
- Source-generation and dispatch profiling for float32 constructors. In the
  screen, float32 three-dimensional linspace is about 174–189 ms across all
  filters at 100M elements; improving a filter cannot remove that common cost.
  This observation does not identify its exact owner without profiling.

### BITSHUFFLE: a measured opportunity

The new two-pass implementation still transposes the bits of every byte plane,
including planes whose bytes are all identical. Floating-point ramps often
have constant sign/exponent bytes and, for integer-valued float64, constant low
mantissa bytes.

The candidate probes the first and last 16 bytes of a sufficiently large plane.
If those are uniform, it verifies **every byte** with a vector scan. Only a
verified constant plane is emitted as eight zero/one bit rows using `memset`.
Other planes use the existing SIMD transpose. This preserves the format and
does not infer constancy from a sample.

This is a data-dependent optimization, not a universal bitshuffle speedup.
Random planes normally reject at the probes. A nearly constant plane whose
endpoints match can pay for an unsuccessful full scan and then the normal
transpose. The initial one-byte, single-interior-difference case was about
13–15% slower. The follow-up prefix implementation described below removes this
specific regression.

Additional x86 review finding: AVX-512 is dispatched ahead of AVX2, but its
forward bitshuffle still uses the older three-pass implementation. Porting the
two-pass design and comparing dispatch choices on actual AVX-512 hardware is
worth investigating. Rosetta cannot establish that result.

## Measurement protocol

`bench/bench_filter_constructors.py` launches isolated before/after workers,
alternates their execution order, rotates/reverses cell order, and discards one
warm-up per cell per session. It records wall and process CPU times, validates
sampled values including endpoints/block boundaries, and requires matching
compressed sizes and storage parameters. Only one timed worker runs at a time;
builds and tests are not run concurrently with constructor measurements.

Workloads: `arange(float(n))`, `linspace(0, 1, n)`, and a separate 3-D screen
with shape `(n/1000000, 1000, 1000)`. Dtypes: float32 and float64. Twelve threads.
The 1-D workloads use 16 MB chunks and 128 KB blocks; the 3-D screen uses 16 MB
chunks and 80 KB blocks. Partitions are fixed in bytes across dtypes and are
identical before/after. Timings include construction and compression, but not
validation, garbage collection, or result destruction.

The native paired microbenchmark alternates libraries in the same process and
reports medians of nine batches of 500 calls following a warm-up. It checks
exact transformed-byte equality and includes random, ramp, constant, and
almost-constant inputs. A single cache-resident block represents a segment in
the middle of a billion-element ramp, not the entire constructor.

## Initial 100M-element screen, clevel 1

One session, three measured repetitions. Useful for screening, not final
cross-machine performance claims:

| BITSHUFFLE workload | LZ4 before → after | ZSTD before → after |
|---|---:|---:|
| arange float32 | 45.50 → 43.68 ms | 46.27 → 44.92 ms |
| arange float64 | 25.58 → 21.15 ms | 32.39 → 27.66 ms |
| linspace float32 | 49.06 → 47.38 ms | 54.06 → 51.99 ms |
| linspace float64 | 51.05 → 49.22 ms | 60.88 → 58.92 ms |
| 3-D linspace float32 | 184.12 → 181.75 ms | 189.09 → 187.99 ms |
| 3-D linspace float64 | 63.57 → 61.76 ms | 65.71 → 63.96 ms |

## Isolated forward kernel, 128 KB

Times in microseconds:

| Type bytes | Input | Before | After | Speedup |
|---:|---|---:|---:|---:|
| 4 | random | 16.056 | 16.046 | 1.001x |
| 4 | arange segment | 16.072 | 11.196 | 1.436x |
| 4 | linspace segment | 15.974 | 11.168 | 1.430x |
| 8 | random | 16.030 | 15.998 | 1.002x |
| 8 | arange segment | 15.922 | 8.222 | 1.937x |
| 8 | linspace segment | 16.064 | 12.240 | 1.312x |
| 1 | constant | 13.460 | 3.230 | 4.167x |
| 1 | one interior difference | 13.456 | 15.282 | 0.881x |

## Billion-element qualification, clevel 1

Three fresh process pairs, four measured repetitions per cell per pair: 12
samples per variant. All three sessions improve in every BITSHUFFLE cell.
SHUFFLE and SHUFFLE+BYTEDELTA controls are within 1% in the pooled results.
All compressed byte counts and checked values match before/after.

| BITSHUFFLE workload | Codec | Before | After | Speedup |
|---|---|---:|---:|---:|
| arange float32 | LZ4 | 430.46 ms | 412.76 ms | 1.043x |
| arange float32 | ZSTD | 486.83 ms | 469.21 ms | 1.038x |
| arange float64 | LZ4 | 212.23 ms | 168.24 ms | 1.261x |
| arange float64 | ZSTD | 280.99 ms | 232.42 ms | 1.209x |
| linspace float32 | LZ4 | 446.49 ms | 429.84 ms | 1.039x |
| linspace float32 | ZSTD | 493.53 ms | 476.63 ms | 1.035x |
| linspace float64 | LZ4 | 384.47 ms | 364.62 ms | 1.054x |
| linspace float64 | ZSTD | 523.27 ms | 498.12 ms | 1.050x |

This is an incremental gain over the already optimized NEON implementation.
Float64 arange LZ4 CPU time improves by 1.36x, corroborating the wall-time gain.
This table measures the initial full-plane candidate, not the prefix follow-up.

### Comparing the three pipelines

Candidate timings at clevel 1, one billion float64 elements:

| Constructor | Codec | SHUFFLE | BITSHUFFLE | SHUFFLE+BYTEDELTA |
|---|---|---:|---:|---:|
| arange | LZ4 | 107.66 ms | 168.24 ms | 110.92 ms |
| arange | ZSTD | 121.86 ms | 232.42 ms | 138.78 ms |
| linspace | LZ4 | 359.92 ms | 364.62 ms | 140.10 ms |
| linspace | ZSTD | 533.14 ms | 498.12 ms | 651.91 ms |

Filter choice remains a time/space tradeoff. For example, float64 linspace with
LZ4 is about 2.57x faster with the fused pair than with SHUFFLE alone, and its
compressed size is 105 MB versus 1,060 MB. For ZSTD level 1, the fused pair is
slower but stores 176 MB versus SHUFFLE's 346 MB. Codec level therefore matters;
the earlier level-5 results cannot be applied to level 1.

### Level-5 cross-check

Two fresh process pairs, eight measured samples per variant, float64, same
1-D partitions. These are also initial full-plane candidate measurements:

| Constructor | Codec | SHUFFLE | BITSHUFFLE | SHUFFLE+BYTEDELTA |
|---|---|---:|---:|---:|
| arange | LZ4 | 108.31 ms | 168.45 ms | 110.99 ms |
| arange | ZSTD | 147.23 ms | 555.05 ms | 165.98 ms |
| linspace | LZ4 | 328.64 ms | 366.88 ms | 139.21 ms |
| linspace | ZSTD | 1357.93 ms | 1178.33 ms | 463.63 ms |

Incremental BITSHUFFLE speedups at level 5 are 1.258x/1.061x for LZ4
arange/linspace, and 1.088x/1.010x for ZSTD. ZSTD codec time increasingly
dominates, so the same filter optimization produces a smaller end-to-end gain.
The fused pair remains the strongest measured LZ4 linspace choice.

## LZ4-focused follow-up: constant prefixes

The follow-up stops checking at the first nonconstant 128-byte tile and emits
the verified prefix as bit rows. The existing transpose starts at that tile,
not at the beginning of the plane. It does not rescan/retranspose the already
verified prefix, even when the full plane is not constant.

The scan is now implemented in a small private header shared by NEON, SSE2,
and AVX2. A minimum byte-plane length of 1,024 avoids row-fill setup costs on
tiny planes; for example the initial 128-byte-plane shortcut regressed a
1 KB float64 block. Inputs below the cutoff retain the normal SIMD path.

In the 128 KB paired kernel comparison, float64 arange improves about 2.04x
and linspace 1.34x versus the original optimized baseline. Random-byte kernels
remain essentially flat. The one-byte interior-difference case now improves
about 1.56x instead of regressing. The prefix representation and all emitted
bytes remain identical to the scalar oracle.

### Final LZ4 constructor qualification

Three fresh process pairs, 12 measured samples per cell, one billion elements,
same fixed partitions and 12 threads. Before is the original `f1256e39` SIMD
baseline, not the intermediate constant-plane experiment.

| BITSHUFFLE workload | LZ4 level | Before | Prefix candidate | Speedup |
|---|---:|---:|---:|---:|
| arange float32 | 1 | 427.22 ms | 408.30 ms | 1.046x |
| arange float32 | 5 | 425.63 ms | 408.23 ms | 1.043x |
| arange float64 | 1 | 211.78 ms | 166.64 ms | 1.271x |
| arange float64 | 5 | 211.24 ms | 166.27 ms | 1.271x |
| linspace float32 | 1 | 446.50 ms | 427.46 ms | 1.045x |
| linspace float32 | 5 | 446.72 ms | 427.81 ms | 1.044x |
| linspace float64 | 1 | 384.06 ms | 365.34 ms | 1.051x |
| linspace float64 | 5 | 380.65 ms | 360.99 ms | 1.054x |

SHUFFLE and SHUFFLE+BYTEDELTA control speedups range from 0.993x to 1.007x:
no meaningful change is established for them. The final shortcut improves
BITSHUFFLE LZ4 throughput without changing compressed bytes.

### Explicit byte-group screen

LZ4 level 1, 100M float64 elements, one process pair and three measured samples
per cell. This changes filter metadata rather than native code; it is not a
before/after implementation claim. All sampled values decode correctly.

| Fused byte group | arange time | Compressed | linspace time | Compressed |
|---:|---:|---:|---:|---:|
| Default (8) | 15.66 ms | 2.65 MB | 20.58 ms | 19.21 MB |
| 2 | 23.05 ms | 58.36 MB | 63.14 ms | 800.00 MB |
| 4 | 19.00 ms | 18.94 MB | 92.13 ms | 300.12 MB |
| 16 (staged fallback) | 19.16 ms | 3.45 MB | 25.78 ms | 27.55 MB |

The natural float64 group is the best screened choice for both time and size.
No metadata/default change is warranted. Float32 alternatives also fail to
establish a useful time/space win; its default fused path is effectively flat.

SSE2 and AVX2 oracle checks run under Rosetta with ISA-specific object flags.
They do not establish native Intel/AMD performance. AVX-512 is intentionally
unchanged pending hardware qualification.

## Correctness checks

- All 1,735 native CTest cases pass with the final prefix candidate.
- Constant-plane regression cases use all 256 byte values, 1/2/4/8/16-byte
  element widths, byte-plane lengths around 128/256/1,024 bytes, output/scratch
  guards, and misaligned buffers. Interior and near-tail differences must not
  be mistaken for constant data.
- The standalone SIMD driver now checks random, constant-plane, and
  almost-constant inputs with element counts 8..1,032 and offsets 0..3. It
  compares forward output to the scalar format and checks inverse decoding.
  The NEON checks and strict SSE2/AVX2 Rosetta checks pass.
- The native guarded scratch/pipeline regression program passes AddressSanitizer
  with the candidate NEON kernel instrumented. This is not an assertion that
  all third-party codec archives are sanitizer-instrumented.
- Isolated constructors retain identical compressed sizes/parameters before
  and after, with numerical samples checked outside timing.

## Reproduction

Native candidate: `blosc/bitshuffle-neon.c`.
Regression cases: `tests/test_bitshuffle_scratch.c`, constant-plane tests.
Raw local results and isolated packages: `build/filter-review/`.
The final LZ4 constructor evidence is `prefix-billion-lz4.json`;
the preceding tables retain explicitly labeled initial-experiment results.
Final kernel evidence is `prefix-kernels.csv` and `prefix-kernels-1k.csv`;
the grouping screen is `group-screen-lz4.json`.

```sh
conda run -n blosc2 python -S bench/bench_filter_constructors.py \
  --reference build/filter-review/before --candidate build/filter-review/prefix \
  --output results.json --size 1000000000 --tasks arange linspace \
  --codecs LZ4 --clevels 1 5 --sessions 3 --repeats 4
```

For direct-kernel comparison, compile baseline/candidate bitshuffle-neon,
bitshuffle-generic, shuffle-neon and shuffle-generic sources into separate
shared libraries with `-O3 -DBLOSC_TESTING`, then run
`bench/bench_bitshuffle_pair.c` against those libraries. Include the native
`include/` and `blosc/` directories at build time.

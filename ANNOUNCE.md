# Announcing C-Blosc2 3.8.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

C-Blosc2 3.8.0 delivers major SIMD optimizations in BloscLZ 2.7.0 and fused
SHUFFLE+BYTEDELTA across x86-64 and ARM64, lower thread-pool overhead, and
important security and portability fixes.

- BloscLZ 2.7.0 speeds up both compression and decompression on x86-64
  (SSE2/SSSE3/AVX2) and ARM64 (NEON) with improved match finding, run
  detection, and vectorized literal/match copying.
- Fused SHUFFLE+BYTEDELTA gains an AVX2 path, faster x86-64 and NEON backward
  decoders, a zero-high-planes fast path, and fixes missing NEON symbols on
  ARM64 Linux (#883).
- Reduced shared thread-pool overhead by running worker 0 on the calling
  thread, spawning `nthreads - 1` workers, and reusing thread-local scratch
  buffers.
- Fixed a heap buffer overflow in the ZFP compressors when compressed output
  exceeds `output_len` (#887, #888), added `PREFER_EXTERNAL_ZFP` (#886), and
  fixed portability issues on older macOS / unoptimized GCC builds (#884, #885).

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an
[award-winning library](https://www.blosc.org/posts/prize-push-Blosc2)
that has been around for more than a decade.

Blosc2 expands the capabilities of Blosc by providing a higher level
container that is able to store many chunks on it (hence the super-chunk name).
It supports storing data on both memory and disk using the same API.
Also, it adds more compressors and filters.

## Download sources

The github repository is over here:

https://github.com/Blosc/c-blosc2

Blosc is distributed using the BSD license, see LICENSE.txt
for details.

## Mailing list

There is an official Blosc mailing list at:

blosc@googlegroups.com
https://groups.google.com/g/blosc


Enjoy Data!
- The Blosc Development Team

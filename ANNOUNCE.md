# Announcing C-Blosc2 3.6.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release focuses on robustness, concurrency, multiprocess safety, and slicing performance.

Key highlights include:

- **New Sub-Box Slicing API**: `b2nd_get_slice_cbuffer_into()` allows decompressing ND
  slices directly into a sub-box of an existing larger buffer (e.g. batch items or padded
  arrays) without temporaries or zero-fill (#845).
- **Consistent Frame Serialization**: Super-chunks and b2nd arrays now consistently serialize
  raw chunks as stored regardless of storage layout, resolving double-postfiltering bugs upon
  deserialization and drastically speeding up filter serialization (#858).
- **Multiprocessing & Thread Safety**: Full `fork()` safety for shared thread pools and
  reader caches (#852), race-free lazy initialization of the library and dynamic plugins across
  concurrent threads (#875), thread-safe shuffle initialization (#872), thread pool caching
  across short-lived contexts (#865), and restored thread pool scaling on small workloads (#878).
- **Robustness & Security Hardening**: Strict index bounds and buffer validation for
  orthogonal selections (#864), hardened frame reading and metadata validation against corrupted or
  damaged files (#871), crash prevention on partial frame copies (#869), safe error-path cleanups
  in context creation (#868), and protection of metalayers against filter corruption (#857).
- **Performance & SIMD Optimizations**: Vectorized 64-byte run-length detection via SSE2/NEON
  (5.5%–8% faster compression) (#874), AVX-512/AVX2/SSE2 vectorized bitunshuffle for 1- and 2-byte
  elements (#867), dimension merging for b2nd box copies (#851) and orthogonal selections (~4.7x faster
  for channels-last images) (#861), direct zero-fill for special chunks in slices (#844), and extensive
  scratch buffer reuse across bitshuffle (#860) and slicing (#866).

Special thanks to Fabian Isensee for his outstanding contributions and bug fixes!

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

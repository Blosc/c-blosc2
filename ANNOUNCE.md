# Announcing C-Blosc2 3.5.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This minor release introduces new APIs and delivers major performance enhancements,
particularly for frame slicing, serialization, read-ahead prefetching, and I/O
handling on local disks and NFS, alongside bug and leak fixes.

Key highlights include:

- **Read-ahead prefetching for on-disk frames**: With `blosc2_set_prefetch()` (or
  the `BLOSC_PREFETCH` environment variable), slicing operations can prefetch
  chunks ahead using background threads. On NFS, cold slice reads of 3D images
  are 3x–5x faster; on local NVMe, 1.5x–3x faster.
- **New APIs**:
  - `blosc2_schunk_to_buffer_alloc()`: serializes an SChunk into a contiguous
    frame provided by a caller callback without extra copying.
  - `blosc2_schunk_get_chunk_range()`: returns the byte offset and size of a
    chunk in an on-disk contiguous frame.
  - `blosc2_set_prefetch()` / `blosc2_get_prefetch()`: controls background
    prefetching parameters for frame reads.
- **1-byte shuffle optimization**: The filter pipeline now skips the shuffle step
  for 1-byte elements, accelerating uint8/int8 compression by ~2x and decompression
  by ~3.5x when SHUFFLE is enabled.
- **Faster b2nd slices and frame copies**:
  - Slice reads/writes decompress/compress directly into/from caller buffers where
    contiguous, bypassing chunk scratch copies and reusing chunk temporaries via
    thread-local caches.
  - Frame copies and serialization (`blosc2_schunk_copy()`) write chunks and frame
    metadata in a single pass.
- **Robustness and Bug Fixes**:
  - Fixed a divide-by-zero vulnerability during slice operations when `chunksize <= 0` (#834).
  - Fixed uninitialized chunk reads returning stale memory in `b2nd_uninit()`.
  - Fixed an issue where chunks stored uncompressed with `use_dict` retained dictionary
    flags and failed on decompression.
  - Fixed multiple memory leaks and dangling state on error paths.
  - Silenced CMake CMP0218 and minimum version deprecation warnings on newer CMake versions.

Because new APIs were added, the minor version and library SOVERSION have been bumped.

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

# Announcing C-Blosc2 3.9.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

C-Blosc2 3.9.0 delivers a critical Zstandard 1.6.0 security update, new
explicit flush APIs with frame close error propagation, BloscLZ branchless
optimizations, and several important bug fixes.

- Updated bundled/fetched Zstandard to 1.6.0, fixing CVE-2026-124851 and other
  memory-safety issues (legacy format support is now disabled by default).
- Added `blosc2_schunk_flush()` and `b2nd_flush()` and propagated frame close
  errors from `blosc2_schunk_free()` / `b2nd_free()` so delayed I/O errors on
  `fsync()`/`close()` (such as NFS `EDQUOT` or `ENOSPC`) are caught reliably
  (#848).
- Made BloscLZ tagged hash-table lookups and short-match row selection
  branchless for faster GCC compression and decompression, and fixed a
  declaration-after-label portability issue for pre-C23 compilers.
- Fixed super-chunk copy truncation for `typesize > 255` (#890), double-free of
  `vlmetalayers` in `b2nd` views (#876), and ZFP compressed blocks smaller than
  8 bytes when `ZFP_BIT_STREAM_WORD_SIZE < 64` (#893).

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

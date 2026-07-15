# Announcing C-Blosc2 3.2.2
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a performance-focused follow-up to 3.2.1, with two main
improvements for b2nd get_slice operations:

* **Small slice reads from large chunks are now up to 3x faster.**
  Previously, every ``get_slice`` call allocated a full chunk-sized
  scratch buffer, even when only one block was needed.  Now small requests
  decompress individual blocks on the fly into a
  reusable block-sized buffer, so cost scales with the request instead of
  the chunk size.  Large requests still use the parallel path unchanged.

Plus a documentation refresh (updated roadmap, stale references fixed).

This release introduces no breaking API or ABI changes.

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

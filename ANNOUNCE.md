# Announcing C-Blosc2 3.4.1
A fast, compressed, and persistent binary data store library for C.

## What is new?

This bugfix release brings substantial performance improvements for in-place
writes to on-disk contiguous frames (especially on NFS), security hardening for
sparse frames, critical fixes for filter pipelines chaining 3 or more filters
alongside multithreaded DELTA decoding, and a new example for sparse coordinate
handling.

First, in-place writes to on-disk contiguous frames on POSIX systems are now
substantially faster.  Operations like appending, inserting, updating, or deleting
chunks (as well as `b2nd_set_slice`) previously reopened and closed the frame file
multiple times and truncated on every trailer update, incurring synchronous flush
penalties on NFS (around ~20 ms per chunk).  Frames now cache a writable file
descriptor and avoid unnecessary truncations and flushes, reducing chunk writes
on NFS to ~1 ms and improving local disk writes by ~1.4x.

Second, sparse (directory) frames have been hardened against out-of-bounds heap
reads: `sframe_get_chunk()` and `frame_decompress_chunk()` now verify that the
chunk's compressed size (`cbytes`) recorded in the chunk header does not exceed
the actual size of the chunk file on disk before allocating or decompressing
(#822).  Thanks to Mar Dominguez (@mardgez).

Third, the filter pipeline has been fixed when chaining 3 or more filters:
- `pipeline_forward()` now ping-pongs strictly between thread-local scratch
  buffers, ensuring that caller input data is never overwritten and preserving
  block 0 integrity as the reference block for DELTA encoding (#820).
- `pipeline_backward()` now prevents buffer aliasing across chained backward
  filters and coordinates multithreaded DELTA decoding so worker threads wait
  until block 0 has finished its entire backward filter pipeline before decoding
  subsequent blocks.

Fourth, a new example (`examples/sparse_coords.c`) was added to demonstrate
sparse coordinate handling and selective retrieval.

There are no breaking API or format changes in this release.

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

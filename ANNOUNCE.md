# Announcing C-Blosc2 3.6.1
A fast, compressed, and persistent binary data store library for C.

## What is new?

This bugfix and performance release optimizes read-ahead prefetching for
memory-mapped frames on GPFS (IBM Storage Scale) and introduces the
`BLOSC_PREFETCH_POPULATE` environment variable.

Key highlights include:

- **GPFS Read-Ahead Prefetching Optimization**: Read-ahead threads reading
  memory-mapped frames (`BLOSC2_IO_FILESYSTEM_MMAP`) on GPFS now populate
  mappings via `madvise(MADV_POPULATE_READ)` (Linux >= 5.14) instead of issuing
  `read()` calls, avoiding redundant page faults and delivering 1.10x–1.34x faster
  reads for multi-dimensional slices (#880).
- **Filesystem Detection Caching**: Cached filesystem type checks per thread
  via `statx()` avoid redundant `fstatfs()` server RPC calls when accessing
  remote filesystems such as NFS (#880).
- **Kernel Capability Probing & `BLOSC_PREFETCH_POPULATE`**: Added runtime probing
  for kernel `MADV_POPULATE_READ` support and the `BLOSC_PREFETCH_POPULATE` environment
  variable to control memory-mapping prefetch behaviour across filesystems (#880).
- **Clang Test Build Fix**: Ensured `madvise()` visibility in `test_b2nd_prefetch.c`
  under Clang on Linux (#880).

Special thanks to Fabian Isensee for this contribution!

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

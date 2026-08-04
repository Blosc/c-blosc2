# Announcing C-Blosc2 3.3.1
A fast, compressed, and persistent binary data store library for C.

## What is new?

Reads from on-disk frames got noticeably faster.  The default filesystem
I/O backend now uses positioned reads and writes (``pread``/``pwrite``,
``ReadFile``/``WriteFile`` with an explicit offset on Windows) instead of
seek + stdio, and a frame keeps a single read handle open instead of
opening and closing the file several times per chunk fetch.  Scattered
small reads out of a cframe are about 3x faster in our microbenchmarks.

Concurrent readers gain the most, since the per-access open/close this
removes was paid by every process and contended in the kernel.  Eight
processes each doing 300 random slice reads over the same 269 MB frame
went from 0.52 s to 0.36 s of wall time (Apple M4 Pro): about 1.4x,
against 13% for a single reader.

There are no API or format changes in this release.

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

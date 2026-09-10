# Announcing C-Blosc2 3.3.4
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release introduces bounded-memory tail compaction for contiguous on-disk
frames and clean file eviction for payload-free chunks in sparse frames.

Updating chunks in place within an on-disk contiguous frame previously allocated
memory for the entire remaining payload tail in RAM. For large frames (tens or
hundreds of gigabytes), this caused large memory spikes or out-of-memory aborts.
Tail compaction now moves payload data using a bounded scratch buffer capped at
1 MiB, shifting data forward on shrink and backward on expansion for overlap
safety, ensuring RAM usage stays minimal regardless of dataset size.

In sparse frames, updating chunks to payload-free special values (ZERO, UNINIT,
NAN) now unlinks and deletes the old physical chunk files instead of leaving
0-byte files on disk, preventing directory clutter and inode exhaustion during
cache churn. Deletion is coordinated safely after committing index updates and
is fully integrated with custom I/O backends.

Additionally, memory mapping (`mmap`) stability has been improved on POSIX and
Windows, avoiding `MAP_FIXED` pitfalls on BSD/macOS and preserving external file
growth during cleanup.

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

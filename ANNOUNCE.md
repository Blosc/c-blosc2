# Announcing C-Blosc2 3.2.3
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a hot-fix release for 3.2.2.  The new fast path for small b2nd
``get_slice`` reads returned truncated (corrupted) data for arrays with
typesize > 255, because such chunks are compressed with an internal
typesize of 1 and the block reads were requested in array-typesize
units.  This is now fixed (with a new regression test), and the fast
path remains as fast for large typesizes too.

Users of 3.2.2 reading slices from b2nd arrays with typesize > 255
should upgrade.  This release introduces no API or ABI changes.

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

# Announcing C-Blosc2 2.15.1
A fast, compressed and persistent binary data store library for C.

## What is new?

This is a maintenance release in which we are fixing calling instructions
more advanced than available in current CPU, causing a SIGKILL.
Furthermore, a new `b2nd_nans` function has been added. In addition,
the internal LZ4 sources were updated to 1.10.0 and some
other improvements were made.

For more info, please see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

Also, there is blog post introducing the most relevant changes in Blosc2:

https://www.blosc.org/posts/blosc2-ready-general-review/

## What is it?

Blosc2 is a high performance data container optimized for binary data.
It builds on the shoulders of Blosc, the high performance meta-compressor
(https://github.com/Blosc/c-blosc).

Blosc2 expands the capabilities of Blosc by providing a higher lever
container that is able to store many chunks on it (hence the super-block name).
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

## Tweeter feed

Please follow @Blosc2 to get informed about the latest developments.


Enjoy Data!
- The Blosc Development Team

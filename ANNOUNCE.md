# Announcing C-Blosc2 2.10.0
A fast, compressed and persistent binary data store library for C.

## What is new?

This is a maintenance release with a couple of important fixes.  The first
is bytedelta, which receives a new ID (35), whereas the previous one (34)
is still available for backward compatibility.  The second is a fix for
the filter pipeline, which e.g. allows to use several shuffle filters in a row
(thanks to Tom Birch). There are also several fixes for helping integration
of C-Blosc2 in other projects (thanks to Alex Huebel).

Due to the important fixes, an upgrade is recommended for all users.

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

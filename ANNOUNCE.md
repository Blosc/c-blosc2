# Announcing C-Blosc2 2.20.0
A fast, compressed and persistent binary data store library for C.

## What is new?

This release introduces several public APIs, including:
* `blosc2_shuffle`
* `blosc2_unshuffle`
* `blosc2_bitshuffle`
* `blosc2_bitunshuffle`

Now, users can use these functions to shuffle and unshuffle data directly.
It also adds a new `b2nd_save_append` API, useful for appending a b2nd
array into a file.

Finally, several improvements have been made to the build system.

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an [award-winning]
(https://www.blosc.org/posts/prize-push-Blosc2)` library that has been
around for more than a decade.

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

## Fosstodon feed

Please follow https://fosstodon.org/@Blosc2 to get informed about the latest developments.


Enjoy Data!
- The Blosc Development Team

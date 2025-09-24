# Announcing C-Blosc2 2.21.3
A fast, compressed and persistent binary data store library for C.

## What is new?

The main change is an increase in the number of max dimensions for a 
b2nd array from 8 to 16.
Otherwise, this is a maintenance release, with a few fixes and some optimizations.
Thanks to Barak Ugav and Antonio Valentino for their contributions.

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

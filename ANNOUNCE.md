# Announcing C-Blosc2 3.2.1
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a small follow-up to 3.2.0's Growth-SWMR (single writer, multiple
readers) support: a new `blosc2_schunk_refresh()` API gives plain
`blosc2_schunk` readers the same explicit, data-free re-sync point that
`b2nd_refresh()` already gave b2nd arrays, for polling another handle's
changes without touching data.

This release introduces no breaking API or ABI changes.

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an
[award-winning library](https://www.blosc.org/posts/prize-push-Blosc2)
that has been around for more than a decade.

Blosc2 expands the capabilities of Blosc by providing a higher level
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


Enjoy Data!
- The Blosc Development Team

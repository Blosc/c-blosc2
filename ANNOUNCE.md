# Announcing C-Blosc2 3.3.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release hardens the paths that read data described by untrusted
metadata.  A crafted ``.b2nd`` file could overflow the b2nd shape
arithmetic and reach a division by zero or an undersized allocation, and
a crafted chunk carrying no extended header could be read past its end.
Anyone opening b2nd files or chunks they did not produce themselves
should upgrade.

It also adds ``blosc2_getitem_bytes_ctx()``, a byte-counting counterpart
to ``blosc2_getitem_ctx()``.  The unit of the latter is the typesize the
chunk records, which is one byte for typesizes above 255, so its meaning
silently changes with the data; bytes do not.  Prefer the new entry point
in code that does not choose the typesize itself.  Two schunk read paths
that got this wrong, returning errors or the wrong bytes for typesizes
above 255, are fixed as well.

Finally, the build on FreeBSD and the other BSDs is fixed.

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

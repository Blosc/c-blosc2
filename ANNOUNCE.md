# Announcing C-Blosc2 3.3.2
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a bugfix release, mainly about BYTEDELTA.

The BYTEDELTA filter silently corrupted the tail of any block whose
length is not a multiple of the typesize.  The last ``length % typesize``
bytes belong to no byte stream, and were never written to the output in
either direction, leaving whatever the destination buffer happened to
hold.  They are now passed through verbatim, as SHUFFLE already does with
the same remainder.

Blocks whose length is a multiple of the typesize are unaffected, so
existing data still decodes bit for bit.  Data written by the old encoder
at a non-multiple length cannot be recovered: those tail bytes were never
encoded in the first place.

Also, the installed CMake package now exports the configured
``CMAKE_INSTALL_INCLUDEDIR`` instead of a hardcoded ``include``.

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

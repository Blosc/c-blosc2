# Announcing C-Blosc2 3.0.2
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release is a bugfix release:

* Fix for windows when using ctx API from multiple threads.  Closes #763.
  Thanks to Christoph Gohlke (@cgohlke).

* Harden metalayer APIs against invalid lengths and unsafe memory usage.
  PR #758.  Thanks to @metsw24-max.

* Fix DELTA pipelines after byte-transforming filters (e.g. shuffle).

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an [award-winning]
(https://www.blosc.org/posts/prize-push-Blosc2) library that has been
around for more than a decade.

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

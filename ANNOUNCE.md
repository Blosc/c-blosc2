# Announcing C-Blosc2 3.1.1
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a maintenance release focused on performance and documentation
polish:

* **Faster orthogonal selections**: ``b2nd_get_orthogonal_selection()`` is
  now significantly faster for axis-based row/column selections thanks to
  internal buffer reuse and batched element copies.  In internal benchmarks,
  this yields around a **2.6x speedup** on the ``blosc2.take()`` ndim=2
  benchmark.

* **More complete API documentation**: static-inline functions are now
  included in the generated user-facing docs, and recently added public APIs
  that were missing from the reference manual are now documented too.

* **Doc coverage checks**: a new ``doc/check_missing_docs.py`` helper makes
  it easier to catch undocumented public functions before a release.

* **No API/ABI changes**: 3.1.1 is a drop-in maintenance update over 3.1.0.

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

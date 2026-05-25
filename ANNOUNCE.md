# Announcing C-Blosc2 3.1.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release brings a new sparse coords getter API, header-only metalayer
access for plugins, and new registered codec IDs:

* **Sparse getitem**: ``blosc2_schunk_get_sparse_buffer()`` and
  ``b2nd_get_sparse_cbuffer()`` extract arbitrary sets of coordinates in a
  single call, batching by chunk internally to minimize decompression
  overhead.  Much faster than repeated individual ``getitem`` calls.

* **Header-only metalayer access for plugins**: ``b2nd_deserialize_meta_inline()``
  lets external plugins read b2nd metadata without linking against
  ``libblosc2``, avoiding symbol conflicts with other libraries.

* **New codec IDs**: J2K (124) and HTJ2K (125) are now globally registered
  for upcoming JPEG 2000 / High-Throughput JPEG 2000 plugins.

* Several bug fixes: ``swap_store()`` on big-endian, divide-by-zero in
  ``b2nd_update_shape``, and trailer vlmetalayer parsing.

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

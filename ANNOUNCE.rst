===============================================================
 Announcing C-Blosc 2.0.0-beta.1
 A simple, compressed and fast data store library for C
===============================================================

What is new?
============

This is the first beta version, so the API has been declared frozen from
now on.  To avoid name collisions with existing C-Blosc 1.x deployments,
the library is officially called `blosc2` and the main header is `blosc2.h`.
Also, this version offers a frame object so that data can be stored
sequentially both in-memory and on-disk.  Last but not least, blosc2 supports
the optimized implementations of LZ4 in Intel's IPP for improved compression
ratios and speed.

In principle, C-Blosc2 should be backward compatible with C-Blosc, so you
can start using it right away and slowly begin to use its new functionality,
like the new filters, prefilters, super-chunks and frames.  See docs in:
https://blosc2.readthedocs.io

**IMPORTANT**: Please note that, even if the API has been declared frozen,
that does not mean that Blosc2 is ready for production yet: internal structures
can change, formats can change and most importantly, bugs can be normal at this
stage.  So *do not assume* that your blosc2 data can be read with future versions.

For more info, please see the release notes in:

https://github.com/Blosc/c-blosc2/blob/master/RELEASE_NOTES.md


What is it?
===========

Blosc2 is a high performance data container optimized for binary data.
It builds on the shoulders of Blosc, the high performance
meta-compressor (http://www.blosc.org).

Blosc2 expands the capabilities of Blosc by providing a higher lever
container that is able to store many chunks on it (hence the
super-block name).  It supports storing data on both memory and disk
 using the same API.  Also, it adds more compressors and filters.


Download sources
================

The github repository is over here:

https://github.com/c-blosc2

Blosc is distributed using the BSD license, see LICENSES/BLOSC2.txt
for details.


Mailing list
============

There is an official Blosc mailing list at:

blosc@googlegroups.com
http://groups.google.es/group/blosc


Enjoy Data!
- The Blosc develop team
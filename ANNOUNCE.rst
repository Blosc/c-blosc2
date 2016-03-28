===============================================================
 Announcing C-Blosc 2.0.0a3
 A simplistic, compressed and fast data store library for C
===============================================================

What is new?
============

This is the third alpha release of Blosc2.  It introduces the concept
of contexts (for use in multi-threaded apps for example) via the new
blosc2_create_context() and blosc2_free_context() functions.  The
contexts can be used with the new blosc2_compress_ctx() and
blosc2_decompress_ctx().

Also important is the support for new Zstd codec
(https://github.com/Cyan4973/zstd).  This is a new compressor by Yann
Collet, the author of LZ4 and LZ4HC.  For details on Zstd, see this
nice intro:
http://fastcompression.blogspot.com.es/2015/01/zstd-stronger-compression-algorithm.html.

For more info, please see the release notes in:

https://github.com/Blosc/c-blosc2/blob/master/RELEASE_NOTES.rst



What is it?
===========

Blosc2 is a high performance data container optimized for binary data.
It builds on the shoulders of Blosc, the high performance
meta-compressor (http://www.blosc.org).

Blosc2 expands the capabilities of Blosc by providing a higher lever
container that is able to store many chunks on it (hence the
super-block name).  Also, it will add more compressors and filters
(e.g. a new delta filter is here already).


Download sources
================

The github repository is over here:

https://github.com/c-blosc2

Blosc is distributed using the MIT license, see LICENSES/BLOSC2.txt
for details.


Mailing list
============

There is an official Blosc mailing list at:

blosc@googlegroups.com
http://groups.google.es/group/blosc


Enjoy Data!

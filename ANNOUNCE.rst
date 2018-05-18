===============================================================
 Announcing C-Blosc 2.0.0a5
 A simplistic, compressed and fast data store library for C
===============================================================

What is new?
============

Support for Zstd dictionaries; this allows for (much) better compression
ratios when using small blocksizes, while improving compression speed.
The delta filter is using a XOR instead of a NEG for better numerical
stability.

Last but not least, ARM support has been greatly enhanced, as ARMv7 and
ARMv8 (specially AArch64) are supported out of the box.  Support for NEON
for shuffle and bitshuffle is there (but use the latter with caution, as
there are implementation flaws still).

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

Blosc is distributed using the BSD license, see LICENSES/BLOSC2.txt
for details.


Mailing list
============

There is an official Blosc mailing list at:

blosc@googlegroups.com
http://groups.google.es/group/blosc


Enjoy Data!

===============================================================
 Announcing C-Blosc 2.0.0-beta.2
 A simple, compressed and fast data store library for C
===============================================================

What is new?
============

This is the second beta version, and the frame format has been much improved and [documented (yay!)](https://github.com/Blosc/c-blosc2/blob/master/README_FRAME_FORMAT.rst).  Also, the metalayer-related functions are now updating the super-chunk instead of the frame.

In principle, C-Blosc2 should be backward compatible with C-Blosc, so you can start using it right away and slowly begin to use its new functionality, like the new filters, prefilters, super-chunks and frames.  See docs in:
https://blosc2.readthedocs.io

**IMPORTANT**: Please note that, even if the API has been declared frozen, that does not mean that Blosc2 is ready for production yet: internal structures can change, formats can change and most importantly, bugs can be normal at this stage.  So *do not assume* that your blosc2 data can be read with future versions.

For more info, please see the release notes in:

https://github.com/Blosc/c-blosc2/blob/master/RELEASE_NOTES.md

Also, there is blog post introducing the most relevant changes in Blosc2:

http://blosc.org/posts/blosc2-first-beta

What is it?
===========

Blosc2 is a high performance data container optimized for binary data.  It builds on the shoulders of Blosc, the high performance meta-compressor (https://github.com/c-blosc).

Blosc2 expands the capabilities of Blosc by providing a higher lever container that is able to store many chunks on it (hence the super-block name).  It supports storing data on both memory and disk using the same API.  Also, it adds more compressors and filters.

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
- The Blosc Development Team

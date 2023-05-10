# Announcing C-Blosc2 2.9.1
A fast, compressed and persistent binary data store library for C.

## What is new?

This a maintenance release for allowing the use of BTUNE by simply
activating the ``BTUNE_BALANCE`` environment variable.

C-Blosc2 should be backward compatible with C-Blosc, so you can start using
it right away and increasingly start to use its new functionality, like the
new filters, prefilters, super-chunks and frames.
See docs in: https://www.blosc.org/c-blosc2/c-blosc2.html

For more info, please see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

Also, there is blog post introducing the most relevant changes in Blosc2:

https://www.blosc.org/posts/blosc2-ready-general-review/

## What is it?

Blosc2 is a high performance data container optimized for binary data.
It builds on the shoulders of Blosc, the high performance meta-compressor
(https://github.com/Blosc/c-blosc).

Blosc2 expands the capabilities of Blosc by providing a higher lever
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

## Tweeter feed

Please follow @Blosc2 to get informed about the latest developments.


Enjoy Data!
- The Blosc Development Team

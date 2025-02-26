# Announcing C-Blosc2 2.17.0
A fast, compressed and persistent binary data store library for C.

## What is new?

This introduces some new features and improvements:

* New b2nd_copy_buffer2() function for copying buffers with typesizes
  larger than 255.  The previous b2nd_copy_buffer() function is now
  deprecated and will be removed in a future release.

* Support repeated values larger than 8-bit, also for n-dim arrays.
  This is useful for compressing arrays with large runs of repeated
  values, like in the case of images with large areas of the same color.

* Fix a leak in the pthreads emulation for Windows.  Fixes #647.
  Thanks to @jocbeh for the report and fix (#655).

* Update zstd to 1.5.7.  Thanks to Tom Birch.

For more info, please see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high performance data container optimized for binary data.
It builds on the shoulders of Blosc, the high performance meta-compressor
(https://github.com/Blosc/c-blosc).  Blosc2 is the next generation of Blosc,
an award-winning (https://www.blosc.org/posts/prize-push-Blosc2)` library
that has been around for more than a decade.

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

# Announcing C-Blosc2 2.17.1
A fast, compressed and persistent binary data store library for C.

## What is new?

Several fixes affecting uninitialized memory access and others:

* Fix uninitialized memory access in newly added unshuffle12_sse2 and unshuffle12_avx2 functions
* Fix unaligned access in _sw32 and sw32_
* Fix DWORD being printed as %s in sprintf call
* Fix warning on unused variable (since this variable was only being used in the linux branch)
* `splitmode` variable was uninitialized if goto was triggered

See PR #658.  Many thanks to @EmilDohne for this nice job.

For more info, see the release notes in:

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

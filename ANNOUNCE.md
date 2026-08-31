# Announcing C-Blosc2 3.3.3
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a bugfix and security release fixing bounds checking issues in BloscLZ.

Match length accumulation in the length-extension loop was unbounded, allowing
crafted chunks to trigger integer or pointer overflow. On 32-bit platforms
(and WebAssembly `wasm32`), pointer arithmetic wrapped around when destination
buffers landed high in memory, bypassing bounds guards and causing heap
out-of-bounds writes. On 64-bit platforms, excessively large inputs could trigger
signed `int32_t` overflow.

This is resolved by capping length accumulation early (`len > maxout`),
rewriting all pointer-addition bounds checks in safe subtraction form
(`len > op_limit - op`), and adding regression testing.

There are no API or format changes in this release.

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an
[award-winning library](https://www.blosc.org/posts/prize-push-Blosc2)
that has been around for more than a decade.

Blosc2 expands the capabilities of Blosc by providing a higher level
container that is able to store many chunks on it (hence the super-chunk name).
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

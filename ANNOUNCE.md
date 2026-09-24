# Announcing C-Blosc2 3.3.5
A fast, compressed, and persistent binary data store library for C.

## What is new?

This is a bugfix and maintenance release fixing robustness and security issues
discovered via OSS-Fuzz.

First, an integer overflow in the BloscLZ decompressor's long-match length
accumulation loop was fixed. A crafted input with excessive continuation bytes
could wrap a 32-bit length past `INT32_MAX` into negative values, bypassing bounds
checks and leading to a heap buffer overflow. Length bounds are now verified
prior to addition.

Second, input buffer size validation was added to the ZFP codec decompressors
(`zfp_acc_decompress`, `zfp_prec_decompress`, `zfp_rate_decompress`). Previously,
crafted frames with high metadata values could cause ZFP's bitstream reader to
read beyond buffer bounds. The required bitstream size is now validated against
the input length prior to decompression.

Third, a potential division-by-zero (floating-point exception) in
`frame_get_chunk()` and `frame_get_lazychunk()` when encountering crafted frames
with `chunksize <= 0` during special-value chunk retrieval was guarded.

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

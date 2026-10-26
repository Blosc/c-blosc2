# Announcing C-Blosc2 3.4.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This minor release introduces an opt-in writer for C-Blosc 1.x compatible chunks,
bounded-memory APIs for variable-length block decompression, alongside critical
bug fixes and robustness enhancements in DELTA filter decoding, blocksize tuning,
ZFP codec decompression, and threading.

First, an opt-in writer for Blosc1 (C-Blosc 1.x compatible) format chunks is
now available via `blosc2_ctx_set_blosc1_format()` and queryable with
`blosc2_ctx_get_blosc1_format()`. In this mode, chunks are written in format
version 2 with the 16-byte header and adhere to C-Blosc 1.x bitshuffle and
block-splitting rules, allowing them to be decompressed seamlessly by legacy
C-Blosc 1.x decoders. The `BLOSC_BLOSC1_COMPAT` environment variable also activates
this mode for `blosc1_compress()`.

Second, two new APIs were added for variable-length (VL) blocks:
`blosc2_vlblock_get_nbytes_ctx()` inspects a single block's uncompressed size
without allocating buffers or dictionaries, and `blosc2_vldecompress_block_into_ctx()`
decompresses a single variable-length block directly into a caller-owned buffer.
Both work with standard and lazy VL chunks.

Third, multiple critical bug fixes have landed:
- Fixed DELTA filter decoding corrupted data during partial reads, slicing, or
  maskout (Blosc/python-blosc2#740) by disabling the b2nd compact read path for
  DELTA, always unmasking reference block 0, and falling back to full chunk
  decompression in `_blosc_getitem` and `blosc2_decompress_block_ctx`.
- Fixed a bug where chunk headers stored requested rather than tuner-adjusted
  block sizes when not divisible by the typesize, causing decompression errors (#815).
- Fixed block size selection when typesize exceeds `BLOSC_MAX_TYPESIZE`.
- Fixed reading format version 2 bit-shuffled Blosc1 chunks that have leftover trailing
  bytes (#812).
- Validated that split blocks fully cover blocksize on decompression, rejecting
  incompletely covered blocks with `BLOSC2_ERROR_DATA` (backported from C-Blosc #407).

Fourth, robustness and performance were strengthened:
- Hardened the ZFP codec plugin against heap-buffer-overflow over-reads during
  unaligned 64-bit word reads by decompressing from a zero-padded buffer, and
  validated metalayer shapes and bounds in `zfp_getcell()`.
- Hardened `b2nd_get_slice_nchunks` bounds clamping and allocation arithmetic.
- Single-block operations remain serial when `nthreads > 1`, deferring worker thread
  backend initialization and avoiding unused thread overhead. Worker queues also cap
  active worker entries to the block count.

There are no breaking API or format changes in this release.

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

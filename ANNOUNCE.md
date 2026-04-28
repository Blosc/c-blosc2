# Announcing C-Blosc2 3.0.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release is officially announcing the production-ready release of
C-Blosc2 3.0.0. This is the culmination of a streamlined development
process leading to variable-length chunks and blocks (VL-blocks),
a new threading execution model, and improved dictionary compression.
As usual, it also includes a number of bug fixes, cleanups, and
improvements in security, robustness, and build system modernization.

In short, the 3.0.0 release makes Blosc2 more scalable, flexible, and secure.

Highlights:

* Support for variable-length chunks and variable-length blocks.  This is
  especially useful for workloads made of naturally variable-size pieces
  of data, like strings, records, JSON fragments, or other irregular payloads
  that previously had to be padded, split awkwardly, or stored as independent
  chunks.  The new layout keeps these pieces grouped together while still
  making them individually recoverable.

  With this, there are new public APIs for VL-block chunks:
  `blosc2_vlcompress_ctx()`, `blosc2_vldecompress_ctx()`,
  `blosc2_vlchunk_get_nblocks()`, `blosc2_vldecompress_block_ctx()`, and
  `blosc2_schunk_get_vlblock()`.  Lazy loading also works with VL-block chunks,
  so individual blocks can be fetched on demand without materializing the whole
  chunk first.

* The chunk and cframe formats have been extended to represent variable chunk
  sizes, VL-block chunks, and dictionary usage more explicitly.  Forward
  compatibility checks were tightened as part of this work, and regular chunks
  keep their previous stable format version while VL-block chunks use a new one.

* Dictionary compression has been expanded and improved:
  `use_dict` now works with LZ4 and LZ4HC in addition to ZSTD, the dictionary
  state is preserved correctly across chunk compression/decompression, and the
  frame metadata now round-trips the dictionary setting.  There is also a new
  minimum useful dictionary threshold to avoid training or using dictionaries
  that are too small to help.

* The necessary changes for accommodating all these improvements have been fully
  documented in README_CHUNK_FORMAT.md and README_CFRAME_FORMAT.md.  Again,
  care has been taken to ensure that the chunk and frame formats are backward
  compatible with previous versions of C-Blosc2.

* The internal parallel execution model now uses a shared managed thread pool
  instead of private worker pools per context.  This reduces redundant thread
  creation, avoids idle thread accumulation when many contexts coexist, and
  improves behavior in downstream workloads such as python-blosc2—especially in
  applications that create large numbers of arrays or contexts over time.

* A broad set of robustness and security hardening fixes landed across frame,
  schunk, lazy-chunk, metadata, mmap, and getitem paths, including tighter
  bounds checking, malformed-input rejection, integer-overflow prevention, and
  many new regression tests.  As a result, version 3.0.0 is much stronger
  for production usage.

* CMake dependency handling was modernized: `lz4`, `zlib-ng`, `zstd`, and the
  optional ZFP plugin are now obtained from external packages or via
  `FetchContent` with pinned upstream versions, instead of vendored source
  copies.  As a result, `blosclz` is now the only codec still vendored in-tree,
  making the project leaner and easier to maintain.

* Static-package install/export support in CMake was improved, and embedded
  third-party headers are now installed under `blosc2/thirdparty/...` to reduce
  the risk of include-name collisions.

* The unmaintained Intel IPP integration has been removed.

For more info, see the release notes in:

https://github.com/Blosc/c-blosc2/blob/main/RELEASE_NOTES.md

## What is it?

Blosc2 is a high-performance data container optimized for binary data.
Blosc2 is the next generation of Blosc, an [award-winning]
(https://www.blosc.org/posts/prize-push-Blosc2) library that has been
around for more than a decade.

Blosc2 expands the capabilities of Blosc by providing a higher level
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


Enjoy Data!
- The Blosc Development Team

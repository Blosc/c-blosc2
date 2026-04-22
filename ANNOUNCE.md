# Announcing C-Blosc2 3.0.0 RC2
A fast, compressed and persistent binary data store library for C.

## What is new?

This release builds on the big 3.x features introduced in RC1—like
variable-length chunks & blocks (VL-blocks) and improved dictionary
compression —and adds an important round of polish before
the final 3.0.0 release.

In short, RC2 makes Blosc2 more scalable, more robust, and easier to integrate
in modern build environments.

Highlights include:

* `blosc2_get_slice_nchunks()`, `schunk_get_slice_nchunks()`, and
  `b2nd_get_slice_nchunks()` now return `int64_t` instead of `int`, removing
  an artificial `INT_MAX` limit for large slices.

* The internal parallel execution model now uses a shared managed thread pool
  instead of private worker pools per context.  This reduces redundant thread
  creation, avoids idle thread accumulation when many contexts coexist, and
  improves behavior in downstream workloads such as python-blosc2—especially in
  applications that create large numbers of arrays or contexts over time.

* A broad set of robustness and security hardening fixes landed across frame,
  schunk, lazy-chunk, metadata, mmap, and getitem paths, including tighter
  bounds checking, malformed-input rejection, integer-overflow prevention, and
  many new regression tests.  RC2 is a much stronger candidate for production
  testing as a result.

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

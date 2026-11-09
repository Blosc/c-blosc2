# Announcing C-Blosc2 3.6.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release focuses on robustness, security, and thread safety by hardening
input and frame validation, eliminating memory corruption vulnerabilities on
damaged or malformed frames, fixing multithreading races, and restoring thread
pool scaling performance for jobs with small block counts.

Key highlights include:

- **Robustness and Frame Validation**:
  - Consistently serialize super-chunks and b2nd arrays with chunks as stored regardless of
    storage layout, eliminating double-postfiltering bugs upon deserialization and drastically
    accelerating filter serialization (#858).
  - Fixed an out-of-bounds indexing and buffer validation vulnerability in
    orthogonal selections (`b2nd_get_orthogonal_selection()` / `b2nd_set_orthogonal_selection()`) (#864).
  - Prevented segmentation faults when copying damaged frames where chunk loading
    fails partway through (#869).
  - Hardened frame reading against corrupt files: prevented improper freeing of mmap
    buffers in lazy chunk reads, prevented NULL pointer dereferences on failed reads,
    validated block sizes within chunks, and guarded against 64-bit stride arithmetic overflows (#871).
  - Validated that super-chunk chunk counts match b2nd metadata upon opening (#871).
- **Performance and SIMD Optimizations**:
  - Accelerated run-length detection in `get_run()` comparing 64 bytes per iteration
    via SSE2 and ARM NEON intrinsics (5.5%–8% faster compression on repetitive data) (#874).
  - Vectorized the last transposition step of bitunshuffle for 1- and 2-byte elements
    using AVX-512, AVX2, and SSE2 bit-matrix transpositions (#867).
  - Retained up to 2 unused thread pools across context frees to eliminate thread spawn
    and join overhead for short-lived contexts (#865).
- **Concurrency and Thread Safety**:
  - Thread-safe lazy initialization of the library, dynamic plugins, and `blosc2_list_compressors()`,
    eliminating races, deadlocks, and segfaults when threads call Blosc concurrently without prior `blosc2_init()` (#875).
  - Ensured thread-safe lazy initialization for shuffle/bitshuffle operations across
    multiple threads using `pthread_once` (#872).
  - Restored thread pool worker wakeup performance in `parallel_blosc()` by broadcasting
    after releasing the pool mutex, resolving lock contention regressions on few-block jobs (#878).
- **Bug and Leak Fixes**:
  - Prevented crashes when loading broken dynamic plugins with missing or null symbols (#873).
  - Ensured complete cleanup of contexts, preparams, postparams, and mutexes across all
    error return paths in `blosc2_create_cctx()` and `blosc2_create_dctx()` (#868).
  - Reused thread-local scratch buffers in `b2nd_get_slice()` to avoid glibc heap trim
    and page-fault churn during slice operations (#866).
  - Skipped redundant advisory start hints when opening unmapped stdio frame files (#877).

Special thanks to Fabian Isensee for his outstanding contributions and bug fixes!

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

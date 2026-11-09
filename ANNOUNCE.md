# Announcing C-Blosc2 3.6.0
A fast, compressed, and persistent binary data store library for C.

## What is new?

This release brings major enhancements across robustness, concurrency, multiprocess
safety, and slicing performance. Highlights include:

- **New Features & APIs**:
  - `b2nd_get_slice_cbuffer_into()`: Decompresses ND slices directly into a sub-box of
    an existing larger buffer without extra copies, temporaries, or zero-fill (#845).
  - Consistent frame serialization: Super-chunks and b2nd arrays serialize raw chunks
    as stored regardless of layout, fixing double-postfilter application on reload (#858).

- **Robustness and Frame Validation**:
  - Fixed out-of-bounds indexing and buffer validation vulnerabilities in orthogonal
    selections (`b2nd_get_orthogonal_selection()` / `b2nd_set_orthogonal_selection()`) (#864).
  - Prevented segmentation faults when copying damaged frames where chunk loading
    fails partway through (#869).
  - Hardened frame reading against corrupt files: prevented improper freeing of mmap
    buffers in lazy chunk reads, prevented NULL pointer dereferences on failed reads,
    validated block sizes within chunks, and guarded against 64-bit stride arithmetic overflows (#871).
  - Validated that super-chunk chunk counts match b2nd metadata upon opening (#871).
  - Prevented crashes when loading broken dynamic plugins with missing or null symbols (#873).
  - Ensured complete cleanup of contexts, preparams, postparams, and mutexes across all
    error return paths in `blosc2_create_cctx()` and `blosc2_create_dctx()` (#868).
  - Kept compression/decompression filters away from `vlmetalayer` content (#857).

- **Concurrency, Multiprocessing & Fork Safety**:
  - Thread-safe lazy initialization of the library, dynamic plugins, and `blosc2_list_compressors()`,
    eliminating races, deadlocks, and segfaults when threads call Blosc concurrently without prior `blosc2_init()` (#875).
  - Ensured thread-safe lazy initialization for shuffle/bitshuffle operations across
    multiple threads using `pthread_once` (#872).
  - Added full `fork()` safety for shared thread pools and reader caches, allowing safe usage
    in multiprocessing/forked child processes (#852).
  - Dedicated prefetch thread shares per active slice to prevent read-ahead thread starvation (#854).
  - Restored thread pool worker wakeup performance in `parallel_blosc()` by broadcasting
    after releasing the pool mutex, resolving lock contention regressions on few-block jobs (#878).
  - Retained up to 2 unused thread pools across context frees to eliminate thread spawn
    and join overhead for short-lived contexts (#865).

- **Performance and SIMD Optimizations**:
  - Accelerated run-length detection in `get_run()` comparing 64 bytes per iteration
    via SSE2 and ARM NEON intrinsics (5.5%–8% faster compression on repetitive data) (#874).
  - Vectorized the last transposition step of bitunshuffle for 1- and 2-byte elements
    using AVX-512, AVX2, and SSE2 bit-matrix transpositions (#867).
  - Merged contiguous trailing dimensions in orthogonal selection copies, speeding up
    channels-last image selections ~4.7x (#861).
  - Merged contiguous dimensions in b2nd box copies and zeroing for faster multidimensional copies (#851).
  - Accelerated short row copies in `b2nd_copy_buffer2()` with fixed-size moves instead of `memcpy()` (#846).
  - Skipped decompression of zero and uninitialized special chunks in b2nd slice reads (#844).
  - Reused thread-local scratch buffers in bitshuffle (#860) and `b2nd_get_slice()` (#866) to
    eliminate heap trimming churn and page faults.
  - Kept serial thread context per OS thread instead of reallocating per context (#849).
  - Reused chunk compression buffers across writes in `compress_chunks_into_frame()` (#850).
  - Closed frame slice holes once across the entire write instead of per chunk (#847).
  - Optimized prefetch cache checking to inspect only needed blocks of partly read chunks (#862).
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

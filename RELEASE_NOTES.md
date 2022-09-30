Release notes for C-Blosc2 2.4.3
================================


Changes from 2.4.2 to 2.4.3
===========================

#XXX version-specific blurb XXX#


Changes from 2.4.1 to 2.4.2
===========================

* Fixed BLOSC1_COMPAT mode.  If the symbol `BLOSC1_COMPAT` is defined, one should actually get a correct map for the BLOSC1 API now.  Thanks to @MehdiChinoune for pointing this out.

* Optimizations for `blosc2_schunk_get_slice_buffer()`.  Now more caution has been put in avoiding memcpy's as much as possible.

* Now `blosc2_set_nthreads()` also modifies the schunk compression and decompression defaults.

* Several other fixes, specially when converting schunks to cframe format.


Changes from 2.4.0 to 2.4.1
===========================

* New `blosc2_schunk_avoid_cframe_free()` for avoiding the free of a cframe when destroying a super-chunk.  Mainly useful for situations where you build a super-chunk out of an existing cframe, so you don't want it to be freed automatically.


Changes from 2.3.1 to 2.4.0
===========================

* New `blosc2_schunk_get_slice_buffer()` and `blosc2_schunk_set_slice_buffer()` functions for getting and setting slices from/to a super-chunk.


Changes from 2.3.0 to 2.3.1
===========================

* Support for negative values for BLOSC_TRUNC_PREC filter.  Negatives values mean reduce mantissa precision bits, whereas positive values mean keep precision bits.

* Re-add the check for small buffers and a new test (see https://github.com/Blosc/python-blosc2/issues/46).

* Make `static` a couple of funcs to avoid collisions.  This can be useful in case someone tries to compile both C-Blosc and C-Blosc2 in the same name space (however, don't do that unless you know what you are doing; better use dynamic libraries which allow for a much better name space separation).


Changes from 2.2.0 to 2.3.0
===========================

* [API change] In order to allow to compile with both C-Blosc and C-Blosc2 libraries, a new API has been created for the symbols and function names that had collisions.  Here are the changed symbols and functions:

  * Blosc2 symbols that take different values than in Blosc1:
    - BLOSC_VERSION_MAJOR -> BLOSC2_VERSION_MAJOR
    - BLOSC_VERSION_MINOR -> BLOSC2_VERSION_MINOR
    - BLOSC_VERSION_RELEASE -> BLOSC2_VERSION_RELEASE
    - BLOSC_VERSION_STRING -> BLOSC2_VERSION_STRING
    - BLOSC_VERSION_DATE -> BLOSC2_VERSION_DATE
    - BLOSC_MAX_OVERHEAD -> BLOSC2_MAX_OVERHEAD
    - BLOSC_MAX_BUFFERSIZE -> BLOSC2_MAX_BUFFERSIZE

  * Original Blosc1 API that takes the `blosc1_` prefix:
    - blosc_compress -> blosc1_compress
    - blosc_decompress -> blosc1_decompress
    - blosc_getitem -> blosc1_getitem
    - blosc_get_compressor -> blosc1_get_compressor
    - blosc_set_compressor -> blosc_set_compressor
    - blosc_cbuffer_sizes -> blosc1_cbuffer_sizes
    - blosc_cbuffer_validate -> blosc1_cbuffer_validate
    - blosc_cbuffer_metainfo -> blosc1_cbuffer_metainfo
    - blosc_get_blocksize -> blosc1_get_blocksize
    - blosc_set_blocksize -> blosc1_set_blocksize
    - blosc_set_splitmode -> blosc1_set_splitmode

  * API that has been migrated to blosc2_ prefix
    - blosc_init -> blosc2_init
    - blosc_destroy -> blosc2_destroy
    - blosc_free_resources -> blosc2_free_resources
    - blosc_get_nthreads -> blosc2_get_nthreads
    - blosc_set_nthreads -> blosc2_set_nthreads
    - blosc_compcode_to_compname -> blosc2_compcode_to_compname
    - blosc_compname_to_compcode -> blosc2_compname_to_compcode
    - blosc_list_compressors -> blosc2_list_compressors
    - blosc_get_version_string -> blosc2_get_version_string
    - blosc_get_complib_info -> blosc2_get_complib_info
    - blosc_cbuffer_versions -> blosc2_cbuffer_versions
    - blosc_cbuffer_complib -> blosc2_cbuffer_complib

  It is recommended to migrate to the new API as soon as possible.  In the meanwhile, you can still compile with the previous API (corresponding to C-Blosc2 pre-2.3.0), by defining the `BLOSC1_COMPAT` symbol in your C-Blosc2 app (before including the 'blosc2.h' header).

* Fixed some issues in converting from super-chunks to frames and back.  Now it is possible to do a rountrip without (known) problems.

* LZ4 codec has been bumped to 1.9.4.


Changes from 2.1.1 to 2.2.0
===========================

* Added new `blosc2_schunk_open_offset()` and `blosc2_schunk_append_file()` functions for being able to open a super-chunk inside of file, and append a super-chunk to the end of an existing file.  See https://github.com/Blosc/c-blosc2/pull/409.

* Protect the update of a field in compression/decompression context.  Fixes issues when compressing/decompressing super-chunks in multi-thread mode.

* Fix issue when inserting a chunk in a super-chunk.  See https://github.com/Blosc/c-blosc2/pull/408.

* Fix issue when appending a special chunk in a super-chunk.  See https://github.com/Blosc/c-blosc2/pull/407.

* Optimized the blockshape calculation when using the ZFP plugin in `BLOSC_CODEC_ZFP_FIXED_RATE` mode.  See https://github.com/Blosc/c-blosc2/pull/406.

* New `blosc2_unidim_to_multidim` and `blosc2_multidim_to_unidim` which are useful for codecs that are meant to deal with multidimensional arrays (like ZFP).

* Another round of squashing warnings has been carried out.  Thanks to Marta Iborra.

* Added locks in situations where different threads were trying to update the same variable.  Thanks to Marta Iborra (and helgrind!).

* Use proper types for native zlib-ng interface.  This allows for linking with zlib-ng native API. Thanks to Klaus Zimmermann.

Changes from 2.1.0 to 2.1.1
===========================

* Fixed a nasty bug introduced in 2.1.0 that affects to the blosclz codec in ARM arch. See https://github.com/Blosc/c-blosc2/issues/392.

* Added support for external zlib-ng (see https://github.com/Blosc/c-blosc2/pull/393).  Thanks to Mark Harfouche.

* Fixed more compiler warnings (https://github.com/Blosc/c-blosc2/pull/394).  Now C-Blosc2 should be relatively free of them.

* Fixed more fuzzer issues.


Changes from 2.0.4 to 2.1.0
===========================

* Support of the [ZFP lossy codec](https://zfp.readthedocs.io/) as a plugin.  This is mainly meant for compressing multidimensional arrays of floating point data, so it is better used in combination with [Caterva](https://github.com/Blosc/caterva).  More info at our blog: https://www.blosc.org/posts/support-lossy-zfp/. Thanks to Oscar Guiñon and Francesc Alted.

* Massive cleanup of warnings.  Thanks to Marta Iborra.

* Fixed some bugs in test updates and insertions in 64-bit super chunks.  Thanks to Francesc Alted.

* Fixed the build on FreeBSD/powerpc64le.  Thanks to @pkubaj.

* Updated internal zstd sources to 1.5.2.

* Updated internal zlib-ng to 2.0.6.


Changes from 2.0.3 to 2.0.4
===========================

* Restored support for type size that is not a divisor of a buffer size for more info.  See #356.

* Implemented a `blosc2_rename_urlpath`, a portable function to rename a file or a full directory.  See #355.

* Several improvements for packaging.  See #354, #357, #359.

* Fixed a bug in BloscLZ codec (bumped to 2.5.1).


Changes from 2.0.2 to 2.0.3
===========================

* Improved BloscLZ codec (bumped to 2.5.0) for achieving better compression ratios on data with high entropy.  Although in general LZ4 still does a better job in this scenario, *BloscLZ* can sometimes achieve better compression ratios.

* Added `blosc2_vlmeta_delete()` for removing vlmeta data.  Thanks to Marta Iborra.

* Update pkg-config file to support blosc2. Fixes #236.  Thanks to Håvard Flaget Aasen.

* Build system: Change hard coded library path with `CMAKE_INSTALL_LIBDIR` CMake variable.  Thanks to Håvard Flaget Aasen.


Changes from 2.0.1 to 2.0.2
===========================

* Fixed data chunk memory leaks in frame (see #335).

* Fixed blosc2_stdio_open never returns NULL if it cannot open file.

* Standardized places for headers in blosc/ and include/ dirs.

* `nthreads` is int16_t everywhere in the API. Fixes #331.

* Add blosc2_remove_urlpath function (see #330).

* Fixed a bug when a lazy_chunk was created from a small, memcpyed chunk.
  (see #329).
  
* Fixed many issues in documentation (see #333).


Changes from 2.0.0 to 2.0.1
===========================

* The `blosc2_schunk_fill_special` function was not exported,
  so not in the shared library. This has been fixed; see #328.
  Thanks to Mark Kittisopikul.


Changes from 2.0.0.rc2 to 2.0.0 (final)
=======================================

* Now Blosc is always compiled with LZ4. See #324.

* Implemented a system to register plugins (PR #314).
  See our blog at: https://www.blosc.org/posts/registering-plugins.

* Added Blosc Lite version. Just activate `BUILD_LITE` cmake option with:
  `-DBUILD_LITE`.  See #316.

* You can deactivate the plugins by setting cmake option `BUILD_PLUGINS` to OFF.

* Created `include` folder. See #310.

* Moved codecs-registry.h and filters-registry.h to include/blosc2. See #325.

* Fix error in endian-handler function affecting frames metadata. See #320.

* Improved tolerance to Win64 workflows failure. See #319.

* zlib-ng updated to 2.0.5.

* New COMPILING_WITH_WHEELS.rst doc added.


Changes from 2.0.0.rc.1 to 2.0.0.rc2
====================================

* New compatibility with MinGW32/64.  See #302.

* Improved support for AArch64 (aka ARM64), ARMv7l, ARMv6l and powerpc64le.
  See #306. Thanks to Alexandr Romanenko, Mark Kittisopikul and Mosè Giordano
  from the Julia packaging team for their help.

* BloscLZ updated to 2.4.0.  Aligned access in ARM has been enabled, as well
  as other performance improvements.  Expect much better performance,
  specially on ARM platforms (like Apple Silicon).

* Zstd sources updated to 1.5.0.

* zlib-ng sources updated to 2.0.3.


Changes from 2.0.0-beta.5 to 2.0.0.rc.1
=======================================

* [API change] `blosc2_decompress_ctx()` gets a new `srcsize`
 parameter to ensure that it does not read past the end
 of the provided buffer.  See #144.  Thanks to Nathan Moinvaziri
 (@nmoinvaz).

* [BREAKING CHANGE] The format for frames has changed and
  BLOSC2_VERSION_FRAME_FORMAT is now set to 2.  There is no attempt to support
  previous formats, but there will probably be backward compatibility support
  starting from version 2 on.

* New functionality for updating, inserting and deleting chunks in a super-chunk.

* Support for special values. Large sequences of repeated values can be represented
  with an efficient, simple and fast run-length representation, without the need to use
  regular codecs.

* Internal Zstd sources updated to 1.4.9.

* Internal LZ4 sources updated to 1.9.3.

* Internal zlib support is provided now by new zlib-ng 2.0.2 (replacing miniz).

* The support for Snappy codec has been completely removed.  Snappy is a C++
  library, which is not good for a library that aims to be fully pure C.
  Snappy was removed from sources in C-Blosc(1) some years ago, so there
  should not be a lot of data compressed with Blosc/Snappy out there (and
  for the existing ones, a transcoding is always possible using C-Blosc(1)).

* The Lizard codec has been removed.  Lizard is a pretty good one, but it
  looks like it is not timely maintained.  Zstd/Zlib can cover its place pretty
  nicely.

* The split of blocks only happens for BLOSCLZ and LZ4 codecs.  All
  the rest are not split at all.

* Public APIs for frames have been removed.  Frames should be considered an
  storage detail, so having them accessible publicly should only bring
  unnecessary cognitive load.  Care have been carried out so as to ensure
  the same functionality via the super-chunk (schunk) API.

* [FORMAT] New *sparse frame* format for on-disk I/O.  This allows for storing
  data chunks in super-chunks in separate files inside a directory.  The way
  to generate sparse frames is via `storage.contiguous=false` and
  `storage.urlpath=dirname`.  See `README_SFRAME_FORMAT.rst` for details.


Changes from 2.0.0-beta.4 to 2.0.0.beta5
========================================

* The prefilter functionality has been introduced and declared stable.
  With that, you can callback arbitrary functions previous to any filter.
  This can be useful for performing (parallel) computations on chunks.
  For an example of use, see `tests/test_prefilter.c`.

* New blosc2_set_maskout() function to avoid decompressing blocks.  This
  can be handy when it is not needed to decompress all the blocks in a
  chunk. This should be always called before blosc2_decompress_ctx() and
  its effect is reset to the default (decompress all blocks) after that.

* New ALTIVEC optimizations for Power architecture.  These include support
  for both shuffle and bitshuffle filters.  For details, see
  https://github.com/Blosc/c-blosc2/pull/98.  Thanks to Jerome Kieffer
  and ESRF for making this happen.

* New frame_from_bframe() function for getting a `blosc2_frame`
  out of an in-memory serialized frame.

* Zstd codec updated to 1.4.4.


Changes from 2.0.0-beta.3 to 2.0.0-beta.4
=========================================

* New pluggable threading backend.  Instead of having Blosc use its own
  thread pool, you can instead call
  `blosc2_set_threads_callback(threads_callback, callback_data)` to install
  your own threading backend.  This gives Blosc the possibility to use the
  same threading mechanism as one you are using in the rest of your program
  (e.g. OpenMP or Intel TBB), sharing the same threads, rather than starting
  its own threads that compete with yours for the CPU cores. See #PR 81.
  Thanks to Steven G. Johnson.

* The endianness of the platform that is writing the data in chunks is stored
  now in the headers of the chunks.  This info is not used yet, but this
  should allow a good hint for implementing format compatibility among
  platforms with different endianness in other layers.  See PR #84. 

* Fixed a nasty bug that prevented frames to go more than 2 GB in size.

* Added a cache for on-disk offsets.  This accelerates the reading of slices
  from disk quite a lot (up to 50% with my benchmarks).

* Zstd codec upgraded from 1.4.0 to 1.4.3.

Changes from 2.0.0-beta.2 to 2.0.0-beta.3
=========================================

* Quick release to fix that beta.2 was tagged on top of a branch, not master.

* The msgpack trailer now properly starts with `0x90 + 4` value, not plain
  wrong `0x09 + 4`.

* Trailer version bumped to 1.


Changes from 2.0.0-beta.1 to 2.0.0-beta.2
========================================

* A new `vlmetalayers` chunk in `schunk` allows to store arbitrary meta-information
  that is up to the user.  If the `schunk` has an attached `frame`, the later
  will be updated accordingly too.  For more info, see PR #74 and docstrings of
  new `blosc2_update_vlmetalayer()` and `blosc2_get_vlmetalayer()` functions.

* Metalayers must now be attached to super-chunks, not frames.  The reason is
  that frames are increasingly treated as a storage specifier (in-memory or
  disk now, but can be other means in the future), whereas the actual API for
  I/O (including metainfo) goes into super-chunks.  See PR #75.

* New frame format documented in
  [README_CFRAME_FORMAT.rst](README_CFRAME_FORMAT.rst). Remember that the frame
  format is not written in stone yet, so some changes may be introduced before
  getting out of beta.

* BREAKING CHANGE: the format for frames has changed and
  BLOSC2_VERSION_FRAME_FORMAT is now set to 1.  There is no attempt to support
  previous formats, but there will probably be backward compatibility support
  starting from version 1 on.

* BREAKING CHANGE: the next APIs have been renamed:
  + blosc2_frame_has_metalayer -> blosc2_has_metalayer
  + blosc2_frame_add_metalayer -> blosc2_add_metalayer
  + blosc2_frame_update_metalayer -> blosc2_update_metalayer
  + blosc2_frame_metalayer -> blosc2_get_metalayer

  Although the API was declared stable in beta.1, the fact that metalayers are
  attached now to super-chunks directly, made this change completely necessary.

* BREAKING CHANGE: the next symbols have been renamed:
  + BLOSC_CPARAMS_DEFAULTS -> BLOSC2_CPARAMS_DEFAULTS
  + BLOSC_DPARAMS_DEFAULTS -> BLOSC2_DPARAMS_DEFAULTS


Changes from 2.0.0a5 to 2.0.0-beta.1
====================================

* The library is called now `blosc2` and not `blosc` anymore.  This is necessary
  so as to prevent collisions with existing `blosc` deployments.

* The `make install` now install all the necessary requirements out-of-the-box.

* Use Intel IPP's LZ4Safe when compressing/decompressing: this provides better
  compression ratios and speed (in some cases).  It is activated automatically
  if Intel IPP is found in the system, but you can always disable it with:
  `cmake -DDEACTIVATE_IPP=ON`

* BREAKING CHANGE: the next API have been made private:
  + blosc2_frame_append_chunk -> frame_append_chunk
  + blosc2_frame_get_chunk -> frame_get_chunk
  + blosc2_frame_decompress_chunk -> frame_decompress_chunk

  Now the appending and retrieval of data in frames should always be made via the frame-backed super-chunk API. The idea is to deduplicate the I/O primitives as much as possible, and the super-chunks are the logical way for doing this.

* BREAKING CHANGE: the next APIs have been renamed:
  + blosc2_get_cparams - > blosc2_schunk_get_cparams
  + blosc2_get_dparams - > blosc2_schunk_get_dparams

* Internal BloscLZ sources bumped to 2.0.0.

* Internal LZ4 sources bumped to 1.9.1.

* Internal Zstd sources bumped to 1.4.0.


Changes from 2.0.0a4 to 2.0.0a5
===============================

- Delta filter now implemented as a XOR instead of a NEG for better numerical
  stability.

- Zstd dictionaries are supported.  This allows for (much) better compression
  ratios when using small blocksizes, while improving compression speed.

- Performance upgrades in the BloscLZ codec, specially for decompression
  in ARM architectures.

- Preliminary support for Neon instruction set in ARM CPUs.  Shuffle seems
  to work well, but still some issues with bitshuffle.

- Internal LZ4 sources bumped to 1.8.2.

- Internal Zstd sources bumped to 1.3.4.


Changes from 2.0.0a3 to 2.0.0a4
===============================

- New filter pipeline designed to work inside a chunk.  The pipeline has a
  current capacity of 5 slots, and is designed to apply different filters
  sequentially over the same chunk.  For this, a new extended header for the
  chunk has been put in place (see README_HEADER.rst).

- New delta filter meant to work inside a chunk.  Previously delta was
  working inside a super-chunk, but the new implementation is both faster and
  simpler and gets better compression ratios (at least for the tested datasets).

- New re-parametrization of BloscLZ for improved compression ratio
  and also more speed (at least on modern Intel/AMD CPUs).  Version
  for internal BloscLZ codec bumped to 1.0.6.

- Internal zstd sources bumped to 1.3.0.

- The BLOSC2_MAX_OVERHEAD symbol is always 32 bytes, not 16 as in Blosc1.
  This is needed in order to allow buffers larger than 32 bits (31 actually :).


Changes from 2.0.0a2 to 2.0.0a3
===============================

* New blosc2_create_context() and blosc2_free_context() for creating
  contexts so that Blosc can be used from threaded applications
  without the global lock. For now contexts can be only used from
  blosc2_compress_ctx() and blosc2_decompress_ctx() (see below).

* The blosc_compress_ctx() and blosc_decompress_ctx() have been
  replaced by blosc2_compress_ctx() and blosc2_decompress_ctx() that
  do accept actual contexts.

* Added support for new Zstd codec (https://github.com/Cyan4973/zstd).
  This is a new compressor by Yann Collet, the author of LZ4 and
  LZ4HC.  For details on Zstd, see this nice intro:
  http://fastcompression.blogspot.com.es/2015/01/zstd-stronger-compression-algorithm.html.

* The blosc2_append_chunk() has been removed.  This is this because an
  existing chunk may not fulfill the sequence of filters in super
  header.  It is best that the user will use blosc2_schunk_append_buffer()
  and compress it internally.

* The split of blocks only happens for BLOSCLZ and SNAPPY codecs.  All
  the rest are not split at all.  This allows for faster operation for
  the rest of codecs (most specially zstd).

* The internal zlib 1.2.8 sources have been replaced by the miniz
  library, which is meant to be fully compatible with Zlib, but much
  smaller and besides tends to be a bit faster.  Also, miniz is
  preferred to an external Zlib.

* The internal Snappy sources have been removed.  If the Snappy library
  is found, the support for it is still there.

* Internal LZ4 sources upgraded to 1.7.1.


Changes from 2.0.0a1 to 2.0.0a2
===============================

* The delta filter runs inside of the compression pipeline.

* Many fixes for the delta filter in super-chunks.

* Added tests for delta filter in combination with super-chunks.

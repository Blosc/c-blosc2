 Release notes for C-Blosc2 2.0.0.beta.6 (sixth beta)
======================================================


Changes from 2.0.0-beta.5 to 2.0.0.beta6
========================================

* [API change] `blosc2_decompress_ctx()` gets a new `srcsize`
 parameter to ensure that it does not read past the end
 of the provided buffer.  See #144.  Thanks to Nathan Moinvaziri
 (@nmoinvaz).

* [BREAKING CHANGE] The format for frames has changed and
  BLOSC2_VERSION_FRAME_FORMAT is now set to 2.  There is no attempt to support
  previous formats, but there will probably be backward compatibility support
  starting from version 2 on.

* Internal Zstd sources updated to 1.4.5.


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

* New blosc2_frame_from_sframe() function for getting a `blosc2_frame`
  out of an in-memory serialized frame.

* Zstd codec updated to 1.4.4.


Changes from 2.0.0-beta.3 to 2.0.0-beta.4
=========================================

* New pluggable threading backend.  Instead of having Blosc use its own
  thread pool, you can instead call
  `blosc_set_threads_callback(threads_callback, callback_data)` to install
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

* A new `usermeta` chunk in `schunk` allows to store arbitrary meta-information
  that is up to the user.  If the `schunk` has an attached `frame`, the later
  will be updated accordingly too.  For more info, see PR #74 and docstrings of
  new `blosc2_update_usermeta()` and `blosc2_get_usermeta()` functions.

* Metalayers must now be attached to super-chunks, not frames.  The reason is
  that frames are increasingly treated as a storage specifier (in-memory or
  disk now, but can be other means in the future), whereas the actual API for
  I/O (including metainfo) goes into super-chunks.  See PR #75.

* New frame format documented in
  [README_FRAME_FORMAT.rst](README_FRAME_FORMAT.rst). Remember that the frame
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

- Internal LZ4 sources bumbed to 1.8.2.

- Internal Zstd sources bumbed to 1.3.4.


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

- Internal zstd sources bumbed to 1.3.0.

- The BLOSC_MAX_OVERHEAD symbol is always 32 bytes, not 16 as in Blosc1.
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

* The internal snappy sources have been removed.  If snappy library
  is found, the support for it is still there.

* Internal LZ4 sources upgraded to 1.7.1.


Changes from 2.0.0a1 to 2.0.0a2
===============================

* The delta filter runs inside of the compression pipeline.

* Many fixes for the delta filter in super-chunks.

* Added tests for delta filter in combination with super-chunks.

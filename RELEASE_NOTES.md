Release notes for C-Blosc2
==========================

Changes from 2.15.1 to 2.15.2
=============================

#XXX version-specific blurb XXX#


Changes from 2.15.0 to 2.15.1
=============================

* Do not pass `-m` flags when compiling `shuffle.c`. This prevents the
  compiler from incidentally optimizing the code called independently
  of the runtime CPU check to these instruction sets, effectively
  causing `SIGILL` on other CPUs.  Fixes #621.  Thanks to @t20100 and @mgorny.

* Internal LZ4 sources bumped to 1.10.0.

* Allow direct loading of plugins by name, without relying on
  the presence of python. Thanks to @boxerab.

* Add `b2nd_nans` method (PR #624). Thanks to @waynegm.


Changes from 2.14.4 to 2.15.0
=============================

* Removed some duplicated functions. See https://github.com/Blosc/c-blosc2/issues/503.

* Added a new io mode to memory map files. This forced to change the `io_cb` read API.
  See https://github.com/Blosc/c-blosc2/blob/main/tests/test_mmap.c to see an example on
  how to use it.

* Updated the `SOVERSION` to 4 due to the API change in `io_cb` read.

* Added functions to get cparams, dparams, storage and io defaults respectively.

* Internal zstd sources updated to 1.5.6.

* Fixed a bug when setting a slice using prefilters.


Changes from 2.14.3 to 2.14.4
=============================

* Bumped SONAME due to recent API changes. See https://github.com/Blosc/c-blosc2/issues/581.


Changes from 2.14.2 to 2.14.3
=============================

* More fixes for internal fuzzer.


Changes from 2.14.1 to 2.14.2
=============================

* Fixes for CVE-2024-3203 and CVE-2024-3204.


Changes from 2.14.0 to 2.14.1
=============================

* When loading plugins, first try with `python` and then `python3`.
  This is because many linux distros do not have `python` as a
  symlink to `python3` anymore.


Changes from 2.13.2 to 2.14.0
=============================

* Fixed a bug preventing buffers to be appended to empty (0-sized) b2nd arrays.

* New acceleration path for `b2nd_append()`.  This new path is
  much faster (up to 4x) than the previous one, specially for large arrays.
  See `bench/bench_stack_append.c` for the bench of use.

* New examples for using the `b2nd_set_slice_cbuffer()` and
  `b2nd_append()` functions for adding data into existing b2nd arrays.
  See `examples/example_stack_images.c`.

* Now, ``python3`` is used for finding plugins instead of ``python``.
  This is because many linux distros do not have ``python`` as a symlink
  to ``python3`` anymore.

* New round of fixing warnings.  Now, C-Blosc2 should be relatively free of them.

* Small performance tweak for clevel 1 in BloscLZ codec.

* Fixed a leak in frame code.  Closes #591.  Thanks to @LuMingYinDetect.

* Disable shuffle repeat in filters pipeline.  This was broken
  since the initial implemented, and it was never documented.
  Also, compression ratios do not seem to be improved in our experiments,
  so this capability has been removed completely.

* Support for new Intel compilers (2023.0.1 and on).  Fixes #533.
  Thanks to Nick Papior.


Changes from 2.13.1 to 2.13.2
=============================

* Better checking for `SSSE3` availability in Visual Studio.  Probably fixes #546 too.
  Thanks to @t20100 (Thomas Vincent) for the PR (#586).

* Documented the globally registered filters and codecs.  See:
  https://www.blosc.org/c-blosc2/reference/utility_variables.html#codes-for-filters
  https://www.blosc.org/c-blosc2/reference/utility_variables.html#compressor-codecs


Changes from 2.13.0 to 2.13.1
=============================

* Removed private include in `b2nd.h`. This fixes issue #579.


Changes from 2.12.0 to 2.13.0
=============================

* Added a new BLOSC_FILTER_INT_TRUNC filter for truncating integers to a
  given number of bits.  This is useful for compressing integers that are
  not using all the bits of the type.  See PR #577.

* Optimized zstd, specially when using dicts. See PR #578.

* Initialize grok library when loading the plugin. This is needed for other plugins
  to be able to use it without the need of importing the package.


Changes from 2.11.3 to 2.12.0
=============================

* New `blosc2_get_slice_nchunks` function for getting the unidimensional chunk indexes of a Blosc2 container slice.

* Globally registered new codec `grok`. This will be loaded dynamically.


Changes from 2.11.2 to 2.11.3
=============================

* Frames accept now typesizes that are larger than 255 (and up to 2**31).
  See https://github.com/PyTables/PyTables/issues/1086.  Thanks to
  @chris-allan for the report.

* AVX512 runtime dispatching has been fixed (basically disabled) for GCC
  versions <= 10.

* Use typedef for blosc_timestamp_t.  Thanks to Magnus Ulimoen.


Changes from 2.11.1 to 2.11.2
=============================

* Added support for ARMv7l platforms (Raspberry Pi).  The NEON version
  of the bitshuffle filter was not compiling there, and besides it offered
  no performance advantage over the generic bitshuffle version (it is 2x to
  3x slower actually). So bitshuffle-neon.c has been disabled by default in
  all ARM platforms.

* Also, unaligned access has been disabled in all ARM non-64bits platforms.
  It turned out that, at least the armv7l CPU in Raspberry Pi 4, had issues
  because `__ARM_FEATURE_UNALIGNED` C macro was asserted in the compiler
  (both gcc and clang), but it actually made binaries to raise a "Bus error".

* Thanks to Ben Nuttall for providing a Raspberry Pi for tracking down these
  issues.


Changes from 2.11.0 to 2.11.1
=============================

* Fix ALTIVEC header.  Only affects to IBM POWER builds. Thanks to
  Michael Kuhn for providing a patch.


Changes from 2.10.5 to 2.11.0
=============================

* New AVX512 support for the bitshuffle filter.  This is a backport of the upstream
  bitshuffle project (https://github.com/kiyo-masui/bitshuffle).  Expect up to [20%
  better compression speed](https://github.com/Blosc/c-blosc2/pull/567#issuecomment-1789239842)
  on AMD Zen4 architecture (7950X3D CPU).

* Add c-blosc2 package definition for Guix.  Thanks to Ivan Vilata.

* Properly check calls to `strtol`.  Fixes #558.

* Export the `b2nd_copy_buffer` function. This may be useful for other projects
  dealing with multidimensional arrays in memory. Thanks to Ivan Vilata.

* Better check that nthreads must be >= 1 and <= INT16_MAX.  Fixes #559.

* Fix compile arguments for armv7l. Thanks to Ben Greiner.


Changes from 2.10.4 to 2.10.5
=============================

* Fix a variable name in a test that was causing a segfault in some platforms.

* Change tuner's functions signature to return always an error code.  This allows
  for better error checking when using pluggable tuners in Blosc2.

* Do checks when creating contexts.


Changes from 2.10.3 to 2.10.4
=============================

* Remove duplicated tune initialization since it is already done in blosc2_create_cctx.
  Thanks to Marta Iborra

* Typos fixed.  Thanks to Dimitri Papadopoulos.


Changes from 2.10.2 to 2.10.3
=============================

* Globally registered new codec `openhtj2k`. This will be loaded dynamically. See PR #557.

* Added a `BLOSC_INFO` macro for details on compression params.

* Added `get_blocksize.c` example on automatic blocksizes.

* Warning fixes.

* Fixes for mingw.


Changes from 2.10.1 to 2.10.2
=============================

* Several fixes for the CMake system.  Thanks to Axel Huebl. See PR #541 and #542.

* Several fixes for mingw platform.  Thanks to Biswapriyo Nath.  See PR #540 and #543.


Changes from 2.10.0 to 2.10.1
=============================

* `blosc2_remove_urlpath(const char *urlpath)` does not return an error
  when path does not exist.

* Changes in CMake installer to conserve targets and properties
  on install, so CMake users do not need to write `FindBlosc2.cmake`
  files anymore.  This also helps to preserve transitive dependencies on
  CMake targets, especially useful for fully static builds, e.g., for
  Python wheels.  Thanks to @ax3l (Axel Huebl). See PR #537.

* Fix new typos.  Thanks to @DimitriPapadopoulos. See PR #538.


Changes from 2.9.3 to 2.10.0
============================

* bytedelta filter has been fixed. For backward compatibility, the old
  bytedelta filter is still available as `BLOSC_FILTER_BYTEDELTA_BUGGY`
  symbol, with the same ID (34) than before.  The new, fixed bytedelta
  filter has received a new ID (35) and it can be used via the usual
  `BLOSC_FILTER_BYTEDELTA` symbol. That means that old data written with
  the buggy bytedelta filter should be decompressed without issues.
  Thanks to @foody (Tom Birch) for the fix. See #531, #532 for more info.

* Filter buffers are correctly cycled now.  Now it is possible to use e.g.
  shuffle and bitshuffle filters in the pipeline.  Thanks to @foody (Tom Birch)
  for the fix.  See #528 and PR #530.

* Assorted fixes for allowing better inclusion in external projects.
  Thanks to @ax3l (Axel Huebel). See #525, #527 and #529.

* Minor fixes in the documentation.  Thanks to @ivilata (Ivan Vilata).
  See #523.


Changes from 2.9.2 to 2.9.3
===========================

* Thanks to Dimitri Papadopoulos for an extensive set of improvements in
  documentation and code.

* `load_lib` is now a private function. Before was public, but
  never meant to be.

* Several fixes for bugs discovered by the fuzzer.


Changes from 2.9.1 to 2.9.2
===========================

* Now is possible to register the same plugin (as long as they have the same
  ID *and* name) without errors.  This is useful for registering the same
  plugin without worrying on whether it has been registered already.

* Improved detection of dynamic plugin locations.  Now they must implement
  `plugin_module.print_libpath()` as the canonical way to find the path for
  the dynamic library plugin.

* The `blosc2_static` has gained the cmake POSITION_INDEPENDENT_CODE property.
  This should allow to use the static library in more situations.

* `BLOSC_STUNE` is defined in `blosc2.h` now.  Fixes #481.  Thanks to
  @DimitriPapadopoulos.

* Fixed an issue when having incompressible data in combination with lazy_chunks.

* Fix linking with static -DBUILD_STATIC=0. Fixes #480.

* Visual Studio 2010 (version 10.0) has been deprecated.  Now, users will
  need to use Visual Studio 2012 (version 11.0) or later.

* Many small fixes and code improvements.  Thanks to @DimitriPapadopoulos,
  @bnavigator.


Changes from 2.9.0 to 2.9.1
===========================

* Allow the use of BTUNE by detecting the ``BTUNE_BALANCE`` environment
  variable.


Changes from 2.8.0 to 2.9.0
===========================

* Dynamic plugins as Python wheels are supported now!
  This new feature allows for creating plugins in C, distribute
  them as wheels, and load them dynamically in runtime.
  Small example at https://github.com/Blosc/blosc2_plugin_example

* BloscLZ can achieve more speed in clevel 1 now.

* Internal Zstd sources updated to latest 1.5.5 version.

* Copyright notice updated.  Thanks to @DimitriPapadopoulos.


Changes from 2.7.1 to 2.8.0
===========================

* New bytedelta filter added.  SIMD support for Intel and ARM platforms is there.
  We have blogged about this: https://www.blosc.org/posts/bytedelta-enhance-compression-toolset.rst
  Thanks to Aras Pranckevičius for inspiration and initial implementation.

* Minor improvements in BloscLZ, leading to better compression ratios in general.
  BLoscLZ version bumped to 2.5.2.

* Updated internal zlib-ng to 2.0.7.

* Used `const` qualifier where possible in b2nd.  Thanks to @cf-natali.


Changes from 2.6.1 to 2.7.1
===========================

* Caterva has been merged and carefully integrated in C-Blosc2 in the new b2nd interface.
  For more info on the new interface, see https://www.blosc.org/c-blosc2/reference/b2nd.html.
  Thanks to Marta Iborra, Oscar Guiñón, J. David Ibáñez and Francesc Alted.  Also thanks to
  Aleix Alcacer for his great work in the Caterva project.

  We have a blog about this: https://www.blosc.org/posts/blosc2-ndim-intro

* Updated internal zstd sources to 1.5.4.  Thanks to Dimitri Papadopoulos.

* `blosc2_schunk_avoid_cframe_free` and `blosc2_schunk_append_file` are exported as public functions now.
  Thanks to @bnavigator.

* BloscLZ codec is now treated exactly the same as LZ4.  Before BloscLZ was considered less capable of reaching
 decent compression ratios, but this has changed quite a bit lately, so there is no point in treating both differently.

* Fixed some leaks, mainly on the test suite.

* Fixed quite a bit of compiler warnings.


Changes from 2.6.0 to 2.6.1
===========================

* Add support for macos universal2 binaries (arm64+x86_64 build). Thanks to Thomas Vincent.


Changes from 2.5.0 to 2.6.0
===========================

* [API] Now it is possible to pass filter ID to a User Defined Filter.

* Unified convention for BLOSC_SPLITMODE environment variable in Blosc
  and Blosc2. The list of valid values is "ALWAYS", "NEVER", "AUTO" and
  "FORWARD_COMPAT" now.

* Unified convention for BLOSC_SPLITMODE enum in Blosc and Blosc2 headers.


Changes from 2.4.3 to 2.5.0
===========================

* Fixed a nasty bug that prevented retrieving data correctly with large super-chunks (> 2^31 elements).

* Fixed an issue in `blosc2_schunk_get_slice_buffer()` in the interpretation of the `stop` param.
  Now `stop` is not part of the selected slice (as advertised).

* Now `blosc2_create_cctx()` supports the same environment variables than `blosc2_compress()`.

* Now `blosc2_create_dctx()` supports the same environment variables than `blosc2_decompress()`.

* Added support for the split mode to be serialized in cframes/sframes.

* A new `splitmode` field has been added to the `blosc2_schunk` structure.

* Changed some fields in `blosc2_preparams` and `blosc2_postparams` structs:
  * `in` -> `input`
  * `out` -> `output`
  * `out_size` -> `output_size`
  * `out_typesize` -> `output_typesize`
  * `out_offset` -> `output_offset`
  This was needed to allow Cython to map the fields (`in` is a reserved word in Python).

* Disabled maskout reads in `blosc2_schunk_get_slice_buffer()` as they are not faster than getitem there.

* Add an intermediate block size in its automatic calculation based on `clevel`.


Changes from 2.4.2 to 2.4.3
===========================

* Disable automatic split of blocks when not using shuffle.  Experiments are showing that cratio is suffering too much, specially when using BloscLZ.

* Changed xxhash.h and xxhash.c to the newest version.  Thanks to Dimitri Papadopoulos.


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

* Fixed some issues in converting from super-chunks to frames and back.  Now it is possible to do a roundtrip without (known) problems.

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
  https://fastcompression.blogspot.com/2015/01/zstd-stronger-compression-algorithm.html.

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

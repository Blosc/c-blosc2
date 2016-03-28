==================================================
 Release notes for c-blosc2 2.0.0a3 (third alpha)
==================================================

:Author: Francesc Alted
:Contact: francesc@blosc.org
:URL: http://www.blosc.org


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
  header.  It is best that the user will use blosc2_append_buffer()
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

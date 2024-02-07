Utility variables
+++++++++++++++++
This are enum values which avoid the nuisance of remembering codes and IDs.

Limits for different features
-----------------------------
.. doxygenenumvalue:: BLOSC_MIN_HEADER_LENGTH

.. doxygenenumvalue:: BLOSC_EXTENDED_HEADER_LENGTH

.. doxygenenumvalue:: BLOSC2_MAX_OVERHEAD

.. doxygenenumvalue:: BLOSC_MIN_BUFFERSIZE

.. doxygenenumvalue:: BLOSC2_MAX_BUFFERSIZE

.. doxygenenumvalue:: BLOSC_MAX_TYPESIZE

.. doxygenenumvalue:: BLOSC2_MAX_FILTERS


Codes for filters
-----------------
.. doxygenenumvalue:: BLOSC_NOSHUFFLE

.. doxygenenumvalue:: BLOSC_NOFILTER

.. doxygenenumvalue:: BLOSC_SHUFFLE

.. doxygenenumvalue:: BLOSC_BITSHUFFLE

.. doxygenenumvalue:: BLOSC_DELTA

.. doxygenenumvalue:: BLOSC_TRUNC_PREC

.. doxygenenumvalue:: BLOSC_FILTER_NDCELL

.. doxygenenumvalue:: BLOSC_FILTER_NDMEAN

.. doxygenenumvalue:: BLOSC_FILTER_BYTEDELTA

.. doxygenenumvalue:: BLOSC_FILTER_INT_TRUNC


Compressor codecs
-----------------
.. doxygenenumvalue:: BLOSC_BLOSCLZ

.. doxygenenumvalue:: BLOSC_LZ4

.. doxygenenumvalue:: BLOSC_LZ4HC

.. doxygenenumvalue:: BLOSC_ZLIB

.. doxygenenumvalue:: BLOSC_ZSTD

.. doxygenenumvalue:: BLOSC_CODEC_NDLZ

.. doxygenenumvalue:: BLOSC_CODEC_ZFP_FIXED_ACCURACY

.. doxygenenumvalue:: BLOSC_CODEC_ZFP_FIXED_PRECISION

.. doxygenenumvalue:: BLOSC_CODEC_ZFP_FIXED_RATE

.. doxygenenumvalue:: BLOSC_CODEC_OPENHTJ2K

.. doxygenenumvalue:: BLOSC_CODEC_GROK


Compressor names
----------------
.. doxygendefine:: BLOSC_BLOSCLZ_COMPNAME

.. doxygendefine:: BLOSC_LZ4_COMPNAME

.. doxygendefine:: BLOSC_LZ4HC_COMPNAME

.. doxygendefine:: BLOSC_ZLIB_COMPNAME

.. doxygendefine:: BLOSC_ZSTD_COMPNAME


Internal flags (blosc1_cbuffer_metainfo)
----------------------------------------
.. doxygenenumvalue:: BLOSC_DOSHUFFLE

.. doxygenenumvalue:: BLOSC_MEMCPYED

.. doxygenenumvalue:: BLOSC_DOBITSHUFFLE

.. doxygenenumvalue:: BLOSC_DODELTA

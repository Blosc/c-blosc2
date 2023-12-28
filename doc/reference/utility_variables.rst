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


Compressor codecs
-----------------
.. doxygenenumvalue:: BLOSC_BLOSCLZ

.. doxygenenumvalue:: BLOSC_LZ4

.. doxygenenumvalue:: BLOSC_LZ4HC

.. doxygenenumvalue:: BLOSC_ZLIB

.. doxygenenumvalue:: BLOSC_ZSTD


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

Blosc1 API
==========

This is the classic API from Blosc1 with 32-bit limited containers.

Main API
++++++++

.. doxygenfunction:: blosc_init

.. doxygenfunction:: blosc_destroy

.. doxygenfunction:: blosc_compress

.. doxygenfunction:: blosc_decompress

.. doxygenfunction:: blosc_getitem

.. doxygenfunction:: blosc_get_nthreads

.. doxygenfunction:: blosc_set_nthreads

.. doxygenfunction:: blosc_get_compressor

.. doxygenfunction:: blosc_set_compressor

.. doxygenfunction:: blosc_set_delta

.. doxygenfunction:: blosc_set_blocksize

.. doxygenfunction:: blosc_free_resources


Compressed buffer information
+++++++++++++++++++++++++++++

.. doxygenfunction:: blosc_cbuffer_sizes

.. doxygenfunction:: blosc_cbuffer_metainfo

.. doxygenfunction:: blosc_cbuffer_versions

.. doxygenfunction:: blosc_cbuffer_complib


Utility functions
+++++++++++++++++

.. doxygenfunction:: blosc_compcode_to_compname

.. doxygenfunction:: blosc_compname_to_compcode

.. doxygenfunction:: blosc_list_compressors

.. doxygenfunction:: blosc_get_version_string

.. doxygenfunction:: blosc_get_complib_info

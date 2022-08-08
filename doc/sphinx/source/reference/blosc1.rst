Blosc1 API
==========

This is the classic API from Blosc1 with 32-bit limited containers.

Main API
++++++++

.. doxygenfunction:: blosc1_init

.. doxygenfunction:: blosc1_destroy

.. doxygenfunction:: blosc1_compress

.. doxygenfunction:: blosc1_decompress

.. doxygenfunction:: blosc1_getitem

.. doxygenfunction:: blosc1_get_nthreads

.. doxygenfunction:: blosc1_set_nthreads

.. doxygenfunction:: blosc1_get_compressor

.. doxygenfunction:: blosc1_set_compressor

.. doxygenfunction:: blosc1_set_delta

.. doxygenfunction:: blosc1_get_blocksize

.. doxygenfunction:: blosc1_set_blocksize

.. doxygenfunction:: blosc1_set_splitmode

.. doxygenfunction:: blosc1_free_resources


Compressed buffer information
+++++++++++++++++++++++++++++

.. doxygenfunction:: blosc1_cbuffer_sizes

.. doxygenfunction:: blosc1_cbuffer_metainfo

.. doxygenfunction:: blosc1_cbuffer_versions

.. doxygenfunction:: blosc1_cbuffer_complib

.. doxygenfunction:: blosc1_cbuffer_validate


Utility functions
+++++++++++++++++

.. doxygenfunction:: blosc1_compcode_to_compname

.. doxygenfunction:: blosc1_compname_to_compcode

.. doxygenfunction:: blosc1_list_compressors

.. doxygenfunction:: blosc1_get_version_string

.. doxygenfunction:: blosc1_get_complib_info

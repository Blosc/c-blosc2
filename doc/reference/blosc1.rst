Blosc1 API
==========

This is the classic API from Blosc1 with 32-bit limited containers.

Main API
++++++++

.. doxygenfunction:: blosc2_init

.. doxygenfunction:: blosc2_destroy

.. doxygenfunction:: blosc1_compress

.. doxygenfunction:: blosc1_decompress

.. doxygenfunction:: blosc1_getitem

.. doxygenfunction:: blosc2_get_nthreads

.. doxygenfunction:: blosc2_set_nthreads

.. doxygentypedef:: blosc_threads_callback

.. doxygenfunction:: blosc2_set_threads_callback

.. doxygenfunction:: blosc1_get_compressor

.. doxygenfunction:: blosc1_set_compressor

.. doxygenfunction:: blosc2_set_delta

.. doxygenfunction:: blosc1_get_blocksize

.. doxygenfunction:: blosc1_set_blocksize

.. doxygenfunction:: blosc1_set_splitmode

.. doxygenfunction:: blosc2_free_resources


Compressed buffer information
+++++++++++++++++++++++++++++

.. doxygenfunction:: blosc1_cbuffer_sizes

.. doxygenfunction:: blosc1_cbuffer_metainfo

.. doxygenfunction:: blosc2_cbuffer_versions

.. doxygenfunction:: blosc2_cbuffer_complib

.. doxygenfunction:: blosc1_cbuffer_validate


Utility functions
+++++++++++++++++

.. doxygenfunction:: blosc2_compcode_to_compname

.. doxygenfunction:: blosc2_compname_to_compcode

.. doxygenfunction:: blosc2_list_compressors

.. doxygenfunction:: blosc2_get_version_string

.. doxygenfunction:: blosc2_get_complib_info

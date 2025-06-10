Context
=======

In Blosc 2 there is a special `blosc2_context` struct that is created from
compression and decompression parameters. This allows the compression and
decompression to happen in multithreaded scenarios, without the need for
using the global lock.

..
  .. doxygenstruct:: blosc2_cparams
..
    :members:
..
  .. doxygenvariable:: BLOSC2_CPARAMS_DEFAULTS

.. doxygenstruct:: blosc2_dparams
   :members:
.. doxygenvariable:: BLOSC2_DPARAMS_DEFAULTS

.. doxygenfunction:: blosc2_create_cctx

.. doxygenfunction:: blosc2_create_dctx

.. doxygenfunction:: blosc2_free_ctx

.. doxygenfunction:: blosc2_compress_ctx

.. doxygenfunction:: blosc2_decompress_ctx

.. doxygenfunction:: blosc2_set_maskout

.. doxygenfunction:: blosc2_getitem_ctx

.. doxygenfunction:: blosc2_ctx_get_cparams

.. doxygenfunction:: blosc2_ctx_get_dparams

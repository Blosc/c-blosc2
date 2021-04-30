
Super-chunk
+++++++++++

This API describes the new Blosc 2 container, the super-chunk (or `schunk` for
short).

.. doxygenstruct:: blosc2_storage
   :members:

.. doxygenstruct:: blosc2_schunk
   :project: blosc2
   :members:
.. doxygenfunction:: blosc2_schunk_new
.. doxygenfunction:: blosc2_schunk_free

.. doxygenfunction:: blosc2_schunk_append_buffer
.. doxygenfunction:: blosc2_schunk_decompress_chunk

Dealing with chunks
-------------------

.. doxygenfunction:: blosc2_schunk_get_chunk
.. doxygenfunction:: blosc2_schunk_append_chunk
.. doxygenfunction:: blosc2_schunk_insert_chunk
.. doxygenfunction:: blosc2_schunk_update_chunk
.. doxygenfunction:: blosc2_schunk_delete_chunk
.. doxygenfunction:: blosc2_schunk_reorder_offsets

.. doxygenfunction:: blosc2_schunk_get_cparams
.. doxygenfunction:: blosc2_schunk_get_dparams

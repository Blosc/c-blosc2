
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

.. doxygenfunction:: blosc2_schunk_open
.. doxygenfunction:: blosc2_schunk_open_offset
.. doxygenfunction:: blosc2_schunk_open_udio
.. doxygenfunction:: blosc2_schunk_copy
.. doxygenfunction:: blosc2_schunk_from_buffer
.. doxygenfunction:: blosc2_schunk_to_buffer
.. doxygenfunction:: blosc2_schunk_to_file
.. doxygenfunction:: blosc2_schunk_append_file

.. doxygenfunction:: blosc2_schunk_get_cparams
.. doxygenfunction:: blosc2_schunk_get_dparams
.. doxygenfunction:: blosc2_schunk_reorder_offsets
.. doxygenfunction:: blosc2_schunk_frame_len
.. doxygenfunction:: blosc2_schunk_fill_special

.. doxygenfunction:: blosc2_schunk_append_buffer

.. doxygenfunction:: blosc2_schunk_get_slice_buffer
.. doxygenfunction:: blosc2_schunk_set_slice_buffer

.. doxygenfunction:: blosc2_schunk_avoid_cframe_free


Dealing with chunks
-------------------

.. doxygenfunction:: blosc2_schunk_get_chunk
.. doxygenfunction:: blosc2_schunk_get_lazychunk
.. doxygenfunction:: blosc2_schunk_decompress_chunk
.. doxygenfunction:: blosc2_schunk_append_chunk
.. doxygenfunction:: blosc2_schunk_insert_chunk
.. doxygenfunction:: blosc2_schunk_update_chunk
.. doxygenfunction:: blosc2_schunk_delete_chunk

Creating chunks
---------------

.. doxygenfunction:: blosc2_chunk_zeros
.. doxygenfunction:: blosc2_chunk_nans
.. doxygenfunction:: blosc2_chunk_repeatval
.. doxygenfunction:: blosc2_chunk_uninit

Frame specific functions
------------------------

.. doxygenfunction:: blosc2_frame_get_offsets

Blosc2 NDim
===========

It contains both the data and metalayer that stores the dimensional info for the array.
Blosc2 NDim has a managed internal context that stores the different properties of each array.

Context
-------

.. doxygentypedef:: b2nd_context_t

Creation
++++++++

..  doxygenfunction:: b2nd_create_ctx

Destruction
+++++++++++

..  doxygenfunction:: b2nd_free_ctx

Array
-----

A Blosc2 NDim array is a n-dimensional object that can be managed by the associated functions.
The functions let users to perform different operations with these arrays like copying, getting, setting or
converting data into buffers or files and vice-versa.
Furthermore, Blosc2 NDim only stores the type size (not the data type), and every item of an array has the same size.

The `b2nd_array_t` type struct is where all data and metadata for an array is stored:

.. doxygenstruct:: b2nd_array_t


Creation
++++++++

Constructors
~~~~~~~~~~~~

.. doxygenfunction:: b2nd_uninit
.. doxygenfunction:: b2nd_empty
.. doxygenfunction:: b2nd_zeros
.. doxygenfunction:: b2nd_nans
.. doxygenfunction:: b2nd_full

From/To buffer
~~~~~~~~~~~~~~

.. doxygenfunction:: b2nd_from_cbuffer
.. doxygenfunction:: b2nd_to_cbuffer

From/To file
~~~~~~~~~~~~

.. doxygenfunction:: b2nd_open
.. doxygenfunction:: b2nd_open_offset
.. doxygenfunction:: b2nd_save

From Blosc object
~~~~~~~~~~~~~~~~~

.. doxygenfunction:: b2nd_from_schunk
.. doxygenfunction:: b2nd_from_cframe
.. doxygenfunction:: b2nd_to_cframe

Modify data
~~~~~~~~~~~

.. doxygenfunction:: b2nd_insert
.. doxygenfunction:: b2nd_append
.. doxygenfunction:: b2nd_delete

Copying
+++++++

.. doxygenfunction:: b2nd_copy


Slicing
+++++++

.. doxygenfunction:: b2nd_get_slice
.. doxygenfunction:: b2nd_get_slice_cbuffer
.. doxygenfunction:: b2nd_set_slice_cbuffer
.. doxygenfunction:: b2nd_get_orthogonal_selection
.. doxygenfunction:: b2nd_set_orthogonal_selection
.. doxygenfunction:: b2nd_squeeze
.. doxygenfunction:: b2nd_squeeze_index


Utils
+++++

.. doxygenfunction:: b2nd_print_meta
.. doxygenfunction:: b2nd_serialize_meta
.. doxygenfunction:: b2nd_deserialize_meta
.. doxygenfunction:: b2nd_resize


Destruction
+++++++++++

..  doxygenfunction:: b2nd_free

Utilities
---------

These functions may be used for working with plain C buffers representing multidimensional arrays.

.. doxygenfunction:: b2nd_copy_buffer

Blosc2 NDim
===========

The fundamental array object in Blosc NDim is the `b2nd_array_t` type.
It contains both the data and metalayer that stores the dimensional info for the array.
Blosc2 NDim has an internal context managed that stores the different properties of each array.

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
A Blosc2 NDim array is a multidimensional dataset (compressed or not) easy to handle using the Blosc2 NDim functions.
Furthermore, Blosc2 NDim only stores item size instead of the data type and every item of a Blosc2 NDim array has the same size.
On the other hand, Blosc2 NDim functions let users to perform different operations with these arrays like copying, slicing, setting them or converting them into buffers or files and vice versa.

.. doxygenstruct:: b2nd_array_t


Creation
++++++++

Constructors
~~~~~~~~~~~~

.. doxygenfunction:: b2nd_uninit
.. doxygenfunction:: b2nd_empty
.. doxygenfunction:: b2nd_zeros
.. doxygenfunction:: b2nd_full

From/To buffer
~~~~~~~~~~~~~~

.. doxygenfunction:: b2nd_from_buffer
.. doxygenfunction:: b2nd_to_buffer

From/To file
~~~~~~~~~~~~

.. doxygenfunction:: b2nd_open
.. doxygenfunction:: b2nd_save

From Blosc object
~~~~~~~~~~~~~~~~~

.. doxygenfunction:: b2nd_from_schunk
.. doxygenfunction:: b2nd_from_cframe
.. doxygenfunction:: b2nd_to_cframe


Copying
+++++++

.. doxygenfunction:: b2nd_copy


Slicing
+++++++

.. doxygenfunction:: b2nd_get_slice_buffer
.. doxygenfunction:: b2nd_set_slice_buffer
.. doxygenfunction:: b2nd_get_slice
.. doxygenfunction:: b2nd_squeeze
.. doxygenfunction:: b2nd_squeeze_index


Destruction
+++++++++++

..  doxygenfunction:: b2nd_free
..  doxygenfunction:: b2nd_delete

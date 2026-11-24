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
.. doxygenfunction:: b2nd_linspace
.. doxygenfunction:: b2nd_arange

Floating-point sequence constructors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``b2nd_linspace(ctx, &array, start, stop, endpoint)`` uses the product of the
context's shape as its sample count. ``b2nd_arange(ctx, &array, start, stop,
step)`` requires its computed sequence length to match that product; a mismatch
is an error rather than an implicit reshape. Both fill in logical C order,
independent of chunk/block layout, and support NumPy-format float32 and float64
dtypes (including explicit byte order). The context's compression typesize must
match the dtype. Shape ``{0}`` is empty; rank zero is a scalar.

Parameters and intermediate arithmetic use doubles, followed by a cast to the
output dtype. Integer/complex outputs and user compression prefilters are not
supported. The caller's codec, filters, compression level, threading and storage
settings are preserved. Only chunk/block-sized scratch storage is allocated.
On error the result pointer is NULL; the context remains reusable. As with
other storage constructors, a failure after opening persistent storage can leave
a partial file, which the caller may remove.

These constructors work in ordinary builds without extra dependencies. To use
miniexpr's ahead-of-time SIMD ramp kernels, configure with
``-DBLOSC2_MINIEXPR_ROOT=/path/to/miniexpr``. This optional dependency must already
be built, including ``miniexpr_artifact``; the path may be a source tree with
libraries under ``build/``, or an installation with headers/libraries under
``include/`` and ``lib/``. No miniexpr types appear in the public API.

For the billion-element creation/sum benchmark, build the ``sum_linspace``
target and run ``build/bench/sum_linspace``. It inherits
``BLOSC2_MINIEXPR_ROOT`` and uses ``b2nd_linspace`` for creation and miniexpr's
pairwise reduction API for the sum. To benchmark the native C constructors
instead, leave ``BLOSC2_MINIEXPR_ROOT`` empty and set only
``-DBENCH_MINIEXPR_ROOT=/path/to/miniexpr``.

From/To buffer
~~~~~~~~~~~~~~

.. doxygenfunction:: b2nd_from_cbuffer
.. doxygenfunction:: b2nd_to_cbuffer

From/To file
~~~~~~~~~~~~

.. doxygenfunction:: b2nd_open
.. doxygenfunction:: b2nd_open_offset
.. doxygenfunction:: b2nd_save
.. doxygenfunction:: b2nd_save_append

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
.. doxygenfunction:: b2nd_get_slice_cbuffer_into
.. doxygenfunction:: b2nd_get_sparse_cbuffer
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
.. doxygenfunction:: b2nd_deserialize_meta_inline
.. doxygenfunction:: b2nd_resize
.. doxygenfunction:: b2nd_refresh


Destruction
+++++++++++

..  doxygenfunction:: b2nd_free

Utilities
---------

These functions may be used for working with plain C buffers representing multidimensional arrays.

.. doxygenfunction:: b2nd_copy_buffer
.. doxygenfunction:: b2nd_concatenate
.. doxygenfunction:: b2nd_copy_buffer2
.. doxygenfunction:: b2nd_expand_dims

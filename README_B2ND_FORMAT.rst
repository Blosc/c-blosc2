B2ND Format
===========

The B2ND format is meant for storing multidimensional datasets defined by a shape and a data type.
The logical array is C ordered (the last dimension varies fastest), and data type information can use the NumPy
typestr convention.  The enclosing Blosc2 frame's ``type_size`` is authoritative for the physical element size.

It is just a `B2ND metalayer <https://github.com/Blosc/c-blosc2/blob/main/README_B2ND_METALAYER.rst>`_
on top of a Blosc2 `CFrame <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst>`_
(for contiguous storage) or `SFrame <https://github.com/Blosc/c-blosc2/blob/main/README_SFRAME_FORMAT.rst>`_
(for sparse storage).

The complete initial-format specification, including validation rules, chunk and
block ordering, padding and coordinate mapping, is available in
`SPECS/B2ND.md <https://github.com/Blosc/c-blosc2/blob/main/SPECS/B2ND.md>`_.

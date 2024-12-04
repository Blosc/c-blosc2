B2ND Format
===========

The B2ND format is meant for storing multidimensional datasets defined by a shape and a data type.
Both the shape and the data type follow the NumPy conventions.

It is just a `B2ND metalayer <https://github.com/Blosc/c-blosc2/blob/main/README_B2ND_METALAYER.rst>`_
on top of a Blosc2 `CFrame <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst>`_
(for contiguous storage) or `SFrame <https://github.com/Blosc/c-blosc2/blob/main/README_SFRAME_FORMAT.rst>`_
(for sparse storage).

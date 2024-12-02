b2nd Format
===========

The b2nd format is meant for storing multidimensional datasets defined by a shape and a data type.
Both the shape and the data type follow the NumPy conventions.

It is just a `b2nd metalayer <https://github.com/Blosc/c-blosc2/blob/main/README_B2ND_METALAYER.rst>`_
on top of a Blosc2 `cframe <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst>`_
(for contiguous storage) or `sframe <https://github.com/Blosc/c-blosc2/blob/main/README_SFRAME_FORMAT.rst>`_
(for sparse storage).

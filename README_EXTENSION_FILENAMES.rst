Extensions for Blosc2 Filenames
===============================

Blosc2 has some recommendations for different file extensions for different purposes.  Here is a list of the currently supported ones:

- `.b2frame` (but also `.b2f` or `.b2`) (Blosc2 Frame): this is the main extension for storing `Blosc2 Contiguous Frames <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst>`_.

- `.b2nd` (Blosc2 N-Dim): this is just a contiguous frame file with `a metalayer for storing n-dimensional information <https://github.com/Blosc/c-blosc2/blob/main/README_B2ND_METALAYER.rst>`_ like shape, chunkshape, blockshape and dtype.

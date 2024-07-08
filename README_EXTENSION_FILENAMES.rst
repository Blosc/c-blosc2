Extensions for Blosc2 filenames
===============================

Blosc2 has different file extensions for different purposes.  Here is a list of the currently supported ones:

- `.b2frame` (but also `.b2f` or `.b2`) (Blosc2 frame): this is the main extension for storing `Blosc2 contiguous frames <README_CFRAME_FORMAT.rst>`_.

- `.b2nd` (Blosc2 n-dimensional): this is just a contiguous frame file with `a metalayer for storing n-dimensional information <README_B2ND_METALAYER.rst>`_ like shape, chunkshape, blockshape and dtype.

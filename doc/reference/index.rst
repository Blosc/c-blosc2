C-Blosc2 API
============

This section contains the C-Blosc2 public API and the structures needed to
use it.  C-Blosc2 tries to be backward compatible with both the C-Blosc1 API
and format.  Furthermore, if you just use the C-Blosc1 API you are guaranteed
to generate compressed data containers that can be read with a Blosc1 library.

Having said that, the C-Blosc2 API gives you much more functionality, like
64-bit data containers, more filters, more support for vector instructions,
support for accelerated versions of some codecs in Intel's IPP (like LZ4),
the ability to work with data either in-memory or on-disk (frames) or attach
metainfo to your datasets (metalayers).


.. toctree::
   :maxdepth: 2
   :caption: Contents:

   utility_variables
   utility_functions
   blosc1
   context
   plugins
   schunk
   metalayers
   b2nd

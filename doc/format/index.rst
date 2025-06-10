
Blosc2 Format
=============

.. toctree::
   :maxdepth: 1
   :caption: Contents:

   b2nd_format
   b2nd_metalayer
   sframe_format
   cframe_format
   chunk_format
   extension_filenames

The Blosc2 format is a specification for storing compressed data in a way that is simple to read and parse, and that allows for fast random access to the compressed data. The format is designed to be used with the Blosc2 library, but it is not tied to it, and can be used independently. Emphasis has been put on simplicity and robustness, so that the format can be used in a wide range of applications.

In this section there is a list of the different parts of the format, from the highest level to the lowest.

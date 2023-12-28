Metalayers
==========

Metalayers are meta-information that can be attached to super-chunks.  They can
also be serialized to disk.

.. doxygenstruct:: blosc2_metalayer
   :members:

Fixed-length metalayers
-----------------------

.. doxygenfunction:: blosc2_meta_exists

.. doxygenfunction:: blosc2_meta_add

.. doxygenfunction:: blosc2_meta_update

.. doxygenfunction:: blosc2_meta_get


Variable-length metalayers
--------------------------

.. doxygenfunction:: blosc2_vlmeta_exists

.. doxygenfunction:: blosc2_vlmeta_add

.. doxygenfunction:: blosc2_vlmeta_update

.. doxygenfunction:: blosc2_vlmeta_get

.. doxygenfunction:: blosc2_vlmeta_delete

.. doxygenfunction:: blosc2_vlmeta_get_names

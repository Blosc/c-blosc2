Plugins
=======

Filters
-------

.. doxygentypedef:: blosc2_filter_forward_cb
.. doxygentypedef:: blosc2_filter_backward_cb

.. doxygenstruct:: blosc2_filter
   :members:

.. doxygenfunction:: blosc2_register_filter

Codecs
------

.. doxygentypedef:: blosc2_codec_encoder_cb
.. doxygentypedef:: blosc2_codec_decoder_cb

.. doxygenstruct:: blosc2_codec
   :members:

.. doxygenfunction:: blosc2_register_codec

Tuners
------

.. doxygenstruct:: blosc2_tuner
   :members:

.. doxygenfunction:: blosc2_register_tuner


IO backends
-----------

.. doxygentypedef:: blosc2_open_cb
.. doxygentypedef:: blosc2_close_cb
.. doxygentypedef:: blosc2_tell_cb
.. doxygentypedef:: blosc2_seek_cb
.. doxygentypedef:: blosc2_write_cb
.. doxygentypedef:: blosc2_read_cb
.. doxygentypedef:: blosc2_truncate_cb


.. doxygenstruct:: blosc2_io_cb
   :members:

.. doxygenstruct:: blosc2_io
   :members:

.. doxygenfunction:: blosc2_register_io_cb

.. doxygenfunction:: blosc2_get_io_cb

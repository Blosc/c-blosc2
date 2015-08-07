===============================================================
 Blosc: A blocking, shuffling and lossless compression library
===============================================================

:Author: Francesc Alted
:Contact: francesc@blosc.org
:Author: Valentin Haenel
:Contact: valentin@blosc.org
:URL: http://www.blosc.org
:Travis CI: |travis|
:Appveyor: |appveyor|

.. |travis| image:: https://travis-ci.org/Blosc/c-blosc2.svg?branch=master
        :target: https://travis-ci.org/Blosc/c-blosc2

.. |appveyor| image:: https://ci.appveyor.com/api/projects/status/gccmb03j8ghbj0ig/branch/master?svg=true
        :target: https://ci.appveyor.com/project/FrancescAlted/c-blosc2/branch/master


What is it?
===========

Blosc [1]_ is a high performance compressor optimized for binary data.
It has been designed to transmit data to the processor cache faster
than the traditional, non-compressed, direct memory fetch approach via
a memcpy() OS call.  Blosc is the first compressor (that I'm aware of)
that is meant not only to reduce the size of large datasets on-disk or
in-memory, but also to accelerate memory-bound computations.

C-Blosc2 is the new major version of C-Blosc, with a revamped API and
support for new filters (including filter pipelining), new compressors,
but most importantly new data containers that are meant to overcome the
32-bit limitation of the original C-Blosc.  These new data containers
will be available in various forms, including in-memory and on-disk
implementations.

C-Blosc2 is currently in alpha stage and its development will probably
take several months.  If you want to collaborate in the this development
you are welcome.  We are going to need help in supervising and refining
the API, as well as in the design of the new containers.  Testing for
other platforms than Intel (specially ARM) will be appreciated as well.

Blosc is distributed using the MIT license, see LICENSES/BLOSC.txt for
details.

.. [1] http://www.blosc.org

Meta-compression and other advantages over existing compressors
===============================================================

C-Blosc is not like other compressors: it should rather be called a
meta-compressor.  This is so because it can use different compressors
and filters (programs that generally improve compression ratio).  At
any rate, it can also be called a compressor because it happens that
it already comes with several compressor and filters, so it can
actually work like so.

Currently C-Blosc comes with support of BloscLZ, a compressor heavily
based on FastLZ (http://fastlz.org/), LZ4 and LZ4HC
(https://github.com/Cyan4973/lz4), Snappy
(https://github.com/google/snappy) and Zlib (http://www.zlib.net/), as
well as a highly optimized (it can use SSE2 or AVX2 instructions, if
available) shuffle and bitshuffle filters (for info on how and why
shuffling works, see slide 17 of
http://www.slideshare.net/PyData/blosc-py-data-2014).  However,
different compressors or filters may be added in the future.

C-Blosc is in charge of coordinating the different compressor and
filters so that they can leverage the blocking technique (described
above) as well as multi-threaded execution (if several cores are
available) automatically. That makes that every compressor and filter
will work at very high speeds, even if it was not initially designed
for doing blocking or multi-threading.

Other advantages of Blosc are:

* Meant for binary data: can take advantage of the type size
  meta-information for improved compression ratio (using the
  integrated shuffle and bitshuffle filters).

* Small overhead on non-compressible data: only a maximum of (16 + 4 *
  nthreads) additional bytes over the source buffer length are needed
  to compress *any kind of input*.

* Maximum destination length: contrarily to many other compressors,
  both compression and decompression routines have support for maximum
  size lengths for the destination buffer.

When taken together, all these features set Blosc apart from other
similar solutions.

Compiling the Blosc library with CMake
======================================

Blosc can also be built, tested and installed using CMake_. Although
this procedure might seem a bit more involved than the one described
above, it is the most general because it allows to integrate other
compressors than BloscLZ either from libraries or from internal
sources. Hence, serious library developers are encouraged to use this
way.

The following procedure describes the "out of source" build.

Create the build directory and move into it:

.. code-block:: console

  $ mkdir build
  $ cd build

Now run CMake configuration and optionally specify the installation
directory (e.g. '/usr' or '/usr/local'):

.. code-block:: console

  $ cmake -DCMAKE_INSTALL_PREFIX=your_install_prefix_directory ..

CMake allows to configure Blosc in many different ways, like prefering
internal or external sources for compressors or enabling/disabling
them.  Please note that configuration can also be performed using UI
tools provided by CMake_ (ccmake or cmake-gui):

.. code-block:: console

  $ ccmake ..      # run a curses-based interface
  $ cmake-gui ..   # run a graphical interface

Build, test and install Blosc:

.. code-block:: console

  $ cmake --build .
  $ ctest
  $ cmake --build . --target install

The static and dynamic version of the Blosc library, together with
header files, will be installed into the specified
CMAKE_INSTALL_PREFIX.

.. _CMake: http://www.cmake.org

Once you have compiled your Blosc library, you can easily link your
apps with it as shown in the `example/ directory
<https://github.com/Blosc/c-blosc/blob/master/examples>`_.

Adding support for other compressors (LZ4, LZ4HC, Snappy, Zlib) with CMake
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The CMake files in Blosc are configured to automatically detect other
compressors like LZ4, LZ4HC, Snappy or Zlib by default.  So as long as
the libraries and the header files for these libraries are accessible,
these will be used by default.  See an `example here
<https://github.com/Blosc/c-blosc/blob/master/examples/many_compressors.c>`_.

*Note on Zlib*: the library should be easily found on UNIX systems,
although on Windows, you can help CMake to find it by setting the
environment variable 'ZLIB_ROOT' to where zlib 'include' and 'lib'
directories are. Also, make sure that Zlib DDL library is in your
'\Windows' directory.

However, the full sources for LZ4, LZ4HC, Snappy and Zlib have been
included in Blosc too. So, in general, you should not worry about not
having (or CMake not finding) the libraries in your system because in
this case, their sources will be automatically compiled for you. That
effectively means that you can be confident in having a complete
support for all the supported compression libraries in all supported
platforms.

If you want to force Blosc to use external libraries instead of
the included compression sources:

.. code-block:: console

  $ cmake -DPREFER_EXTERNAL_LZ4=ON ..

You can also disable support for some compression libraries:

.. code-block:: console

  $ cmake -DDEACTIVATE_SNAPPY=ON ..

Mac OSX troubleshooting
=======================

If you run into compilation troubles when using Mac OSX, please make
sure that you have installed the command line developer tools.  You
can always install them with:

.. code-block:: console

  $ xcode-select --install

Wrapper for Python
==================

Blosc has an official wrapper for Python.  See:

https://github.com/Blosc/python-blosc

Command line interface and serialization format for Blosc
=========================================================

Blosc can be used from command line by using Bloscpack.  See:

https://github.com/Blosc/bloscpack

Filter for HDF5
===============

For those who want to use Blosc as a filter in the HDF5 library,
there is a sample implementation in the blosc/hdf5 project in:

https://github.com/Blosc/hdf5

Mailing list
============

There is an official mailing list for Blosc at:

blosc@googlegroups.com
http://groups.google.es/group/blosc

Acknowledgments
===============

See THANKS.rst.


----

  **Enjoy data!**

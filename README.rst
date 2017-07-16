===============================================================
 Blosc: A blocking, shuffling and lossless compression library
===============================================================

:Author: Francesc Alted
:Contact: francesc@blosc.org
:URL: http://www.blosc.org
:Gitter: |gitter|
:Travis CI: |travis|
:Appveyor: |appveyor|

.. |gitter| image:: https://badges.gitter.im/Blosc/c-blosc.svg
        :alt: Join the chat at https://gitter.im/Blosc/c-blosc
        :target: https://gitter.im/Blosc/c-blosc?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge

.. |travis| image:: https://travis-ci.org/Blosc/c-blosc2.svg?branch=master
        :target: https://travis-ci.org/Blosc/c-blosc2

.. |appveyor| image:: https://ci.appveyor.com/api/projects/status/3mlyjc1ak0lbkmte/branch/master?svg=true
        :target: https://ci.appveyor.com/project/FrancescAlted/c-blosc2/branch/master


What is it?
===========

`Blosc <http://blosc.org/pages/blosc-in-depth/>`_ is a high performance compressor optimized for binary data.
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
you are welcome.  We would need help in supervising and refining
the API, as well as in the design of the new containers.  Testing for
other platforms than Intel (specially ARM) will be appreciated as well.

Blosc is distributed using the BSD license, see `<LICENSES/BLOSC2.txt>`_ for
details.

Meta-compression and other advantages over existing compressors
===============================================================

C-Blosc2 is not like other compressors: it should rather be called a
meta-compressor.  This is so because it can use different compressors
and filters (programs that generally improve compression ratio).  At
any rate, it can also be called a compressor because it happens that
it already comes with several compressor and filters, so it can
actually work like so.

Another important aspect of C-Blosc2, is that it is meant to host blocks of data
in smaller containers.  These containers are called *chunks* in C-Blosc2
jargon (they are basically `Blosc1 containers <https://github.com/Blosc/c-blosc>`_).
For achieving maximum speed, these chunks are meant to fit in the
LLC (Last Level Cache) of modern CPUs.  In practice, this means that in
order to leverage C-Blosc2 containers effectively, the user code should
ask for C-Blosc2 to uncompress the chunks, consume them before they hit
main memory and then proceed with the new chunk (as in any streaming operation).
I call this process **Streamed Compressed Computing** and it effectively
avoids uncompressed data to travel to RAM, saving precious time in
modern architectures where RAM access is very expensive compared with
CPU speeds (most specially when those cores are working cooperatively
to solve some computational task).

Currently C-Blosc2 comes with support of BloscLZ, a compressor heavily
based on FastLZ (http://fastlz.org/), LZ4 and LZ4HC
(https://github.com/Cyan4973/lz4), Zstd
(https://github.com/Cyan4973/zstd) and Zlib (via miniz:
https://github.com/richgel999/miniz), as well as a highly optimized
(it can use SSE2 or AVX2 instructions, if available) shuffle and
bitshuffle filters (for info on how and why shuffling works, see slide
17 of http://www.slideshare.net/PyData/blosc-py-data-2014).  However,
different compressors or filters may be added in the future.

C-Blosc2 is in charge of coordinating the different compressor and
filters so that they can leverage the blocking technique (described
above) as well as multi-threaded execution (if several cores are
available) automatically. That makes that every compressor and filter
will work at very high speeds, even if it was not initially designed
for doing blocking or multi-threading.

C-Blosc2 can be used as a regular compressed data container, although it should
shine when used in streamed processes that do computations with the chunks
fitting in CPU caches.  These streamed process can be anything, but e.g. C-Blosc2
working as a RDD (Resilient Distributed Dataset) inside
`Spark <https://spark.apache.org/docs/latest/rdd-programming-guide.html#overview>`_
is the thing that should be taken as a model for that.

Compiling the C-Blosc2 library with CMake
=========================================

Blosc can be built, tested and installed using 
`CMake <http://www.cmake.org>`_.  The following procedure
describes a typical CMake build.

Create the build directory inside the sources and move into it:

.. code-block:: console

  $ cd c-blosc2-sources
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

Once you have compiled your Blosc library, you can easily link your
apps with it as shown in the `example/ directory
<https://github.com/Blosc/c-blosc2/blob/master/examples>`_.

Handling support for codecs (LZ4, LZ4HC, Zstd, Zlib)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C-Blosc comes with full sources for LZ4, LZ4HC, Snappy, Zlib and Zstd and in general, you should not worry about not having (or CMake not finding) the libraries in your system because by default the included sources will be automatically compiled and included in the C-Blosc library. This effectively means that you can be confident in having a complete support for all the codecs in all the Blosc deployments (unless you are explicitly excluding support for some of them).

If you want to force Blosc to use external libraries instead of
the included compression sources:

.. code-block:: console

  $ cmake -DPREFER_EXTERNAL_LZ4=ON ..

You can also disable support for some compression libraries:

.. code-block:: console

  $ cmake -DDEACTIVATE_SNAPPY=ON ..

Supported platforms
~~~~~~~~~~~~~~~~~~~

C-Blosc2 is meant to support all platforms where a C99 compliant C
compiler can be found.  The ones that are mostly tested are Intel
(Linux, Mac OSX and Windows) and ARM (Linux), but exotic ones as IBM
Blue Gene Q embedded "A2" processor are reported to work too.

For Windows, you will need at least VS2015 or higher on x86 and
x64 targets (i.e. ARM is not supported on Windows).

Mac OSX troubleshooting
=======================

If you run into compilation troubles when using Mac OSX, please make
sure that you have installed the command line developer tools.  You
can always install them with:

.. code-block:: console

  $ xcode-select --install

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

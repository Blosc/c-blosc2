===============================================================
 Blosc: A blocking, shuffling and lossless compression library
===============================================================

:Author: The Blosc Development Team
:Contact: blosc@blosc.org
:URL: http://www.blosc.org
:Gitter: |gitter|
:Travis CI: |travis|
:Appveyor: |appveyor|
:NumFOCUS: |numfocus|

.. |gitter| image:: https://badges.gitter.im/Blosc/c-blosc.svg
        :alt: Join the chat at https://gitter.im/Blosc/c-blosc
        :target: https://gitter.im/Blosc/c-blosc?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge

.. |travis| image:: https://travis-ci.org/Blosc/c-blosc2.svg?branch=master
        :target: https://travis-ci.org/Blosc/c-blosc2

.. |appveyor| image:: https://ci.appveyor.com/api/projects/status/3mlyjc1ak0lbkmte/branch/master?svg=true
        :target: https://ci.appveyor.com/project/FrancescAlted/c-blosc2/branch/master

.. |numfocus| image:: https://img.shields.io/badge/powered%20by-NumFOCUS-orange.svg?style=flat&colorA=E1523D&colorB=007D8A
        :target: https://numfocus.org

What is it?
===========

`Blosc <http://blosc.org/pages/blosc-in-depth/>`_ is a high performance compressor optimized for binary data.  It has been designed to transmit data to the processor cache faster than the traditional, non-compressed, direct memory fetch approach via a memcpy() OS call.  Blosc is the first compressor (that I'm aware of) that is meant not only to reduce the size of large datasets on-disk or
in-memory, but also to accelerate memory-bound computations.

C-Blosc2 is the new major version of C-Blosc, with a revamped API and support for new filters (including filter pipelining), new compressors, but most importantly new data containers that are meant to overcome the 32-bit limitation of the original C-Blosc.  These new data containers will be available in various forms, including in-memory and on-disk implementations (frames).  Finally, the frames can be annotated with metainfo (metalayers) that is provided by the user.

C-Blosc2 tries to be backward compatible with both the C-Blosc1 API and format.  Furthermore, if you just use the C-Blosc1 API you are guaranteed to generate compressed data containers that can be read with a Blosc1 library, but getting the benefit of better performance, like for example leveraging the accelerated versions of codecs present in Intel's IPP (LZ4 now and maybe others later on).

C-Blosc2 is currently in beta stage, so not ready to be used in production yet.  Having said this, the beta stage means that the API has been declared frozen, so there is guarantee that your programs will continue to work with future versions of the library. If you want to collaborate in this development you are welcome.  We need help in the different areas listed at the `ROADMAP <https://github.com/Blosc/c-blosc2/ROADMAP.md>`_; also, be sure to read our `DEVELOPING-GUIDE <https://github.com/Blosc/c-blosc2/DEVELOPING-GUIDE.rst>`_.  Blosc is distributed using the `BSD license <https://github.com/Blosc/c-blosc2/LICENSE.txt>`_.

Meta-compression and other advantages over existing compressors
===============================================================

C-Blosc2 is not like other compressors: it should rather be called a meta-compressor.  This is so because it can use different compressors and filters (programs that generally improve compression ratio).  At any rate, it can also be called a compressor because it happens that it already comes with several compressor and filters, so it can actually work like so.

Another important aspect of C-Blosc2 is that it is meant to host blocks of data in smaller containers.  These containers are called *chunks* in C-Blosc2 jargon (they are basically `Blosc1 containers <https://github.com/Blosc/c-blosc>`_). For achieving maximum speed, these chunks are meant to fit in the LLC (Last Level Cache) of modern CPUs.  In practice, this means that in order to leverage C-Blosc2 containers effectively, the user should ask for C-Blosc2 to uncompress the chunks, consume them before they hit
main memory and then proceed with the new chunk (as in any streaming operation).  We call this process *Streamed Compressed Computing* and it effectively avoids uncompressed data to travel to RAM, saving precious time in modern architectures where RAM access is very expensive compared with CPU speeds (most specially when those cores are working cooperatively to solve some computational task).

Currently C-Blosc2 comes with support of BloscLZ, a compressor heavily based on FastLZ (http://fastlz.org/), LZ4 and LZ4HC
(https://github.com/Cyan4973/lz4), Zstd (https://github.com/facebook/zstd), Lizard (https://github.com/inikep/lizard) and Zlib (via miniz: https://github.com/richgel999/miniz), as well as a highly optimized (it can use SSE2, AVX2, NEON or ALTIVEC instructions, if available) shuffle and bitshuffle filters (for info on how shuffling works, see slide 17 of http://www.slideshare.net/PyData/blosc-py-data-2014).  However, different compressors or filters may be added in the future.

C-Blosc2 is in charge of coordinating the different compressor and filters so that they can leverage the blocking technique (described
above) as well as multi-threaded execution (if several cores are available) automatically. That makes that every compressor and filter
will work at very high speeds, even if it was not initially designed for doing blocking or multi-threading.


Compiling the C-Blosc2 library with CMake
=========================================

Blosc can be built, tested and installed using `CMake <http://www.cmake.org>`_.  The following procedure describes a typical CMake build.

Create the build directory inside the sources and move into it:

.. code-block:: console

  $ cd c-blosc2-sources
  $ mkdir build
  $ cd build

Now run CMake configuration and optionally specify the installation
directory (e.g. '/usr' or '/usr/local'):

.. code-block:: console

  $ cmake -DCMAKE_INSTALL_PREFIX=your_install_prefix_directory ..

CMake allows to configure Blosc in many different ways, like prefering internal or external sources for compressors or enabling/disabling them.  Please note that configuration can also be performed using UI tools provided by CMake_ (ccmake or cmake-gui):

.. code-block:: console

  $ ccmake ..      # run a curses-based interface
  $ cmake-gui ..   # run a graphical interface

Build, test and install Blosc:

.. code-block:: console

  $ cmake --build .
  $ ctest
  $ cmake --build . --target install

The static and dynamic version of the Blosc library, together with header files, will be installed into the specified CMAKE_INSTALL_PREFIX.

Once you have compiled your Blosc library, you can easily link your apps with it as shown in the `examples/ directory <https://github.com/Blosc/c-blosc2/blob/master/examples>`_.

Handling support for codecs (LZ4, LZ4HC, Zstd, Lizard, Zlib)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C-Blosc2 comes with full sources for LZ4, LZ4HC, Zstd, Lizard and Zlib and in general, you should not worry about not having (or CMake not finding) the libraries in your system because by default the included sources will be automatically compiled and included in the C-Blosc2 library. This effectively means that you can be confident in having a complete support for all the codecs in all the Blosc deployments (unless you are explicitly excluding support for some of them).

If you want to force Blosc to use external libraries instead of the included compression sources:

.. code-block:: console

  $ cmake -DPREFER_EXTERNAL_LZ4=ON ..

You can also disable support for some compression libraries:

.. code-block:: console

  $ cmake -DDEACTIVATE_SNAPPY=ON ..

Supported platforms
~~~~~~~~~~~~~~~~~~~

C-Blosc2 is meant to support all platforms where a C99 compliant C compiler can be found.  The ones that are mostly tested are Intel
(Linux, Mac OSX and Windows) and ARM (Linux), but exotic ones as IBM Blue Gene Q embedded "A2" processor are reported to work too.

For Windows, you will need at least VS2015 or higher on x86 and x64 targets (i.e. ARM is not supported on Windows).

Mac OSX troubleshooting
=======================

If you run into compilation troubles when using Mac OSX, please make sure that you have installed the command line developer tools.  You can always install them with:

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

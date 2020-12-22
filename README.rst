=============================================================================
 C-Blosc2: A simple, compressed, fast and persistent data store library for C
=============================================================================

:Author: The Blosc Development Team
:Contact: blosc@blosc.org
:URL: http://www.blosc.org
:Gitter: |gitter|
:Actions: |actions|
:NumFOCUS: |numfocus|
:Code of Conduct: |Contributor Covenant|

.. |gitter| image:: https://badges.gitter.im/Blosc/c-blosc.svg
        :alt: Join the chat at https://gitter.im/Blosc/c-blosc
        :target: https://gitter.im/Blosc/c-blosc?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge

.. |actions| image:: https://github.com/Blosc/c-blosc2/workflows/CI%20CMake/badge.svg
        :target: https://github.com/Blosc/c-blosc2/actions?query=workflow%3A%22CI+CMake%22

.. |appveyor| image:: https://ci.appveyor.com/api/projects/status/qiaxywqrouj6nkug/branch/master?svg=true
        :target: https://ci.appveyor.com/project/FrancescAlted/c-blosc2/branch/master

.. |numfocus| image:: https://img.shields.io/badge/powered%20by-NumFOCUS-orange.svg?style=flat&colorA=E1523D&colorB=007D8A
        :target: https://numfocus.org

.. |Contributor Covenant| image:: https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg
        :target: code_of_conduct.md


What is it?
===========

`Blosc <http://blosc.org/pages/blosc-in-depth/>`_ is a high performance compressor optimized for binary data (i.e. floating point numbers, integers and booleans).  It has been designed to transmit data to the processor cache faster than the traditional, non-compressed, direct memory fetch approach via a memcpy() OS call.  Blosc main goal is not just to reduce the size of large datasets on-disk or in-memory, but also to accelerate memory-bound computations.

C-Blosc2 is the new major version of `C-Blosc <https://github.com/Blosc/c-blosc>`_, with full support for 64-bit containers, filter pipelining, new filters, new codecs and dictionaries for improved compression ratio.  The new 64-bit data containers support both sparse (super-chunks) and sequential (frames) storage, either in-memory or on-disk.  The `frame is a sequential format that is very simple <https://github.com/Blosc/c-blosc2/blob/master/README_FRAME_FORMAT.rst>`_ and meant to be used for either persistency or send to other processes or machines.  Finally, the frames can be annotated with metainfo (metalayers, usermeta) that is provided by the user.  More info about the `improved capabilities of C-Blosc2 can be found in this talk <https://www.blosc.org/docs/Caterva-HDF5-Workshop.pdf>`_.

C-Blosc2 tries hard to be backward compatible with both the C-Blosc1 API and in-memory format.  Furthermore, if you just use the C-Blosc1 API you are guaranteed to generate compressed data containers that can be read with C-Blosc2, but getting the benefit of better performance, like for example leveraging the accelerated versions of codecs present in Intel's IPP (LZ4 is supported now and others will follow).

C-Blosc2 is currently in beta stage, so not ready to be used in production yet.  Having said this, the beta stage means that the API has been declared frozen, so there is guarantee that your programs will continue to work with future versions of the library. If you want to collaborate in this development you are welcome.  We need help in the different areas listed at the `ROADMAP <https://github.com/Blosc/c-blosc2/blob/master/ROADMAP.md>`_; also, be sure to read our `DEVELOPING-GUIDE <https://github.com/Blosc/c-blosc2/blob/master/DEVELOPING-GUIDE.rst>`_.  Blosc is distributed using the `BSD license <https://github.com/Blosc/c-blosc2/blob/master/LICENSE.txt>`_.

Meta-compression and other advantages over existing compressors
===============================================================

C-Blosc2 is not like other compressors: it should rather be called a meta-compressor.  This is so because it can use different compressors and filters (programs that generally improve compression ratio).  At any rate, it can also be called a compressor because it happens that it already comes with several compressor and filters, so it can actually work like so.

Currently C-Blosc2 comes with support of BloscLZ, a compressor heavily based on `FastLZ <http://fastlz.org/>`_, `LZ4 and LZ4HC <https://github.com/lz4/lz4>`_, `Zstd <https://github.com/facebook/zstd>`_, `Lizard <https://github.com/inikep/lizard>`_ and `Zlib, via miniz: <https://github.com/richgel999/miniz>`_, as well as a highly optimized (it can use SSE2, AVX2, NEON or ALTIVEC instructions, if available) shuffle and bitshuffle filters (for info on how shuffling works, see slide 17 of http://www.slideshare.net/PyData/blosc-py-data-2014).

Blosc is in charge of coordinating the different compressor and filters so that they can leverage the `blocking technique <https://www.blosc.org/docs/StarvingCPUs-CISE-2010.pdf>`_ as well as multi-threaded execution automatically. That makes that every codec and filter in the pipeline will run efficiently on modern CPUs, even if it was not initially designed for doing blocking or multi-threading.

Another important aspect of C-Blosc2 is that it splits large datasets in smaller containers called *chunks*, which are basically `Blosc1 containers <https://github.com/Blosc/c-blosc>`_. For maximum performance, these chunks are meant to fit in the LLC (Last Level Cache) of CPUs.  In practice this means that in order to leverage C-Blosc2 containers effectively, the user should ask for C-Blosc2 to uncompress the chunks, consume them before they hit main memory and then proceed with the new chunk (as in any streaming operation).  We call this process *Streamed Compressed Computing* and it effectively avoids uncompressed data to travel to RAM, saving precious time in modern architectures where `RAM access is very expensive compared with CPU speeds <https://www.blosc.org/docs/StarvingCPUs-CISE-2010.pdf>`_.

Multidimensional containers
===========================

As said, C-Blosc2 adds a powerful mechanism for adding different metalayers on top of its containers.  `Caterva <https://github.com/Blosc/Caterva>`_ is a sibling library that adds such a metalayer specifying not only the dimensionality of a dataset, but also the dimensionality of the chunks inside the dataset.  In addition, Caterva adds machinery for retrieving arbitrary multi-dimensional slices (aka hyper-slices) out of the multi-dimensional containers in the most efficient way.  Hence, Caterva brings the convenience of multi-dimensional containers to your application very easily.  For more info, check out the `Caterva documentation <https://caterva.readthedocs.io>`_.

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

CMake allows to configure Blosc in many different ways, like prefering internal or external sources for compressors or enabling/disabling them.  Please note that configuration can also be performed using UI tools provided by CMake (`ccmake`  or `cmake-gui`):

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

C-Blosc2 comes with full sources for LZ4, LZ4HC, Zstd, Lizard and Zlib and in general, you should not worry about not having (or CMake not finding) the libraries in your system because by default the included sources will be automatically compiled and included in the C-Blosc2 library. This means that you can be confident in having a complete support for all the codecs in all the Blosc deployments (unless you are explicitly excluding support for some of them).

If you want to force Blosc to use external libraries instead of the included compression sources:

.. code-block:: console

  $ cmake -DPREFER_EXTERNAL_LZ4=ON ..

You can also disable support for some compression libraries:

.. code-block:: console

  $ cmake -DDEACTIVATE_SNAPPY=ON ..

Supported platforms
~~~~~~~~~~~~~~~~~~~

C-Blosc2 is meant to support all platforms where a C99 compliant C compiler can be found.  The ones that are mostly tested are Intel (Linux, Mac OSX and Windows), ARM (Linux, Mac), and PowerPC (Linux) but exotic ones as IBM Blue Gene Q embedded "A2" processor are reported to work too.  More on ARM support in `README_ARM.rst`.

For Windows, you will need at least VS2015 or higher on x86 and x64 targets (i.e. ARM is not supported on Windows).

For Mac OSX, make sure that you have installed the command line developer tools.  You can always install them with:

.. code-block:: console

  $ xcode-select --install

For Mac OSX on arm64 architecture, you need to compile like this:

.. code-block:: console

  $ CC="clang -arch arm64" cmake ..


Support for the LZ4 optimized version in Intel IPP
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C-Blosc2 comes with support for a highly optimized version of the LZ4 codec present in Intel IPP, and actually if the cmake machinery in C-Blosc2 discovers IPP installed in your system it will use it automatically by default.  Here it is a way to easily install Intel IPP in Ubuntu machines:

.. code-block:: console

   $ wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB
   $ apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB
   $ sudo sh -c 'echo deb https://apt.repos.intel.com/ipp all main > /etc/apt/sources.list.d/intel-ipp.list'
   $ sudo apt-get update && sudo apt-get install intel-ipp-64bit-2019.X  # replace .X by the latest version

Check `Intel IPP website <https://software.intel.com/en-us/articles/intel-integrated-performance-primitives-intel-ipp-install-guide>`_ for instructions on how to install it for other platforms.


Display error messages
~~~~~~~~~~~~~~~~~~~~~~

By default error messages are disabled. To display them, you just need to activate the Blosc tracing machinery by setting
the ``BLOSC_TRACE`` environment variable.


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

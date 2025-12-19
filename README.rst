========
C-Blosc2
========

A fast, compressed and persistent data store library for C
==========================================================


:Author: Blosc Development Team
:Contact: blosc@blosc.org
:URL: https://www.blosc.org
:Gitter: |gitter|
:Actions: |actions|
:NumFOCUS: |numfocus|
:Code of Conduct: |Contributor Covenant|

.. |gitter| image:: https://badges.gitter.im/Blosc/c-blosc.svg
        :alt: Join the chat at https://gitter.im/Blosc/c-blosc
        :target: https://gitter.im/Blosc/c-blosc?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge

.. |actions| image:: https://github.com/Blosc/c-blosc2/workflows/CI%20CMake/badge.svg
        :target: https://github.com/Blosc/c-blosc2/actions?query=workflow%3A%22CI+CMake%22

.. |numfocus| image:: https://img.shields.io/badge/powered%20by-NumFOCUS-orange.svg?style=flat&colorA=E1523D&colorB=007D8A
        :target: https://numfocus.org

.. |Contributor Covenant| image:: https://img.shields.io/badge/Contributor%20Covenant-v2.0%20adopted-ff69b4.svg
        :target: https://github.com/Blosc/community/blob/master/code_of_conduct.md


What is it?
===========

`Blosc2 <https://www.blosc.org/pages/blosc-in-depth/>`_ is a high-performance compressor and data format optimized for binary data, including numerical arrays, tensors, and other structured formats. It provides a flexible framework of codecs and filters, enabling developers to balance compression speed and ratio for specific use cases, from high-throughput data pipelines to persistent storage. As the successor to the original Blosc library (released in 2010), Blosc2 is built on a mature foundation and is integrated into many popular scientific computing libraries, such as PyTables, h5py, and Zarr.

C-Blosc2 is the new major version of `C-Blosc <https://github.com/Blosc/c-blosc>`_, and is backward compatible with both the C-Blosc1 API and its in-memory format.  However, the reverse thing is generally not true for the format; buffers generated with C-Blosc2 are not format-compatible with C-Blosc1 (i.e. forward compatibility is not supported).  In case you want to ensure full API compatibility with C-Blosc1 API, define the `BLOSC1_COMPAT` symbol.

See a 3 minutes  `introductory video to Blosc2 <https://www.youtube.com/watch?v=ER12R7FXosk>`_.


Blosc2 NDim: an N-Dimensional store
===================================

One of more exciting additions in C-Blosc2 is the `Blosc2 NDim layer <https://www.blosc.org/c-blosc2/reference/b2nd.html>`_ (or B2ND for short), allowing to create *and* read n-dimensional datasets in an extremely efficient way thanks to a n-dim 2-level partitioning, that allows to slice and dice arbitrary large and compressed data in a more fine-grained way:

.. image:: https://github.com/Blosc/c-blosc2/blob/main/images/b2nd-2level-parts.png?raw=true
  :width: 75%

To wet you appetite, here it is how the `NDArray` object in the  `Python wrapper`_ performs on getting slices orthogonal to the different axis of a 4-dim dataset:

.. image:: https://github.com/Blosc/c-blosc2/blob/main/images/Read-Partial-Slices-B2ND.png?raw=true
  :width: 75%

We have blogged about this: https://www.blosc.org/posts/blosc2-ndim-intro

We also have a ~2 min explanatory video on `why slicing in a pineapple-style (aka double partition)
is useful <https://www.youtube.com/watch?v=LvP9zxMGBng>`_:

.. image:: https://github.com/Blosc/blogsite/blob/master/files/images/slicing-pineapple-style.png?raw=true
  :width: 50%
  :alt: Slicing a dataset in pineapple-style
  :target: https://www.youtube.com/watch?v=LvP9zxMGBng

.. include:: ../WHATS-NEW.rst

More info about the `improved capabilities of C-Blosc2 can be found in this paper <https://www.blosc.org/docs/Exploring-MilkyWay-SciPy2023-paper.pdf>`_.  Please, cite it if you use C-Blosc2 in your research!


Open format
===========

The Blosc2 format is open and `fully documented <https://github.com/Blosc/c-blosc2/blob/main/README_FORMAT.rst>`_.

The format specs are defined in less than 4000 words, so they should be easy to read and understand.  In our opinion, this is critical for the long-term success of the library, as it allows for third-party implementations of the format, and also for the users to understand what is going on under the hood.


Python wrapper
==============

We are officially supporting (thanks to the Python Software Foundation) a `Python wrapper for Blosc2 <https://github.com/Blosc/python-blosc2>`_.  It supports all the features of the predecessor `python-blosc <https://github.com/Blosc/python-blosc>`_ package plus most of the bells and whistles of C-Blosc2, like 64-bit and multidimensional containers.  As a bonus, the `python-blosc2` package comes with wheels and binary versions of the C-Blosc2 libraries, so anyone, even non-Python users can install C-Blosc2 binaries easily with:

.. code-block:: console

  pip install blosc2


Compiling the C-Blosc2 library with CMake
=========================================

Blosc can be built, tested and installed using `CMake <https://cmake.org>`_.  The following procedure describes a typical CMake build.

Create the build directory inside the sources and move into it:

.. code-block:: console

  git clone https://github.com/Blosc/c-blosc2
  cd c-blosc2
  mkdir build
  cd build

Now run CMake configuration and optionally specify the installation
directory (e.g. '/usr' or '/usr/local'):

.. code-block:: console

  cmake -DCMAKE_INSTALL_PREFIX=your_install_prefix_directory ..

CMake allows to configure Blosc in many different ways, like preferring internal or external sources for compressors or enabling/disabling them.  Please note that configuration can also be performed using UI tools provided by CMake (`ccmake`  or `cmake-gui`):

.. code-block:: console

  ccmake ..      # run a curses-based interface
  cmake-gui ..   # run a graphical interface

Build, test and install Blosc:

.. code-block:: console

  cmake --build .
  ctest
  cmake --build . --target install

The static and dynamic version of the Blosc library, together with header files, will be installed into the specified CMAKE_INSTALL_PREFIX.

Once you have compiled your Blosc library, you can easily link your apps with it as shown in the `examples/ directory <https://github.com/Blosc/c-blosc2/blob/main/examples>`_.


Handling support for codecs (LZ4, LZ4HC, Zstd, Zlib)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

C-Blosc2 comes with full sources for LZ4, LZ4HC, Zstd, and Zlib and in general, you should not worry about not having (or CMake not finding) the libraries in your system because by default the included sources will be automatically compiled and included in the C-Blosc2 library. This means that you can be confident in having a complete support for all these codecs in all the official Blosc deployments.  Of course, if you consider this is too bloated, you can exclude support for some of them.

For example, let's suppose that you want to disable support for Zstd:

.. code-block:: console

  cmake -DDEACTIVATE_ZSTD=ON ..

Or, you may want to use a codec in an external library already in the system:

.. code-block:: console

  cmake -DPREFER_EXTERNAL_LZ4=ON ..

For OpenZL, there are problems with the build seemingly, so, after building and installing into ``build-cmake`` in the ``openzl`` directory, one has to run:

.. code-block:: console

  cmake   -DPREFER_EXTERNAL_OPENZL=ON   -DOPENZL_LIBRARY=$HOME/openzl/build-cmake/install/lib/libopenzl.a   -DOPENZL_INCLUDE_DIR=$HOME/openzl/build-cmake/install/include   ..

Supported platforms
~~~~~~~~~~~~~~~~~~~

C-Blosc2 is meant to support all platforms where a C99 compliant C compiler can be found.  The ones that are mostly tested are Intel (Linux, Mac OSX and Windows), ARM (Linux, Mac), and PowerPC (Linux).  More on ARM support in `README_ARM.rst`.

For Windows, you will need at least VS2015 or higher on x86 and x64 targets (i.e. ARM is not supported on Windows).

For Mac OSX, make sure that you have the command line developer tools available.  You can always install them with:

.. code-block:: console

  xcode-select --install

For Mac OSX on arm64 architecture, you may want to compile it like this:

.. code-block:: console

  CC="clang -arch arm64" cmake ..


Display error messages
~~~~~~~~~~~~~~~~~~~~~~

By default error messages are disabled. To display them, you just need to activate the Blosc tracing machinery by setting
the ``BLOSC_TRACE`` environment variable.


Contributing
============

If you want to collaborate in this development you are welcome.  We need help in the different areas listed at the `ROADMAP <https://github.com/Blosc/c-blosc2/blob/main/ROADMAP-TO-3.0.rst>`_; also, be sure to read our `DEVELOPING-GUIDE <https://github.com/Blosc/c-blosc2/blob/main/DEVELOPING-GUIDE.rst>`_ and our `Code of Conduct <https://github.com/Blosc/community/blob/master/code_of_conduct.md>`_.  Blosc is distributed using the `BSD license <https://github.com/Blosc/c-blosc2/blob/main/LICENSE.txt>`_.


Tweeter feed
============

Follow `@Blosc2 <https://twitter.com/Blosc2>`_ so as to get informed about the latest developments.


Citing Blosc
============

You can cite our work on the different libraries under the Blosc umbrella as:

.. code-block:: console

  @ONLINE{blosc,
    author = {{Blosc Development Team}},
    title = "{A fast, compressed and persistent data store library}",
    year = {2009-2025},
    note = {https://blosc.org}
  }


Acknowledgments
===============

See `THANKS document <https://github.com/Blosc/c-blosc2/blob/main/THANKS.rst>`_.


----

*Compress Better, Compute Bigger*

-- Blosc Development Team

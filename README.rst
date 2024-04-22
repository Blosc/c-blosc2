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

`Blosc <https://www.blosc.org/pages/blosc-in-depth/>`_ is a high performance compressor optimized for binary data (i.e. floating point numbers, integers and booleans, although it can handle string data too).  It has been designed to transmit data to the processor cache faster than the traditional, non-compressed, direct memory fetch approach via a memcpy() OS call.  Blosc main goal is not just to reduce the size of large datasets on-disk or in-memory, but also to accelerate memory-bound computations.

C-Blosc2 is the new major version of `C-Blosc <https://github.com/Blosc/c-blosc>`_, and is backward compatible with both the C-Blosc1 API and its in-memory format.  However, the reverse thing is generally not true for the format; buffers generated with C-Blosc2 are not format-compatible with C-Blosc1 (i.e. forward compatibility is not supported).  In case you want to ensure full API compatibility with C-Blosc1 API, define the `BLOSC1_COMPAT` symbol.

See a 3 minutes  `introductory video to Blosc2 <https://www.youtube.com/watch?v=ER12R7FXosk>`_.


Blosc2 NDim: an N-Dimensional store
===================================

One of the latest and more exciting additions in C-Blosc2 is the `Blosc2 NDim layer <https://www.blosc.org/c-blosc2/reference/b2nd.html>`_ (or b2nd for short), allowing to create *and* read n-dimensional datasets in an extremely efficient way thanks to a n-dim 2-level partitioning, that allows to slice and dice arbitrary large and compressed data in a more fine-grained way:

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


New features in C-Blosc2
========================

* **64-bit containers:** the first-class container in C-Blosc2 is the `super-chunk` or, for brevity, `schunk`, that is made by smaller chunks which are essentially C-Blosc1 32-bit containers.  The super-chunk can be backed or not by another container which is called a `frame` (see later).

* **NDim containers (b2nd):** allow to store n-dimensional data that can efficiently read datasets in slices that can be n-dimensional too. To achieve this, a n-dimensional 2-level partitioning has been implemented.  This capabilities were formerly part of `Caterva <https://github.com/Blosc/caterva>`_, and now it is included in C-Blosc2 for convenience.  Caterva is now deprecated.

* **More filters:** besides `shuffle` and `bitshuffle` already present in C-Blosc1, C-Blosc2 already implements:

  - `bytedelta`: calculates the difference between bytes in a block that has been shuffled already.  We have `blogged about bytedelta <https://www.blosc.org/posts/bytedelta-enhance-compression-toolset/>`_.

  - `delta`: the stored blocks inside a chunk are diff'ed with respect to first block in the chunk.  The idea is that, in some situations, the diff will have more zeros than the original data, leading to better compression.

  - `trunc_prec`: it zeroes the least significant bits of the mantissa of float32 and float64 types.  When combined with the `shuffle` or `bitshuffle` filter, this leads to more contiguous zeros, which are compressed better.

* **A filter pipeline:** the different filters can be pipelined so that the output of one can the input for the other.  A possible example is a `delta` followed by `shuffle`, or as described above, `trunc_prec` followed by `bitshuffle`.

* **Prefilters:** allow to apply user-defined C callbacks **prior** the filter pipeline during compression.  See `test_prefilter.c <https://github.com/Blosc/c-blosc2/blob/main/tests/test_prefilter.c>`_ for an example of use.

* **Postfilters:** allow to apply user-defined C callbacks **after** the filter pipeline during decompression. The combination of prefilters and postfilters could be interesting for supporting e.g. encryption (via prefilters) and decryption (via postfilters).  Also, a postfilter alone can be used to produce on-the-flight computation based on existing data (or other metadata, like e.g. coordinates). See `test_postfilter.c <https://github.com/Blosc/c-blosc2/blob/main/tests/test_postfilter.c>`_ for an example of use.

* **SIMD support for ARM (NEON):** this allows for faster operation on ARM architectures.  Only `shuffle` is supported right now, but the idea is to implement `bitshuffle` for NEON too.  Thanks to Lucian Marc.

* **SIMD support for PowerPC (ALTIVEC):** this allows for faster operation on PowerPC architectures.  Both `shuffle`  and `bitshuffle` are supported; however, this has been done via a transparent mapping from SSE2 into ALTIVEC emulation in GCC 8, so performance could be better (but still, it is already a nice improvement over native C code; see PR https://github.com/Blosc/c-blosc2/pull/59 for details).  Thanks to Jerome Kieffer and `ESRF <https://www.esrf.fr>`_ for sponsoring the Blosc team in helping him in this task.

* **Dictionaries:** when a block is going to be compressed, C-Blosc2 can use a previously made dictionary (stored in the header of the super-chunk) for compressing all the blocks that are part of the chunks.  This usually improves the compression ratio, as well as the decompression speed, at the expense of a (small) overhead in compression speed.  Currently, it is only supported in the `zstd` codec, but would be nice to extend it to `lz4` and `blosclz` at least.

* **Contiguous frames:** allow to store super-chunks contiguously, either on-disk or in-memory.  When a super-chunk is backed by a frame, instead of storing all the chunks sparsely in-memory, they are serialized inside the frame container.  The frame can be stored on-disk too, meaning that persistence of super-chunks is supported.

* **Sparse frames:** each chunk in a super-chunk is stored in a separate file or different memory area, as well as the metadata.  This is allows for more efficient updates/deletes than in contiguous frames (i.e. avoiding 'holes' in monolithic files).  The drawback is that it consumes more inodes when on-disk.  Thanks to Marta Iborra for this contribution.

* **Partial chunk reads:** there is support for reading just part of chunks, so avoiding to read the whole thing and then discard the unnecessary data.

* **Parallel chunk reads:** when several blocks of a chunk are to be read, this is done in parallel by the decompressing machinery.  That means that every thread is responsible to read, post-filter and decompress a block by itself, leading to an efficient overlap of I/O and CPU usage that optimizes reads to a maximum.

* **Meta-layers:** optionally, the user can add meta-data for different uses and in different layers.  For example, one may think on providing a meta-layer for `NumPy <https://numpy.org>`_ so that most of the meta-data for it is stored in a meta-layer; then, one can place another meta-layer on top of the latter for adding more high-level info if desired (e.g. geo-spatial, meteorological...).

* **Variable length meta-layers:** the user may want to add variable-length meta information that can be potentially very large (up to 2 GB). The regular meta-layer described above is very quick to read, but meant to store fixed-length and relatively small meta information.  Variable length metalayers are stored in the trailer of a frame, whereas regular meta-layers are in the header.

* **Efficient support for special values:** large sequences of repeated values can be represented with an efficient, simple and fast run-length representation, without the need to use regular codecs.  With that, chunks or super-chunks with values that are the same (zeros, NaNs or any value in general) can be built in constant time, regardless of the size.  This can be useful in situations where a lot of zeros (or NaNs) need to be stored (e.g. sparse matrices).

* **Nice markup for documentation:** we are currently using a combination of Sphinx + Doxygen + Breathe for documenting the C-API.  See https://www.blosc.org/c-blosc2/c-blosc2.html.  Thanks to Alberto Sabater and Aleix Alcacer for contributing the support for this.

* **Plugin capabilities for filters and codecs:** we have a plugin register capability inplace so that the info about the new filters and codecs can be persisted and transmitted to different machines.  See https://github.com/Blosc/c-blosc2/blob/main/examples/urfilters.c for a self-contained example.  Thanks to the NumFOCUS foundation for providing a grant for doing this, and Oscar Griñón and Aleix Alcacer for the implementation.

* **Pluggable tuning capabilities:** this will allow users with different needs to define an interface so as to better tune different parameters like the codec, the compression level, the filters to use, the blocksize or the shuffle size.  Thanks to ironArray for sponsoring us in doing this.

* **Support for I/O plugins:** so that users can extend the I/O capabilities beyond the current filesystem support.  Things like the use of databases or S3 interfaces should be possible by implementing these interfaces.  Thanks to ironArray for sponsoring us in doing this.

* **Python wrapper:**  we have a preliminary wrapper in the works.  You can have a look at our ongoing efforts in the `python-blosc2 repo <https://github.com/Blosc/python-blosc2>`_.  Thanks to the Python Software Foundation for providing a grant for doing this.

* **Security:** we are actively using using the `OSS-Fuzz <https://github.com/google/oss-fuzz>`_ and `ClusterFuzz <https://oss-fuzz.com>`_ for uncovering programming errors in C-Blosc2.  Thanks to Google for sponsoring us in doing this, and to Nathan Moinvaziri for most of the work here.

More info about the `improved capabilities of C-Blosc2 can be found in this talk <https://www.blosc.org/docs/Caterva-HDF5-Workshop.pdf>`_.

C-Blosc2 API and format have been frozen, and that means that there is guarantee that your programs will continue to work with future versions of the library, and that next releases will be able to read from persistent storage generated from previous releases (as of 2.0.0).


Open format
===========

The Blosc2 format is open and documented in the next documents:

* [The chunk; the basic building block](https://github.com/Blosc/c-blosc2/blob/main/README_CHUNK_FORMAT.rst)
* [The cframe; this is made of different chunks in contiguous storage](https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst)
* [The sframe; a variation of the cframe for sparse storage](https://github.com/Blosc/c-blosc2/blob/main/README_SFRAME_FORMAT.rst)
* [The b2nd metalayer; info for the n-dimensional data container](https://github.com/Blosc/c-blosc2/blob/main/README_B2ND_METALAYER.rst)

All these documents take less than 1000 lines of text, so they should be easy to read and understand.  In our opinion, this is very important for the long-term success of the library, as it allows for third-party implementations of the format, and also for the users to understand what is going on under the hood.


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

If you want to collaborate in this development you are welcome.  We need help in the different areas listed at the `ROADMAP <https://github.com/Blosc/c-blosc2/blob/main/ROADMAP.rst>`_; also, be sure to read our `DEVELOPING-GUIDE <https://github.com/Blosc/c-blosc2/blob/main/DEVELOPING-GUIDE.rst>`_ and our `Code of Conduct <https://github.com/Blosc/community/blob/master/code_of_conduct.md>`_.  Blosc is distributed using the `BSD license <https://github.com/Blosc/c-blosc2/blob/main/LICENSE.txt>`_.


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
    year = {2009-2023},
    note = {https://blosc.org}
  }


Acknowledgments
===============

See `THANKS document <https://github.com/Blosc/c-blosc2/blob/main/THANKS.rst>`_.


----

-- Blosc Development Team.  **We make compression better.**

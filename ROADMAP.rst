Roadmap
=======

C-Blosc2 is the new iteration of C-Blosc 1.x series, adding more features and `better documentation <https://www.blosc.org/c-blosc2/c-blosc2.html>`_.
This document lists the new features for the production release of C-Blosc2, as well as the plans for the future.

Many of the features implemented so far have been possible with the funds of a `generous donation <https://www.blosc.org/posts/blosc-donation/>`_.  Thanks to HUAWEI, and specially to Zeeman Wang!


New features
------------

Right now, the next features are already implemented (although they may require some refactoring or more tests):

* **64-bit containers:** the first-class container in C-Blosc2 is the `super-chunk` or, for brevity, `schunk`, that is made by smaller chunks which are essentially C-Blosc1 32-bit containers.  The super-chunk can be backed or not by another container which is called a `frame` (see later).

* **More filters:** besides `shuffle` and `bitshuffle` already present in C-Blosc1, C-Blosc2 already implements:

  - `delta`: the stored blocks inside a chunk are diff'ed with respect to first block in the chunk.  The idea is that, in some situations, the diff will have more zeros than the original data, leading to better compression.

  - `trunc_prec`: it zeroes the least significant bits of the mantissa of float32 and float64 types.  When combined with the `shuffle` or `bitshuffle` filter, this leads to more contiguous zeros, which are compressed better.

* **A filter pipeline:** the different filters can be pipelined so that the output of one can be the input for the other.  A possible example is a `delta` followed by `shuffle`, or as described above, `trunc_prec` followed by `bitshuffle`.

* **Prefilters:** allows to apply user-defined C callbacks **prior** the filter pipeline during compression.  See `test_prefilter.c <https://github.com/Blosc/c-blosc2/blob/main/tests/test_prefilter.c>`_ for an example of use.

* **Postfilters:** allows to apply user-defined C callbacks **after** the filter pipeline during decompression. The combination of prefilters and postfilters could be interesting for supporting e.g. encryption (via prefilters) and decryption (via postfilters).  Also, a postfilter alone can be used to produce on-the-flight computation based on existing data (or other metadata, like e.g. coordinates). See `test_postfilter.c <https://github.com/Blosc/c-blosc2/blob/main/tests/test_postfilter.c>`_ for an example of use.

* **SIMD support for ARM (NEON):** this allows for faster operation on ARM architectures.  Only `shuffle` is supported right now, but the idea is to implement `bitshuffle` for NEON too.  Thanks to Lucian Marc.

* **SIMD support for PowerPC (ALTIVEC):** this allows for faster operation on PowerPC architectures.  Both `shuffle`  and `bitshuffle` are supported; however, this has been done via a transparent mapping from SSE2 into ALTIVEC emulation in GCC 8, so performance could be better (but still, it is already a nice improvement over native C code; see PR https://github.com/Blosc/c-blosc2/pull/59 for details).  Thanks to Jerome Kieffer and `ESRF <https://www.esrf.fr>`_ for sponsoring the Blosc team in doing this task.

* **Dictionaries:** when a block is going to be compressed, C-Blosc2 can use a previously made dictionary (stored in the header of the super-chunk) for compressing all the blocks that are part of the chunks.  This usually improves the compression ratio, as well as the decompression speed, at the expense of a (small) overhead in compression speed.  Currently, it is only supported in the `zstd` codec, but would be nice to extend it to `lz4` and `blosclz` at least.

* **Contiguous frames:** allow to store super-chunks contiguously, either on-disk or in-memory.  When a super-chunk is backed by a frame, instead of storing all the chunks sparsely in-memory, they are serialized inside the frame container.  The frame can be stored on-disk too, meaning that persistence of super-chunks is supported.

* **Sparse frames:** each chunk in a super-chunk, as well as the metadata, are stored separately.  This allows for more efficient updates than in frames (i.e. avoiding 'holes' in monolithic files).

* **Partial chunk reads:** there is support for reading just part of a chunk, so avoiding reading the whole thing and then discarding the unnecessary data (which is a waste of resources).

* **Parallel chunk reads:** when several blocks of a chunk are to be read, this is done in parallel by the decompressing machinery.  That means that every thread is responsible to read, post-filter and decompress a block by itself, leading to an efficient overlap of I/O and CPU usage that optimizes reads to a maximum.

* **Meta-layers:** optionally, the user can add meta-data for different uses and in different layers.  For example, one may think on providing a meta-layer for `NumPy <https://numpy.org>`_ so that most of the meta-data for it is stored in a meta-layer; then, one can place another meta-layer on top of the latter for adding more high-level info if desired (e.g. geo-spatial, meteorological...).

* **Variable length meta-layers:** the user may want to add variable-length meta information that can be potentially very large (up to 2 GB). The regular meta-layer described above is very quick to read, but meant to store fixed-length and relatively small meta information.  Variable length metalayers are stored in the trailer of a frame, whereas regular meta-layers are in the header.

* **Efficient support for special values:** large sequences of repeated values can be represented with an efficient, simple and fast run-length representation, without the need to use regular codecs.  With that, chunks or super-chunks with values that are the same (zeros, NaNs or any value in general) can be built in constant time, regardless of the size.  This can be useful in situations where a lot of zeros (or NaNs) need to be stored (e.g. sparse matrices).

* **Nice markup for documentation:** we are currently using a combination of Sphinx + Doxygen + Breathe for documenting the C-API.  See https://www.blosc.org/c-blosc2/c-blosc2.html.  Thanks to Alberto Sabater and Aleix Alcacer for contributing the support for this.

* **Plugin capabilities for filters and codecs:** we have a plugin register capability inplace so that the info about the new filters and codecs can be persisted and transmitted to different machines.  Thanks to the NumFOCUS foundation for providing a grant for doing this.

* **Centralized plugin repository:** we have implemented a centralized repository so that people can send their plugins (using the existing machinery) to the Blosc2 team.  If the plugins fulfill a series of requirements, they will be officially accepted, and distributed within the library. Thanks to NumFOCUS foundation for providing a grant for doing this. See https://www.blosc.org/posts/registering-plugins/.

* **Support for lossy codecs:** besides supporting the `trunc_prec` filter (described above), we also offer support for `zfp <https://github.com/LLNL/zfp>`_, a codec that is specifically meant for lossy compression of *multidimensional* floating point data.  For details on how use it, see https://github.com/Blosc/c-blosc2/tree/main/plugins/codecs/zfp.  Support for more lossy codecs may come in the future.

* **Pluggable tuning capabilities:** this will allow users with different needs to define an interface so as to better tune different parameters like the codec, the compression level, the filters to use, the blocksize or the shuffle size.  Thanks to ironArray for sponsoring us in doing this.

* **Support for I/O plugins:** so that users can extend the I/O capabilities beyond the current filesystem support.  Things like use databases or S3 interfaces should be possible by implementing these interfaces.  Thanks to ironArray for sponsoring us in doing this.

* **Python wrapper:**  we have a preliminary wrapper in the works.  You can have a look at our ongoing efforts in the `python-blosc2 repo <https://github.com/Blosc/python-blosc2>`_.  Thanks to the Python Software Foundation for providing a grant for doing this.

* **Security:** we are actively using the `OSS-Fuzz <https://github.com/google/oss-fuzz>`_ and `ClusterFuzz <https://oss-fuzz.com>`_ for uncovering programming errors in C-Blosc2.  Thanks to Google for sponsoring us in doing this.


Actions to be done
------------------

* **Improve the safety of the library:**  even if we have already made a long way in improving our safety, mainly thanks to the efforts of Nathan Moinvaziri, we take safety seriously, so this is always a work in progress.

* **Checksums:** the frame can benefit from having a checksum per every chunk/index/metalayer.  This will provide more safety towards frames that are damaged for whatever reason.  Also, this would provide better feedback when trying to determine the parts of the frame that are corrupted.  Candidates for checksums can be the xxhash32 or xxhash64, depending on the goals (to be decided).

* **Multiple index chunks in frames:** right now, only `one chunk <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst#chunks>`_ is allowed for indexing other chunks.  Provided the 2GB limit for a chunksize, that means that 'only' 256 million of chunks can be stored in a frame.  Allowing for more than one index chunk would overcome this limitation.

* **More robust detection of CPU capabilities:** although currently this detection is quite sophisticated, the code responsible for that has organically grow for more than 10 years and it is time to come with a more modern and robust way of doing this. https://github.com/google/cpu_features may be a good helper for doing this refactoring.

* **Documentation:** utterly important for attracting new users and making the life easier for existing ones.  Important points to have in mind here:

  - **Quality of API docstrings:** is the mission of the functions or data structures clearly and succinctly explained? Are all the parameters explained?  Is the return value explained?  What are the possible errors that can be returned?  (mostly completed by Alberto Sabater).

  - **Tutorials/book:** besides the API docstrings, more documentation materials should be provided, like tutorials or a book about Blosc (or at least, the beginnings of it).  Due to its adoption in GitHub and Jupyter notebooks, one of the most extended and useful markup systems is Markdown, so this should also be the first candidate to use here.

* **Wrappers for other languages:** Java, R or Julia are the most obvious candidates.  Still not sure if these should be produced and maintained by the Blosc development team, or leave them for third-party players that would be interested. The steering `council discussed this <https://github.com/Blosc/governance/blob/master/steering_council_minutes/2020-03-26.md>`_, and probably just the Python wrapper (python-blosc2, see above) should be maintained by Blosc maintainers themselves, while the other languages should be maintained by the community.

* **Lock support for super-chunks:** when different processes are accessing concurrently to super-chunks, make them to sync properly by using locks, either on-disk (frame-backed super-chunks), or in-memory. Such a lock support would be configured in build time, so it could be disabled with a cmake flag.

* **Hierarchical structure (aka Groups):** some libraries (like `xarray <https://xarray.dev>`_) need an easy way to tie different datasets together (groups).  This would also allow to create whole hierarchies so as to endow a structure to these datasets.  Besides the structural part (that will be part of the format specification), this will need an accompanying API that allows the user to create groups, add datasets to groups, (recursively) list datasets in groups, access a dataset inside a group, an so on.


Outreaching
-----------

* **Improve the Blosc website:** create a nice, modern-looking and easy to navigate website so that new potential users can see at first glimpse what's Blosc all about and power-users can access the documentation part easily.  Ideally, a site-only search box would be great (sphinx-based docs would offer this for free).

* **Attend to meetings and conferences:** it is very important to plan going to conferences for advertising C-Blosc2 and meeting people in-person.  We need to decide which meetings to attend.  When on the Python arena, the answer would be quite clear, but for general C libraries like C-Blosc2, it is not that straightforward which ones are the most suited.

* Other outreaching activities would be to produce videos of the kind 'Blosc in 10 minutes', but not sure if this would be interesting for potential Blosc users (probably short tutorials in docs would be better suited).


Increase diversity
------------------

* **We strive to make our team as diverse as possible:**  we are actively looking into more women and people from a variety of cultures to join our team.  Update: we are glad to have Marta Iborra, our first female among us; thanks to the Python Software Foundation and NumFOCUS for providing funds for allowing this.

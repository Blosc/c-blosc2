ROADMAP for C-Blosc2
====================

C-Blosc2 is the new iteration of C-Blosc 1.x series, adding more features and better documentation.
This document lists the goals for a production release of C-Blosc2.

Naming conventions
------------------

Throughout this document we are going to use the next conventions:

* `C-Blosc1` will be used instead of the official `C-Blosc` name for referring to the 1.x series of the library.  This is to make the distinction between the C-Blosc 2.x series and C-Blosc 1.x series more explicit.
 

Existing features
-----------------

Right now (August 2019), the next features are already implemented (although they may require some refactoring or more tests):

* 64-bit containers.  The first-class container in C-Blosc2 is the `super-chunk` or, for brevity, `schunk`, that is made by smaller containers which are essentially C-Blosc1 32-bit containers.  The super-chunk can be backed or not by another container which is called a `frame` (see later).

* More filters: besides `shuffle` and `bitshuffle` already present in C-Blosc1, C-Blosc2 already implements:
  
  - `delta`: the stored blocks inside a chunk are diff'ed with respect to first block in the chunk.  The idea is that, in some situations, the diff will have more zeros than the original data, leading to better compression.
  
  - `trun_prec`: it zeroes the least significant bits of the mantissa of float32 and float64 types.  When combined with the `shuffle` or `bitshuffle` filter, this leads to more contiguous zeros, which are compressed better.
  
* A filter pipeline: the different filters can be pipelined so that the output of one can the input for the other.  A possible example is a `delta` followed by `shuffle`, or as described above, `trunc_prec` followed by `bitshuffle`.

* Prefilter support for user functions: allows to apply user-defined C callbacks prior to the filter pipeline.  See [test_prefilter.c](https://github.com/Blosc/c-blosc2/blob/master/tests/test_prefilter.c) for an example of use. 

* SIMD support for ARM (NEON): this allows for faster operation on ARM architectures.  Only `shuffle` is supported right now, but the idea is to implement `bitshuffle` for NEON too.

* SIMD support for PowerPC (ALTIVEC): this allows for faster operation on PowerPC architectures.  Both `shuffle`  and `bitshuffle` are supported; however, this has been done via a transparent mapping from SSE2 into ALTIVEC emulation in GCC 8, so performance could be better (but still, it is already a nice improvement over native C code; see PR https://github.com/Blosc/c-blosc2/pull/59 for details).  Thanks to Jerome Kieffer.

* Dictionaries: when a block is going to be compressed, C-Blosc2 can use a previously made dictionary (stored in the header of the super-chunk) for compressing all the blocks that are part of the chunks.  This usually improves the compression ratio, as well as the decompression speed, at the expense of a (small) overhead in compression speed.  Currently, it is only supported in the `zstd` codec, but would be nice to extend it to `lz4` and `blosclz` at least.

* Frames: allow to store super-chunks contiguously, either on-disk or in-memory.  When a super-chunk is backed by a frame, instead of storing all the chunks sparsely in-memory, they are serialized inside the frame container.  The frame can be stored on-disk too, meaning that persistence of super-chunks is supported.

* Sparse storage support for super-chunks.  This means that each chunk is stored in a separate file, as well as the metadata.  This is be the counterpart of in-memory super-chunk, and allows for more efficient updates than in frames (i.e. avoiding 'holes' in monolithic files).

* Meta-layers: optionally, the user can add meta-data for different uses and in different layers.  For example, one may think on providing a meta-layer for [NumPy](http://www.numpy.org) so that most of the meta-data for it is stored in a meta-layer; then, one can place another meta-layer on top of the latter for adding more high-level info if desired (e.g. geo-spatial, meteorological...).

* Nice markup for documentation: we are currently using a combination of Sphinx + Doxygen + Breathe for documenting the C-API.  Thanks to Alberto Sabater for contributing the support for this.


Actions to be done
------------------

* Support for network storage.  Sparse storage already supports disk I/O, but it could be nice to expand this support to network I/O (using S3, HDFS or others).  We need to consider which backends are more useful for users, and also that the implementation should be feasible (read: reasonably simple) for a C library as C-Blosc2. 

* Plugin capabilities for allowing users to add more filters and codecs.  There should also be a plugin register capability so that the info about the new filters and codecs can be persistent and propagated to different machines.

* Checksums: the frame can benefit from having a checksum per every chunk/index/metalayer.  This will provide more safety towards frames that are damaged for whatever reason.  Also, this would provide better feedback when trying to determine the parts of the frame that are corrupted.  Candidates for checksums can be the xxhash32 or xxhash64, depending on the gaols (to be decided).

* Documentation: utterly important for attracting new users and making the life easier for existing ones.  Important points to have in mind here:

  - Quality of API docstrings: is the mission of the functions or data structures clearly and succinctly explained? Are all the parameters explained?  Is the return value explained?  What are the possible errors that can be returned?  [Mostly completed by Alberto Sabater].
  
  - Tutorials/book: besides the API docstrings, more documentation materials should be provided, like tutorials or a book about Blosc (or at least, the beginnings of it).  Due to its adoption in GitHub and Jupyter notebooks, one of the most extended and useful markup systems is MarkDown, so this should also be the first candidate to use here.
  
* Wrappers for other languages: Python and Java are the most obvious candidates, but others like R or Julia would be nice to have.  Still not sure if these should be produced and maintained by the Blosc development team, or leave them for third-party players that would be interested. [The steering [council discussed this](https://github.com/Blosc/governance/blob/master/steering_council_minutes/2020-03-26.md), and probably just the Python wrapper should be maintained by Blosc maintainers themselves, while the other languages should be maintained by the community.]

* It would be nice to use [LGTM](https://lgtm.com), a CI-friendly analyzer for security.

* Lock support for super-chunks: when different processes are accessing concurrently to super-chunks, make them to sync properly by using locks, either on-disk (frame-backed super-chunks), or in-memory. Such a lock support would be configured in build time, so it could be disabled with a cmake flag.


Outreaching
-----------

* Improve the Blosc website: create a nice, modern-looking and easy to navigate website so that new potential users can see at first glimpse what's Blosc all about and power-users can access the documentation part easily.  Ideally, a site-only search box would be great (sphix-based docs would offer this for free).

* Attend to meetings and conferences: it is very important to plan going to conferences for advertising C-Blosc2 and meeting people in-person.  Questions is, which meetings?  When on the Python arena, the answer would be quite clear, but for general C libraries like C-Blosc2, it is not that straightforward which ones are the most suited.
  
* Other outreaching activities would be to produce videos of the kind 'Blosc in 10 minutes', but not sure if this would be interesting for potential Blosc users (probably short tutorials in docs would be better suited).

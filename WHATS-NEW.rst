New features in C-Blosc2
========================

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

More info about the `improved capabilities of C-Blosc2 can be found in this talk <https://www.blosc.org/docs/Caterva-HDF5-Workshop.pdf>`_.

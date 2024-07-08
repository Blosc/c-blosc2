Blosc Chunk Format
==================

A regular chunk is composed of a header and a blocks section::

    +---------+--------+
    |  header | blocks |
    +---------+--------+

Also, there are the so-called lazy chunks that do not have the actual compressed data,
but only meta-information about how to read it. Lazy chunks typically appear when reading
data from persistent media.  A lazy chunk has header and bstarts sections in place and
in addition, an additional trailer for allowing to read the data blocks::

    +---------+---------+---------+
    |  header | bstarts | trailer |
    +---------+---------+---------+

All these sections are described below.  Note that the bstarts section is described as
part of the blocks section.

*Note:* All integer types in this document are stored in little endian.


Header
------

Blosc (as of version 1.0.0) has the following 16 byte header that stores
information about the compressed chunk::

    |-0-|-1-|-2-|-3-|-4-|-5-|-6-|-7-|-8-|-9-|-A-|-B-|-C-|-D-|-E-|-F-|
      ^   ^   ^   ^ |     nbytes    |   blocksize   |     cbytes    |
      |   |   |   |
      |   |   |   +--typesize
      |   |   +------flags
      |   +----------versionlz
      +--------------version

Starting in Blosc 2.0.0, there is an extension of the header above that allows
for encoding blocks with a filter pipeline::

  1+|-0-|-1-|-2-|-3-|-4-|-5-|-6-|-7-|-8-|-9-|-A-|-B-|-C-|-D-|-E-|-F-|
    |     filters           | ^ | ^ |     filters_meta      | ^ | ^ |
                              |   |                           |   |
                              |   +- compcode_meta            |   +-blosc2_flags
                              +- user-defined codec           +-reserved

:version:
    (``uint8``) Blosc format version.

:versionlz:
    (``uint8``) Version of the *format* of the internal compressor used (normally always 1).

:flags and compressor enumeration:
    (``bitfield``) The flags of the buffer

    :bit 0 (``0x01``):
        Whether the byte-shuffle filter has been applied or not.
    :bit 1 (``0x02``):
        Whether the internal buffer is a pure memcpy or not.
    :bit 2 (``0x04``):
        Whether the bit-shuffle filter has been applied or not.
    :bit 3 (``0x08``):
        Whether the delta codec has been applied or not.
    :bit 4 (``0x10``):
        If set, blocks are *not* split into multiple compressed data streams.
    :bit 5 (``0x20``):
        Part of the enumeration for compressors.
    :bit 6 (``0x40``):
        Part of the enumeration for compressors.
    :bit 7 (``0x80``):
        Part of the enumeration for compressors.

    Note:: If both bit 0 and bit 2 are both set, that means that an
        extended header (see above) is used.

    The last three bits form an enumeration that allows for the use of alternative compressors.

    :``0``:
        ``blosclz``
    :``1``:
        ``lz4`` or ``lz4hc``
    :``2``:
        reserved (slot previously occupied by ``snappy`` and free now)
    :``3``:
        ``zlib``
    :``4``:
        ``zstd``
    :``5``:
        Reserved
    :``6``:
        The compressor is defined in the user-defined codec slot (see below).
    :``7``:
        The compressor is defined in the super-chunk.

:typesize:
    (``uint8``) Number of bytes for the atomic type.

:nbytes:
    (``int32``) Uncompressed size of the buffer (this header is not included).

:blocksize:
    (``int32``) Size of internal blocks.

:cbytes:
    (``int32``) Compressed size of the buffer (including this header).

:filters:
    (``uint8``) Filter ID.

    :``0``:
        No shuffle (for compatibility with Blosc1).
    :``0``:
        No filter.
    :``1``:
        Byte-wise shuffle.
    :``2``:
        Bit-wise shuffle.
    :``3``:
        Delta filter.
    :``4``:
        Truncate precision filter.
    :``5``:
        Sentinel. IDs larger than this are either global registered or user-defined filters.

    The filter pipeline has 6 reserved slots for the filters IDs. They are applied sequentially
    to the chunk according to their index (in increasing order). The type of filter applied is
    specified by the ID. Each ID has an associated field in `filters_meta` that can contain metadata
    about the filter.

:udcodec:
    (``uint8``) User-defined codec identifier.

:compcode_meta:
    (``uint8``) Compression codec metadata.

    Metadata associated with the compression codec.

:filters_meta:
    (``uint8``) Filter metadata associated to each filter ID.

    Metadata associated with the filter ID.

:blosc2_flags:
    (``bitfield``) The flags for a Blosc2 buffer.

    :bit 0 (``0x01``):
        Whether the codec uses dictionaries or not.
    :bit 1 (``0x02``):
        Whether the header is extended with +32 bytes coming right after this byte.
    :bit 2 (``0x04``):
        Whether the codec is stored in a byte previous to this compressed buffer
        or it is in the global `flags` for chunk.
    :bit 3 (``0x08``):
        Whether the chunk is 'lazy' or not.
    :bits 4, 5 and 6:
        Indicate special values for the entire chunk.
            :``0``:
                No special values.
            :``1``:
                A run of zeros.
            :``2``:
                A run of NaN (Not-a-Number) floats (whether f32 or f64 depends on typesize).
            :``3``:
                Run-length of a value that follows the header (i.e. no blocks section).
            :``4``:
                Values that are not initialized.
            :``5``:
                Reserved.
            :``6``:
                Reserved.
            :``7``:
                Reserved.
    :bit 7 (``0x80``):
        Indicate whether codec has been instrumented or not.


Blocks
------

The blocks section is composed of a list of offsets to the start of each block, an optional dictionary to aid in
compression, and finally a list of compressed data streams::

    +=========+======+=========+
    | bstarts | dict | streams |
    +=========+======+=========+

Each block is equal-sized as specified by the `blocksize` header field. The size of the last block can be shorter
or equal to the rest.

**Block starts**

The *block starts* section contains a list of offsets `int32 bstarts` that indicate where each block starts in the
chunk. These offsets are relative to the start of the chunk and point to the start of one or more compressed
data streams containing the contents of the block::

    +=========+=========+========+=========+
    | bstart0 | bstart1 |   ...  | bstartN |
    +=========+=========+========+=========+

**Dictionary (optional)**

*Only for C-Blosc2*

Dictionaries are small datasets that are known to be repeated a lot and can help to compress data in blocks better.
The dictionary section contains the size of the dictionary `int32_t dsize` followed by the dictionary data::

    +=======+=================+
    | dsize | dictionary data |
    +=======+=================+

**Compressed Data Streams**

Compressed data streams are the compressed set of bytes that are passed to codecs for decompression. Each compressed
data stream (`uint8_t* cdata`) is stored with the size of the stream (`int32_t csize`) preceding it::

    +=======+=======+
    | csize | cdata |
    +=======+=======+

There are a couple of special cases for `int32_t csize`.  If zero, that means that the stream is fully made of zeros, *and* there is not a `cdata` section. The actual size of the stream is inferred from `blocksize` and whether or not the block is split.
If negative, the stream is stored like this::

    +=======+=======+=======+
    | csize | token | cdata |
    +=======+=======+=======+

where **token** is a byte for providing different meanings to `int32_t csize`:

:bit 0:
    Repeated byte (stream is a run-length of bytes). This byte, representing the repeated value in the stream, is encoded in the LSB of the `int32_t csize`. In this case there is not a `cdata` section. Note that repeated zeros cannot happen here (already handled by the `csize == 0` case above).
:bits 1 and 2:
    Reserved for two-codecs in a row. TODO: complete description
:bits 3, 4 and 5:
    Reserved for secondary codec. TODO: complete description
:bits 6 and 7:
    Reserved for future use.

If bit 4 of the `flags` header field is set, each block is stored in a single data stream::

    +=========+
    | stream0 |
    +=========+
    | block0  |
    +=========+

If bit 4 of the `flags` header is *not* set, each block can be stored using multiple data streams::

    +=========+=========+=========+=========+
    | stream0 | stream1 |    ...  | streamN |
    +=========+=========+=========+=========+
    | block0                                |
    +=========+=========+=========+=========+

The uncompressed size for each block is equivalent to the `blocksize` field in the header, with the exception
of the last block which may be equal to or less than the `blocksize`.

Trailer
-------

This is an optional section, mainly for lazy chunks use.  A lazy chunk is similar to a regular one, except that
only the meta-information has been loaded.  The actual data from blocks is 'lazily' only loaded on demand.
This allows for improved selectivity, and hence less input bandwidth demands, during partial chunk reads
(e.g. `blosc1_getitem`) from data that is on disk.

It is arranged like this::

    +=========+=========+========+========+=========+
    | nchunk  | offset  | bsize0 |   ...  | bsizeN |
    +=========+=========+========+========+=========+

:nchunk:
    (``int32_t``) The number of the chunk in the super-chunk.

:offset:
    (``int64_t``) The offset of the chunk in the frame (contiguous super-chunk).

:bsize0 .. bsizeN:
    (``int32_t``) The sizes in bytes for every block.

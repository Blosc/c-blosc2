Blosc2 Frame Format
===================

Blosc (as of version 2.0.0) has a frame format that allows for the storage of different Blosc data chunks sequentially,
either in-memory or on-disk.

The frame is composed of a header, a chunks section, and a trailer::

    +---------+--------+---------+
    |  header | chunks | trailer |
    +---------+--------+---------+

Each of the three parts of the frame are variable length; with the header and trailer both stored using the
`msgpack <https://msgpack.org>`_ format.

*Note:*  All integer types are stored in little endian.


Header
------

The header contains information needed to decompress the Blosc chunks contained in the frame. It is encoded using
`msgpack <https://msgpack.org>`_ and the format is as follows::

    |-0-|-1-|-2-|-3-|-4-|-5-|-6-|-7-|-8-|-9-|-A-|-B-|-C-|-D-|-E-|-F-|-10|-11|-12|-13|-14|-15|-16|-17|
    | 9X| aX| "b2frame\0"                   | d2| header_size   | cf| frame_size                    |
    |---|---|-------------------------------|---|---------------|---|-------------------------------|
      ^   ^       ^                           ^                   ^
      |   |       |                           |                   |
      |   |       |                           |                   +--[msgpack] uint64
      |   |       |                           |
      |   |       |                           +--[msgpack] int32
      |   |       +---magic number, currently "b2frame"
      |   +------[msgpack] str with 8 elements
      +---[msgpack] fixarray with X=0xD (13) elements

    |-18|-19|-1A|-1B|-1C|-1D|-1E|-1F|-20|-21|-22|-23|-24|-25|-26|-27|-28|-29|-2A|-2B|-2C|-2D|-2E|
    | a4|_f0|_f1|_f2|_f3| d3| uncompressed_size             | d3| compressed_size               |
    |---|---|---|---|---|---|-------------------------------|---|-------------------------------|
      ^   ^   ^   ^   ^   ^                                   ^
      |   |   |   |   |   |                                   +--[msgpack] int64
      |   |   |   |   |   +--[msgpack] int64
      |   |   |   |   +-- reserved flags
      |   |   |   +--codec_flags (see below)
      |   |   +---reserved flags
      |   +------general_flags (see below)
      +---[msgpack] str with 4 elements (flags)

    |-2F|-30|-31|-32|-33|-34|-35|-36|-37|-38|-39|-3A|-3B|-3C|-3D|-3E|-3F|
    | d2| type_size     | d2| chunk_size    | d1| tcomp | d1|tdecomp| cX|
    |---|---------------|---|---------------|---|-------|---|-------|---|
      ^                   ^                   ^     ^     ^     ^     ^
      |                   |                   |     |     |     |     +--[msgpack] bool for has_usermeta
      |                   |                   |     |     |     +--number of threads for decompression
      |                   |                   |     |     +-- [msgpack] int16
      |                   |                   |     +--number of threads for compression
      |                   |                   +---[msgpack] int16
      |                   +------[msgpack] int32
      +---[msgpack] int32

The filter pipeline is stored next in the header. It contains 8 slots, one for each filter that can be applied. For
each slot there is a byte used to store the filter code in `filter_codes` and an associated byte used to store any
possible filter meta-info in `filter_meta`::


    |-40|-41|-42|-43|-44|-45|-46|-47|-48|-49|-4A|-4B|-4C|-4D|-4E|-4F|-50|-51|
    | d2| X | filter_codes                  | filter_meta                   |
    |---|---|-------------------------------|-------------------------------|
      ^   ^
      |   |
      |   +--number of filters
      +--[msgpack] fixext 16

At the end of the header *metalayers* are stored which contain meta-information about the chunked data stored in the
frame. It is up to the user to store whatever data they want with the only (strong) suggestion that they be stored
using the msgpack format. Here is the format for the *metalayers*::

  |-52|-53|-54|-55|-56|-----------------------
  | 93| cd| idx   | de| map_of_metalayers
  |---|---------------|-----------------------
    ^   ^    ^      ^
    |   |    |      |
    |   |    |      +--[msgpack] map of name/offset pairs
    |   |    +--size of the map
    |   +--[msgpack] uint16
    +-- [msgpack] fixarray with 3 elements

:header_size:
    (``int32``) Size of the header of the frame (including metalayers).

:frame_size:
    (``uint64``) Size of the whole frame (including compressed data).

:general_flags:
    (``uint8``) General flags.

    :``0`` to ``3``:
        Format version.
    :``4`` and ``5``:
        Enumerated for chunk offsets.

        :``0``:
            32-bit
        :``1``:
            64-bit
        :``2``:
            128-bit
        :``3``:
            256-bit
    :``6``:
        Chunks of fixed length (0) or variable length (1)
    :``7``:
        Reserved

:codec_flags:
    (``uint8``) Compressor enumeration (defaults for all the chunks in storage).

    :``0`` to ``3``:
        Enumerated for codecs (up to 16).
        
        :``0``:
            ``blosclz``
        :``1``:
            ``lz4`` or ``lz4hc``
        :``2``:
            ``snappy``
        :``3``:
            ``zlib``
        :``4``:
            ``zstd``
        :``5``:
            ``lizard``
    :``4`` to ``7``: Compression level (up to 16)

:reserved_flags:
    (``uint8``) Space reserved.

:uncompressed_size:
    (``int64``) Size of uncompressed data in frame (excluding metadata).

:compressed_size:
    (``int64``) Size of compressed data in frame (excluding metadata).

:type_size:
    (``int32``) Size of each item.

:chunk_size:
    (``int32``) Size of each data chunk.  0 if not a fixed chunksize.

:tcomp:
    (``int16``) Number of threads for compression.  If 0, same than `cctx`.

:tdecomp:
    (``int16``) Number of threads for decompression.  If 0, same than `dctx`.

:map of metalayers:
    This is a *msgpack-formattted* map for the different metalayers.  The keys will be a string (0xa0 + namelen) for
    the names of the metalayers, followed by an int32 (0xd2) for the *offset* of the value of this metalayer.  The
    actual value will be encoded as a bin32 (0xc6) value later in frame.

Chunks
------

The chunks section is composed of one or more Blosc data chunks followed by an index chunk::

    +========+========+========+========+===========+
    | chunk0 | chunk1 |   ...  | chunkN | chunk idx |
    +========+========+========+========+===========+

Each chunk is stored sequentially and follows the format described in the
`chunk format <README_CHUNK_FORMAT.rst>`_ document.

The `chunk idx` is a Blosc chunk containing the indexes to each chunk in this section.  The data in the
chunk is a list of (32-bit, 64-bit or more, see above) offsets to each chunk. The index chunk follows
the regular Blosc chunk format and can be compressed.

**Note:** The offsets can take *special values* so as to represent chunks with run-length (equal) values.
The codification for the offsets is as follows::

    +========+========+========+========+
    | byte 0 | byte 1 |   ...  | byte N |
    +========+========+========+========+
                                   ^
                                   |
                                   +--> Byte for special values

If the most significant bit (7) of the most significant byte above (byte N, as little endian is used) is set,
that represents a chunk with a run-length of special values.  The supported special values are:

:special_values:
    (``uint8``) Flags for special values.

        :``0``:
            A run-length of zeros.
        :``1``:
            A run-length of NaNs. The size of the NaN depends on the typesize.
        :``2``:
            Reserved.
        :``3``:
            Reserved.
        :``4``:
            Reserved.
        :``5``:
            Reserved.
        :``6``:
            Reserved.
        :``7``:
            Indicates a special value.  If not set, a regular value.


Trailer
-------

The trailer for the frame is encoded via `msgpack <https://msgpack.org>`_ and contains a user meta data chunk and
a fingerprint.::

    |-0-|-1-|-2-|-3-|-4-|-5-|-6-|====================|---|---------------|---|---|=================|
    | 9X| aX| c6| usermeta_len  |   usermeta_chunk   | ce| trailer_len   | d8|fpt| fingerprint     |
    |---|---|---|---------------|====================|---|---------------|---|---|=================|
      ^   ^   ^       ^                                ^       ^           ^   ^
      |   |   |       |                                |       |           |   +-- fingerprint type
      |   |   |       |                                |       |           +--[msgpack] fixext 16
      |   |   |       |                                |       +-- trailer length (network endian)
      |   |   |       |                                +--[msgpack] uint32 for trailer length
      |   |   |       +--[msgpack] usermeta length (network endian)
      |   |   +---[msgpack] bin32 for usermeta
      |   +------[msgpack] int8 for trailer version
      +---[msgpack] fixarray with X=4 elements

The *usermeta* chunk which stores the user meta data can change in size during the lifetime of the frame.
This is an important feature and the reason why the *usermeta* is stored in the trailer and not in the header.

:usermeta_len:
    (``int32``) The length of the usermeta chunk.

:usermeta_chunk:
    (``varlen``) The usermeta chunk (a Blosc chunk).

:trailer_len:
    (``uint32``) Size of the trailer of the frame (including usermeta chunk).

:fpt:
    (``int8``) Fingerprint type:  0 -> no fp; 1 -> 32-bit; 2 -> 64-bit; 3 -> 128-bit

:fingerprint:
    (``uint128``) Fix storage space for the fingerprint, padded to the left.

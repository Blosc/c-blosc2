Blosc2 Frame Format
===================

Blosc (as of version 2.0.0) has a frame format that allows to store different data chunks sequentially, either in-memory or on-disk.

The frame is composed by a header, a chunks section and a trailer, which are variable-length and stored sequentially::

    +---------+--------+---------+
    |  header | chunks | trailer |
    +---------+--------+---------+

These are described below.

The header section
------------------

The header of a frame is encoded via  `msgpack <https://msgpack.org>`_ and it follows the next format::

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
      +---[msgpack] fixarray with X=0xC (12) elements

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

Then it follows the info about the filter pipeline.  There is place for a pipeline that is 8 slots deep, and there is a reserved byte per every filter code and another byte for a possible associated meta-info::

    |-40|-41|-42|-43|-44|-45|-46|-47|-48|-49|-4A|-4B|-4C|-4D|-4E|-4F|-50|-51|
    | d2| X | filter_codes                  | filter_meta                   |
    |---|---|-------------------------------|-------------------------------|
      ^   ^
      |   |
      |   +--number of filters
      +--[msgpack] fixext 16

In addition, a frame can be completed with meta-information about the stored data; these data blocks are called metalayers and it is up to the user to store whatever data they want there, with the only (strong) suggestion that they have to be in the msgpack format.  Here it is the format for the case that there exist some metalayers::

  |-52|-53|-54|-55|-56|-----------------------
  | 93| cd| idx   | de| map_of_metalayers
  |---|---------------|-----------------------
    ^   ^    ^      ^
    |   |    |      |
    |   |    |      +--[msgpack] map 16 with N keys
    |   |    +--size of the map (index) of offsets
    |   +--[msgpack] uint16
    +-- [msgpack] fixarray with 3 elements

:map of metalayers:
    This is a *msgpack-formattted* map for the different metalayers.  The keys will be a string (0xa0 + namelen) for the names of the metalayers, followed by an int32 (0xd2) for the *offset* of the value of this metalayer.  The actual value will be encoded as a bin32 (0xc6) value later in frame.

Description for different fields in header
__________________________________________

:header_size:
    (``int32``) Size of the header of the frame (including metalayers).

:frame_size:
    (``uint64``) Size of the whole frame (including compressed data).

:general_flags:
    (``uint8``) General flags.

    :``0`` to ``3``:
        Format version
    :``4`` and ``5``: Enumerated for chunk offsets.
        :``0``:
            16-bit
        :``1``:
            32-bit
        :``2``:
            64-bit
        :``3``:
            Reserved
    :``6``:
        Chunks of fixed length (0) or variable length (1)
    :``7``:
        Reserved

:filter_flags:
    (``uint8``) Filter flags that are the defaults for all the chunks in storage.

    :bit 0:
        If set, blocks are *not* split in sub-blocks.
    :bit 1:
        Filter pipeline is described in bits 3 to 6; else in `_filter_pipeline` system metalayer.
    :bit 2:
        Reserved
    :bit 3:
        Whether the shuffle filter has been applied or not.
    :bit 4:
        Whether the internal buffer is a pure memcpy or not.
    :bit 5:
        Whether the bitshuffle filter has been applied or not.
    :bit 6:
        Whether the delta codec has been applied or not.
    :bit 7:
        Reserved

:codec_flags:
    (``uint8``) Compressor enumeration (defaults for all the chunks in storage).

    :``0`` to ``3``: Enumerated for codecs (up to 16)
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


The chunks section
------------------

Here there is the actual data chunks stored sequentially::

    +========+========+========+===========+
    | chunk0 | chunk1 |   ...  | chunk idx |
    +========+========+========+===========+

The different chunks are described in the `chunk format <README_CHUNK_FORMAT.rst>`_ document.  The `chunk idx` is an index for the different chunks in this section.  It is made by the 64-bit offsets to the different chunks and compressed into a new chunk, following the regular Blosc chunk format.


The trailer section
-------------------

Here it is data that can change in size, mainly the `metauser` chunk::

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

Description for different fields in trailer
___________________________________________

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

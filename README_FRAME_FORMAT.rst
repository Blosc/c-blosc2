Blosc2 Frame Format
===================

Blosc (as of version 2.0.0) has a framing format that allows to store different data chunks sequentially, either in-memory or on-disk.  The header of a frame is encoded via  `msgpack <https://msgpack.org>`_ and it follows the next format::

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
      +---[msgpack] fixed array with X=0xB (11, no metalayers) or X=0xC (12) elements

    |-18|-19|-1A|-1B|-1C|-1D|-1E|-1F|-20|-21|-22|-23|-24|-25|-26|-27|-28|-29|-2A|-2B|-2C|-2D|-2E|
    | a4|_f0|_f1|_f2|_f3| d3| uncompressed_size             | d3| compressed_size               |
    |---|---|---|---|---|---|-------------------------------|---|-------------------------------|
      ^   ^   ^   ^   ^   ^                                   ^
      |   |   |   |   |   |                                   +--[msgpack] int64
      |   |   |   |   |   +--[msgpack] int64
      |   |   |   |   +-- reserved flags
      |   |   |   +--codec_flags (see below)
      |   |   +---filter_flags (see below)
      |   +------general_flags (see below)
      +---[msgpack] str with 4 elements (flags)

    |-2F|-30|-31|-32|-33|-34|-35|-36|-37|-38|-39|-3A|-3B|-3C|-3D|-3E|-3F|
    | d2| type_size     | d2| chunk_size    | d1| tcomp | d1|tdecomp| cX|
    |---|---------------|---|---------------|---|-------|---|-------|---|
      ^                   ^                   ^     ^     ^     ^     ^
      |                   |                   |     |     |     |     +--[msgpack] bool for has_metalayers
      |                   |                   |     |     |     +--number of threads for decompression
      |                   |                   |     |     +-- [msgpack] int16
      |                   |                   |     +--number of threads for compression
      |                   |                   +---[msgpack] int16
      |                   +------[msgpack] int32
      +---[msgpack] int32

  In addition, a frame can be completed with meta-information about the stored data; these data blocks are called metalayers and it is up to the user to store whatever data they want there, with the only (strong) suggestion that they have to be in the msgpack format.  Here it is the format for the case that there exist some metalayers::

    |-40|-41|-42|-43|-44|-----------------------
    | 93| cd| idx   | de| map_of_metalayers
    |---|---------------|-----------------------
      ^   ^    ^      ^
      |   |    |      |
      |   |    |      +--[msgpack] map 16 with N keys
      |   |    +--size of the map (index) of offsets
      |   +--[msgpack] uint16
      +-- [msgpack] array with 3 elements


:map of metalayers:
    This is a *msgpack-formattted* map for the different metalayers.  The keys will be a string (0xa0 + namelen) for the names of the metalayers, followed by an int32 (0xd2) for the *offset* of the value of this metalayer.  The actual value will be encoded as a bin32 (0xc6) value later in frame.


Description for different fields
--------------------------------

:header_size:
    (``int32``) Size of the header of the frame (including metalayers).

:frame_size:
    (``uint64``) Size of the whole frame (including compressed data).

:general_flags:
    (``uint8``) General flags.

    :``0`` and ``1``:
        Format version
    :``2`` and ``3``: Enumerated
        :``0``:
            Reserved (regular chunk?)
        :``1``:
            Reserved (extended chunk?)
        :``2``:
            Frame
        :``3``:
            Reserved (extended frame?)
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
        Reserved.

:filter_flags:
    (``uint8``) Filter flags that are the defaults for all the chunks in storage.

    :``0``:
        Data is memcpy'ed
    :``1``:
        Blocks are splitted
    :``2`` and ``3``: Enumerated for filters
        :``0``:
            No shuffle
        :``1``:
            Shuffle
        :``2``:
            Bitshuffle
        :``3``:
            Reserved
    :``4`` to ``7``:
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

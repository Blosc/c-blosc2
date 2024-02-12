Blosc2 Contiguous Frame Format
==============================

Blosc (as of version 2.0.0) has a contiguous frame format (cframe for short) that allows for the storage of
different Blosc data chunks contiguously, either in-memory or on-disk.

The frame is composed of a header, a chunks section, and a trailer::

    +---------+--------+---------+
    |  header | chunks | trailer |
    +---------+--------+---------+

Each of the three parts of the frame are variable length; with the header and trailer both stored using the
`msgpack <https://msgpack.org>`_ format.

*Note:*  Integer types are stored in big endian for msgpack format. All the rest are stored in little endian.


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
      |   +------[msgpack] str with X=8 elements
      +---[msgpack] fixarray with X=0xE (14) elements

    |-18|-19|-1A|-1B|-1C|-1D|-1E|-1F|-20|-21|-22|-23|-24|-25|-26|-27|-28|-29|-2A|-2B|-2C|-2D|-2E|
    | a4|_f0|_f1|_f2|_f3| d3| uncompressed_size             | d3| compressed_size               |
    |---|---|---|---|---|---|-------------------------------|---|-------------------------------|
      ^   ^   ^   ^   ^   ^                                   ^
      |   |   |   |   |   |                                   +--[msgpack] int64
      |   |   |   |   |   +--[msgpack] int64
      |   |   |   |   +-- other flags
      |   |   |   +--codec_flags (see below)
      |   |   +---frame_type (see below)
      |   +------general_flags (see below)
      +---[msgpack] str with 4 elements (flags)

    |-2F|-30|-31|-32|-33|-34|-35|-36|-37|-38|-39|-3A|-3B|-3C|-3D|-3E|-3F|-40|-41|-42|-43|-44|
    | d2| type_size     | d2| block_size    | d2| chunk_size    | d1| tcomp | d1|tdecomp| cX|
    |---|---------------|---|---------------|---|---------------|---|-------|---|-------|---|
      ^                   ^                   ^                   ^     ^     ^     ^     ^
      |                   |                   |                   |     |     |     |     +-- [msgpack] bool for has_vlmetalayers
      |                   |                   |                   |     |     |     +-- number of threads for decompression
      |                   |                   |                   |     |     +-- [msgpack] int16
      |                   |                   |                   |     +-- number of threads for compression
      |                   |                   |                   +-- [msgpack] int16
      |                   |                   +-- [msgpack] int32
      |                   +-- [msgpack] int32
      +-- [msgpack] int32

The filter pipeline is stored next in the header. It contains 6 slots, one for each filter that can be applied. For
each slot there is a byte used to store the filter ID in `filters` and an associated byte used to store any
possible filter meta-info in `filters_meta`::


    |-45|-46|-47|-48|-49|-4A|-4B|-4C|-4D|-4E|-4F|-50|-51|-52|-53|-54|-55|-56|
    | d2| X | filters               |_f4|_f5| filters_meta          |   |   |
    |---|---|-------------------------------|-------------------------------|
      ^   ^                           ^   ^                           ^   ^
      |   |                           |   |                           |   +-- reserved
      |   |                           |   |                           +-- reserved
      |   |                           |   +-- compcodec_meta
      |   |                           +-- udcodec
      |   +--number of filters
      +--[msgpack] fixext 16

The last section of the header is for the *metalayers*, which contain meta-information about the data in the
frame.  It is mandatory the use of the msgpack format for storing them, although the user may use another format
(e.g. json) encoded as msgpack (in this case as a string). Here it is the format for the *metalayers*::

    |-57|-58|-59|-5A|-5B|-5C|-5D|====================|---|---|---|================|
    | 93| cd| idx   | de| size  | meta keys/values   | dc|  idy  | meta content   |
    |---|---|-------|---|---|---|====================|---|-------|================|
     ^   ^      ^     ^     ^             ^            ^     ^            ^
     |   |      |     |     |             |            |     |            +-- [msgpack] bin32
     |   |      |     |     |             |            |     +-- number of elements in the array
     |   |      |     |     |             |            +-- [msgpack] array16
     |   |      |     |     |             +-- [msgpack] fixstr/int32
     |   |      |     |     +-- number of elements in the map
     |   |      |     +-- [msgpack] map of name/offset pairs
     |   |      +-- size of metalayers
     |   +-- [msgpack] uint16
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

:frame_type:
    (``uint8``) The type of frame.

    :``0`` to ``3``:
        Enumerated for the type of frame (up to 16).

        :``0``:
            ``Contiguous``
        :``1``:
            ``Sparse (directory)``
        :``2 to 15``:
            Reserved

    :``4`` to ``7``: Reserved for user-defined frame types (up to 16)

:codec_flags:
    (``uint8``) Compressor enumeration (defaults for all the chunks in storage).

    :``0`` to ``3``:
        Enumerated for codecs (up to 16).

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
            reserved
        :``6``:
            The compressor is defined in the user-defined codec slot (see below).
        :``7 to 15``:
            Reserved
    :``4`` to ``7``: Compression level (up to 16)

:other_flags:
    (``uint8``) Split mode and others.

    :``0`` to ``1``:
            Enumerated for splitmodes (up to 4).

            :``0``:
                ``BLOSC_ALWAYS_SPLIT``
            :``1``:
                ``BLOSC_NEVER_SPLIT``
            :``2``:
                ``BLOSC_AUTO_SPLIT``
            :``3``:
                ``BLOSC_FORWARD_COMPAT_SPLIT``
    :``2`` to ``7``: Reserved.

:uncompressed_size:
    (``int64``) Size of uncompressed data in frame (excluding metadata).

:compressed_size:
    (``int64``) Size of compressed data in frame (excluding metadata).

:type_size:
    (``int32``) Size of each item.

:block_size:
    (``int32``) Size of data blocks when all data chunks are equal size (the only case supported so far).

:chunk_size:
    (``int32``) Size of each data chunk.  0 if not a fixed chunksize (not supported yet).

:tcomp:
    (``int16``) Number of threads for compression.  If 0, same than `cctx`.

:tdecomp:
    (``int16``) Number of threads for decompression.  If 0, same than `dctx`.

:udcodec:
    (``uint8``) User-defined codec identifier.

:compcode_meta:
    (``uint8``) Compression codec metadata associated with the compression codec. Only used in user-defined codecs.

:map of metalayers:
    This is a *msgpack-formatted* map for the different metalayers.  The keys will be a string (0xa0 + namelen) for
    the names of the metalayers, followed by an int32 (0xd2) for the *offset* of the value of this metalayer.  The
    actual value will be encoded as a bin32 (0xc6) value later in header.

Dumping info in metalayers
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note:** The method in this section only works for Unix.

Here it is a trick for printing the content of metalayers using the nice set of
`msgpack-tools <https://github.com/ludocode/msgpack-tools>`_ command line utilities.  After installing the package we
can do e.g.::

    $ msgpack2json -Bi plugins/test_data/example_day_month_temp.b2nd
    ["b2frame\u0000",166,3947,"\u0012\u0000P\u0003",5472,3682,4,684,1368,1,1,false,
     "ext:6:base64:AAAAAAABAAAAAAAAAAAAAA==",[17,{"b2nd":107},
     ["lgACktMAAAAAAAABkNMAAAAAAAAAA5LSAAAAbtIAAAADktIAAAA50gAAAAPbAAAABXVpbnQ4"]]]

Here we see that we have a `b2nd` metalayer that starts at position 107; but as there is a msgpack `bin32` there, we
must add 5 bytes (4 bytes for an int32 and 1 byte for the msgpack `bin32` header), so the actual starting position is
112 (107 + 5).  Also, although we don't know the length of the `b2nd` metalayer, it is typically less than 100 bytes,
so let's err on the safe side and dump the first 1000 bytes, just in case::

    $ dd bs=1 skip=112 count=1000 <  plugins/test_data/example_day_month_temp.b2nd | msgpack2json -B
    <snip>
    [0,2,[400,3],[110,3],[57,3],0,"|u1"]

By having a look at the
`Blosc2 NDim metalayer format <https://github.com/Blosc/c-blosc2/blob/main/README_B2ND_METALAYER.rst>`_
one may note that the number of dimensions is 2, `shape` is [400, 3], `chunkshape` is [110, 3], blockshape is
[57, 3], dtype format is 0 (NumPy) and dtype is "|u1", which is a NumPy shortcut for `np.uint8`.

Chunks
------

The chunks section is composed of one or more Blosc data chunks followed by an index chunk::

    +========+========+========+========+===========+
    | chunk0 | chunk1 |   ...  | chunkN | chunk idx |
    +========+========+========+========+===========+

Each chunk is stored contiguously one after the other, and each follows the format described in the
`chunk format <https://github.com/Blosc/c-blosc2/blob/main/README_CHUNK_FORMAT.rst>`_ document.

The `chunk idx` is a Blosc2 chunk containing the offsets (starting from the beginning of the header)
to each chunk in this section.  The data in the chunk is a list of offsets (they can be 32-bit, 64-bit
or more, see above; currently only 64-bit are implemented) to each chunk.  The index chunk follows the
regular Blosc2 chunk format and can be compressed (the default).

**Note:** The offsets can take *special values* so as to represent chunks with run-length (equal) values.
The codification for the offsets is as follows::

    +========+========+========+========+
    | byte 0 | byte 1 |   ...  | byte N |
    +========+========+========+========+
                                   ^
                                   |
                                   +--> Byte for special values

If the most significant bit (7) of the most significant byte above (byte N, as little endian is used) is set,
that represents a chunk with a run-length of special values.

More specifically the **byte for special values** has this format:

:bits 0, 1 and 2:
     Indicate special values for the entire chunk.

     :``0``:
        Reserved.
     :``1``:
         A run of zeros.
     :``2``:
         A run of NaN (Not-a-Number) floats (whether f32 or f64 depends on typesize).
     :``3``:
         Reserved.
     :``4``:
         Values that are not initialized.
     :``5``:
         Reserved.
     :``6``:
         Reserved.
     :``7``:
         Reserved.

:bit 3 (``0x08``):
    Reserved.
:bit 4 (``0x10``):
    Reserved.
:bit 5 (``0x20``):
    Reserved.
:bit 6 (``0x40``):
    Reserved.
:bit 7 (``0x80``):
    Indicates a special value.  If not set, a regular value.


Trailer
-------

The trailer for the frame is encoded via `msgpack <https://msgpack.org>`_ and contains a user meta data chunk and
a fingerprint.::

    |-0-|-1-|================|---|---------------|---|---|---------------|
    | 9X| aX| vlmetalayers   | ce| trailer_len   | d8|fpt| fingerprint   |
    |---|---|================|---|---------------|---|---|---------------|
      ^   ^   ^    ^           ^       ^           ^   ^
      |   |   |    |           |       |           |   +-- fingerprint type
      |   |   |    |           |       |           +--[msgpack] fixext 16
      |   |   |    |           |       +-- trailer length
      |   |   |    |           +--[msgpack] uint32 for trailer length
      |   |   |    +--Variable-length metalayers (See header metalayers)
      |   |   +---[msgpack] bin32 for vlmetalayers
      |   +------[msgpack] int8 for trailer version
      +---[msgpack] fixarray with X=4 elements

The *vlmetalayers* object which stores the variable-length user meta data can change in size during the lifetime of the frame.
This is an important feature and the reason why the *vlmetalayers* are stored in the trailer and not in the header.
However, the *vlmetalayers* follows the same format as the ones stored in the header.


:trailer_len:
    (``uint32``) Size of the trailer of the frame (including vlmetalayers chunk).

:fpt:
    (``int8``) Fingerprint type:  0 -> no fp; 1 -> 32-bit; 2 -> 64-bit; 3 -> 128-bit

:fingerprint:
    (``uint128``) Fix storage space for the fingerprint (16 bytes), padded to the left.

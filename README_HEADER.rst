Blosc Header Format
===================

Blosc (as of Version 1.0.0) has the following 16 byte header that stores
information about the compressed chunk::

    |-0-|-1-|-2-|-3-|-4-|-5-|-6-|-7-|-8-|-9-|-A-|-B-|-C-|-D-|-E-|-F-|
      ^   ^   ^   ^ |     nbytes    |   blocksize   |     cbytes    |
      |   |   |   |
      |   |   |   +--typesize
      |   |   +------flags
      |   +----------versionlz
      +--------------version

In addition, starting in Blosc 2.0.0, there is an extension of the header
above that allows to encode the filter pipeline::

  1+|-0-|-1-|-2-|-3-|-4-|-5-|-6-|-7-|-8-|-9-|-A-|-B-|-C-|-D-|-E-|-F-|
    |   filter codes    | reserved  |   filter meta     | resvd | ^ |
                                                                  |
                                                                  +-blosc2_flags

So there is a complete byte for encoding the filter and another one to encode
possible metadata associated with the filter.  The filter pipeline has 5
reserved slots for the filters to be applied sequentially to the chunk.  The
filters are applied sequentially following the slot order.

Datatypes of the Header Entries
-------------------------------

All entries are little endian.

:version:
    (``uint8``) Blosc format version.

:versionlz:
    (``uint8``) Version of the internal compressor used.

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
        If set, the blocks will not be split in sub-blocks during compression.
    :bit 5 (``0x20``):
        Part of the enumeration for compressors.
    :bit 6 (``0x40``):
        Part of the enumeration for compressors.
    :bit 7 (``0x80``):
        Part of the enumeration for compressors.

    Note:: If both bit 0 and bit 2 are set at once, that means that an
        extended header (see above) is used.

    The last three bits form an enumeration that allows to use alternative
    compressors.

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
    :``6``:
        Reserved
    :``7``:
        The compressor is defined in the super-chunk.

:typesize:
    (``uint8``) Number of bytes for the atomic type.

:nbytes:
    (``uint32``) Uncompressed size of the buffer.

:blocksize:
    (``uint32``) Size of internal blocks.

:cbytes:
    (``uint32``) Compressed size of the buffer.

:blosc2_flags:
    (``bitfield``) The flags for a Blosc2 buffer.

    :bit 0 (``0x01``):
        Whether the codec uses dictionaries or not.

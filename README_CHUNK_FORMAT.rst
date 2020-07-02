Blosc Chunk Format
==================

The chunk is composed by a header and a blocks / splits section::

    +---------+--------+---------+
    |  header | blocks / splits  |
    +---------+--------+---------+

These are described below.

The header section
------------------

Blosc (as of version 1.0.0) has the following 16 byte header that stores
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
    |     filter codes      |   ^   |     filter meta       | ^ | ^ |
                                |                             |   |
                                +-reserved                    |   +-blosc2_flags
                                                              +-reserved

So there is a complete byte for encoding the filter and another one to encode
possible metadata associated with the filter.  The filter pipeline has 6
reserved slots for the filters to be applied sequentially to the chunk.  The
filters are applied sequentially following the slot number in increasing order.

Datatypes of the Header Entries
-------------------------------

All entries are little endian.

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
        If set, the blocks are *not* split in sub-blocks.
    :bit 5 (``0x20``):
        Part of the enumeration for compressors.
    :bit 6 (``0x40``):
        Part of the enumeration for compressors.
    :bit 7 (``0x80``):
        Part of the enumeration for compressors.

    Note:: If both bit 0 and bit 2 are both set, that means that an
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
    (``uint32``) Uncompressed size of the buffer (this header is not included).

:blocksize:
    (``uint32``) Size of internal blocks.

:cbytes:
    (``uint32``) Compressed size of the buffer (including this header).

:blosc2_flags:
    (``bitfield``) The flags for a Blosc2 buffer.

    :bit 0 (``0x01``):
        Whether the codec uses dictionaries or not.
    :bit 1 (``0x02``):
        Whether the header is extended with +32 bytes coming right after this byte. 
    :bit 2 (``0x04``):
        Whether the codec is stored in a byte previous to this compressed buffer or it is in the global `flags` for chunk. 


The blocks / splits section
---------------------------

After the header, there come the blocks / splits section.  Blocks are equal-sized parts of the chunk, except for the last block that can be shorter or equal than the rest.

At the beginning of the blocks section, there come a list of `int32_t bstarts` to indicate where the different encoded blocks starts (counting from the end of this `bstarts` section)::

    +=========+=========+========+=========+
    | bstart0 | bstart1 |   ...  | bstartN |
    +=========+=========+========+=========+

[**Only for C-Blosc2**] Next, it comes an optional dictionary section prepended with a `int32_t dsize`.  Dictionaries are small datasets that are known to be repeated a lot and can help to compress data in blocks/splits better::

    +========+=================+
    | dsize  | dictionary data |
    +========+=================+

Finally, it comes the actual list of compressed blocks / splits data streams.  It turns out that a block may optionally (see bit 4 in `flags` above) be further split in so-called splits which are the actual data streams that are transmitted to codecs for compression.  If a block is not split, then the split is equivalent to a whole block.  Before each split in the list, there is the compressed size of it, expressed as an `int32_t`::

    +========+========+========+========+========+========+========+
    | csize0 | split0 | csize1 | split1 |   ...  | csizeN | splitN |
    +========+========+========+========+========+========+========+

[**Only for C-Blosc2**] The `csize` can be signed.  Positive values mean regular compressed sizes (the only ones supported by C-Blosc1).  Negative values mean splits that are made of a sequence of the same byte value; such value is encoded as the lowest significant byte of the `int32_t csize`.  For example, a csize of 10000 means that the compressed split stream that follows is 10000 bytes long.  On its hand, a csize of -32 means that the whole block/split is made of bytes with a value of 32.

*Note*: all the integers are stored in little endian.
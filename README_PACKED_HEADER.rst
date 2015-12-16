Blosc Packed Header Format
==========================

Blosc (as of Version 2.0.0) has the following 96 byte header that stores
information about the compressed buffer::

    |-0-|-1-|-2-|-3-|-4-|-5-|-6-|-7-|-8-|-9-|-A-|-B-|-C-|-D-|-E-|-F-|
      ^   ^   ^   ^ | cname |clevel |filters|f_meta |   chunksize   |
      |   |   |   |
      |   |   |   +--flags3
      |   |   +------flags2
      |   +----------flags1
      +--------------version

    |16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|
    |        nchunks        |       nbytes          |         cbytes        |

:location of special chunks:
    (``int64``) Integer that marks the position of the chunk starting from the start of the header

    :bytes 40 - 47:  filters chunk
    :bytes 48 - 55:  codec chunk
    :bytes 64 - 71:  metadata chunk
    :bytes 72 - 79:  userdata chunk
    :bytes 80 - 87:  where the data starts are
    :bytes 88 - 95:  reserved

The special 'data starts' block looks like:

    |X+0|X+1|X+2|X+3|X+4|X+5|X+6|X+7| ... |Z+0|Z+1|Z+2|Z+3|Z+4|Z+5|Z+6|Z+7|
    |      start data chunk 0       | ... |     start data chunk N        |

where X is where the 'data starts' block begins and Z = X + (nchunks - 1) * 8.


Datatypes of the Header Entries
-------------------------------

All entries are little endian.

:version:
    (``uint8``) Blosc packed format version.
:flags1:
    (``uint8``) Space reserved.
:flags2:
    (``uint8``) Space reserved.
:flags3:
    (``uint8``) Space reserved.
:cname:
    (``uint16``) Compressor enumeration.

    The next are defined.

    :``0``:
        ``blosclz``
    :``1``:
        ``lz4`` or ``lz4hc``
    :``2``:
        ``snappy``
    :``3``:
        ``zlib``

:clevel:
    (``uint16``) The compression level and other compress params.
:filters:
    (``bitfield``) 3-bit encoded filters.

    :bit 0 - 3 (``0x7``):
        Filter 1.
    :bit 3 - 6 (``0x38``):
        Filter 2.
    :bit 6 - 9 (``0x1c0``):
        Filter 3.
    :bit 9 - 12 (``0xe00``):
        Filter 4.
    :bit 12 - 15 (``0x7000``):
        Filter 5.
    :bit 15 (``0x8000``):
        Reserved.

:f_meta:
    (``uint64``) Reserved space for filter metadata.
:chunksize:
    (``uint64``) Size of each data chunk in super-chunk.  0 if not a fixed chunksize.
:nchunks:
    (``uint64``) Number of data chunks.
:nbytes:
    (``uint64``) Uncompressed size of the packed buffer (header + metadata + data).
:cbytes:
    (``uint64``) Compressed size of the packed buffer (header + metadata + data).

B2ND Metalayer Format
=====================

This is a `metalayer <https://www.blosc.org/posts/blosc-metalayers/>`_ on top of a Blosc2
`CFrame <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst>`_ or
`SFrame <https://github.com/Blosc/c-blosc2/blob/main/README_SFRAME_FORMAT.rst>`_
that is meant for storing multidimensional information.

Specifically, this metalayer is named 'b2nd'.  The initial format (version 0)
follows this layout::

    |-0-|-1-|-2-|-3-|~~~~~~~~~~~|---|~~~~~~~~~~~~~~~|---|~~~~~~~~~~~~~~|---|---|--4 bytes--|~~~~~~~~~~~~|
    | 97| 00| nd| 9X| shape     | 9X| chunkshape    | 9X| blockshape   | df| db| dtype_len | dtype      |
    |---|---|---|---|~~~~~~~~~~~|---|~~~~~~~~~~~~~~~|---|~~~~~~~~~~~~~~|---|---|-----------|~~~~~~~~~~~~|
      ^   ^   ^   ^               ^                   ^                  ^   ^
      |   |   |   |               |                   |                  |   |
      |   |   |   |               |                   |                  |   +--[msgpack] str32
      |   |   |   |               |                   |                  +--[msgpack] positive fixint; dtype_format
      |   |   |   |               |                   +--B2ND dimension vector with X=nd elements
      |   |   |   |               +--B2ND dimension vector with X=nd elements
      |   |   |   +--B2ND dimension vector with X=nd elements
      |   |   +--[msgpack] positive fixnum for the number of dimensions (0 through 16)
      |   +--[msgpack] positive fixnum for the metalayer format version (0 initially)
      +---[msgpack] fixarray with 7 elements

Each dimension vector starts with the byte ``0x90 + nd``.  For ``nd`` from 0
through 15 this is the corresponding MessagePack ``fixarray`` marker.  For
``nd = 16`` the marker is ``0xa0``.  Although MessagePack defines ``0xa0`` as
``fixstr(0)``, the initial B2ND format assigns it the contextual meaning
"dimension vector with 16 elements" in these three positions.

The `shape` section is meant to store the actual shape info::

    |---|--8 bytes---|---|--8 bytes---|~~~~~|---|--8 bytes---|
    | d3| first_dim  | d3| second_dim | ... | d3| nth_dim    |
    |---|------------|---|------------|~~~~~|---|------------|
      ^                ^                      ^
      |                |                      |
      |                |                      +--[msgpack] int64
      |                +--[msgpack] int64
      +--[msgpack] int64


Next, the `chunkshape` section is meant to store the actual chunk shape info::

    |---|--4 bytes---|---|--4 bytes---|~~~~~|---|--4 bytes---|
    | d2| first_dim  | d2| second_dim | ... | d2| nth_dim    |
    |---|------------|---|------------|~~~~~|---|------------|
      ^                ^                      ^
      |                |                      |
      |                |                      +--[msgpack] int32
      |                +--[msgpack] int32
      +--[msgpack] int32


Next, the `blockshape` section is meant to store the actual block shape info::

    |---|--4 bytes---|---|--4 bytes---|~~~~~|---|--4 bytes---|
    | d2| first_dim  | d2| second_dim | ... | d2| nth_dim    |
    |---|------------|---|------------|~~~~~|---|------------|
      ^                ^                      ^
      |                |                      |
      |                |                      +--[msgpack] int32
      |                +--[msgpack] int32
      +--[msgpack] int32

Finally, the `dtype` section is meant to store the data type information::

    |---|---|--4 bytes---|--------------|
    | XX| db| dtype_len  | dtype_string |
    |---|---|------------|--------------|
      ^   ^
      |   |
      |   +--[msgpack] str32
      +--[msgpack] positive fixint (7-bit integer). dtype_format; 0 means NumPy format.

The 0 value for dtype_format means that the dtype_string field follows the NumPy convention
(e.g. an `int32_t` dtype is represented as "<i4").  For more examples on NumPy dtype specs, see
https://numpy.org/doc/stable/reference/arrays.dtypes.html#arrays-dtypes-constructing.

The enclosing frame's ``type_size`` is authoritative for physical element size
and byte offsets.  Writers should use a dtype whose element size agrees with it.

Version 1 should replace the ``0xa0`` exception with standard MessagePack array
encodings: ``fixarray`` for dimension vectors of up to 15 elements and
``array16`` for longer vectors.  For a 16-element vector, the header is
``0xdc 0x00 0x10``.  This changes the initial-format wire representation and
therefore requires a new version.  Once version 1 defines this general rule,
raising the implementation limit from 16 to, for example, 32 dimensions does
not require another format version.

For compatibility, readers may also accept historical outer markers ``0x96``
(all seven fields present) and ``0x95`` (no dtype fields).  New writers must use
the canonical ``0x97`` representation shown above.

The complete initial-format specification is in
`SPECS/B2ND.md <https://github.com/Blosc/c-blosc2/blob/main/SPECS/B2ND.md>`_.

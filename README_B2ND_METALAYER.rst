b2nd metalayer
==============

b2nd format is specified as a metalayer on top of a Blosc2 container for storing
multidimensional information.  Specifically, this metalayer is named 'b2nd'
and follows this format::

    |-0-|-1-|-2-|-3-|~~~~~~~~~~~~~~~~|---|~~~~~~~~~~~~~~~~|---|~~~~~~~~~~~~~~~~|
    | 9X| v | nd| 9X| shape          | 9X| chunkshape     | 9X| blockshape     |
    |---|---|---|---|~~~~~~~~~~~~~~~~|---|~~~~~~~~~~~~~~~~|---|~~~~~~~~~~~~~~~~|
      ^   ^   ^   ^                    ^                    ^
      |   |   |   |                    |                    |
      |   |   |   |                    |                    +--[msgpack] fixarray with X=nd elements
      |   |   |   |                    +--[msgpack] fixarray with X=nd elements
      |   |   |   +--[msgpack] fixarray with X=nd elements
      |   |   +--[msgpack] positive fixnum for the number of dimensions (up to 127)
      |   +--[msgpack] positive fixnum for the metalayer format version (up to 127)
      +---[msgpack] fixarray with X=7 elements

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

# B2ND initial format specification (version 0)

Status: draft

This document specifies the initial version (0) of B2ND, the multidimensional
array representation used by C-Blosc2. It is intentionally limited to
information needed to exchange and recover B2ND arrays. APIs, slicing
operations and compression choices are outside its scope.

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT** and **MAY** in
this document are to be interpreted as described by RFC 2119.

## 1. Overview

A B2ND array consists of:

1. a Blosc2 super-chunk stored as a contiguous frame (CFrame) or sparse frame
   (SFrame);
2. a fixed metalayer named `b2nd`, which describes the logical array and its
   two-level partitioning; and
3. zero or more Blosc2 chunks containing the array data.

The enclosing frame, its metalayers and its chunks are defined by the
[CFrame](../README_CFRAME_FORMAT.rst),
[SFrame](../README_SFRAME_FORMAT.rst) and
[chunk](../README_CHUNK_FORMAT.rst) specifications. This document defines only
the additional B2ND semantics.

An array is partitioned first into chunks and then into blocks. Chunks, blocks
and elements inside a block are all ordered in C order: the last dimension
varies fastest.

## 2. Terms and notation

The following vectors have `N` entries, one per array dimension:

| Symbol | Metalayer field | Meaning |
| --- | --- | --- |
| `S` | `shape` | Logical array shape |
| `C` | `chunkshape` | Logical shape covered by each chunk |
| `B` | `blockshape` | Shape of each block inside a chunk |
| `E` | derived | Array shape padded to complete chunks |
| `P` | derived | Chunk shape padded to complete blocks |
| `G` | derived | Number of chunks along each dimension |
| `H` | derived | Number of blocks per chunk along each dimension |

For positive `d`, define:

```text
ceildiv(x, d) = (x + d - 1) / d
```

where `/` denotes integer division. For every dimension `i` of a non-empty
array:

```text
E[i] = ceildiv(S[i], C[i]) * C[i]
P[i] = ceildiv(C[i], B[i]) * B[i]
G[i] = E[i] / C[i]
H[i] = P[i] / B[i]
```

The product of an empty vector is 1. Consequently, a zero-dimensional B2ND
array is a scalar containing one element.

## 3. B2ND metalayer

The enclosing super-chunk MUST contain a fixed metalayer whose name is the
ASCII string `b2nd`. Its content is a MessagePack-derived sequence whose outer
value is an array with exactly seven entries, in this order:

```text
[
  version,
  ndim,
  shape,
  chunkshape,
  blockshape,
  dtype_format,
  dtype
]
```

The initial format stores `0` in its `version` field and uses the following
exact encodings. MessagePack multi-byte integers are big-endian.

| Entry | MessagePack encoding | Constraint |
| --- | --- | --- |
| outer array | `fixarray(7)` (`0x97`) | exactly seven entries |
| `version` | positive fixint | MUST be `0` |
| `ndim` | positive fixint | MUST be in `[0, 16]` |
| `shape` | B2ND dimension vector of `int64` (`0xd3`) | each value MUST be non-negative |
| `chunkshape` | B2ND dimension vector of `int32` (`0xd2`) | see Section 4 |
| `blockshape` | B2ND dimension vector of `int32` (`0xd2`) | see Section 4 |
| `dtype_format` | positive fixint | `0` denotes NumPy dtype syntax |
| `dtype` | `str32` (`0xdb`) | dtype string, without a terminating NUL |

The `str32` length is an unsigned 32-bit big-endian byte count. A conforming
initial-format writer MUST use the encodings in the table even when MessagePack
offers a shorter encoding for a value.

Each B2ND dimension vector starts with the single marker byte `0x90 + ndim`.
For `ndim` from 0 through 15, this is the corresponding MessagePack `fixarray`
marker. For `ndim = 16`, the marker is `0xa0`. MessagePack normally defines
`0xa0` as `fixstr(0)`, but the initial B2ND format assigns it the contextual
meaning "dimension vector with 16 elements" in the `shape`, `chunkshape` and
`blockshape` positions. Consequently, a 16-dimensional initial-format
metalayer is not a valid generic MessagePack document and requires a
B2ND-aware reader.

### 3.1 Data type

`dtype_format = 0` means that `dtype` uses NumPy dtype-string syntax, for
example `<i4` for a little-endian signed 32-bit integer and `|u1` for an
unsigned byte.

The enclosing frame's `type_size` field is authoritative for element size and
all byte-offset calculations. For `dtype_format = 0`, writers SHOULD use a
fixed-size dtype whose element size equals `type_size`. A reader MAY warn when
it can determine that they disagree, but MUST use `type_size` for physical
layout.

Other non-negative `dtype_format` values are reserved for separately specified
dtype syntaxes. A reader that does not recognize the value MAY expose the
array as opaque fixed-size elements, but MUST NOT claim to understand their
type.

## 4. Array invariants

The enclosing frame's `type_size` MUST be positive.

For every dimension `i`:

- `S[i]` MUST be non-negative.
- If `S[i] > 0`, both `C[i]` and `B[i]` MUST be positive.
- If `S[i] = 0`, `C[i]` and `B[i]` MUST be non-negative. Writers SHOULD use
  positive partition sizes unless zero is needed to preserve an existing
  empty-array representation.
- `product(S)` MUST fit in a signed 64-bit integer.
- `product(C)` and `product(B)` MUST fit in a signed 32-bit integer.
- `product(P) * type_size` MUST fit in the maximum uncompressed Blosc2 chunk
  size supported by the enclosing format.

The frame fields MUST agree with the B2ND geometry:

```text
frame.chunk_size = product(P) * frame.type_size
frame.block_size = product(B) * frame.type_size
```

If every entry in `S` is positive, the super-chunk MUST contain
`product(G)` chunks. If any entry in `S` is zero, the array has zero logical
elements and the super-chunk MUST contain zero chunks. A scalar (`ndim = 0`)
MUST contain one chunk with one block and one element.

Readers MUST reject metadata that violates a required invariant or would
overflow while computing derived sizes or offsets.

## 5. Storage mapping

### 5.1 C-order linearization

For a coordinate vector `x` within a shape vector `D`, define its C-order
linear index as:

```text
linear(x, D) = sum(x[i] * product(D[i+1:N])) for i in [0, N)
```

For `N = 0`, `linear([], [])` is 0.

### 5.2 Logical coordinate to stored element

For a logical coordinate `x`, where `0 <= x[i] < S[i]`, compute:

```text
q[i] = x[i] / C[i]                 # chunk coordinate
r[i] = x[i] % C[i]                 # coordinate in nominal chunk
b[i] = r[i] / B[i]                 # block coordinate in padded chunk
u[i] = r[i] % B[i]                 # coordinate in block

chunk_number = linear(q, G)
block_number = linear(b, H)
element_in_block = linear(u, B)

element_in_chunk = block_number * product(B) + element_in_block
byte_offset_in_chunk = element_in_chunk * type_size
```

`chunk_number` is the zero-based position of the Blosc2 chunk in the enclosing
super-chunk. After decompression, the chunk contains exactly `product(P)`
elements. Each block occupies `product(B) * type_size` uncompressed bytes.

This layout is block-major within a chunk. It differs from simply storing the
entire padded chunk as one C-contiguous `P`-shaped array whenever more than one
block is present.

### 5.3 Padding

Two kinds of padding can occur:

- array-edge padding where a chunk covers coordinates outside `S`; and
- intra-chunk padding where `P[i] > C[i]` because blocks do not divide the
  nominal chunk shape.

Padding elements have no logical value. Readers MUST ignore them. Writers
SHOULD initialize padding bytes to zero to avoid retaining unrelated data, but
readers MUST NOT depend on their contents.

## 6. Worked example

Consider a two-dimensional little-endian `int32` array with:

```text
S = [3, 5]
C = [2, 3]
B = [1, 2]
dtype_format = 0
dtype = "<i4"
type_size = 4
```

The derived geometry is:

```text
E = [4, 6]
P = [2, 4]
G = [2, 2]
H = [2, 2]
```

There are four chunks in this order:

```text
chunk 0: q = [0, 0]
chunk 1: q = [0, 1]
chunk 2: q = [1, 0]
chunk 3: q = [1, 1]
```

Each decompressed chunk contains eight elements arranged as four contiguous
two-element blocks. In chunk 0 the stored positions are:

| Stored elements | Block coordinate | Logical coordinates |
| --- | --- | --- |
| 0, 1 | `[0, 0]` | `[0, 0]`, `[0, 1]` |
| 2, 3 | `[0, 1]` | `[0, 2]`, padding |
| 4, 5 | `[1, 0]` | `[1, 0]`, `[1, 1]` |
| 6, 7 | `[1, 1]` | `[1, 2]`, padding |

For example, logical coordinate `[1, 2]` maps to chunk 0, block 3, element 0
of that block, and therefore byte offset `6 * 4 = 24` in the decompressed
chunk.

The complete `b2nd` metalayer content is the following 53-byte sequence:

```text
97 00 02
92 d3 00 00 00 00 00 00 00 03 d3 00 00 00 00 00 00 00 05
92 d2 00 00 00 02 d2 00 00 00 03
92 d2 00 00 00 01 d2 00 00 00 02
00 db 00 00 00 03 3c 69 34
```

Line breaks are for readability and are not part of the encoding.

## 7. Reader behavior and compatibility

A conforming reader MUST validate the metalayer container and field markers;
it MUST NOT infer field boundaries solely from fixed byte offsets.

An initial-format reader:

- MUST reject an unsupported `version`;
- MUST reject truncated metadata, trailing entries in the outer array, invalid
  field types and inconsistent vector lengths;
- MUST reject inconsistent frame geometry or an incorrect number of chunks;
- MUST ignore padding contents; and
- SHOULD report unsupported `dtype_format` separately from malformed metadata.

For backward compatibility, readers MAY accept `0x96` as the outer marker when
all seven fields follow, and `0x95` when the metadata ends after `blockshape`.
For `ndim = 16`, readers MUST interpret `0xa0` as the required marker for each
of the three 16-element B2ND dimension vectors.

Future versions may use different MessagePack encodings or add semantics. They
must use a new `version` value if an initial-format reader could otherwise
interpret the data incorrectly.

## 8. Non-normative implementation note

Version 1 should replace the version-0 `0xa0` exception with standard
MessagePack array encodings: `fixarray` for dimension vectors of up to 15
elements and `array16` for longer vectors. For a 16-element vector, the header
is marker `0xdc` followed by the unsigned 16-bit element count `0x00 0x10`.
This changes the length and bytes of the version-0 16-element vector header, so
writers must identify the new encoding with a new `version` value. Once
version 1 defines the encoding generically, raising an implementation limit
from 16 to, for example, 32 dimensions does not require another format
version. Version-0 readers must continue to interpret `0xa0` as specified
above.

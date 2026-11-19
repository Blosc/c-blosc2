# Blosc2 EmbedStore (B2E)

Blosc2's **EmbedStore** is a key-value container that associates each key (a string)
with a Blosc2 container (typically an `SChunk`, and, in some versions, a remote array
reference). All the values are stored *inside* a single backing `SChunk`.

This document covers both the EmbedStore API and the serialization format used to
persist its index. Most of the current implementation lives in **c-blosc2**, while the
original format (Version 1) and the high-level `EmbedStore` class live in
**python-blosc2**.

## Overview

An EmbedStore is built on top of one `SChunk` that acts as a contiguous, append-only
arena: every value is first serialized (usually to a cframe) and then appended to this
arena. A hash table maps each key to the `(offset, length)` pair that locates the
value's serialized bytes inside the arena.

The following diagram shows a possible layout of the arena:

```
offset(0)       offset(1)                 offset(2)             offset(M)
v               v                         v                     v
+---------------+-------------------------+-------------+-------+-------------------+
|    value 0    |         value 1         |   value 2   |  ...  |      value M      |
+---------------+-------------------------+-------------+-------+-------------------+
<--length(0)---><-------length(1)--------><-length(2)-->        <----length(M)----->
```

Each value `i` occupies the half-open byte range
`[offset(i), offset(i) + length(i))`, and the next value starts right where the previous
one ends, so `offset(i + 1) = offset(i) + length(i)`. The `length` annotations below the
arena show the number of bytes each value takes.

Because the arena is append-only, removing a key only deletes its entry from the index;
the bytes it occupied stay in the arena until the store is rewritten. This is harmless
(they are unreachable) and simplifies the write path.

Both [Version 1](#version-1) and [Version 2](#version-2) share this arena layout; they
differ only in how the index (the key → location map) is serialized.

## API

### C API (c-blosc2)

The public C API is declared in `include/blosc2/embed-store.h`.

Open modes (`b2_es_open_mode`):

| Value | Name | Meaning |
| ----- | ---- | ------- |
| `0` | `B2_ES_RDWONLY` | Read-only (as spelled in the current header). |
| `1` | `B2_ES_WRWONLY` | Write-only. |
| `2` | `B2_ES_RDWR` | Read-write; the default for a newly created store. |

Main functions:

- `blosc2_embed_store_new(estore, storage)` — Allocate a new EmbedStore. If `storage` is
  `NULL`, a default contiguous storage with `typesize = 1` is used. If `storage` is
  provided, `cparams.typesize` must be `1`, and `urlpath` and `contiguous = true` are
  mutually exclusive.
- `blosc2_embed_store_free(estore)` — Free the resources owned by the store. Calling it more
  than once can segfault.
- `blosc2_embed_store_open(estore, urlpath, open_mode)` — Open a persisted store. In
  `B2_ES_RDONLY` and `B2_ES_RDWR` modes it loads the backing SChunk at `urlpath` and
  deserializes its index; in `B2_ES_WRONLY` mode the file must not exist yet and a fresh
  store is created instead.
- `blosc2_embed_store_from_schunk(estore, schunk, copy)` — Build a store from an existing
  backing SChunk. The SChunk must have `typesize = 1` and carry the EmbedStore metadata
  (the `b2embed` version metalayer plus the serialized index in the `estore_metadata`
  vlmeta). If `copy` is `false` the store adopts `schunk`, so freeing the store also frees
  the SChunk. If `copy` is `true` the store works on a copy made with
  `blosc2_schunk_copy(schunk, schunk->storage)`, so it is subject to the same rules as
  `blosc2_schunk_new`; the caller is responsible for how `schunk` was created and with which
  storage (in particular, copying a persistent SChunk that reuses its own `urlpath` fails,
  as `blosc2_schunk_new` refuses to overwrite an existing frame).
- `blosc2_embed_store_close(estore)` — Serialize the index to msgpack and write it to the
  `estore_metadata` vlmeta of the backing SChunk. It can be called multiple times and is
  a no-op when nothing changed since the last call, but it must be called at least once
  to persist changes.
- `blosc2_embed_store_to_buffer(estore, pbuf, pbuf_size)` — Serialize the whole store (the
  backing SChunk as a cframe) into a newly allocated buffer.

Data operations:

- `blosc2_embed_store_contains_key(estore, key)` — Return whether `key` is present.
- `blosc2_embed_store_get(estore, key, pschunk)` — Deserialize and return the value
  associated with `key`.
- `blosc2_embed_store_set(estore, key, schunk)` — Append `schunk` to the arena and add it
  under `key`. Reassigning an existing key is not supported.
- `blosc2_embed_store_remove(estore, key)` — Remove `key` from the index (the arena is left
  untouched).
- `blosc2_embed_store_get_length(estore)` — Return the number of keys.
- `blosc2_embed_store_get_open_mode(estore)` — Return the store's open mode.

Iteration over the keys:

- `blosc2_embed_store_iter_new(estore)` — Allocate an iterator.
- `blosc2_embed_store_iter_next(iter)` — Advance the iterator; returns `false` at the end.
- `blosc2_embed_store_iter_get_key(iter)` — Key at the current position.
- `blosc2_embed_store_iter_get_container(iter, pschunk)` — Load the value at the current
  position.
- `blosc2_embed_store_iter_free(iter)` — Free the iterator.

Key validation (`s_embed_key_is_valid`) requires keys to be non-empty, start with `/`, not
end with `/` (unless the key is the root key `"/"`), and not contain `:`, NUL, `\n`, `\r`
or `\t`.

## Serialization format

### Notation

The msgpack tags used in the formats below:

- `fixmap`: a fixmap tag whose length is less than or equal to 15.
- `map16` / `map32`: a map tag with a 16-bit or 32-bit length prefix (2 or 4 extra bytes).
- `fixarray`: a fixarray tag whose length is less than or equal to 15.
- `fixstr`: a fixstr tag whose length is less than or equal to 31.
- `str8` / `str16` / `str32`: string tags with an 8-, 16- or 32-bit length prefix.
- `uint`: any of the msgpack unsigned-integer tags. All integers are stored in
  **big-endian** byte order.

Every tag occupies exactly one byte.

### Version 1

Version 1 is the first EmbedStore format; its serialization is implemented in **pure
Python** (python-blosc2). The serialized index is a msgpack map with two fields:

- `current_offset`: an unsigned integer with the current offset (arena high-water mark).
- `embed_map`: a map from each key (a string) to a nested map with the location:
  - `offset`: the offset of the serialized value.
  - `length`: the length of the serialized value.

If a value is a remote `C2Array` reference instead of an embedded container, the nested
map uses these keys instead:

- `urlbase`: the Caterva2 (or other) server URL.
- `path`: the path on the server where the data lives.

A Python representation of this structure looks like this:

```python
# `metadata` is another map
metadata["estore_metadata"] = {
    "current_offset": 1234,
    "embed_map": {
        "/first_key": {
            "offset": 0,
            "length": 10,
        },
        "/caterva_data": {
            "urlbase": "catervaserver.com",
            "path": "/on/my/way",
        },
        "/second_key": {
            "offset": 11,
            "length": 20,
        },
    },
}
```

In msgpack, the first entries above are laid out as follows:

| Tag / bytes | Meaning |
| ----------- | ------- |
| `fixmap` | Map header with 2 entries |
| `fixstr` `"current_offset"` | Key |
| `uint` `1234` | Value |
| `fixstr` `"embed_map"` | Key |
| `fixmap` | Map header with 3 entries |
| `fixstr` `"/first_key"` | Key |
| `fixmap` | Value map header with 2 entries |
| `fixstr` `"offset"` | Key |
| `uint` `0` | Value |
| `fixstr` `"length"` | Key |
| `uint` `10` | Value |
| `fixstr` `"/caterva_data"` | Key |
| `fixmap` | Value map header with 2 entries |
| `fixstr` `"urlbase"` | Key |
| `fixstr` `"catervaserver.com"` | Value (a string) |
| `fixstr` `"path"` | Key |
| `fixstr` `"/on/my/way"` | Value (a string) |
| `...` | Remaining entries |

### Version 2

Version 2 is the format implemented natively in **c-blosc2**
(`blosc/utils/msgpack.c`). It is designed to be smaller and simpler than Version 1:

- The payload is a **two-element array**: `[current_offset, embed_map]`.
- The first element is `current_offset`, an unsigned integer holding the arena's
  high-water mark (the offset at which the next value will be appended).
- The second element is the key map itself: unlike Version 1, the map is not nested
  under an outer `embed_map` key.
- Each map value is a **two-element array** `[offset, length]` instead of a nested map.

The layout is therefore:

```
fixarray(2) {
    <current_offset: uint>,
    map(N entries) {
        <key: str> -> fixarray(2) { <offset: uint>, <length: uint> }
        ...
    }
}
```

Remote (`C2Array`) values are not part of Version 2 yet; the serializer has a TODO for
them.

The 52-byte fixture used by the C tests (`tests/test_embed_store.msgpack`) has three keys
that all point at the same `[offset, length] = [10, 10]` and a `current_offset` of `100`:

| Tag / bytes | Meaning |
| ----------- | ------- |
| `fixarray` | Array header with 2 elements |
| `uint` `100` | `current_offset` (first element) |
| `fixmap` | Map header with 3 entries (second element) |
| `fixstr` `"/some/key"` | Key |
| `fixarray` | Value header with 2 elements |
| `uint` `10` | `offset` |
| `uint` `10` | `length` |
| `fixstr` `"/that/key"` | Key |
| `fixarray` | Value header with 2 elements |
| `uint` `10` | `offset` |
| `uint` `10` | `length` |
| `fixstr` `"/wait/another/key/?"` | Key |
| `fixarray` | Value header with 2 elements |
| `uint` `10` | `offset` |
| `uint` `10` | `length` |

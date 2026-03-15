# Variable-Length Blocks In Chunks

## Goal

Add support for chunks whose blocks have variable uncompressed lengths, while keeping the implementation as local as possible:

- new chunk-format support, not a large refactor
- prefer targeted `if`/`else` handling inside current compression/decompression paths
- add only the new public APIs that are strictly necessary


## Agreed Format Decisions

### Frame-level signal

- Use bit 7 in `general_flags` in `README_CFRAME_FORMAT.rst` to mean:
  - all chunks in the frame use variable-length blocks
- Mixed populations inside one frame/schunk will be forbidden:
  - either all chunks use regular fixed-size blocks
  - or all chunks use variable-length blocks

### Chunk-level signal

- Reuse the currently reserved byte in the extended chunk header.
- Rename/document that byte as `blosc2_flags2`.
- Use bit 0 in `blosc2_flags2` to mean:
  - this chunk uses variable-length blocks
- Current C-Blosc2 writers already zero this byte, so old chunks produced by current code should carry `0` there.

### Keep `blocksize` positive

- Do not use `blocksize == 0` as a sentinel.
- Keep `blocksize` as a positive size in the chunk header.
- For VL-block chunks, `blocksize` in the header will store `nblocks`.
- The runtime context can still keep the maximum block size separately for temp-buffer sizing.

### Stream layout

- For VL-block chunks, force one compressed stream per block.
- In practice, VL-block chunks should behave as if split mode is disabled for the chunk.
- This keeps the interpretation of the per-stream `csize` field simple:
  - `csize` stores the uncompressed size of the block
  - compressed size is derived from `bstarts`
- This also avoids a wider redesign of the current multi-stream-per-block format.


## Compatibility Rules

- Existing chunk format remains unchanged for regular chunks.
- Old decoders will not understand VL-block chunks; this is acceptable for the new format.
- Regular chunks and VL-block chunks must not be mixed in the same schunk/frame.
- Standalone chunks must remain self-describing, so the chunk-level `blosc2_flags2` bit is required even if the frame-level bit exists.


## Public API

### Compression

Add a new public function in `include/blosc2.h`:

```c
int blosc2_vlcompress_ctx(blosc2_context* context,
                          const void* const* srcs,
                          const int32_t* srcsizes,
                          int32_t nblocks,
                          void* dest,
                          int32_t destsize);
```

Intent:

- each `srcs[i]` points to one contiguous uncompressed block buffer
- each `srcsizes[i]` is the uncompressed size for that block
- the function builds one chunk containing all those blocks

Notes:

- model the function after `blosc2_compress_ctx`
- preserve multithreading by assigning one or more blocks per worker, as today
- do not try to generalize the current API for all cases; keep the new API separate

### Decompression

Add a new public function in `include/blosc2.h`:

```c
int blosc2_vldecompress_ctx(blosc2_context* context,
                            const void* src,
                            int32_t srcsize,
                            void** dests,
                            int32_t* destsizes,
                            int32_t maxblocks);
```

Intent:

- decompress one VL-block chunk into a list of per-block output buffers
- fill `destsizes[i]` with the uncompressed size of each block
- return the number of blocks on success

Notes:

- the function allocates the output buffers
- `maxblocks` protects the API from overrunning caller arrays
- decompression can still use the current threaded block loop


## Minimal-Change Implementation Strategy

The implementation should mostly extend current code paths instead of introducing many new helpers.

### 1. Header and constants

- Add a named constant for `blosc2_flags2` offset in the extended chunk header.
- Add a named bit constant for `BLOSC2_VL_BLOCKS`.
- Rename the reserved-byte wording in the docs and internal comments to `blosc2_flags2`.
- Update the internal `blosc_header` struct field name from `reserved2` to `blosc2_flags2`.

### 2. Chunk header read/write

- In `read_chunk_header()`, read `blosc2_flags2` and keep it available in the local header struct.
- In `blosc2_intialize_header_from_context()`, explicitly populate `blosc2_flags2` from the context.
- For the special chunk constructors (`zeros`, `uninit`, `nans`, `repeatval`), keep `blosc2_flags2 == 0`.

### 3. Context state

- Add the minimum extra state needed in `blosc2_context`:
  - a flag for `vlblocks`
  - a pointer to per-block uncompressed sizes for compression
  - a pointer to per-block uncompressed sizes for decompression output
- Avoid broad context redesign.

### 4. Compression path

- Implement `blosc2_vlcompress_ctx()` as a sibling of `blosc2_compress_ctx()`.
- Reuse `initialize_context_compression()` as much as possible, but set:
  - `context->blosc2_flags2 |= BLOSC2_VL_BLOCKS`
  - `dont_split = 1`
  - `context->nblocks` from the caller-provided block count
  - `context->sourcesize` as the sum of all block sizes
  - header `blocksize` as `nblocks`
  - runtime `context->blocksize` as the maximum block size
- In the block compression loop, branch on `vlblocks`:
  - use the caller-provided block size for the current block
  - write `bstarts` as today
  - write stream `csize` as the uncompressed block size, not compressed bytes
- Keep current block scheduling and worker model.

### 5. Decompression path

- In `initialize_context_decompression()`, detect `BLOSC2_VL_BLOCKS` from `blosc2_flags2`.
- Keep current setup for regular chunks unchanged.
- For VL-block chunks:
  - still use `bstarts`
  - derive compressed block size from adjacent `bstarts` values and the chunk end
  - read the stored per-block uncompressed size from the stream `csize`
- In `blosc_d()`, branch on `vlblocks` before the current stream parsing logic diverges too much.
- Use one block -> one stream semantics for VL-block chunks.

### 6. Lazy chunks and frame integration

- Extend lazy-chunk trailer generation/reading so VL-block chunks still expose enough information to load one block lazily.
- Keep changes narrow:
  - reuse current trailer structure where possible
  - only add conditional handling when `BLOSC2_VL_BLOCKS` is set
- In frames:
  - set `general_flags` bit 7 when writing VL-block schunks
  - reject append/insert/update operations that try to mix regular and VL-block chunks
  - persist the homogeneous-mode invariant across reopen

### 7. Validation

- Add header validation for VL-block chunks:
  - `blosc2_flags2` bit 0 implies extended header
  - `blocksize > 0`
  - for VL-block chunks, `blocksize` is interpreted as `nblocks`
  - one stream per block
  - `bstarts` monotonic
  - decoded compressed span stays inside the chunk
  - stored per-block uncompressed sizes are positive and sum to `nbytes`


## Documentation Changes

### `README_CHUNK_FORMAT.rst`

- Rename the reserved byte to `blosc2_flags2`.
- Document bit 0 as:
  - chunk uses variable-length blocks
- Document the VL-block variant of the blocks section:
  - one stream per block
  - `csize` stores uncompressed block size
  - compressed size comes from `bstarts`
- Clarify that regular chunks keep the current meaning of `csize`.

### `README_CFRAME_FORMAT.rst`

- Use `general_flags` bit 7 for frames containing VL-block chunks.
- State explicitly that mixing regular chunks and VL-block chunks in the same frame is not supported.


## Tests

Add focused tests rather than a large matrix.

### Chunk tests

- roundtrip for one VL-block chunk with different block sizes
- roundtrip for multithreaded compression/decompression
- reopen/decompress through schunk/frame
- invalid header tests:
  - missing chunk flag
  - inconsistent `bstarts`
  - inconsistent block-size sum vs `nbytes`

### Frame/schunk tests

- set/clear frame `general_flags` bit 7 correctly
- reject mixed regular/VL-block append/insert/update
- verify reopen preserves VL-block mode


## Examples

Add one or two small examples in `examples/`:

- build a VL-block chunk from several strings or byte buffers
- decompress it back into per-block buffers and sizes
- optionally, store/reopen via schunk/frame to show the frame flag behavior


## Suggested Implementation Order

1. Add constants, struct-field rename to `blosc2_flags2`, and docs for the new header byte.
2. Add context flag/state for VL blocks.
3. Implement `blosc2_vlcompress_ctx()` with one-stream-per-block semantics.
4. Implement `blosc2_vldecompress_ctx()` and the VL branch in the existing decompression path.
5. Integrate frame `general_flags` bit 7 and enforce non-mixing in schunks/frames.
6. Update docs.
7. Add tests.
8. Add examples.


## Current Implementation Status

The first implementation is in place.

Implemented:

- chunk and frame format versions were bumped
- the extended chunk-header reserved byte was renamed to `blosc2_flags2`
- `BLOSC2_VL_BLOCKS` was added in `blosc2_flags2` bit 0
- frame `general_flags` bit 7 is now used for homogeneous VL-block frames
- `blosc2_vlcompress_ctx()` was added
- `blosc2_vldecompress_ctx()` was added
- VL-block chunks use one stream per block
- for VL-block chunks, header `blocksize` stores `nblocks`
- runtime decompression derives compressed block sizes from `bstarts`
- schunks/frames reject mixing regular chunks and VL-block chunks
- reopen/copy/frame-buffer paths preserve VL-block mode
- docs were updated in `README_CHUNK_FORMAT.rst` and `README_CFRAME_FORMAT.rst`
- tests were added in `tests/test_vlblocks.c`
- an example was added in `examples/vlblocks.c`

Current limitations:

- lazy VL-block chunks are still rejected
- `blosc2_getitem_ctx()` currently rejects VL-block chunks
- there is not yet a broader invalid-header/fuzz-style test matrix for malformed VL-block chunks


## Possible Future Work

- benchmark VL-block compression and decompression against regular chunks to understand:
  - header/trailer overhead
  - impact of forcing one stream per block
  - multithreading efficiency for many small blocks vs fewer large blocks
- add targeted malformed-input tests:
  - non-monotonic `bstarts`
  - wrong final compressed span
  - wrong sum of stored block sizes
  - illegal combinations with special chunks or memcpyed chunks
- test Zstd dictionary support with VL-block chunks:
  - verify `blosc2_vlcompress_ctx()` works correctly when dictionary support is enabled
  - verify decompression reuses the embedded dictionary correctly
  - measure whether dictionary training/use is still effective with one-stream-per-block VL chunks
- evaluate lazy-chunk support for VL-block chunks
- evaluate `getitem` support for VL-block chunks
- measure whether storing per-block outputs via freshly allocated buffers is the right public API long term
- add a small benchmark/example that uses larger and more realistic heterogeneous payloads
- review whether frame metadata should expose the homogeneous VL-block mode more directly in higher-level APIs


## Main Risk Areas

- lazy chunk support, because block compressed size is no longer read directly from stream `csize`
- `getitem` behavior for VL-block chunks
- ensuring the minimal-change branching does not accidentally alter regular chunk behavior

To keep risk low, regular chunks should remain on the current path with as little code movement as possible.

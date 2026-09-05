# Plan: bounded-buffer contiguous-frame tail updates

## Status and scope

Implemented; bounded-buffer contiguous tail updates and sparse payload-free special chunk file removal implemented.

Replace the full-tail scratch allocation in on-disk contiguous
`frame_update_chunk()` with bounded-buffer movement. Preserve file format and
update semantics while preventing scratch RAM from scaling with cached payload.
This does not eliminate tail I/O, provide atomic updates, or solve server quota.

## Current mechanism

Relevant code is in `blosc/frame.c` (`frame_update_chunk`,
`frame_update_trailer`) and `blosc/blosc2-stdio.c` (stdio/mmap I/O callbacks).

For a regular chunk whose compressed size changes, the update computes:

```text
src = old_offset + old_chunk_size
dst = old_offset + new_chunk_size
length = total_payload_bytes - src
```

Offsets are relative to the payload; physical I/O adds frame file offset and
header length. The current disk path allocates `length` bytes, reads the whole
tail, and writes it at `dst`. It then writes the replacement payload and new
compressed offsets, updates the header/trailer, and truncates at the frame end.
An UNINIT chunk has no stored payload, so it follows the shrinking path.

The in-memory frame path already uses memmove; sparse frames use separate chunk
files. Neither should be changed by this work unless a shared correctness issue
is independently demonstrated. Compaction logic appeared in commit `9200990b`;
review subsequent changes and supported I/O contracts before implementation.

## Proposed algorithm

Use one reusable scratch allocation of at most a small internal cap, provisionally
1 MiB. Benchmark alternatives before choosing the default. Avoid a new public
configuration option unless measurements justify one.

For each operation, allocate `min(cap, length)` once, before mutation begins.

- Shrink (`dst < src`): move forward, starting at the beginning of the tail.
- Growth (`dst > src`): move backward, starting at the end of the tail.
- Equal positions or zero length: skip movement and allocation.

Read each segment completely into scratch before writing any part of that
segment. Direction is essential when source and destination ranges overlap.
Always copy through owned scratch, even when an I/O callback returns a borrowed
mmap pointer: a write can remap storage and invalidate that pointer. Do not retain
borrowed addresses across writes. Preserve the existing lock scope around the
whole frame mutation, not merely individual copy iterations.

After moving the tail, retain existing payload/index/header/trailer handling and
final truncation order unless tests reveal a separate bug. Account for physical
frame offsets consistently; embedded frames with trailing container data require
an explicit support audit, not an assumption that truncating the containing file
is safe.

## Correctness and failure handling

- Audit callback contracts for positional reads/writes, short transfers,
  maximum transfer sizes, mmap remapping, and local/custom backends.
- Use checked signed/unsigned conversions and checked offset arithmetic.
  Never allocate based on a negative or overflowing length.
- Follow the backend's short-I/O contract: either finish transfers safely or
  return the existing appropriate error. Never silently accept partial copies.
- Free scratch and close handles on all exits; preserve informative errors.
- Keep frame and offset-cache invalidation behavior unchanged.
- Do not claim crash atomicity: after the first write, interruption may still
  leave inconsistent contents. A bounded buffer is not a journal or rollback.
- Avoid increasing the mutation's lock fragmentation or exposing intermediate
  states to readers that currently rely on frame-level locking.

Extract a file-local overlap-safe range mover if it makes testing and ownership
clearer. Keep the public C API unchanged initially. Reuse in other operations
only after separately auditing their range and metadata invariants.

## Tests

Exercise shrink/growth at first, middle, and last physical chunk positions;
regular-to-UNINIT and UNINIT-to-regular updates; equal-size replacements; empty
tails; logically reordered chunks; and repeated transitions. Check all unaffected
chunk bytes, offsets, metadata, successful reopening, and final file length.

Boundary cases: tail length and displacement below/equal/above the buffer cap,
overlaps smaller than the cap, multiple full segments plus a remainder, and
64-bit file offsets. Test stdio and supported writable mmap modes, and inject
allocation, read, write, and truncate failures. Add a callback fixture recording
transfer positions and maximum lengths to verify direction and bounded transfers
deterministically rather than relying only on RSS measurements.

Validate C tests with sanitizers where available, plus Python update_special/LRU
and B2ND resize regression tests. Check that sparse and in-memory behavior is
unchanged. Test Windows and POSIX implementations in CI where supported.

## Benchmarks and acceptance

Compare current and bounded-buffer updates with early/middle/late replacements,
small/large growth deltas, cache eviction loops, and concurrent independent
frames. Measure scratch allocation size, peak RSS, transferred bytes, latency,
and different buffer sizes. Avoid compression/network costs obscuring movement.

Acceptance: payload-movement scratch never exceeds the selected cap; surviving
data and final on-disk size are correct; no unrelated API/format change; no major
avoidable throughput regression. Total memory may still scale with index tables,
metadata, replacement chunks, or mmap mappings—this is not a process-RAM bound.

Deferred: batched multi-chunk compaction (to reduce repeated tail movement),
journaling/crash recovery, sparse cache lifecycle, and filesystem hole punching.

## Addendum: remove sparse chunk files for payload-free special values

This is a separate, explicitly scoped sparse-frame improvement; it supersedes
the earlier statement that sparse behavior is entirely unchanged. Keep it
independently testable from bounded-buffer contiguous compaction.

### Problem and verified code path

`frame_update_chunk()` represents ZERO, UNINIT, and NAN using special offsets
and sets their stored payload size to zero. For a previously materialized sparse
chunk, it nevertheless calls `sframe_create_chunk()` with that zero size.
That function opens the chunk file with `"wb"`, truncating its contents but
leaving the file and directory entry. Repeated cache churn therefore leaves
empty files that still consume inodes and directory-management work.

The sparse format already represents never-materialized special chunks without
chunk files. Preserve that representation when a materialized chunk becomes
payload-free. Do not apply this rule to every special value indiscriminately:
VALUE chunks carry a repeated-value payload and must retain their required data.

### Proposed change

- Capture the previous physical chunk ID before replacing its logical offset.
  Use that ID, not `nchunk`: reordering means they need not be equal.
- For a payload-free special replacement of a materialized chunk, remove the
  old chunk file instead of creating/truncating a zero-byte file.
- Special-to-special updates should not create files. Special-to-regular updates
  must allocate/write a valid chunk file and restore ordinary index semantics.
- Audit `sframe_delete_chunk()` for reuse. It currently calls libc `remove()`
  directly rather than an I/O callback; verify supported custom backends before
  assuming this works for every storage implementation. Preserve lock ordering
  and explicitly handle Windows open-handle deletion behavior.
- Specify the missing-file policy. Absence is normal for a committed special
  slot; a missing file still referenced by a regular slot can indicate damage.
  Do not silently treat arbitrary deletion errors as successful eviction.
- Do not automatically sweep existing zero-byte files based on file length.
  Only remove an orphan after checking the authoritative index under its guard;
  an empty file referenced as regular may be an interrupted write, not an
  eligible cleanup target. Legacy cleanup can be a separate maintenance path.

### Ordering and failure semantics

Decide index publication versus unlink ordering explicitly. Unlinking first can
leave a regular index entry pointing at a missing file if publication fails.
Publishing the special index first can leave an orphan if unlink fails. Prefer
the recoverable-orphan direction where the existing update protocol permits it,
but audit header/trailer updates, ID reuse and recovery before selecting it.
Hold the mutation guard through publication and deletion so another writer
cannot reuse the old physical ID in between. Shared readers must honor the same
frame-level protocol; separate files do not create atomic multi-file updates.

This addendum does not promise crash atomicity. Document partial-success/error
semantics and provide an index-aware recovery strategy for interrupted cleanup.
Do not report freed space or inode reclamation until deletion actually succeeds;
POSIX open handles can retain the deleted file's blocks until their last close.

### Tests and acceptance

Test regular-to-UNINIT/ZERO/NAN, repeated special updates, and refills; verify
chunk-file absence, correct index state, reopening, unaffected chunks, and no
growth in orphan-file count over repeated eviction/refill cycles. Include VALUE
as a non-removal control, reordered physical IDs, last/all chunks evicted, and
legacy empty files. Inject failures around index publication and deletion;
test permission errors, missing files, supported I/O backends and platforms,
and concurrent readers/writers under locking.

Measure both payload bytes and directory-entry/inode counts. Acceptance: clean
successful eviction of payload-free chunks leaves no corresponding chunk file,
without deleting unrelated IDs or introducing new format requirements.

## Adjacent streamlining opportunities (not part of the initial implementation)

1. **Batch sparse updates and index serialization.** The current update path
   decompresses/recompresses the full offsets table and creates/frees a codec
   context for each update. A batch update could amortize that work, especially
   for multi-chunk eviction. Profile before adding an API; retain one consistent
   index/fetched-state publication boundary and define partial failure behavior.
2. **Avoid repeated physical-ID scans.** Special-to-regular sparse updates scan
   the offsets to find a maximum ID. Consider a validated allocator/high-water
   mark if profiling shows it matters. Any cached value needs cross-handle stale
   state detection, restart reconstruction, overflow checks, and safe handling
   of orphan files. Do not casually add a process-local counter or format field.
3. **Consolidate failure-path cleanup.** Nearby functions have early returns
   after allocation/opening: for example, `frame_update_trailer()` returns on
   failed trailer write/truncation without closing its opened handle. Audit
   ownership and use consistent cleanup paths, with fault-injection tests.

These are source-based opportunities, not measured speedup claims. Prioritize
resource cleanup and sparse-file removal; measure index overhead before pursuing
allocator or batching redesigns.

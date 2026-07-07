# Transparent growth-SWMR: readers follow shape changes made by another handle

Status: future consideration (not scheduled). Follows up on the stale-handle
resync + frame locking work (see `plans/locking-sidecar-flock.md` and
`python-blosc2/plans/file-locking.md`).

## Context

With the resync + `locking=True` machinery, **fixed-shape SWMR already works**:
one writer mutating chunk contents, N readers (threads or processes), all
coherent, verified end-to-end. What does *not* work transparently is the
classic HDF5-SWMR use case: the writer **grows** the array (resize/append)
while readers follow. Verified behavior today: after `w.resize((20, 10))`, an
existing reader still reports `(10, 10)` and gets `IndexError` for the grown
region; a fresh handle sees everything. Readers must reopen to observe new
extents.

Root cause: the resync refreshes the *frame* layer (offsets index, lengths,
trailer) and the *schunk counters* (`nchunks`/`nbytes`/`cbytes`), but not:

1. the **header metalayers** cached in `schunk->metalayers[]` at open
   (`frame_get_metalayers`, frame.c) — the `b2nd` metalayer there holds
   shape/chunkshape/blockshape/dtype;
2. the **vlmetalayers** cached from the trailer (`frame_get_vlmetalayers`);
3. the **`b2nd_array_t` struct** (shape/extshape/strides/...), deserialized
   once in `b2nd_from_schunk` → `b2nd_deserialize_meta`;
4. (Python is free: `NDArray.shape` reads through to the C struct on every
   access — once C refreshes, Python follows with no change.)

## Design

### 1. Change tick on the schunk (c-blosc2)

Add a monotonically increasing `schunk->change_tick` (or similar), bumped by
`frame_refresh_if_stale()` whenever it actually refreshes (it already touches
`frame->schunk` there), and by local mutations that alter metadata. This gives
upper layers a cheap "did anything change since I last looked?" signal without
re-reading metalayers on every access. Note: `blosc2_schunk` is
library-allocated, so appending a field is safe.

### 2. Metalayer reload on refresh (c-blosc2, frame.c/schunk.c)

When `frame_refresh_if_stale()` fires:

- free and re-read the header metalayers (`schunk->metalayers[]`,
  `schunk->nmetalayers`) via the same code path used at open
  (`frame_get_metalayers`) — needs a small "reset + reload" wrapper since the
  open-time function assumes empty state;
- same for the trailer vlmetalayers (`frame_get_vlmetalayers`).

Contract notes:
- `blosc2_meta_get`/`blosc2_vlmeta_get` copy content out, so reloading does
  not invalidate anything user-held. Audit internal callers that keep
  `schunk->metalayers[i]` pointers across operations (there should be none
  outside open paths — verify).
- Re-entrancy: the reload path calls `get_header_info` again; by then
  `frame->len` matches disk and `force_refresh` is cleared, so no recursion —
  but add a guard flag anyway.
- Cost: only on a *detected* change (header/trailer chunk decompression);
  zero on the hot path.

### 3. b2nd struct refresh (c-blosc2, b2nd.c)

`b2nd_array_t` ops read cached shape/strides. At the top of the b2nd entry
points that depend on shape (`b2nd_get_slice*`, `b2nd_set_slice*`,
`b2nd_squeeze`, iterators, ...):

- compare a cached `array->last_tick` against `array->sc->change_tick`;
- on mismatch, re-deserialize the `b2nd` metalayer into the struct, reusing
  the existing `update_shape()` (b2nd.c:143) / `b2nd_deserialize_meta`
  machinery (the same code `b2nd_resize` uses to keep struct+metalayer in
  sync).

Choke point: find the minimal set of entry points (possibly a single
`b2nd_refresh_if_stale(array)` helper called from each public b2nd function
that reads geometry).

### 4. Writer-side ordering (correctness of the observation window)

Each operation is atomic under locking, but a resize is a *sequence*
(shape-metalayer update + append/insert/delete of special chunks — see
`b2nd_resize`/`update_shape`, which does `blosc2_meta_update(array->sc,
"b2nd", ...)`). A reader landing mid-sequence must never see a shape that
points at chunks that don't exist yet. Verify the current order in
`b2nd_resize` and, if needed, enforce **data first, shape last** for growth
(and the reverse for shrink) — the HDF5 SWMR "flush dependency" rule, applied
once at this single spot.

Alternative considered: hold the exclusive frame lock across the whole resize
sequence (frame_lock is re-entrant, so a `frame_lock/unlock` bracket in
`b2nd_resize` would work when locking is on). That gives atomic resize for
locked handles and ordering is then only a fallback for unlocked ones. Cheap;
probably worth doing both.

### 5. Guarantee levels (document)

- With `locking=True` on every handle: growth-SWMR is exact (generation
  counter detects every mutation, including same-length metalayer rewrites).
- Without locking: best-effort — growth changes the frame length so detection
  virtually always fires, but the documented FRAME_LEN blind spot applies.
- Consistency stays per-operation: no whole-array snapshot isolation (readers
  can see shape N while a concurrent writer is already at N+1 — same weak
  ordering HDF5 SWMR offers).
- Contract remains **single writer**; cross-handle append/insert counter gap
  (see locking plan, out-of-scope list) must be fixed first if multi-writer
  growth is ever wanted.

## Touched components (expected)

- `blosc/frame.h|c` — change tick, metalayer reload hook in
  `frame_refresh_if_stale`
- `blosc/schunk.c` — metalayer/vlmetalayer reset+reload helpers (factor out of
  the free path in `blosc2_schunk_free`)
- `blosc/b2nd.c` — `b2nd_refresh_if_stale` + calls; resize ordering / lock
  bracket
- `include/blosc2.h` — doc notes (vlmeta readers also become
  refresh-on-change)
- tests: C (two handles, resize via h1, h2 reads grown region + shrink case +
  vlmeta visibility), python-blosc2 (twin of the manual experiment: reader
  follows `resize` without reopening; a growing-array hammer variant)
- python-blosc2: expected **zero code changes** (shape reads through); just
  tests + doc note

## Open questions for when this is picked up

1. Should vlmeta auto-refresh be part of the same change or split out? (Same
   mechanism; splitting keeps the b2nd review smaller.)
2. Refresh-on-read only, or also expose an explicit `blosc2_schunk_refresh()`
   / `NDArray.refresh()` for callers that want deterministic sync points?
3. dtype/chunkshape changes are also possible metalayer rewrites — reject as
   "not SWMR" (raise on mismatch) or follow them too? (Shape-only is the
   HDF5-compatible scope; chunks/blocks changing under a reader is likely
   better treated as an error.)

# Locking / SWMR — remaining work (todo)

## Context

Status as of 2026-07-07, after the locking/SWMR push landed across both repos:

- c-blosc2: stale-handle coherence (`frame_refresh_if_stale`), growth-SWMR
  (`change_tick`, metalayer reload, `b2nd_refresh()`), opt-in sidecar locking
  (flock/LockFileEx + generation counter), `BLOSC_LOCKING` env var, the
  `blosc2_schunk_lock()/unlock()` bracket API, the open-vs-writer race fix,
  and the `blosc2_vlmeta_exists()` staleness poll.
- python-blosc2: `locking=` plumbing, `SChunk.holding_lock()`, `change_tick`
  exposure, cross-process EmbedStore (transactional writes + key-map re-sync),
  cross-process DictStore `.b2d` (estore lock doubles as store-wide lock +
  `dstore_tick` change signal), atomic `to_b2z()`, docs and cross-process
  tests for all of it.

This file collects what is still open, ranked.  Prior plans with design
details: `plans/locking-sidecar-flock.md` (mechanism, rejected alternatives),
`plans/high-level-formats-locking.md` (store formats, incl. status notes).

---

## 1. Stale-writer append/insert re-sync (c-blosc2) — HIGH

The one known correctness gap left at the frame level (deliberate, from the
original coherence work).  `blosc2_schunk_append_chunk()` and
`blosc2_schunk_insert_chunk()` apply the nbytes/cbytes/nchunks deltas to the
*cached* schunk counters before any frame read, so a writer whose handle went
stale persists wrong header counters.  `update`/`delete` are safe because they
read a chunk (and therefore refresh) first.

Locking alone does not close it: the lock acquisition only *flags* the
staleness (`force_refresh`); the refresh happens on the first frame access,
which is after the deltas were applied.  The python stores dodge it because
`_sync_metadata()` performs a frame-polling vlmeta read inside the bracket
before any write — but bare C callers appending through a second handle are
exposed.

Fix shape:

- In the locked wrappers (`blosc2_schunk_append_chunk` etc., schunk.c), after
  `frame_lock(frame, true)` succeeds, call `frame_check_stale()` before
  invoking the `_unlocked` body, so the counters the deltas are applied to are
  current.  It nests on the held lock, and is a no-op when nothing changed.
- Decide whether the non-locking path should poll too (it already half-does
  via FRAME_LEN checks in read paths; appending from stale handles without
  locking is outside the single-writer contract — probably just document).
- Test: extend `tests/test_frame_lock.c` — two locked handles, h1 appends,
  then h2 appends; the header counters (nbytes/cbytes/nchunks re-read from a
  fresh open) must equal the sum of both appends.  Verify it fails before the
  fix.  A fork-based two-appender variant would pin the cross-process case.

## 2. Caterva2 integration — HIGH (different repo)

The original motivation for the whole effort; everything it needs is now
shipped.  Per the parked plan `caterva2/plans/peercache-locking.md`:

- Replace Caterva2's process-wide io_lock with per-cache asyncio locks plus
  `locking=True` (or `BLOSC_LOCKING=1` fleet-wide, now that it exists).
- Key insight recorded there: the fetch→read→touch sequence needs a critical
  section because untouched chunks sort as oldest in eviction.
- Sanity-check the eviction/refill hammer against a real peer-cache workload
  (the c-blosc2 fork hammer models it, but through the Caterva2 code paths).

## 3. Release coordination (both repos) — MEDIUM, blocks next release

- python-blosc2 now hard-requires a c-blosc2 with `blosc2_schunk_lock` (new
  export): before releasing python-blosc2, point the bundled pin at a
  *released* c-blosc2 (3.1.6) and bump any system-blosc2 minimum-version
  check (`BLOSC2_MIN_VERSION` or CMake equivalent).
- Release notes for both repos should present the feature set together:
  `locking=` / `BLOSC_LOCKING`, `holding_lock()`, cross-process
  EmbedStore/DictStore, atomic `to_b2z()`, and the caveats (advisory, every
  handle must opt in, no NFS, no mmap).

## 4. Growth-SWMR tests in python-blosc2 — MEDIUM

Leftover flagged when growth-SWMR landed: the reader-follows-resize behavior
has C tests and a C example, but python-blosc2 does not pin it.

- Test: writer process `resize()`s and fills an on-disk NDArray progressively
  (as in c-blosc2's growth-SWMR example); a reader handle opened before must
  see `shape` grow and read the new data — both via implicit re-sync on
  access and via the explicit `arr.refresh()`.
- Cover both locking and plain (FRAME_LEN-poll) modes; the latter is the
  documented single-writer SWMR contract.

## 5. User-guide docs page (python-blosc2) — MEDIUM

The multi-process story currently lives only in class docstrings scattered
across SChunk/Storage/EmbedStore/DictStore.  Write one short page, e.g.
"Sharing containers across processes", covering:

- SWMR without locking (single writer, readers follow; growth included).
- Locking: `locking=True`, `BLOSC_LOCKING`, advisory semantics (every handle
  must opt in), the `.b2lock` sidecars, crash-safety.
- `holding_lock()` for multi-operation atomicity.
- The stores: EmbedStore/DictStore cross-process guarantees and accepted
  races; `.b2z` as an atomic snapshot format.
- Caveats: NFS, mmap (silently ignored via env var vs. ValueError when
  explicit), Windows in-use-target rename.

Also: `change_tick` does not render in the SChunk reference (it lives on the
ext base class and autodoc `:members:` skips inherited members) — either add
a thin python-level property with a docstring, or list it explicitly in
`doc/reference/schunk.rst`.

## 6. Fixed-length metalayer staleness (c-blosc2) — LOW

`blosc2_meta_exists()`/`blosc2_meta_get()` are `static inline` in blosc2.h and
read the cached `schunk->metalayers`, so they cannot poll (inline code cannot
reach the internal `frame_check_stale`).  A `blosc2_meta_update()` made
through another handle is therefore invisible to generic readers
(`schunk.meta["x"]` in python).  b2nd is unaffected — it refreshes its own
metalayer through its own paths.

Fix shape: convert them to real exported functions (same signatures; keep the
inline versions as deprecated aliases if ABI fuss matters, though a major-ish
release can just move them) and add the same poll as `blosc2_vlmeta_exists()`.
Test mirrors `test_vlmeta_exists_stale`.  Low urgency: fixed metalayers
rarely change after creation, and their size cannot change by design.

## 7. Parked / explicit non-goals (record, do not do)

- **NFS**: flock is unreliable there; documented as unsupported.  The
  OFD-fcntl fallback idea stays parked until someone actually needs it.
- **Retry-on-torn-read for non-locking readers** ("option C" of the original
  discussion): without locking, a reader racing a writer can get
  BLOSC2_ERROR_FILE_READ and must retry; documented behavior.  Revisit only
  if it bites real users who cannot enable locking.
- **`.b2z` locking**: never — snapshot format; atomic replace is the design.
- **Locking on by default**: rejected (2026-07-07 analysis) because reads
  would create sidecar files — breaks read-only mounts, dirties backups.
  `BLOSC_LOCKING` is the deployment-wide switch instead.
- **Windows concurrency coverage**: the C fork tests are POSIX-only, but the
  python subprocess tests (which do run on Windows CI) exercise the
  LockFileEx path; acceptable.  The `.b2z` atomic-replace test is skipped on
  Windows (in-use targets cannot be replaced there).

## Suggested order

1 (append/insert fix) and 2 (Caterva2) carry real user impact; 3–5 are small
hygiene items that should land before/with the next release pair; 6 rides
along whenever schunk.c is next touched.

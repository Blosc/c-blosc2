# Locking for the high-level store formats (.b2e / .b2d / .b2z)

## Context

The frame-level locking (sidecar `.b2lock` via flock/LockFileEx, July 2026; see
`plans/locking-sidecar-flock.md`) plus the `BLOSC_LOCKING` env var protect
*individual frame operations*: each schunk read/mutation is atomic against
other handles and processes, and stale handles re-sync exactly via the
generation counter.

The python-blosc2-only store formats sit **above** that layer and are *not*
protected by it, because they keep store-level state in Python and perform
multi-step transactions that the frame lock cannot see:

- **`.b2e` (EmbedStore)**: one cframe underneath. `__setitem__` is a data
  write into the backing schunk followed by a *separate* `_save_metadata()`
  vlmeta flush (`embed_store.py`). Each step takes/releases the frame lock
  independently, so the pair is not atomic. Worse, `_embed_map` and
  `_current_offset` are loaded once at open and never re-read: a reader never
  sees another process's new keys, and **two concurrent writers both start
  from the same `_current_offset`**, overwrite the same byte range, and the
  last metadata flush silently discards the other's map entries. Locking makes
  each step atomic while the combined result is still corruption.
- **`.b2d` (DictStore, directory)**: a directory of independent frames plus
  `embed.b2e`. With locking on, each external `.b2nd`/`.b2f` gets its own
  sidecar, so *per-array* access is safe — but store-level operations (add a
  key = create file + update map; delete; externalize-on-threshold) span
  multiple files with no lock covering the ensemble, plus the inner
  EmbedStore's stale-Python-state problem.
- **`.b2z` (DictStore, zip)**: reads open frames at offsets inside the zip
  (all of them share a single accidental `store.b2z.b2lock` sidecar), but
  writes never go through frame ops at all: `to_b2z()` rewrites the zip **in
  place** (`zipfile.ZipFile(b2z_path, "w")`, `dict_store.py`). A concurrent
  reader of the same `.b2z` reads a file being truncated and rewritten; no
  frame locking can help.

Decision from the discussion (2026-07-07): frame locking stays as-is; the
store formats need their own, mostly python-blosc2-side, measures. Split into
a cheap "do soon" step and a "when multi-process stores become a real use
case" step (Caterva2-style deployments being the likely driver).

## Status (2026-07-07)

- **1a, 1b: DONE** (python-blosc2, uncommitted).
- **2a: DONE** (c-blosc2, uncommitted). Implementing the atomicity test
  uncovered and fixed a pre-existing race in the *open* path:
  `blosc2_schunk_open_offset_udio()` read the frame header before acquiring
  the shared lock and could not re-sync afterwards (`frame_refresh_if_stale`
  is a no-op while `frame->schunk == NULL`), so opening a frame while another
  process mutated it failed with "Cannot decompress offsets chunk". Fixed by
  re-reading the frame under the lock when the generation counter flags a
  mutation (costs one extra header read when opening a previously-mutated
  locked frame).
- **2b, 2c: DONE** (python-blosc2, uncommitted, after the 2a bundle bump).
  Implementation notes vs. the original sketch:
  - No new C API was needed beyond 2a: `blosc2_vlmeta_get()` already polls
    via `frame_check_stale()`, so a raw vlmeta read *is* the sync point;
    `change_tick` was exposed to Python (schunk struct field in the pyx) and
    gates the msgpack re-decode.  `SChunk.holding_lock()` context manager
    wraps `blosc2_schunk_lock()`/`unlock()`.
  - EmbedStore: `_sync_metadata()` on every access, `_write_bracket()`
    (lock + sync) around `__setitem__`/`__delitem__`; the empty map is
    flushed at creation so other handles always find the vlmeta to sync from
    (`blosc2_vlmeta_exists` does not poll — a fresh handle would otherwise
    never see a first flush made elsewhere).
  - DictStore (.b2d): no separate Python lock helper — the inner
    embed.b2e's exclusive frame lock doubles as the store-wide lock
    (`_mutation_bracket()`), and external-leaf mutations bump a
    `dstore_tick` vlmeta so readers get a change signal through the estore's
    change_tick (the "every mutation touches the estore" invariant did NOT
    hold for external leaves; the tick bump restores it).  Readers re-scan
    `map_tree` under the exclusive lock so they cannot index half-written
    files of in-flight transactions.  New `locking` parameter on DictStore
    (rejected for .b2z and with mmap); TreeStore inherits it via kwargs.
  - Known accepted races (documented in the DictStore docstring): a reader
    holding a value whose key another process deletes may get errors from
    that value; a crash mid-mutation can leave a partial external file.

## Step 1 — cheap, do soon

### 1a. Atomic replace in `to_b2z()` (python-blosc2)

Write the zip to a temp file in the *same directory* as the target (same
filesystem, so rename is atomic), then `os.replace()` onto `b2z_path`.
Readers then see either the old zip or the new one, never a torn one; POSIX
keeps already-open fds valid across the replace. This is the right semantics
for what is effectively a snapshot format — no locking needed at all.

- Use `tempfile.NamedTemporaryFile(dir=os.path.dirname(b2z_path),
  delete=False)` (or mkstemp) + `os.replace()`; clean up the temp file on
  exception.
- Keep the existing `overwrite` check semantics (check target existence
  *before* writing the temp).
- Windows note: `os.replace()` fails if another process holds the target open
  without `FILE_SHARE_DELETE`; acceptable — document it, don't work around it.
- Test: rewrite a `.b2z` while a reader in a subprocess loops over
  `blosc2.open(b2z_path, offset=...)` reads; zero torn reads allowed
  (mirror the fork-hammer pattern of `tests/test_locking.py`).

### 1b. Documentation: set expectations (python-blosc2)

Add a short "Concurrency" note to the DictStore and EmbedStore docstrings
(and wherever the formats are described in the Sphinx docs):

- `locking=True` / `BLOSC_LOCKING` protects the *arrays inside* the stores,
  not the store structure; the stores themselves are single-process,
  single-writer today.
- `.b2z` is a snapshot format: safe to share read-only; regenerate with
  `to_b2z()` (atomic after 1a).
- Cross-process mutation of the same `.b2d`/`.b2e` is unsupported until
  Step 2 lands.

## Step 2 — real multi-process support (when a use case demands it)

### 2a. c-blosc2: public "hold the frame lock across N operations" API

Today the lock is strictly per-operation (taken/released inside each schunk
entry point, re-entrant via `frame->lock_depth`). Store-level transactions
need to hold the exclusive lock across several operations. The re-entrancy
counter already does the heavy lifting; expose it:

```c
BLOSC_EXPORT int blosc2_schunk_lock(blosc2_schunk* schunk);    // exclusive
BLOSC_EXPORT int blosc2_schunk_unlock(blosc2_schunk* schunk);
```

- `blosc2_schunk_lock()` = `frame_lock(frame, true)` without a matching
  unlock: bumps `lock_depth`, so every schunk op inside the bracket nests
  instead of re-acquiring; `blosc2_schunk_unlock()` drops it.
- No-op (success) when locking is not enabled on the handle — callers don't
  need to care.
- Beware: everything inside the bracket is exclusive, including reads; that's
  fine for short transactions (metadata + a few chunk writes).
- Needs: doc in `blosc2.h`, a test in `tests/test_frame_lock.c` (two
  processes, one holding the bracket while mutating twice; the other must
  never observe the intermediate state), and cython plumbing in
  python-blosc2 (`schunk.lock()` context manager, say
  `with schunk.holding_lock():`).

### 2b. python-blosc2: make EmbedStore transactional and re-syncable

The two halves, both depending on 2a:

1. **Re-sync**: before every operation (read and write), take the lock and
   reload `_embed_map`/`_current_offset` from vlmeta *if the store changed*.
   Detecting "changed" cheaply: c-blosc2's generation counter already forces
   the C-side refresh; expose a change tick to Python (the schunk's
   `change_tick` / lock_seq) so EmbedStore can skip the msgpack decode of
   `estore_metadata` when nothing moved. Readers then see new keys
   (growth-SWMR at the store level).
2. **Transactional writes**: wrap `__setitem__`/`__delitem__` in the 2a
   bracket: lock exclusively → reload metadata → write data → flush metadata
   → unlock. This fixes both the torn set (data without map entry) and the
   two-writers-same-offset corruption, because `_current_offset` is re-read
   under the lock.

- Crash-safety note: a crash between data write and metadata flush inside the
  bracket still leaves orphaned bytes in the schunk (harmless: unreachable,
  reclaimed on next compaction/`to_*` rewrite). Acceptable; document it.
- Test: fork-hammer with two writer processes adding disjoint keys to the
  same `.b2e` plus a reader listing keys; final map must contain all keys and
  every value must round-trip.

### 2c. python-blosc2: store-level lock for `.b2d` DictStore

DictStore mutations span multiple files, so per-frame locks can't cover them.
Add one store-level lock file inside the directory — the sframe sidecar
convention (`<dir>/.b2lock`) fits naturally and is already ignored/cleaned by
the existing tooling:

- Exclusive around whole DictStore mutations (setitem incl. externalization,
  delitem, map maintenance); shared around multi-file reads that must be
  consistent (e.g. iterating keys + values).
- Implementation: small Python helper over `fcntl.flock`/`msvcrt`-or-
  `LockFileEx` via ctypes, *or* reuse the C helpers through cython. Prefer
  the Python helper: it needs no c-blosc2 change and the store layer is
  Python anyway. Follow the same rules as the C side: advisory, per-store
  opt-in via the existing `locking` storage param + `BLOSC_LOCKING`, held
  per operation, no NFS.
- Inner arrays keep their own frame locks (harmless nesting: different lock
  files).
- Re-sync: DictStore's Python-side maps need the same reload-under-lock
  treatment as 2b; the store's `embed.b2e` change tick can serve as the
  store-wide change signal since every mutation touches it (verify this
  invariant, or bump a byte in the store lock file like the C sidecar does).
- Test: two processes adding/removing external arrays in the same `.b2d`
  concurrently; directory contents and map must agree at the end.

### 2d. Explicit non-goals

- **Locking for `.b2z`**: never. Snapshot format; atomic replace (1a) is the
  whole story. If read-while-regenerate becomes common, the answer is still
  rename, not locks.
- **NFS**: same exclusion as the frame-level locking.
- **Multi-writer `.b2z` or concurrent `to_b2z()` of the same target**: last
  writer wins after 1a; fine.

## Order / dependencies

```
1a (atomic to_b2z)          — independent, python-blosc2 only
1b (docs)                   — independent, python-blosc2 only
2a (lock bracket API)       — c-blosc2, prerequisite of 2b/2c re-sync bits
2b (EmbedStore)             — python-blosc2, needs 2a
2c (DictStore .b2d)         — python-blosc2, needs 2b (inner estore), lock
                              helper itself is independent
```

Step 1 is worth doing regardless of whether Step 2 ever happens: it removes
the one silent-corruption path reachable today (`to_b2z` under a concurrent
reader) and stops users from assuming a guarantee that isn't there.

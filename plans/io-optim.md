# I/O optimization for on-disk read paths: pread + cached read handle

Status: implemented (branch `io-optim`).

Measured on python-blosc2 `bench/optim_tips/tip_06_mmap_read.py` (8000 scattered
50-item slices out of a 20 MB cframe, warm page cache, **timed without
tracemalloc** — the bench harness starts it inside the timed region, which
roughly quadruples both numbers and swamps the difference in noise):

| | before | after |
|---|---|---|
| plain `open()` | 1.097 s | 0.617 s |
| `open(mmap_mode="r")` | 0.580 s | 0.556 s |

The default backend is now level with mmap, which was the goal. Note the
consequence for the tip itself: tip_06 no longer demonstrates anything, because
the naive path it was contrasting against is no longer slow.

## As implemented

Five things ended up in the branch that the design below did not anticipate.
They are recorded here because each was forced by a measurement or a bug.

1. **`blosc2_stdio_write` also became positional** (`pwrite`). Keeping `fwrite`
   would have left a coherence hazard: a read through the shared handle would
   miss bytes still sitting in that handle's stdio write buffer. Positional
   writes remove the buffer and cost nothing (each write call was already
   preceded by an `fseek`, which flushes).

2. **No handle caching on Windows** (`frame_reader_acquire`). The CRT opens
   files without `FILE_SHARE_DELETE`, so a cached handle turns every unlink or
   rename of the frame file into a sharing violation — and python-blosc2 does
   both, via `os.replace()` (`ctable_storage.py:931,1527,1540`,
   `dict_store.py:838`) and via remove-then-recreate for `mode="w"`
   (`blosc2_ext.pyx:1715`). On Windows the failure would be a hard
   `PermissionError`, not the stale read POSIX gets. Reinstating the cache there
   needs `CreateFileW(..., FILE_SHARE_DELETE)` plus POSIX-semantics deletion
   (Win10 1709+), and is only worth its cost once the win is measured on
   Windows — which has not been done. Note also that for synchronous handles the
   kernel serializes I/O per handle even with explicit `OVERLAPPED` offsets, so
   the concurrency half of the win may not exist there at all.

3. **The cached handle is revalidated, not blindly trusted**
   (`frame_reader_revalidate`). The original design argued invalidation was
   unnecessary because no rename/replace patterns exist in `frame.c`/`schunk.c`.
   That is true of c-blosc2 and false one layer up — see the paths in point 2 —
   so a cached handle would go on serving an unlinked inode. Demonstrated: open
   an array, rewrite its path with `mode="w"`, and the old handle keeps
   returning the pre-rewrite values.

   The check `fstat`s the cached fd, `stat`s the path, and compares
   `st_dev`/`st_ino`. It must run **before** `get_header_info`'s header read, not
   inside `frame_refresh_if_stale`: that function receives a `frame_len_on_disk`
   already read through the stale handle, so it early-returns, and invalidating
   there would pair old cached metadata with the replacement's bytes — worse
   than a consistent stale view. On mismatch it does **both**
   `frame_reader_invalidate()` and `force_refresh = true`; a replacement is
   often the same length as the original, and then the trailer poll never fires
   on its own.

   If the path cannot be stat'd at all, the handle is kept: the file was renamed
   away, the open inode is still valid, and that is what an open fd does on
   POSIX anyway.

4. **The cache is capped** (`reader_cache_claim` / `reader_cache_return`). A
   cached handle lives as long as its frame, so the process pays one descriptor
   per *open frame* rather than per in-flight read. python-blosc2's ctable
   indexes hold half a dozen arrays each and its test suite reached EMFILE on
   macOS, whose default soft limit is 256.

   A fixed cap does not work. Roughly 196 of the ~204 descriptors held at the
   suite's peak are `mmap_mode` index arrays from `indexing.py`, and **mmap has
   always held one fd per frame** — that baseline is pre-existing and unrelated
   to this branch. Adding a flat 96 on top of ~200 is what crossed 256. The cap
   is therefore `min(96, RLIMIT_NOFILE/16)`: 16 at a 256 limit, 96 anywhere
   sane. A sixteenth is what the suite tolerates — an eighth (32 slots) still
   failed. `BLOSC_MAX_CACHED_READERS` overrides it; `0` disables caching
   entirely, which reproduces main's read behaviour and is the control for any
   "is this branch responsible?" question.

   Deliberately not an LRU. Eviction means closing a descriptor other threads
   may be mid-`pread` on; a cap degrades to the old open-per-operation path
   instead of breaking. No atomics either: the counter only moves when a handle
   is actually opened or closed, i.e. once per frame, never per read, so a plain
   mutex costs nothing and avoids the MSVC `<stdatomic.h>` question. Lock order
   is frame mutex then global, consistently.

5. **No unlocked fast path in `frame_reader_acquire`.** Reading `read_fp`
   outside the mutex races the store under it — a C11 data race that TSan
   flags. An uncontended lock is a few ns against the ~600 ns pread it is
   handing a handle to.

Points 3-5 cost about 5% of the gain (0.586 s → 0.617 s). If that matters, the
cheaper staleness trigger is `fstat(fd).st_nlink == 0` — one syscall instead of
two, and it covers every python-blosc2 pattern; it only misses
rename-away-then-recreate.

## Problem

Every chunk fetch from an on-disk cframe pays a full `fopen`/`fseek`/`fread`/`fclose`
cycle — several times over. For scattered-read workloads (e.g.
python-blosc2 `bench/optim_tips/tip_06_mmap_read.py`: 8000 small slice reads),
this is why plain `open()` loses badly to `mmap_mode="r"`.

Syscall accounting for one `frame_get_lazychunk` on the default filesystem
backend (`frame->cframe == NULL`, non-sframe):

| Step | Where | Cost |
|------|-------|------|
| Re-read frame header | `get_header_info` → `frame.c:665-675` | fopen + fseek + fread + fclose |
| Staleness check | `frame_refresh_if_stale` (`frame.c:827`) | size query (fseek×3/ftell×2 via `blosc2_stdio_size`) |
| Chunk header + csizes | `frame.c:3346` onwards | fopen + fseek + fread (+ more reads) + fclose |

Then, when the lazy chunk is actually decompressed, **every block** read does
another fopen + fseek + fread + fclose (`blosc2.c:1827-1855`), and this runs
inside worker threads.

`fopen`/`fclose` dominate: open syscall + `FILE*` malloc + stdio locking. That
is ~6 open/close cycles per scattered slice read. Measured in isolation on the
bench file (8 KiB reads, warm, ns per read):

| | ns |
|---|---|
| `fopen`+`fseek`+`fread`+`fclose` — what this replaced | 10841 |
| `open`+`pread`+`close` — the naive pread swap | 9058 |
| cached fd + `pread` | 603 |
| cached fd + `lseek`+`read` | 840 |

which is the whole argument for this plan: swapping to pread *alone* buys 16% of
the I/O cost and is not worth the churn. Caching the handle buys 94%. pread is
the enabler — being stateless is what lets one handle be shared across the
decompression workers without a lock — not the win itself.

The mmap backend (`blosc2_stdio_mmap_open`, `blosc2-stdio.c:290`) already avoids
all of this by being idempotent: state lives in `params`, open after the first
is a pointer return.

## Design: two pieces, one PR

Piece 1 (pread) is a prerequisite for piece 2 (shared handle): a single cached
handle only works across threads if reads don't share seek state.

### Piece 1 — positioned reads in `blosc2_stdio_read`

`io_cb->read` is already positional (`read(ptr, size, nitems, position, fp)`),
so this is contained entirely in `blosc2_stdio_read` (`blosc2-stdio.c:188`):

- POSIX: replace the `fseek` + `fread` pair with a `pread(fileno(my_fp->file), ...)`
  loop (pread may return short; loop until `n_bytes` read or EOF/error).
  `off_t` is 64-bit on our POSIX targets, so the current `LONG_MAX` position
  guard (`blosc2-stdio.c:216-221`) can be dropped for the read path.
- Windows: no `pread`. Use `ReadFile` on `(HANDLE)_get_osfhandle(_fileno(file))`
  with an `OVERLAPPED` carrying the 64-bit offset — the positioned-read
  equivalent. It moves the file pointer, which is harmless because every caller
  passes an explicit position. Loop for short reads as well.

Side benefit: raw-fd reads bypass the stdio buffer, so a cached read handle can
never serve stale buffered data after another handle writes to the file.

Note on mixing: after this change the `FILE*`'s stdio read buffer is never
filled by the read path, so there is no buffered/raw coherence hazard within one
handle. `blosc2_stdio_size` still uses `fseek`/`ftell`, which is fine — it reads
no data and nothing depends on the fd offset.

### Piece 2 — cache the read handle in the frame

Add to `blosc2_frame_s` (`frame.h`, internal):

```c
void* read_fp;                    //!< Cached "rb" IO handle; NULL if none
blosc2_pthread_mutex_t read_fp_mutex;  //!< Guards read_fp open/close
```

(`blosc2_pthread_mutex_t` is already used in `blosc2.c`, Windows emulation
included. Init in `frame_new`, destroy in `frame_free` — and on every early-out
path in `frame_new`, `frame_from_file_offset` and `frame_from_cframe`, or the
`CRITICAL_SECTION` leaks on Windows.)

New helpers in `frame.c`:

```c
/* Returns an open "rb" handle for the frame file. On the default filesystem
   backend, opens once and caches in frame->read_fp; other backends fall
   through to io_cb->open per call. */
void* frame_reader_acquire(blosc2_frame_s* frame, const blosc2_io* io);
/* Closes fp only if it is not the cached handle. */
void frame_reader_release(blosc2_frame_s* frame, void* fp);
```

Gating: caching applies **only** when `io->id == BLOSC2_IO_FILESYSTEM`. Reasons:

- The current contract gives every reader its own handle; user-registered
  backends may not support concurrent `read` calls on one handle. Don't change
  their contract.
- The mmap backend's open is already idempotent/cheap; nothing to win.
- Piece 1 makes the default backend's `read` offset-atomic (pread), which is
  exactly what makes one shared handle safe under the threaded block reads in
  `blosc_d`.

Call sites to convert (all currently `io_cb->open(..., "rb", ...)` +
`io_cb->close`; leave `"rb+"`/`"wb"` sites alone):

- `frame.c`: `get_header_info` (`:665`), trailer reads (`:863`, `:1512`,
  `:1572`, `:2538`), `:1090`, `get_coffsets` (`:1933`), `:2044`, `:2288`,
  `frame_get_chunk` / `frame_get_lazychunk` (`:3103`, `:3346`), `:2813`.
- `blosc2.c`: lazy block read in `blosc_d` (`:1827`) — the multithreaded hot
  path; `:2597`; `:4079`.
- `schunk.c` / others: audit with `grep -n '"rb"' blosc/*.c` during
  implementation; convert any frame-file open, skip sframe chunk files.

sframes (`sframe_open_chunk`) open a different file per chunk: excluded.
(A small per-frame LRU of chunk fds is possible future work if sframe reads
ever matter.)

Invalidation — close cached handle (under mutex) at:

- `frame_free` (teardown).
- The `"wb"` / truncate sites in `frame.c` (`frame_from_schunk` and friends):
  the file is recreated from scratch, so invalidate before the write. In-process
  `"rb+"` appends do NOT need invalidation: pread on the same inode sees the new
  data.
- Whenever `frame_reader_revalidate` finds the path no longer resolves to the
  cached inode (see "As implemented", point 3).

`frame_reader_release`'s pointer comparison (`fp != frame->read_fp`) is only
safe because invalidation never interleaves with an acquire/release pair —
`frame_free` is teardown, `frame_from_schunk` is exclusive, and the revalidate
check runs at the start of an operation, before any worker spawns. If
invalidation ever moves somewhere concurrent, this needs refcounting instead:
otherwise the release closes a handle already closed under it.

### Interaction with file locking

When `frame->locking` is on, correctness already flows through
`frame_refresh_if_stale` + the sidecar generation counter. Run the
locking/SWMR test suites with caching enabled to confirm.

## Validation

Done:

1. c-blosc2 `ctest`: 1613/1613, including a new `tests/test_frame_shared_reader.c`
   that reads lazy chunks from a threaded context (`nthreads > 1`) on an on-disk
   frame — concurrent pread on the shared handle — and then replaces the frame
   file underneath the open schunk to pin the revalidation behaviour. Confirmed
   load-bearing: commenting out the `frame_reader_revalidate` call makes it fail
   with `Stale data after replacing the frame`.
2. python-blosc2 suite: 7913 passed at the default limit *and* under
   `ulimit -n 256`.
3. Perf: the table at the top of this document.

Outstanding:

4. **Windows CI.** The `ReadFile`+`OVERLAPPED` path has never executed, and
   after the Windows opt-out it is the *only* Windows read path — there is no
   cached-handle fallback masking it. This gates the merge.
5. On Linux, `strace -c -f` to confirm the openat/close count collapse.

## Non-goals

- No ABI change: `blosc2_io_cb` struct untouched; no new backend id; user IO
  plugins keep their exact current contract (per-call open/close).
- No header-info caching beyond what exists — `frame_refresh_if_stale` already
  governs metadata staleness; this plan only removes handle churn.
- No sframe fd caching (different file per chunk).

## Fixed in passing

`append_frame_to_file` (`schunk.c:532`) opened `"ab"` and wrote at
`io_pos = size`. POSIX ignores the offset on an `O_APPEND` fd; Windows honors
it. Benign while the offset coincides with EOF, but a cross-platform semantic
difference sitting in newly-rewritten code. It now opens `"rb+"`, falling back
to `"wb+"` when the file does not exist yet (which is what `"ab"` used to
cover), so the explicit position means the same thing on both platforms. The
same edit fixed two adjacent problems: the open passed `storage->io` where the
callback expects `storage->io->params` — harmless for the filesystem backend,
which ignores params, but type-confused for any other — and neither the open
nor the write was checked, so a failed open segfaulted in `io_cb->size(fp)`.

Note `blosc2_schunk_to_file`'s helper at `schunk.c:523` and `frame.c:4980` pass
`storage->io` the same way; left alone as out of scope.

Unchanged by the fix: appending to a *fresh* file returns offset 0, which
`blosc2_schunk_append_file` cannot distinguish from failure (`if (offset <= 0)`).
That predates this branch and behaves identically before and after.

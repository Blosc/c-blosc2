# Shared Thread Pool — Implementation Reference

## Overview

c-blosc2 uses a **shared managed thread pool** paradigm for parallel
compression and decompression.  Pools are keyed by thread count and shared
across every `blosc2_context` that requests the same `nthreads`.  This
replaces the former per-context thread pool model, eliminating redundant
thread creation/destruction when many contexts compress or decompress
concurrently.

## Threading Backends

Each context tracks its backend in `context->thread_backend`:

| Value | Constant | Meaning |
|-------|----------|---------|
| 0 | `BLOSC_BACKEND_SERIAL` | Single-threaded; no pool needed. |
| 1 | `BLOSC_BACKEND_SHARED_POOL` | Uses a shared pool from the global registry. |
| 2 | `BLOSC_BACKEND_CALLBACK` | Caller-managed threads via `blosc2_set_threads_callback()`. |

Backend selection happens lazily in `check_nthreads()` (called from
`do_job()` before every operation):

- If `nthreads <= 1` → `BLOSC_BACKEND_SERIAL`.
- If a caller-managed callback is installed → `BLOSC_BACKEND_CALLBACK`.
- Otherwise → `attach_shared_pool()` → `BLOSC_BACKEND_SHARED_POOL`.

When `context->new_nthreads != context->nthreads`, the old backend is
released and a new one is attached, so a context can dynamically rebind to
a different pool mid-lifetime.

## Data Structures

### `blosc_shared_pool` (blosc2.c)

One pool per distinct `nthreads` value.  Stored in a global singly-linked
list (`shared_pools`) protected by `pool_registry_mutex`.

```
nthreads          – number of worker threads
shutdown          – flag to signal workers to exit
context_refs      – how many contexts currently reference this pool
active_jobs       – queue entries in flight (enqueued but not yet completed)
threads[]         – worker pthread handles
thread_contexts[] – per-worker scratch (tmp buffers, tid, owner_pool)
mutex             – protects the job queue and active_jobs
work_cv           – workers wait here for new work
idle_cv           – signalled when active_jobs drops to 0
job_queue_head/tail – singly-linked FIFO of pending job entries
next              – link to the next pool in the global registry
```

### `blosc_job_group` (blosc2.c)

Stack-allocated per call to `parallel_blosc()`.  Holds all shared mutable
state for one compress/decompress operation:

```
context           – back-pointer to the calling blosc2_context
next_block        – atomic counter for dynamic block claiming (starts at -1)
output_bytes      – running total of compressed output
giveup_code       – error/abort flag (1 = ok, 0 = give up, <0 = error)
active_workers    – workers still processing this job
blocks_completed  – workers that reached job_done
completed         – set when active_workers hits 0
static_schedule   – true for decompression / memcpyed (tid-based partitioning)
dref_not_init     – delta filter first-block sentinel
mutex             – protects output_bytes, giveup_code, active_workers, etc.
delta_mutex/cv    – serialises the first delta-filter block
completion_cv     – caller waits here until completed == true
```

### `blosc_job_queue_entry` (blosc2.c)

A node in the pool's FIFO queue.  One entry per worker per operation:

```
job               – pointer to the blosc_job_group
logical_tid       – 0..nthreads-1, used for static block partitioning
next              – link to next entry in queue
```

## Lifecycle

### Initialisation

`blosc2_init()` initialises `pool_registry_mutex`.  No pools are created
until the first multi-threaded operation.

### Attach (per context)

`attach_shared_pool(context)`:

1. Locks `pool_registry_mutex`.
2. Searches `shared_pools` for a pool with matching `nthreads`.
3. If not found, calls `create_shared_pool()` to spawn workers and
   prepend the new pool to the list.
4. Increments `pool->context_refs`.
5. Sets `context->thread_pool`, `context->thread_backend`, and
   `context->threads_started`.

### Operation (`parallel_blosc`)

1. A `blosc_job_group` is stack-allocated and initialised via
   `job_group_init()`.
2. `context->job` is pointed at the group.
3. Under `pool->mutex`, N queue entries are created (one per worker, each
   carrying a `logical_tid` of 0..N-1) and appended to the FIFO.
4. `job.active_workers` is set to N, then workers are woken via
   `pool->work_cv`.
5. The caller blocks on `job.completion_cv` until `job.completed == true`.
6. On return, `context->output_bytes` and `context->thread_giveup_code`
   are copied from the group; the group is destroyed.

### Worker Loop (`shared_pool_worker`)

Each worker thread runs an infinite loop:

1. Lock `pool->mutex`; wait on `pool->work_cv` while the queue is empty.
2. Dequeue the head entry; extract `job` and `logical_tid`.
3. Unlock, free the entry.
4. Set `thcontext->parent_context = job->context` and
   `thcontext->tid = logical_tid`.
5. Call `t_blosc_do_job(thcontext)` — the same work function used by
   the callback backend.
6. After the job, decrement `pool->active_jobs`; signal `pool->idle_cv`
   if everything is idle.

### Block Assignment

Inside `t_blosc_do_job`, blocks are assigned to workers in one of two
modes:

- **Static schedule** (decompression / memcpyed): each worker processes a
  contiguous slice based on its `logical_tid`.  This avoids mutex
  contention for read-only operations.
- **Dynamic schedule** (compression): workers claim blocks one at a time
  via `claim_job_block()`, which atomically increments `job->next_block`
  under `job->mutex`.

### Release (per context)

`release_thread_backend(context)` (called from `blosc2_free_ctx` or when
`new_nthreads` changes):

1. Locks `pool_registry_mutex`.
2. Decrements `pool->context_refs`.
3. If refs reach 0 **and** the pool is idle (no in-flight jobs), unlinks
   the pool from `shared_pools` and calls `destroy_shared_pool()`.
4. Otherwise the pool stays alive for other contexts to use.

### Shutdown

`blosc2_destroy()`:

1. Frees the global context.
2. Walks the `shared_pools` list and destroys every remaining pool
   (sets `shutdown = 1`, broadcasts `work_cv`, joins all worker threads).
3. Destroys `pool_registry_mutex`.

A `g_initlib` guard in `blosc2_free_ctx()` ensures that freeing a context
after `blosc2_destroy()` skips the pool release (no use-after-free).

## Concurrency Model

Multiple contexts can submit jobs to the **same pool concurrently**.
Because each operation creates its own `blosc_job_group` on the caller's
stack, workers from the same pool can interleave work for different
contexts without interference:

- Each worker reads `context->src`, `context->dest`, etc. from its own
  job's context pointer.
- Mutable per-operation state (`output_bytes`, `giveup_code`,
  `next_block`) lives on the job group and is protected by `job->mutex`.
- Worker scratch buffers (`tmp`, `tmp2`, `tmp3`) belong to the pool's
  `thread_context` array and are sized lazily via
  `ensure_thread_context_capacity()` (high-water-mark allocation).

## Key Design Decisions

1. **Queue-based, not barrier-based.**  The old model used POSIX barriers
   (or emulated barriers) to synchronise context-owned threads.  The
   shared-pool model uses a FIFO job queue with condition variables,
   allowing true concurrent submissions.

2. **Logical `tid` per queue entry.**  Pool threads have persistent IDs,
   but these don't correspond 1:1 with job workers (any thread can pick up
   any entry).  Each queue entry carries a `logical_tid` (0..N-1) that is
   used for static block partitioning and user-visible `preparams.tid` /
   `postparams.tid`.

3. **Refcount-driven pool lifetime.**  Pools are created on first use and
   destroyed when the last referencing context detaches *and* the pool is
   idle.  `blosc2_destroy()` acts as a final sweep for any pools that
   remain at shutdown.

4. **`g_initlib` guard.**  Prevents use-after-free if a context outlives
   `blosc2_destroy()`.  The context can still be freed (memory is
   released) but the pool detach is skipped.

5. **Consistent allocator.**  All pool and queue-entry allocations use
   `my_malloc` / `my_free` (32-byte aligned) for consistency with the
   rest of Blosc's internal allocations.

## Files

| File | Role |
|------|------|
| `blosc/blosc2.c` | Pool structs, registry, worker loop, `parallel_blosc()`, `check_nthreads()`, `attach/release/create/destroy_shared_pool()` |
| `blosc/context.h` | `blosc2_context` fields (`thread_pool`, `thread_backend`, `job`), `thread_context` fields (`owner_pool`), backend constants, `ctx_uses_parallel_backend()` helper |
| `blosc/schunk.c` | Uses `ctx_uses_parallel_backend()` to check if parallel backend is active |
| `tests/test_shared_pool.c` | 10 tests: no-pool for nthreads=1, pool sharing, different-nthreads isolation, dynamic rebind, round-trips with shuffle/delta/bitshuffle, refcount destroy, serial delta, many-contexts sharing |

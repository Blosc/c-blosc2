# Shared Managed Thread Pool

## Purpose

This note describes the shared managed thread-pool implementation that is
currently in the `c-blosc2` tree. It is a reference for later maintenance, not
an implementation proposal.

The goal of the implementation is to avoid per-context thread creation for
Blosc-managed multithreaded contexts while preserving concurrent use of
different contexts.

## Backend Model

The runtime has three internal threading backends:

- `BLOSC_BACKEND_SERIAL`
- `BLOSC_BACKEND_SHARED_POOL`
- `BLOSC_BACKEND_CALLBACK`

Selection rules:

- `nthreads <= 1` uses the serial backend.
- `nthreads > 1` and `threads_callback == NULL` uses the shared managed pool.
- `nthreads > 1` and `threads_callback != NULL` uses the callback backend.

`ctx_uses_parallel_backend()` in `blosc/context.h` is the internal predicate
for "this context is parallel" and should be used instead of inferring this
from `threads_started`.

## Shared Pool Registry

Shared pools are process-global and keyed by exact `nthreads`.

Each pool contains:

- the worker threads,
- one reusable `thread_context` per worker,
- a FIFO queue of work items,
- `context_refs`,
- `active_jobs`,
- pool mutex/condition variables,
- shutdown state.

Registry rules:

- pools are looked up by exact `nthreads`,
- contexts with the same `nthreads` share one pool,
- contexts with different `nthreads` use different pools,
- pools are created lazily on first attach.

The registry is protected by `pool_registry_mutex`.

`attach_shared_pool()` calls `blosc2_init()` if needed before accessing the
registry, so user contexts do not depend on explicit global initialization.

## Pool Lifetime

Pool lifetime is independent from the lifetime of the global non-contextual
state.

Important properties:

- attaching a context increments `pool->context_refs`,
- freeing a context decrements `pool->context_refs`,
- a pool is destroyed only when:
  `context_refs == 0`, `active_jobs == 0`, and the queue is empty,
- `blosc2_destroy()` does not forcibly destroy pools still referenced by
  user-created contexts.

This is intentional. A user-created context may remain valid across
`blosc2_destroy()` and free itself later.

`pool_registry_mutex` is initialized from `blosc2_init()`. It is only destroyed
from `blosc2_destroy()` when `shared_pools == NULL`.

## Worker Ownership

Shared-pool workers own their reusable scratch state through their
`thread_context` objects.

This includes:

- temporary buffers (`tmp`, `tmp2`, `tmp3`, `tmp4`),
- codec scratch state such as ZSTD/LZ4 contexts,
- current scratch capacity.

Scratch buffers are high-water-mark allocations: they grow when needed and are
reused afterwards.

Before running a job, `ensure_thread_context_capacity()` makes sure the worker
has enough temporary space for the current context.

## Job Groups

Every call to `parallel_blosc()` creates a stack-allocated `blosc_job_group`
that represents one compression or decompression operation.

The job group owns per-operation mutable state:

- `next_block`,
- `next_output_block`,
- `blocks_completed`,
- `active_workers`,
- `output_bytes`,
- `giveup_code`,
- `dref_not_init`,
- completion and delta-ordering synchronization,
- `static_schedule`.

The context remains the source of immutable parameters such as codec, filter,
buffer, and chunk metadata.

The current job group is exposed to worker code via `context->job` for the
duration of the call.

## Queueing Model

For the shared-pool backend, `parallel_blosc()` enqueues one work item per
logical worker required by the context.

Each queue entry stores:

- a pointer to the job group,
- a `logical_tid`,
- the next queue link.

`logical_tid` is important because static scheduling in `t_blosc_do_job()`
partitions work by thread id. A physical pool worker may execute any queued
entry, so the queued entry carries the logical thread index that should be used
for that job.

On the worker side:

- the worker dequeues an entry under `pool->mutex`,
- sets `thcontext->parent_context` to the job context,
- sets `thcontext->tid` from `entry->logical_tid`,
- runs `t_blosc_do_job()`,
- decrements `pool->active_jobs` under `pool->mutex`.

## Scheduling

The implementation supports two scheduling styles inside a job group:

- static scheduling for decompression and memcpy-like cases when block masking
  is not active,
- dynamic scheduling using `claim_job_block()` for the remaining cases.

Static scheduling computes a per-logical-thread block range. Dynamic
scheduling increments `job->next_block` under `job->mutex`.

Multiple job groups may coexist on the same pool. The pool does not serialize
all contexts through a single active-context slot.

## Delta Filter Ordering

Delta-filter decompression still requires block 0 to establish the reference
value before later blocks proceed.

For shared-pool jobs, this ordering is handled by per-job `delta_mutex`,
`delta_cv`, and `dref_not_init` in the job group, not by shared pool state.

The callback backend keeps using the context-local delta synchronization.

## Error Handling

Notable failure-handling behavior:

- if worker scratch growth fails, the worker sets `job->giveup_code` to
  `BLOSC2_ERROR_MEMORY_ALLOC`,
- if block compression/decompression fails, the first negative error is stored
  in `job->giveup_code`,
- if compression becomes incompressible, `job->giveup_code` is set to `0`,
- if queue-entry allocation fails during submission, already-enqueued entries
  for that stack job are removed from the queue before returning.

That last point is important because job groups live on the caller stack; the
queue must never retain entries pointing to a destroyed stack job.

`create_shared_pool()` also has partial-construction cleanup for failures during
worker startup.

## Release Path

`release_thread_backend()` handles both callback and shared-pool teardown.

For shared pools:

- `context_refs` is decremented under `pool_registry_mutex`,
- pool idleness is checked under `pool->mutex`,
- if the pool is both unreferenced and idle, it is unlinked from the registry
  and destroyed.

The idle check uses `pool->mutex` because workers mutate queue state and
`active_jobs` under that lock.

## Global API Semantics

The global non-contextual API still uses a global context, but the managed
threading backend for multithreaded work is now the shared-pool mechanism.

Practical semantics:

- `blosc2_set_nthreads()` changes the global context's requested thread count,
- user-created contexts keep their own `nthreads`,
- if a context's `new_nthreads` differs from `nthreads`, the current backend is
  released and the context is rebound on next use,
- rebinding to an existing `nthreads` reuses the matching shared pool.

## Current Limits

The implementation intentionally does not do the following:

- it does not support concurrent use of the same `blosc2_context` object from
  multiple caller threads,
- it does not merge pools with different `nthreads`,
- it does not do adaptive pool resizing,
- it does not map callback-managed threads onto shared-pool workers.

## Tests

The dedicated coverage for this implementation is in `tests/test_shared_pool.c`.

Current scenarios covered there:

- `nthreads == 1` stays serial and attaches to no pool,
- same-`nthreads` contexts share a pool,
- different-`nthreads` contexts use different pools,
- changing `new_nthreads` rebinds a context to the matching pool,
- multithreaded roundtrip tests for `SHUFFLE`, `DELTA`, and `BITSHUFFLE`,
- many contexts can share one pool,
- a live user-created context remains usable across `blosc2_destroy()`.

Focused regression checks are also run against `test_contexts` and
`test_nolock`.

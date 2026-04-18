# Shared Thread Pool for C-Blosc2

## Problem Statement

C-Blosc2 currently uses a **per-context thread pool** model: every compression
context (`cctx`) and decompression context (`dctx`) owns a private set of
pthreads.  Each SChunk carries one `cctx` and one `dctx`, so a single
`blosc2.NDArray` in python-blosc2 creates **up to `2 × nthreads`** OS-level
threads (e.g., 24 threads at `nthreads = 12` on a 12-core Mac).

This design causes two concrete problems:

### 1. macOS thread-count ceiling (hang / crash)

macOS enforces a hard per-process limit of **6 144 threads**.  With the default
`nthreads = 12`, only **~256 live arrays** are needed to exhaust this limit.
Once the limit is hit, `pthread_create` fails for new contexts, and the process
can hang or crash.

During the python-blosc2 test suite (~8 400 tests), thousands of arrays
accumulate because Python 3.14 changed the generation-2 GC threshold to 0,
preventing automatic collection of long-lived objects.  Pytest's final
`gc.collect()` then tries to join thousands of pthreads at once, causing an
indefinite hang.

Related issue: <https://github.com/Blosc/python-blosc2/issues/556>

### 2. Resource waste for idle threads

The vast majority of pthreads created by contexts sit idle in a barrier wait
(`WAIT_INIT`) for their entire lifetime.  A context's threads are created on
the first operation (`check_nthreads` → `init_threadpool`) and only destroyed
when the context is freed (`blosc2_free_ctx` → `release_threadpool` →
`pthread_join`).  Between operations, the threads do nothing but consume kernel
resources (stack memory, thread-local storage, scheduler entries).

### Current workarounds in python-blosc2

1. **`with nogil: blosc2_schunk_free()`** — releasing the GIL during
   `SChunk.__dealloc__` prevents the GIL-deadlock aspect of the hang but does
   not prevent thread accumulation.
2. **Periodic `gc.collect()` every 50 tests** — prevents objects from
   accumulating to dangerous levels during the test suite.
3. **`ThreadPoolExecutor(max_workers=os.cpu_count())`** — caps the Python-level
   executor in `lazyexpr.py` (issue #556).

These are palliatives.  The root fix belongs in C-Blosc2.

---

## Proposed Solution: Global Shared Thread Pool

Replace per-context thread ownership with a **single process-wide thread pool**
that all contexts share.  This is the approach used by OpenMP, Intel TBB, and
Apple's Grand Central Dispatch.

### Design goals

| Goal | Rationale |
|------|-----------|
| Bounded thread count | Never exceed `nthreads` OS threads regardless of context count |
| Zero-cost idle contexts | Contexts without active work hold no thread resources |
| API compatibility | `blosc2_create_cctx`, `blosc2_create_dctx`, `blosc2_free_ctx` signatures unchanged |
| Minimal contention | Contexts that run concurrently share the pool without excessive locking |
| Backward compatibility | `blosc2_set_threads_callback` continues to work for caller-managed threading |
| Thread-safe | Multiple Python threads can compress/decompress through different contexts |

### Architecture overview

```
┌─────────────────────────────────────────────────┐
│              Global Shared Pool                 │
│                                                 │
│  ┌─────────┐ ┌─────────┐       ┌─────────┐     │
│  │ Worker 0 │ │ Worker 1 │ ...  │Worker N-1│    │
│  └────┬────┘ └────┬────┘       └────┬────┘     │
│       │           │                  │          │
│       └───────────┴──────────────────┘          │
│                    │                            │
│            ┌───────┴───────┐                    │
│            │  Job Queue    │                    │
│            │  (lock-free   │                    │
│            │   or mutex)   │                    │
│            └───────────────┘                    │
└─────────────────────────────────────────────────┘
        ▲            ▲            ▲
        │            │            │
   ┌────┴───┐  ┌────┴───┐  ┌────┴───┐
   │ cctx A │  │ dctx B │  │ cctx C │
   │(submit)│  │(submit)│  │(submit)│
   └────────┘  └────────┘  └────────┘
```

---

## Implementation Plan

### Phase 0: Preparatory refactor — isolate threading from `blosc2_context`

**Goal:** Decouple the thread-pool fields from `blosc2_context` into a separate
struct, making it mechanically possible to share.

#### 0.1  Extract `blosc2_threadpool` struct

Create a new struct in `blosc/context.h`:

```c
typedef struct blosc2_threadpool_s {
    int16_t         nthreads;         /* requested worker count */
    int16_t         threads_started;  /* actually running */
    int16_t         end_threads;      /* shutdown flag */
    blosc2_pthread_t *threads;        /* thread handles */
    struct thread_context *thread_contexts; /* per-thread scratch */

    /* Synchronization */
    blosc2_pthread_mutex_t count_mutex;
    blosc2_pthread_mutex_t nchunk_mutex;
#ifdef BLOSC_POSIX_BARRIERS
    pthread_barrier_t barr_init;
    pthread_barrier_t barr_finish;
#else
    int              count_threads;
    blosc2_pthread_mutex_t count_threads_mutex;
    blosc2_pthread_cond_t  count_threads_cv;
#endif
#if !defined(_WIN32)
    pthread_attr_t   ct_attr;
#endif

    /* Job dispatch (Phase 1+) */
    blosc2_context  *current_context;  /* context being processed */
    blosc2_pthread_mutex_t dispatch_mutex;
    blosc2_pthread_cond_t  dispatch_cv;
} blosc2_threadpool;
```

#### 0.2  Redirect context fields

In `blosc2_context_s`, replace the thread-related fields (lines 88–112 of
`blosc/context.h`) with a single pointer:

```c
struct blosc2_context_s {
    /* ... existing non-thread fields ... */

    /* Threading — either points to the global pool or a private pool */
    blosc2_threadpool *pool;

    /* Fields that remain per-context (job state, not pool state) */
    int thread_giveup_code;
    int thread_nblock;
    int dref_not_init;
    blosc2_pthread_mutex_t delta_mutex;
    blosc2_pthread_cond_t  delta_cv;
    /* ... */
};
```

#### 0.3  Adapt `init_threadpool` / `release_threadpool`

Rewrite these to operate on `blosc2_threadpool *` instead of
`blosc2_context *`.  At this stage each context still allocates its own pool
(behavior unchanged), but the interface is ready for sharing.

#### 0.4  Validation

All existing tests must pass.  The refactor is purely structural; no behavioral
change.

---

### Phase 1: Introduce the global pool

#### 1.1  Global pool lifecycle

Add to `blosc/blosc2.c`:

```c
static blosc2_threadpool *g_shared_pool = NULL;
static blosc2_pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_pool_refcount = 0;

/* Called by blosc2_init() */
static void init_global_pool(int16_t nthreads);

/* Called by blosc2_destroy() */
static void destroy_global_pool(void);
```

`init_global_pool` creates the pool with `g_nthreads` workers.
`blosc2_set_nthreads` resizes the pool (joining excess threads or spawning
new ones).

#### 1.2  Context creation uses the global pool

Modify `blosc2_create_cctx` / `blosc2_create_dctx` so that when
`threads_callback == NULL` and the requested `nthreads` matches the global
pool size, `context->pool` simply points to `g_shared_pool` and increments
`g_pool_refcount`.  No pthreads are created.

If the caller requests a non-standard `nthreads` different from the global
pool, fall back to allocating a private pool (preserving current behavior for
specialized contexts).

#### 1.3  `blosc2_free_ctx` skips pool destruction for shared contexts

```c
void blosc2_free_ctx(blosc2_context *context) {
    if (context->pool == g_shared_pool) {
        /* Shared pool: just decrement refcount, don't destroy */
        pthread_mutex_lock(&g_pool_mutex);
        g_pool_refcount--;
        pthread_mutex_unlock(&g_pool_mutex);
    } else if (context->pool != NULL) {
        release_threadpool(context->pool);
        free(context->pool);
    }
    /* ... rest of cleanup ... */
}
```

This single change eliminates per-context `pthread_join` during finalization,
which is the direct cause of the python-blosc2 hang.

---

### Phase 2: Job-queue dispatch

Phase 1 shares the threads but still needs a way to dispatch work.  The current
barrier-based model assumes threads are bound to one context.  With a shared
pool, we need a proper job-dispatch mechanism.

#### 2.1  Job submission API

```c
typedef struct {
    blosc2_context *context;
    struct thread_context *tcontext;
    /* completion tracking */
    blosc2_pthread_mutex_t *done_mutex;
    blosc2_pthread_cond_t  *done_cv;
    int *blocks_remaining;
} blosc2_job;

/* Submit a set of block-processing jobs and wait for completion */
static int pool_submit_and_wait(blosc2_threadpool *pool,
                                blosc2_context *context);
```

#### 2.2  Worker loop rewrite

Replace the current `t_blosc` barrier loop with a job-queue consumer:

```c
static void* t_blosc_pooled(void *arg) {
    blosc2_threadpool *pool = (blosc2_threadpool *)arg;
    while (1) {
        pthread_mutex_lock(&pool->dispatch_mutex);
        while (pool->current_context == NULL && !pool->end_threads) {
            pthread_cond_wait(&pool->dispatch_cv, &pool->dispatch_mutex);
        }
        if (pool->end_threads) {
            pthread_mutex_unlock(&pool->dispatch_mutex);
            break;
        }
        blosc2_context *ctx = pool->current_context;
        pthread_mutex_unlock(&pool->dispatch_mutex);

        /* Process blocks from ctx->thread_nblock atomically */
        t_blosc_do_job(/* thread_context for this worker */);

        /* Signal completion */
        /* ... */
    }
    return NULL;
}
```

#### 2.3  Concurrent context support

For Phase 2 the simplest approach is **serialized context dispatch**: only one
context can use the pool at a time, with a dispatch mutex ensuring mutual
exclusion.  This matches the current model where `parallel_blosc` is called
from a single thread per context.

A future Phase 3 could add fine-grained work-stealing for true concurrent
multi-context parallelism, but this is not required for the initial fix.

#### 2.4  `thread_context` scratch buffers

Currently each `thread_context` allocates scratch buffers (`tmp`, `tmp2`,
`tmp3`, `tmp4`) sized for the context's blocksize.  With a shared pool, these
buffers must be re-allocated when a different context is dispatched (if the
blocksize differs).

Strategy: lazily resize — keep the largest allocation and only reallocate when
a larger blocksize is needed.  Track `tmp_blocksize` to detect when
reallocation is necessary (this check already exists in `do_job` for the serial
path).

---

### Phase 3 (future): Advanced features

These are not required for the initial fix but would further improve the
architecture.

#### 3.1  Work-stealing for concurrent contexts

Allow multiple contexts to submit jobs simultaneously.  Each context gets its
own job queue; idle workers steal from other contexts' queues.  This would
benefit python-blosc2's `ThreadPoolExecutor` + async-read pattern where
multiple arrays decompress concurrently.

#### 3.2  Adaptive pool sizing

Monitor work submission rates and resize the pool dynamically (e.g., shrink
when idle for >100ms, grow up to `nthreads` when jobs arrive).

#### 3.3  `blosc2_set_threads_callback` integration

The existing callback mechanism (`threads_callback`) already provides an
external thread dispatch interface.  With a shared pool, the callback path
needs minor adaptation: instead of operating on per-context `thread_contexts`,
it operates on pool-level workers.

---

## Migration Path & Compatibility

### C API compatibility

| Function | Change |
|----------|--------|
| `blosc2_create_cctx` | Internal only: assigns `g_shared_pool` instead of creating threads |
| `blosc2_create_dctx` | Same |
| `blosc2_free_ctx` | Internal only: decrements refcount instead of `pthread_join` |
| `blosc2_set_nthreads` | Resizes the global pool |
| `blosc2_compress` | Unchanged (uses global context → shared pool) |
| `blosc2_decompress` | Unchanged |
| `blosc2_set_threads_callback` | Works as before; bypasses global pool |

No public API signatures change.  The ABI remains stable because the
`blosc2_context` struct is opaque to callers (only accessed through API
functions).

### python-blosc2 changes

Once C-Blosc2 ships the shared pool:

1. Remove the periodic `gc.collect()` from `tests/conftest.py` (no longer
   needed — freeing a context no longer joins threads).
2. Keep the `with nogil: blosc2_schunk_free()` fix (still good practice).
3. Remove `ThreadPoolExecutor(max_workers=...)` cap in `lazyexpr.py` if
   desired (the Python-level thread pressure is a separate concern but becomes
   less critical when C-level threads are bounded).

---

## Impact Assessment

### Thread count reduction

| Scenario | Current | With shared pool |
|----------|---------|-----------------|
| 1 array, nthreads=12 | 24 threads | 12 threads |
| 100 arrays, nthreads=12 | 2 400 threads | 12 threads |
| 1 000 arrays, nthreads=12 | **6 144** (limit!) | 12 threads |
| Test suite (8 400 tests) | **6 144** (hang) | 12 threads |

### Performance

- **No regression expected** for sequential workloads (same number of workers,
  same barrier/block processing).
- **Slight improvement** for context creation/destruction (no `pthread_create`
  / `pthread_join`).
- **Memory savings** — each idle thread stack is typically 512 KB–8 MB;
  eliminating thousands of idle threads saves gigabytes of virtual memory.

### Risks

1. **Thread-context scratch buffers** need resizing when blocksize changes
   between contexts.  Mitigation: lazy resize with high-water-mark allocation.
2. **Concurrent context dispatch** is serialized in Phase 2, which could
   bottleneck workloads that need true parallelism across contexts.
   Mitigation: Phase 3 adds work-stealing.
3. **`threads_callback` users** need testing to ensure compatibility.

---

## References

- Python-blosc2 issue #556: [Unbounded thread growth during pytest](https://github.com/Blosc/python-blosc2/issues/556)
- C-Blosc2 `context.h` struct definition: `blosc/context.h:36–114`
- Thread lifecycle: `blosc/blosc2.c` — `init_threadpool` (line 4583),
  `release_threadpool` (line 4992), `t_blosc` worker (line 4555),
  `blosc2_free_ctx` (line 5304)
- Synchronization: `WAIT_INIT` / `WAIT_FINISH` macros (lines 108–160)
- `blosc2_set_threads_callback` public API: `include/blosc2.h:734–747`

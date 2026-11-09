/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Read-ahead for frames on disk (see blosc2_set_prefetch()).

  A slice of a frame on disk reads its blocks one after the other: with the mmap I/O every page fault, with the
  default I/O every block, is a synchronous read of at most the kernel's read-ahead window.  On a network file system
  each of those is a round trip, and a cold slice pays a few hundred of them in a row.  Here the chunks a slice needs
  are handed to a few threads up front, cut into large pieces and in the order the slice decompresses them; the slice
  waits for each chunk only right before decompressing it.  The threads read into a scratch buffer that they throw
  away: the point is to get the pages into the page cache, where the normal read path finds them (so the mmap I/O
  stays zero-copy).  Pages that are cached already are not read again: a warm slice costs a cachestat() or mincore()
  per piece.

  GPFS (IBM Storage Scale) keeps what read() reads in a cache of its own (the pagepool), never in the page cache, and
  its page faults cost several times what they cost on local file systems.  There a frame read through the mmap I/O
  gains nothing from the threads reading: the slice still faults on every page.  So for such frames the threads
  populate the mapping instead (madvise(MADV_POPULATE_READ), Linux >= 5.14), which brings the pages in and maps them,
  and the slice finds them mapped; cached pages are populated too, as that still saves the slice its faults.  On other
  file systems read() is the faster way to bring cold data in, so it stays (see populate_map()).

  POSIX only (Linux, macOS, BSDs).  On Windows everything here is a no-op and reads work as without read-ahead.
*/

#if !defined(_WIN32)
// Make mincore(), MAP_SHARED and F_RDADVISE visible despite -D_XOPEN_SOURCE=600 (as in frame.c and blosc2-stdio.c)
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#define _DARWIN_C_SOURCE
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#undef _XOPEN_SOURCE
#endif
#endif

#include "prefetch.h"
#include "frame.h"
#include "blosc-private.h"
#include "blosc2.h"
#include "blosc2/blosc2-stdio.h"

#include <stdlib.h>

/* The page cache is asked about at most this many bytes at once (the mincore() vector is on the stack) */
#define CHECK_NBYTES (1 << 20)

/* Of a chunk that a slice needs only some blocks of, those blocks are read, joined across gaps of at most this many
   bytes: blocks are stored in the order they were compressed in, not by position, so the needed ones are scattered
   over the chunk, and fewer, larger requests are worth a little more data */
#define GAP_NBYTES (64 * 1024)

/* Smaller chunks are read whole: their header would cost a round trip for little data */
#define PARTIAL_MIN_NBYTES (4 * GAP_NBYTES)

/* How much of each end of a frame file is asked for at open: of its start the header, of its end the trailer and,
   unless the frame has a huge number of chunks, the chunk index right before it */
#define OPEN_EACH_END_NBYTES (64 * 1024)

/* Frame files up to this size (from the frame's offset on) are asked for whole at open when read-ahead is on */
#define OPEN_WHOLE_NBYTES (1024 * 1024)

static int g_nthreads = 0;
static int g_populate = -1;  // populate mapped frames: -1 on GPFS only, 0 never, 1 always (prefetch_set_populate())
static int64_t g_max_ahead = BLOSC2_PREFETCH_MAX_AHEAD_DEFAULT;
static int64_t g_piece_size = BLOSC2_PREFETCH_PIECE_SIZE_DEFAULT;


int blosc2_set_prefetch(int nthreads, int64_t max_ahead, int64_t piece_size) {
  if (nthreads < 0 || nthreads > BLOSC2_PREFETCH_MAX_NTHREADS || max_ahead < 0 ||
      (piece_size != 0 && piece_size < BLOSC2_PREFETCH_MIN_PIECE_SIZE)) {
    BLOSC_TRACE_ERROR("nthreads must be in [0, %d], max_ahead must not be negative and piece_size must be 0 or at "
                      "least %d.", BLOSC2_PREFETCH_MAX_NTHREADS, BLOSC2_PREFETCH_MIN_PIECE_SIZE);
    return BLOSC2_ERROR_INVALID_PARAM;
  }
  int previous = g_nthreads;
  g_max_ahead = max_ahead > 0 ? max_ahead : BLOSC2_PREFETCH_MAX_AHEAD_DEFAULT;
  g_piece_size = piece_size > 0 ? piece_size : BLOSC2_PREFETCH_PIECE_SIZE_DEFAULT;
  g_nthreads = nthreads;
  return previous;
}


void prefetch_set_populate(int mode) {
  g_populate = mode < 0 ? -1 : (mode > 0);
}


#if defined(_WIN32)

bool prefetch_applies(const blosc2_frame_s *frame) {
  BLOSC_UNUSED_PARAM(frame);
  return false;
}

void prefetch_open_hint(const blosc2_io *io, void *fp, int64_t offset, int64_t file_size) {
  BLOSC_UNUSED_PARAM(io);
  BLOSC_UNUSED_PARAM(fp);
  BLOSC_UNUSED_PARAM(offset);
  BLOSC_UNUSED_PARAM(file_size);
}

prefetch_batch* prefetch_batch_new(blosc2_frame_s *frame, const int64_t *nchunks, int64_t n,
                                   const bool *const *maskouts, int64_t nblocks) {
  BLOSC_UNUSED_PARAM(frame);
  BLOSC_UNUSED_PARAM(nchunks);
  BLOSC_UNUSED_PARAM(n);
  BLOSC_UNUSED_PARAM(maskouts);
  BLOSC_UNUSED_PARAM(nblocks);
  return NULL;
}

void prefetch_batch_wait(prefetch_batch *batch, int64_t nchunk) {
  BLOSC_UNUSED_PARAM(batch);
  BLOSC_UNUSED_PARAM(nchunk);
}

void prefetch_batch_free(prefetch_batch *batch) {
  BLOSC_UNUSED_PARAM(batch);
}

void prefetch_destroy(void) {
}

#else  /* POSIX */

static bool prefetch_enabled(void) {
  return g_nthreads > 0 && blosc2_is_initialized();
}


bool prefetch_applies(const blosc2_frame_s *frame) {
  return prefetch_enabled() && frame != NULL && frame->cframe == NULL && !frame->sframe && frame->urlpath != NULL &&
         frame->schunk != NULL;
}


#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#include <sys/vfs.h>
/* statfs() f_type of GPFS (IBM Storage Scale) */
#define PREFETCH_GPFS_MAGIC 0x47504653
/* Linux >= 5.14; older C libraries lack the name */
#if !defined(MADV_POPULATE_READ)
#define MADV_POPULATE_READ 22
#endif
/* cachestat() (Linux >= 6.5) has no libc wrapper, and older kernel headers lack its number, which is 451 on all
   architectures except alpha, mips and x32 */
#if !defined(SYS_cachestat) && ((defined(__x86_64__) && !defined(__ILP32__)) || defined(__i386__) || \
    defined(__aarch64__) || defined(__arm__) || defined(__riscv) || defined(__powerpc__) || defined(__s390__) || \
    defined(__loongarch__))
#define SYS_cachestat 451
#endif
struct prefetch_cachestat_range {  // struct cachestat_range of <linux/mman.h>
  uint64_t off;
  uint64_t len;
};
struct prefetch_cachestat {  // struct cachestat of <linux/mman.h>
  uint64_t nr_cache;
  uint64_t nr_dirty;
  uint64_t nr_writeback;
  uint64_t nr_evicted;
  uint64_t nr_recently_evicted;
};
#endif

typedef struct {
  int64_t offset;
  int64_t nbytes;
  int64_t cum_nbytes;  // bytes of this and all earlier pieces
  bool done;
} piece_t;

struct prefetch_batch {
  int fd;              // a dup() of the frame's descriptor, closed in prefetch_batch_free()
  const uint8_t *map;  // the frame's mapping, if the threads populate it instead of reading (see populate_map())
  int64_t page;
  piece_t *pieces;
  int64_t npieces;
  int64_t next;        // first piece not handed to a thread yet
  int64_t limit;       // pieces below this may be handed out (the read-ahead budget)
  int64_t ndone;       // pieces below this are all read (or failed)
  int64_t nchunks;
  int64_t *chunks;     // the chunks, in the order the slice reads them
  int64_t *end_piece;  // per chunk: the pieces below this one hold all of it
  int64_t cursor;      // where prefetch_batch_wait() looks for its chunk first
  int64_t max_ahead;
  int nthreads;        // at most this many pieces are read at once (the read-ahead threads per slice)
  int inflight;        // pieces handed to a thread and not read yet
  bool waiting;        // the slice waits for piece `ndone`
  pthread_cond_t progress;
  prefetch_batch *next_batch;  // in the pool's FIFO
};

/* One pool per process, guarded by its mutex.  Every slice being read (a batch) gets up to `nthreads` threads of it
   at once: the pool grows to `nthreads` threads per batch in its FIFO (at most BLOSC2_PREFETCH_MAX_NTHREADS), and no
   batch has more than its `nthreads` pieces in flight.  So N threads of a process that read at the same time get the
   read-ahead of N processes, and a slice does not wait behind the pieces of slices that started before it.  Threads
   do not survive fork(): the child starts over with an empty pool (pool_atfork_child()). */
static struct {
  pthread_mutex_t mutex;
  pthread_cond_t work;  // a batch has pieces to hand out, or stop
  prefetch_batch *batches;
  int nbatches;         // in `batches`
  pthread_t *threads;
  int nthreads;
  bool stop;
  bool atfork_registered;
} pool = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, 0, NULL, 0, false, false};
static pthread_mutex_t destroy_mutex = PTHREAD_MUTEX_INITIALIZER;


static void pool_atfork_prepare(void) {
  pthread_mutex_lock(&pool.mutex);
}

static void pool_atfork_parent(void) {
  pthread_mutex_unlock(&pool.mutex);
}

static void pool_atfork_child(void) {
  // Only the forking thread exists here.  Destroying `work` could block on the parent's threads that wait on it, so
  // the child gets new objects instead (and leaves the parent's batches alone).
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t work = PTHREAD_COND_INITIALIZER;
  free(pool.threads);
  pool.mutex = mutex;
  pool.work = work;
  destroy_mutex = mutex;
  pool.batches = NULL;
  pool.nbatches = 0;
  pool.threads = NULL;
  pool.nthreads = 0;
}


/* Read @p nbytes at @p offset into @p buf (the same bytes of @p buf again for each partial read when @p rewind); how
   many were read (fewer at the end of the file or on an error: the normal read path deals with it) */
static int64_t read_piece(int fd, uint8_t *buf, int64_t offset, int64_t nbytes, bool rewind) {
  int64_t got = 0;
  while (got < nbytes) {
    ssize_t rbytes = pread(fd, buf + (rewind ? 0 : got), (size_t) (nbytes - got), (off_t) (offset + got));
    if (rbytes < 0 && errno == EINTR) {
      continue;
    }
    if (rbytes <= 0) {
      break;
    }
    got += rbytes;
  }
  return got;
}


static void* pool_worker(void *arg) {
  BLOSC_UNUSED_PARAM(arg);
  uint8_t *scratch = NULL;  // as large as the largest piece read so far
  int64_t scratch_nbytes = 0;
  pthread_mutex_lock(&pool.mutex);
  while (!pool.stop) {
    prefetch_batch *batch = pool.batches;
    while (batch != NULL && (batch->next >= batch->limit || batch->inflight >= batch->nthreads)) {
      batch = batch->next_batch;
    }
    if (batch == NULL) {
      pthread_cond_wait(&pool.work, &pool.mutex);
      continue;
    }
    // The batch is not freed before all the pieces handed out have finished (prefetch_batch_free())
    int64_t i = batch->next++;
    batch->inflight++;
    pthread_mutex_unlock(&pool.mutex);
    bool populated = false;
#if defined(__linux__)
    if (batch->map != NULL) {
      // From the start of the page on; if this fails (kernel < 5.14), the piece is read instead
      int64_t start = batch->pieces[i].offset / batch->page * batch->page;
      int64_t stop = batch->pieces[i].offset + batch->pieces[i].nbytes;
      populated = madvise((void *) (batch->map + start), (size_t) (stop - start), MADV_POPULATE_READ) == 0;
    }
#endif
    if (!populated && batch->pieces[i].nbytes > scratch_nbytes) {
      free(scratch);
      scratch = malloc(batch->pieces[i].nbytes);
      scratch_nbytes = scratch != NULL ? batch->pieces[i].nbytes : 0;
    }
    if (!populated && scratch != NULL) {
      read_piece(batch->fd, scratch, batch->pieces[i].offset, batch->pieces[i].nbytes, true);
    }
    pthread_mutex_lock(&pool.mutex);
    batch->inflight--;
    batch->pieces[i].done = true;
    if (i == batch->ndone && batch->waiting) {
      pthread_cond_signal(&batch->progress);
    }
  }
  pthread_mutex_unlock(&pool.mutex);
  free(scratch);
  return NULL;
}


/* Make sure the pool runs (at least) @p nthreads threads; false if it has none.  Called with the pool's mutex held. */
static bool pool_ensure(int nthreads) {
  if (!blosc2_is_initialized()) {
    return false;
  }
  if (!pool.atfork_registered) {
    pool.atfork_registered = pthread_atfork(pool_atfork_prepare, pool_atfork_parent, pool_atfork_child) == 0;
    if (!pool.atfork_registered) {
      return false;
    }
  }
  if (pool.nthreads < nthreads) {
    pthread_t *threads = realloc(pool.threads, nthreads * sizeof(pthread_t));
    if (threads != NULL) {
      pool.threads = threads;
      while (pool.nthreads < nthreads &&
             pthread_create(&pool.threads[pool.nthreads], NULL, pool_worker, NULL) == 0) {
        pool.nthreads++;
      }
    }
  }
  return pool.nthreads > 0;
}


void prefetch_destroy(void) {
  pthread_mutex_lock(&destroy_mutex);
  pthread_mutex_lock(&pool.mutex);
  if (pool.threads == NULL) {
    pthread_mutex_unlock(&pool.mutex);
    pthread_mutex_unlock(&destroy_mutex);
    return;
  }
  pool.stop = true;
  pthread_cond_broadcast(&pool.work);
  int nthreads = pool.nthreads;
  pthread_t *threads = pool.threads;
  pool.threads = NULL;
  pool.nthreads = 0;
  pthread_mutex_unlock(&pool.mutex);

  for (int i = 0; i < nthreads; ++i) {
    pthread_join(threads[i], NULL);
  }
  free(threads);

  pthread_mutex_lock(&pool.mutex);
  pool.stop = false;
  pthread_mutex_unlock(&pool.mutex);
  pthread_mutex_unlock(&destroy_mutex);
}


/* The descriptor and, for the mmap I/O, the mapping behind a reader handle; false for other I/O backends */
static bool backend_fd(const blosc2_io *io, void *fp, int *fd, const uint8_t **map) {
  *map = NULL;
  if (io->id == BLOSC2_IO_FILESYSTEM) {
    *fd = fileno(((blosc2_stdio_file *) fp)->file);
  }
  else if (io->id == BLOSC2_IO_FILESYSTEM_MMAP) {
    *fd = ((blosc2_stdio_mmap *) fp)->fd;
    *map = (const uint8_t *) ((blosc2_stdio_mmap *) fp)->addr;
  }
  else {
    return false;
  }
  return *fd >= 0;
}


#if defined(__linux__)
/* Whether the file behind @p fd is on GPFS.  This runs on every read, and on NFS fstatfs() asks the server every time
   (0.2 ms) and fstat() at times (0.1 ms after the file's pages were dropped), so each thread keeps the answer for the
   last few file systems it read from, by the device of the file from statx() asking for no attributes and not to sync
   them, which NFS answers from its cache.  Without statx() (glibc < 2.28) there is no check, and no populating unless
   asked for (prefetch_set_populate()). */
#define FS_SEEN 4
static bool on_gpfs(int fd) {
#if defined(AT_STATX_DONT_SYNC) && defined(STATX_TYPE)
  static __thread struct {uint64_t dev; bool gpfs;} seen[FS_SEEN];
  static __thread int nseen = 0;
  static __thread int next = 0;
  struct statx sx;
  if (statx(fd, "", AT_EMPTY_PATH | AT_STATX_DONT_SYNC, 0, &sx) != 0) {
    return false;
  }
  uint64_t dev = (uint64_t) sx.stx_dev_major << 32 | sx.stx_dev_minor;
  for (int i = 0; i < nseen; ++i) {
    if (seen[i].dev == dev) {
      return seen[i].gpfs;
    }
  }
  struct statfs st;
  if (fstatfs(fd, &st) != 0) {
    return false;
  }
  seen[next].dev = dev;
  seen[next].gpfs = (int64_t) st.f_type == PREFETCH_GPFS_MAGIC;
  next = (next + 1) % FS_SEEN;
  nseen = nseen < FS_SEEN ? nseen + 1 : FS_SEEN;
  return (int64_t) st.f_type == PREFETCH_GPFS_MAGIC;
#else
  BLOSC_UNUSED_PARAM(fd);
  return false;
#endif
}
#endif


/* Whether the threads should populate @p map, the mmap I/O's mapping of the file behind @p fd (or NULL), instead of
   reading into their scratch buffer: on GPFS, or as prefetch_set_populate() says.  Measured with 3D patches (KiTS,
   BraTS, TotalSegmentator) read through the mmap I/O with 4 threads: on GPFS populating made patches 1.10-1.34x faster
   (cached and cold); on NFS and XFS cold patches got up to 1.3x slower (the faults bring in less at a time than the
   threads' reads) and cached ones up to 1.12x faster for more CPU, so there the threads keep reading. */
static bool populate_map(int fd, const uint8_t *map) {
#if defined(__linux__)
  if (map == NULL || g_populate == 0) {
    return false;
  }
  if (g_populate > 0) {
    return true;
  }
  return on_gpfs(fd);
#else
  BLOSC_UNUSED_PARAM(fd);
  BLOSC_UNUSED_PARAM(map);
  return false;
#endif
}


/* Ask the kernel to start reading [offset, offset + nbytes) into the page cache, without waiting for it */
static void advise_willneed(int fd, int64_t offset, int64_t nbytes) {
#if defined(POSIX_FADV_WILLNEED)
  posix_fadvise(fd, (off_t) offset, (off_t) nbytes, POSIX_FADV_WILLNEED);
#elif defined(F_RDADVISE)
  struct radvisory advice = {(off_t) offset, nbytes > INT32_MAX ? INT32_MAX : (int) nbytes};
  fcntl(fd, F_RDADVISE, &advice);
#endif
}


void prefetch_open_hint(const blosc2_io *io, void *fp, int64_t offset, int64_t file_size) {
  int fd;
  const uint8_t *map;
  int64_t nbytes = file_size - offset;
  if (nbytes <= 0 || fp == NULL || !backend_fd(io, fp, &fd, &map)) {
    return;
  }
  if (nbytes <= 2 * OPEN_EACH_END_NBYTES) {
    advise_willneed(fd, offset, nbytes);
    return;
  }
  /* Both ends are read right after the open, so they are asked for with read-ahead off too.  The start is asked for
     when the file is mapped, so that the header access is not a page fault on an uncached page, which reads the whole
     fault-around window (4-8 MiB on SMB mounts), and before the rest of a small frame, so that the header does not
     wait behind it.  Otherwise the read of the header right after this gets it in the same round trip as the end. */
  bool ask_whole = prefetch_enabled() && nbytes <= OPEN_WHOLE_NBYTES;
  if (map != NULL || ask_whole) {
    advise_willneed(fd, offset, OPEN_EACH_END_NBYTES);
  }
  advise_willneed(fd, file_size - OPEN_EACH_END_NBYTES, OPEN_EACH_END_NBYTES);
  if (ask_whole) {
    advise_willneed(fd, offset + OPEN_EACH_END_NBYTES, nbytes - 2 * OPEN_EACH_END_NBYTES);
  }
}


/* Asks the kernel which pages of a file are in the page cache: cachestat() (Linux >= 6.5; one call per range, cheaper
   than mincore() on most file systems) or else mincore() on a mapping of the file. */
typedef struct {
  int fd;
  int64_t page;
  int64_t lo;              // page-aligned start of the slice's span; `span` maps the file from here on
  const uint8_t *span;
  int64_t span_nbytes;     // > 0 if `span` was mapped just for this
  enum {CACHE_UNKNOWN, CACHE_CACHESTAT, CACHE_MINCORE} how;
} cache_query;


/* Prepare to ask about [lo, hi) of @p fd.  @p map maps the file from its start (mmap I/O), or is NULL. */
static void cache_query_init(cache_query *q, int fd, const uint8_t *map, int64_t lo, int64_t hi, int64_t page) {
  q->fd = fd;
  q->page = page;
  q->lo = lo / page * page;
  q->span = NULL;
  q->span_nbytes = 0;
  q->how = CACHE_UNKNOWN;
#if defined(__linux__)
  // Linux >= 5.0 tells only the owner of a file, or who may write to it, what of it is cached: for others cachestat()
  // fails and mincore() claims that all of it is.  Nothing counts as cached then, so cold slices still get read ahead
  // and warm ones cost a copy of their chunks.
#if defined(SYS_cachestat)
  struct prefetch_cachestat_range range = {(uint64_t) q->lo, 1};
  struct prefetch_cachestat cs;
  if (syscall(SYS_cachestat, fd, &range, &cs, 0) == 0) {
    q->how = CACHE_CACHESTAT;
    return;
  }
#endif
  // No cachestat() (Linux < 6.5, blocked by a seccomp filter, or not allowed for this file): mincore() for our own
  // files only (the files of others count as not cached, even if we may write to them)
  struct stat st;
  if (fstat(fd, &st) != 0 || (st.st_uid != geteuid() && geteuid() != 0)) {
    return;
  }
#endif
  if (map != NULL) {
    q->span = map + q->lo;
  }
  else {  // the default I/O: map the span just for mincore() (no page is touched)
    void *span = mmap(NULL, (size_t) (hi - q->lo), PROT_READ, MAP_SHARED, fd, (off_t) q->lo);
    if (span == MAP_FAILED) {
      return;
    }
    q->span = span;
    q->span_nbytes = hi - q->lo;
  }
  q->how = CACHE_MINCORE;
}


static void cache_query_destroy(cache_query *q) {
  if (q->span_nbytes > 0) {
    munmap((void *) q->span, (size_t) q->span_nbytes);
  }
}


/* Whether all pages of [start, stop) are in the page cache; false if unknown.  For mincore(), [start, stop) must not
   cross a multiple of CHECK_NBYTES.  Only the pages to be read are asked about: the kernel looks at each of them, and
   the chunks of one slice can lie far apart. */
static bool is_cached(const cache_query *q, int64_t start, int64_t stop) {
  int64_t first = (start - q->lo) / q->page * q->page;  // start of the first page, relative to lo
  int64_t npages = (stop - q->lo - first + q->page - 1) / q->page;
#if defined(SYS_cachestat)
  if (q->how == CACHE_CACHESTAT) {
    struct prefetch_cachestat_range range = {(uint64_t) (q->lo + first), (uint64_t) (stop - q->lo - first)};
    struct prefetch_cachestat cs;
    return syscall(SYS_cachestat, q->fd, &range, &cs, 0) == 0 && (int64_t) cs.nr_cache >= npages;
  }
#endif
  unsigned char vec[CHECK_NBYTES / 4096];
  if (q->how != CACHE_MINCORE || npages > (int64_t) sizeof(vec) ||
      mincore((void *) (q->span + first), (size_t) (stop - q->lo - first), (void *) vec) != 0) {
    return false;
  }
  for (int64_t p = 0; p < npages; ++p) {
    if (!(vec[p] & 1)) {
      return false;
    }
  }
  return true;
}


/* Let the threads read up to max_ahead bytes past piece @p from, and at least the pieces below @p need */
static void raise_limit(prefetch_batch *batch, int64_t from, int64_t need) {
  int64_t base = from > 0 ? batch->pieces[from - 1].cum_nbytes : 0;
  int64_t limit = batch->limit > need ? batch->limit : need;
  while (limit < batch->npieces && batch->pieces[limit].cum_nbytes - base <= batch->max_ahead) {
    limit++;
  }
  for (int64_t i = batch->limit; i < limit && i < batch->limit + batch->nthreads; ++i) {
    pthread_cond_signal(&pool.work);
  }
  batch->limit = limit;
}


/* Wait until the pieces below @p upto have finished.  Called with the pool's mutex held. */
static void wait_pieces(prefetch_batch *batch, int64_t upto) {
  batch->waiting = true;
  for (;;) {
    while (batch->ndone < upto && batch->pieces[batch->ndone].done) {
      batch->ndone++;
    }
    if (batch->ndone >= upto) {
      break;
    }
    pthread_cond_wait(&batch->progress, &pool.mutex);
  }
  batch->waiting = false;
}


/* is_cached() for any range */
static bool range_cached(const cache_query *q, int64_t start, int64_t stop) {
  while (q->how != CACHE_UNKNOWN && start < stop) {
    int64_t end = q->how == CACHE_CACHESTAT ? stop : (start / CHECK_NBYTES + 1) * CHECK_NBYTES;
    end = end < stop ? end : stop;
    if (!is_cached(q, start, end)) {
      return false;
    }
    start = end;
  }
  return start >= stop;
}


static void add_piece(prefetch_batch *batch, piece_t *piece) {
  piece->cum_nbytes = piece->nbytes + (batch->npieces > 0 ? batch->pieces[batch->npieces - 1].cum_nbytes : 0);
  batch->pieces[batch->npieces++] = *piece;
  piece->nbytes = 0;
}


/* Start reading the uncached parts of the byte ranges (2 values each: offset, nbytes) of chunks nchunks[0..n) with the
   pool, chunk i's being ranges first_range[i]..first_range[i + 1], in pieces of at most the piece size, ranges that
   follow each other in the file joined; or populating them in @p map, if not NULL.  NULL if there is nothing to
   read. */
static prefetch_batch* batch_start(const cache_query *query, int fd, const uint8_t *map, const int64_t *nchunks,
                                   int64_t n, const int64_t *ranges, const int64_t *first_range) {
  int64_t piece_size = g_piece_size;
  int nthreads = g_nthreads;
  if (nthreads <= 0) {
    return NULL;
  }
  int64_t capacity = 0;
  for (int64_t r = 0; r < first_range[n]; ++r) {
    capacity += ranges[2 * r + 1] / CHECK_NBYTES + ranges[2 * r + 1] / piece_size + 3;  // windows per range
  }
  if (capacity == 0) {
    return NULL;
  }
  prefetch_batch *batch = calloc(1, sizeof(prefetch_batch) + 2 * n * sizeof(int64_t) + capacity * sizeof(piece_t));
  if (batch == NULL) {
    return NULL;
  }
  batch->chunks = (int64_t *) (batch + 1);
  batch->end_piece = batch->chunks + n;
  batch->pieces = (piece_t *) (batch->end_piece + n);
  batch->nchunks = n;
  batch->max_ahead = g_max_ahead;
  batch->nthreads = nthreads;
  batch->map = map;
  batch->page = query->page;

  // Pieces never cross a multiple of the piece size nor a gap in the file, and cached parts (asked about in windows
  // that do not cross a multiple of CHECK_NBYTES either) are left out
  piece_t cur = {0, 0, 0, false};
  for (int64_t i = 0; i < n; ++i) {
    for (int64_t r = first_range[i]; r < first_range[i + 1]; ++r) {
      int64_t start = ranges[2 * r];
      int64_t end = start + ranges[2 * r + 1];
      while (start < end) {
        int64_t stop = (start / piece_size + 1) * piece_size;
        int64_t check_stop = (start / CHECK_NBYTES + 1) * CHECK_NBYTES;
        stop = check_stop < stop ? check_stop : stop;
        stop = end < stop ? end : stop;
        bool cached = is_cached(query, start, stop);
        if (cur.nbytes > 0 && (cached || start != cur.offset + cur.nbytes || start % piece_size == 0)) {
          add_piece(batch, &cur);
        }
        if (!cached) {
          cur.offset = cur.nbytes > 0 ? cur.offset : start;
          cur.nbytes += stop - start;
        }
        start = stop;
      }
    }
    batch->chunks[i] = nchunks[i];
    batch->end_piece[i] = batch->npieces + (cur.nbytes > 0);  // `cur` will be the next piece
  }
  if (cur.nbytes > 0) {
    add_piece(batch, &cur);
  }

  batch->fd = batch->npieces > 0 ? fcntl(fd, F_DUPFD_CLOEXEC, 0) : -1;
  if (batch->fd < 0) {
    free(batch);
    return NULL;
  }
  pthread_cond_init(&batch->progress, NULL);
  pthread_mutex_lock(&pool.mutex);
  int64_t want = (int64_t) nthreads * (pool.nbatches + 1);
  if (!pool_ensure(want < BLOSC2_PREFETCH_MAX_NTHREADS ? (int) want : BLOSC2_PREFETCH_MAX_NTHREADS)) {
    pthread_mutex_unlock(&pool.mutex);
    pthread_cond_destroy(&batch->progress);
    close(batch->fd);
    free(batch);
    return NULL;
  }
  prefetch_batch **tail = &pool.batches;
  while (*tail != NULL) {
    tail = &(*tail)->next_batch;
  }
  *tail = batch;
  pool.nbatches++;
  raise_limit(batch, 0, 1);
  pthread_mutex_unlock(&pool.mutex);
  return batch;
}


static int block_start_cmp(const void *a, const void *b) {
  int64_t x = ((const int64_t *) a)[0];
  int64_t y = ((const int64_t *) b)[0];
  return (x > y) - (x < y);
}


/* The byte ranges (2 values each: offset, nbytes; relative to the chunk) to read of a chunk of @p chunk_nbytes whose
   first bytes are @p head, for the blocks not set in @p maskout (@p nblocks of them): its header and block offsets,
   and the blocks, sorted and joined across gaps of at most GAP_NBYTES; their number (at most nblocks + 1), or -1 to
   read the whole chunk instead (no block offsets to go by: special values, dictionaries, variable-length blocks; or
   a header that does not fit). */
static int64_t block_ranges(const uint8_t *head, int64_t head_nbytes, int64_t chunk_nbytes, const bool *maskout,
                            int64_t nblocks, int64_t *out) {
  int32_t nbytes;
  int32_t cbytes;
  int32_t blocksize;
  if (head_nbytes < BLOSC_EXTENDED_HEADER_LENGTH || blosc2_cbuffer_sizes(head, &nbytes, &cbytes, &blocksize) < 0) {
    return -1;
  }
  uint8_t flags = head[BLOSC2_CHUNK_FLAGS];
  bool memcpyed = flags & BLOSC_MEMCPYED;
  int64_t head_len = BLOSC_EXTENDED_HEADER_LENGTH + (memcpyed ? 0 : 4 * nblocks);
  if (!(flags & BLOSC_DOSHUFFLE) || !(flags & BLOSC_DOBITSHUFFLE) ||  // no extended header
      ((head[BLOSC2_CHUNK_BLOSC2_FLAGS] >> 4) & BLOSC2_SPECIAL_MASK) != 0 ||
      (head[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) || (get_chunk_flags2(head, cbytes) & BLOSC2_VL_BLOCKS) ||
      blocksize <= 0 || nbytes <= 0 || (nbytes + blocksize - 1) / blocksize != nblocks || cbytes > chunk_nbytes ||
      cbytes < head_len || head_nbytes < head_len) {
    return -1;
  }
  int64_t *order = malloc(2 * nblocks * sizeof(int64_t));  // (start, block), sorted by start
  if (order == NULL) {
    return -1;
  }
  bool sorted = true;
  for (int64_t b = 0; b < nblocks; ++b) {
    order[2 * b] = memcpyed ? head_len + b * blocksize : sw32_(head + BLOSC_EXTENDED_HEADER_LENGTH + 4 * b);
    order[2 * b + 1] = b;
    if (order[2 * b] < head_len || order[2 * b] >= cbytes) {
      free(order);
      return -1;
    }
    sorted &= b == 0 || order[2 * b] > order[2 * b - 2];
  }
  if (!sorted) {
    qsort(order, nblocks, 2 * sizeof(int64_t), block_start_cmp);
  }
  int64_t nout = 1;
  out[0] = 0;
  out[1] = head_len;
  for (int64_t k = 0; k < nblocks; ++k) {  // a block ends where the next one in the file starts
    if (maskout[order[2 * k + 1]]) {
      continue;
    }
    int64_t start = order[2 * k];
    int64_t end = k + 1 < nblocks ? order[2 * k + 2] : cbytes;
    int64_t *last = out + 2 * (nout - 1);
    if (start - (last[0] + last[1]) <= GAP_NBYTES) {
      last[1] = end - last[0];
    }
    else {
      out[2 * nout] = start;
      out[2 * nout + 1] = end - start;
      nout++;
    }
  }
  free(order);
  return nout;
}


/* Start a batch for chunks nchunks[0..n) at @p ranges (2 values each, from frame_get_chunk_ranges()).  Chunks all in
   the page cache are left out.  A large enough chunk with a maskout (see prefetch_batch_new()) that leaves blocks out
   is read in part: the headers of all such chunks are asked for at once (one round trip if not cached), then only
   their blocks that are needed are read.  Of such a chunk only the needed blocks are asked about, not the whole chunk:
   the page cache check costs per page on file systems that cache files in single pages (NFS on Linux 6.8: ~22 us per
   MiB), where checking whole chunks of which a slice needs a part made read-ahead cost cached slices several percent. */
static prefetch_batch* plan_and_start(int fd, const uint8_t *map, const int64_t *nchunks, int64_t n,
                                      const int64_t *ranges, const bool *const *maskouts, int64_t nblocks) {
  int64_t page = sysconf(_SC_PAGESIZE);
  int64_t lo = INT64_MAX;
  int64_t hi = 0;
  for (int64_t i = 0; i < n; ++i) {
    if (ranges[2 * i + 1] > 0) {
      lo = ranges[2 * i] < lo ? ranges[2 * i] : lo;
      hi = ranges[2 * i] + ranges[2 * i + 1] > hi ? ranges[2 * i] + ranges[2 * i + 1] : hi;
    }
  }
  if (hi == 0 || page <= 0) {
    return NULL;
  }
  enum {SKIP, WHOLE, PART};
  uint8_t *plan = malloc(n);  // per chunk
  int64_t *first_range = malloc((n + 1) * sizeof(int64_t));
  if (plan == NULL || first_range == NULL) {
    free(plan);
    free(first_range);
    return NULL;
  }
  bool populate = populate_map(fd, map);
  cache_query query;
  cache_query_init(&query, fd, map, lo, hi, page);
  if (populate) {
    query.how = CACHE_UNKNOWN;  // cached pages get populated too: that maps them for the slice
  }
  int64_t head_max = BLOSC_EXTENDED_HEADER_LENGTH + 4 * nblocks;
  int64_t npartial = 0;
  for (int64_t i = 0; i < n; ++i) {
    plan[i] = ranges[2 * i + 1] > 0 ? WHOLE : SKIP;
    const bool *maskout = maskouts != NULL ? maskouts[i] : NULL;
    for (int64_t b = 0; plan[i] == WHOLE && maskout != NULL && ranges[2 * i + 1] >= PARTIAL_MIN_NBYTES &&
                        b < nblocks; ++b) {
      plan[i] = maskout[b] ? PART : WHOLE;
    }
    if (plan[i] == WHOLE && range_cached(&query, ranges[2 * i], ranges[2 * i] + ranges[2 * i + 1])) {
      plan[i] = SKIP;
    }
    if (plan[i] == PART) {  // batch_start() leaves out its blocks that are cached
      advise_willneed(fd, ranges[2 * i], head_max);  // all headers on their way at once
      npartial++;
    }
  }

  prefetch_batch *batch = NULL;
  int64_t *planned = malloc(2 * (n + npartial * (nblocks + 1)) * sizeof(int64_t) + head_max);
  if (planned != NULL) {
    uint8_t *head = (uint8_t *) (planned + 2 * (n + npartial * (nblocks + 1)));
    int64_t nplanned = 0;
    for (int64_t i = 0; i < n; ++i) {
      first_range[i] = nplanned;
      int64_t nranges = -1;
      if (plan[i] == PART) {
        int64_t got = read_piece(fd, head, ranges[2 * i], head_max, false);
        nranges = block_ranges(head, got, ranges[2 * i + 1], maskouts[i], nblocks, planned + 2 * nplanned);
        for (int64_t r = nplanned; r < nplanned + nranges; ++r) {
          planned[2 * r] += ranges[2 * i];
        }
      }
      if (plan[i] != SKIP && nranges < 0) {
        planned[2 * nplanned] = ranges[2 * i];
        planned[2 * nplanned + 1] = ranges[2 * i + 1];
        nranges = 1;
      }
      nplanned += nranges > 0 ? nranges : 0;
    }
    first_range[n] = nplanned;
    batch = batch_start(&query, fd, populate ? map : NULL, nchunks, n, planned, first_range);
  }
  cache_query_destroy(&query);
  free(planned);
  free(first_range);
  free(plan);
  return batch;
}


prefetch_batch* prefetch_batch_new(blosc2_frame_s *frame, const int64_t *nchunks, int64_t n,
                                   const bool *const *maskouts, int64_t nblocks) {
  if (n <= 0 || !prefetch_applies(frame)) {
    return NULL;
  }
  const blosc2_io *io = frame->schunk->storage->io;
  blosc2_io_cb *io_cb = blosc2_get_io_cb(io->id);
  int64_t *ranges = malloc(2 * n * sizeof(int64_t));
  if (io_cb == NULL || ranges == NULL || frame_lock(frame, false) < 0) {
    free(ranges);
    return NULL;
  }
  prefetch_batch *batch = NULL;
  if (frame_get_chunk_ranges(frame, nchunks, n, ranges) >= 0) {  // else the normal read path reports what is wrong
    void *fp = frame_reader_acquire(frame, io);
    int fd;
    const uint8_t *map;
    if (fp != NULL && backend_fd(io, fp, &fd, &map)) {
      batch = plan_and_start(fd, map, nchunks, n, ranges, maskouts, nblocks);
    }
    if (fp != NULL) {
      frame_reader_release(frame, io_cb, fp);
    }
  }
  frame_unlock(frame);
  free(ranges);
  return batch;
}


void prefetch_batch_wait(prefetch_batch *batch, int64_t nchunk) {
  if (batch == NULL) {
    return;
  }
  pthread_mutex_lock(&pool.mutex);
  // The slice reads the chunks in the order given, so this is mostly the next one
  for (int64_t k = 0; k < batch->nchunks; ++k) {
    int64_t i = (batch->cursor + k) % batch->nchunks;
    if (batch->chunks[i] == nchunk) {
      batch->cursor = i + 1;
      raise_limit(batch, batch->end_piece[i], batch->end_piece[i]);
      wait_pieces(batch, batch->end_piece[i]);
      break;
    }
  }
  pthread_mutex_unlock(&pool.mutex);
}


void prefetch_batch_free(prefetch_batch *batch) {
  if (batch == NULL) {
    return;
  }
  pthread_mutex_lock(&pool.mutex);
  batch->limit = batch->next;
  wait_pieces(batch, batch->next);
  prefetch_batch **link = &pool.batches;
  while (*link != batch) {
    link = &(*link)->next_batch;
  }
  *link = batch->next_batch;
  pool.nbatches--;
  pthread_mutex_unlock(&pool.mutex);
  pthread_cond_destroy(&batch->progress);
  close(batch->fd);
  free(batch);
}

#endif  /* POSIX */

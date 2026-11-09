Blosc supports threading
========================

Threads are the most efficient way to program parallel code for
multi-core processors, but also the more difficult to program well.
Also, they have a non-negligible start-up time that does not fit well
with a high-performance compressor as Blosc tries to be.

In order to reduce the overhead of threads as much as possible, I've
decided to implement a pool of threads (the workers) that are waiting
for the main process (the master) to send them jobs (basically,
compressing and decompressing small blocks of the initial buffer).

Despite this and many other internal optimizations in the threaded
code, it does not work faster than the serial version for buffer sizes
around 64/128 KB or less.  This is for Intel Quad Core2 (Q8400 @ 2.66
GHz) / Linux (openSUSE 11.2, 64 bit), but your mileage may vary (and
will vary!) for other processors / operating systems.

In contrast, for buffers larger than 64/128 KB, the threaded version
starts to perform significantly better, being the sweet point at 1 MB
(again, this is with my setup).  For larger buffer sizes than 1 MB,
the threaded code slows down again, but it is probably due to a cache
size issue and besides, it is still considerably faster than serial
code.

This is why Blosc falls back to use the serial version for such a
'small' buffers.  So, you don't have to worry too much about deciding
whether you should set the number of threads to 1 (serial) or more
(parallel).  Just set it to the number of cores in your processor and
your are done!

Francesc Alted

Pluggable Threading Backend
---------------------------

Instead of having Blosc use its *own* thread pool, you can instead call `blosc2_set_threads_callback(threads_callback, callback_data)` to install your own threading backend.  This gives Blosc the possibility to use the same threading mechanism as one you are using in the rest of your program (e.g. OpenMP or Intel TBB), sharing the same threads, rather than starting its own threads that compete with yours for the CPU cores.

Here, `threads_callback` is a function of the form:

.. code-block:: c
   void threads_callback(void *callback_data, void (*dojob)(void *), int numjobs, size_t jobdata_elsize, void *jobdata)
   {
     int i;
     for (i = 0; i < numjobs; ++i)
       dojob(((char *) jobdata) + ((unsigned) i)*jobdata_elsize);
   }

that simply calls `dojob` on the given `jobdata` array for `numjobs` elements of size `jobdata_elsize`, returning when all of the `dojob` calls have completed.  The key point is that your `threads_callback` routine can execute the `dojob` calls *in parallel* if it wants.  For example, if you are using OpenMP your `threads_callback` function might use `#pragma omp parallel for`.

The `blosc2_set_threads_callback` function should be called before any Blosc function (before any Blosc contexts are created), to inhibit Blosc from spawning its own worker threads.   In this case, `blosc2_set_nthreads` and similar functions set an upper bound to the `numjobs` that is passed to your `threads_callback` rather than an actual number of threads.

Read-ahead threads
------------------

The threads above compress and decompress: they split the blocks of one chunk between them, and each chunk is done
before the next one starts.  Reading a slice of a frame on disk is a different job.  Each block that is not in the
page cache yet has to wait for the storage, and on a network file system every such wait is a round trip to the
server.  `blosc2_set_prefetch(nthreads, max_ahead, piece_size)` (or the `BLOSC_PREFETCH` environment variable) starts
separate read-ahead threads for that.  When a slice starts (`b2nd_get_slice_cbuffer`, `b2nd_get_slice_cbuffer_into`,
`b2nd_get_slice`, `b2nd_get_orthogonal_selection`, `blosc2_schunk_get_slice_buffer`), up to `nthreads` of them read
all the chunks it needs (that are not cached already) in pieces of at most `piece_size` bytes, in parallel and in the
order the slice decompresses them, at most `max_ahead` bytes ahead of it; the slice waits for each chunk only right
before decompressing it.  Of a chunk
that a slice of an array covers only in part, only the blocks the slice needs are read (after the chunk's header,
which says where they are: one round trip more when it is not cached).

Read-ahead threads spend most of their time waiting for the storage, but reading is not free: the kernel copies the
data, and it spends more CPU per byte the more threads read the same file at once.  Which settings to use:

* 4 threads and the default 1 MiB pieces, on network file systems (NFS, SMB, ...) and on local disks.  On NFS,
  cold patches of 3D images got 3-5x faster this way (8 threads: 10-25% faster still, for 20-40% more CPU per read);
  on NVMe disks 1.5-3x.
* Less CPU on local disks: fewer threads (1-2) or larger pieces (4-8 MiB).  On one NVMe system that kept the speed at
  up to 40% less CPU; on another it cost 10-30% of the speed.
* Many reader processes or threads at once: they already read in parallel.  When they keep all cores busy, the
  kernel time read-ahead adds can cost more than it saves, and fewer threads (1-2) or none can be better.  In our
  tests on NFS (3D images, 24 to 96 reader processes on 64 hardware threads), 4 threads were the fastest setting or
  within 10% of it.
* GPFS (IBM Storage Scale) needs no setting of its own.  GPFS caches what ``read()`` reads in its own pagepool, not in
  the page cache, so for memory-mapped frames (``BLOSC2_IO_FILESYSTEM_MMAP``) reading ahead would not spare the slice
  its page faults, and GPFS page faults are expensive.  There the threads populate the mapping instead
  (``madvise(MADV_POPULATE_READ)``, Linux >= 5.14): 3D patches got 1.10-1.34x faster than with read-ahead reading
  (cached and cold).  On NFS and local disks populating was slower for cold reads (up to 1.3x), so it is used on GPFS
  only (recognized in builds with glibc >= 2.28); ``BLOSC_PREFETCH_POPULATE=1`` (``0``) uses it for all
  memory-mapped frames (for none).

How many read-ahead threads run: up to `nthreads` for each slice being read, per process.  A process that reads with
one thread has `nthreads` of them; one whose 8 threads read at the same time, up to 8 x `nthreads` (at most 256 in
all); 8 processes that read with one thread each, `nthreads` each.  The decompression threads above do not count: a
read with 8 decompression threads and `nthreads` = 4 has 4 read-ahead threads, which fetch the next chunks while the 8
decompress the current one.  So the two kinds are independent: a program that runs many reader processes or threads
can keep one decompression thread each (`nthreads=1`) and still read ahead in each.  Read-ahead threads run until
`blosc2_destroy()`, each keeping a buffer of up to `piece_size` bytes.  Read-ahead is off by default and has no effect
on Windows.

Threads and fork()
------------------

Threads do not survive `fork()`: a child process has none of its parent's compression, decompression or read-ahead
threads and starts its own on first use, also for the contexts and arrays it inherited.  While another thread is inside
a call that uses the global context (`blosc2_compress()`, `blosc2_decompress()`, `blosc2_set_nthreads()`, ...), `fork()`
waits for that call to return.  As with any library, a context or frame that another thread was using at the moment of
the `fork()` must not be used in the child.

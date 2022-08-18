Blosc supports threading
========================

Threads are the most efficient way to program parallel code for
multi-core processors, but also the more difficult to program well.
Also, they has a non-negligible start-up time that does not fit well
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

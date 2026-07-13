Roadmap to 3.0
==============

.. note::

   C-Blosc2 3.0 was released in April 2026, so this document is now mostly
   of historical interest.  Every item below has been annotated with its
   status: **[Done]**, **[Ongoing]** or **[Deferred]**.  Deferred items did
   not make it into the 3.x series (yet); they are still considered good
   candidates for future releases, and most of them have been carried over
   to `ROADMAP-TO-4.0.rst
   <https://github.com/Blosc/c-blosc2/blob/main/ROADMAP-TO-4.0.rst>`_ as
   current goals.

New Features in C-Blosc2
------------------------

* **[Done]** Adopt `OpenZL <https://openzl.org>`_ as another codec:  OpenZL is a new codec that is being developed by Meta.  It is meant to be a very fast codec with a good compression ratio, especially for floating point data.  Support has been implemented as a separate plugin instead of being part of the C-Blosc2 library itself; see the `blosc2-openzl <https://github.com/Blosc/blosc2-openzl>`_ repository.

* **[Deferred]** Move lazy expressions to C-Blosc2:  right now, lazy expressions are only supported in the Python wrapper (python-blosc2).  Moving them to C-Blosc2 would allow other wrappers (Java, R, Julia...) to benefit from this powerful feature.

* **[Deferred]** Optimization for multi-socket machines:  right now, C-Blosc2 is optimized for single-socket machines.  However, in multi-socket machines, memory access is not uniform (NUMA architecture), so optimizations are needed to make sure that every thread is accessing to local memory as much as possible.  This would require to use e.g. `numactl <https://linux.die.net/man/8/numactl>`_ or `libnuma <https://man7.org/linux/man-pages/man3/numa.3.html>`_ so as to pin threads and memory allocations to the local socket.

* **[Deferred]** Support for GPUs:  nowadays, GPUs are becoming more and more powerful, and having support for them in C-Blosc2 would be a great addition.  The idea is to offload the compression, but most importantly, decompression tasks to the GPU, so that the CPU is free to do other tasks.  This would require to use e.g. `CUDA <https://developer.nvidia.com/cuda-toolkit>`_ or `ROCm <https://rocm.docs.amd.com/>`_ so as to access to the GPU capabilities.

* **[Deferred]** ACID version of the frame format:  right now, frames are not ACID compliant (Atomicity, Consistency, Isolation and Durability).  This means that if a frame is being written and the process is interrupted (e.g. power failure, crash...), the frame may be left in an inconsistent state.  Making frames ACID compliant would require to implement a journaling mechanism so that changes are first written to a journal before being applied to the frame.  This would ensure that frames are always in a consistent state, even in the event of a failure.  Note that the 3.2 series added related (but weaker) guarantees: growth-SWMR (single writer, multiple readers) support and optional file locking for multi-process access.

* **[Deferred]** Framed version of the `TreeStore <https://www.blosc.org/posts/new-treestore-blosc2/>`_:  the TreeStore is a data structure developed in Python-Blosc2. It is meant to be a hierarchical data structure that allows to store and access data in a tree-like fashion.  Making it framed would allow to transfer it more easily between different machines, as well as storing it in memory.

General improvements
--------------------

* **[Ongoing] Improve the safety of the library:**  even if we have already made a long way in improving our safety, mainly thanks to the efforts of Nathan Moinvaziri, we take safety seriously, so this is always a work in progress.

* **[Deferred] Checksums:** the frame can benefit from having a checksum per every chunk/index/metalayer.  This will provide more safety towards frames that are damaged for whatever reason.  Also, this would provide better feedback when trying to determine the parts of the frame that are corrupted.  Candidates for checksums can be the xxhash32 or xxhash64, depending on the goals (to be decided).

* **[Deferred] Multiple index chunks in frames:** right now, only `one chunk <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst#chunks>`_ is allowed for indexing other chunks.  Provided the 2GB limit for a chunksize, that means that 'only' 256 million of chunks can be stored in a frame.  Allowing for more than one index chunk would overcome this limitation.

* **[Deferred] More robust detection of CPU capabilities:** although currently this detection is quite sophisticated, the code responsible for that has organically grown for more than 10 years and it is time to come with a more modern and robust way of doing this. https://github.com/google/cpu_features may be a good helper for doing this refactoring.

* **[Ongoing] Documentation:** utterly important for attracting new users and making the life easier for existing ones.  Important points to have in mind here:

  - **Quality of API docstrings:** is the mission of the functions or data structures clearly and succinctly explained? Are all the parameters explained?  Is the return value explained?  What are the possible errors that can be returned?  (mostly completed by Alberto Sabater).

  - **Tutorials/book:** besides the API docstrings, more documentation materials should be provided, like tutorials or a book about Blosc (or at least, the beginnings of it).  Due to its adoption in GitHub and Jupyter notebooks, one of the most extended and useful markup systems is Markdown, so this should also be the first candidate to use here.

* **[Done] Wrappers for other languages:** the steering `council discussed this <https://github.com/Blosc/governance/blob/master/steering_council_minutes/2020-03-26.md>`_ and the decision stands: the Python wrapper (`python-blosc2 <https://github.com/Blosc/python-blosc2>`_) is maintained by the Blosc maintainers themselves, while wrappers for other languages (Rust, Java, R, Julia...) are left to the community.

* **[Done] Lock support for super-chunks:** implemented in the 3.2 series.  Concurrent access from different processes to frame-backed super-chunks can now be serialized with an opt-in file locking mechanism: set the ``locking`` member of ``blosc2_stdio_params``, or the ``BLOSC_LOCKING`` environment variable to enable it globally (it ended up being a runtime opt-in rather than a build-time cmake flag).  See the `file-locking.c example <https://github.com/Blosc/c-blosc2/blob/main/examples/file-locking.c>`_.


Outreaching
-----------

* **[Done] Improve the Blosc website:** `www.blosc.org <https://www.blosc.org>`_ was revamped with a modern look, easy navigation and searchable sphinx-based documentation.

* **[Ongoing] Attend to meetings and conferences:** it is very important to plan going to conferences for advertising C-Blosc2 and meeting people in-person.  We need to decide which meetings to attend.  When on the Python arena, the answer would be quite clear, but for general C libraries like C-Blosc2, it is not that straightforward which ones are the most suited.

* **[Done]** Produce short introductory videos: see the 3-minute `introduction to Blosc2 <https://www.youtube.com/watch?v=ER12R7FXosk>`_ and the 2-minute video on `slicing pineapple-style <https://www.youtube.com/watch?v=LvP9zxMGBng>`_.


Increase diversity
------------------

**[Ongoing] We strive to make our team as diverse as possible:**  we are actively looking into more women and people from a variety of cultures to join our team.  We have been very fortunate to have had Marta Iborra, our first female among us; she did a great range of first class contributions, like sparse storage, pre and post filters, new dynamic filters (e.g. JPEG2000), improvements in the Btune prediction engine and many others. After Marta, we had Oumaima Ech.Chdig who enhanced documentation and tutorials, especially for the Python wrapper. Thanks to the Python Software Foundation and NumFOCUS for providing funds for allowing this.

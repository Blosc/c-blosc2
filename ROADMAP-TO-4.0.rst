Roadmap to 4.0
==============

This document outlines the main goals for the C-Blosc2 4.x series.  The core
deliverables are funded by a contract from the
`Sovereign Tech Agency (STA) <https://www.sovereign.tech/>`_
(see the `announcement blog post <https://www.blosc.org/posts/sta-award-blosc2/>`_)
to modernize, expand, and harden the C-Blosc2 foundation so that Blosc2's most
powerful container, querying, compute, and security capabilities are natively
available in C—and therefore accessible from C/C++, Rust, Julia, Go,
WebAssembly, and Python without requiring a Python runtime.

For historical context on earlier milestones, see
`ROADMAP-TO-3.0.rst <https://github.com/Blosc/c-blosc2/blob/main/ROADMAP-TO-3.0.rst>`_.
Community feedback and contributions toward any of these goals are warmly
welcomed.

Sovereign Tech Agency (STA) Core Pillars
----------------------------------------

1. Open Data Standards & Native C Containers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Formal open binary specifications (RFCs):** Draft formal, vendor-neutral
  RFC blueprints defining byte-level layouts for Blosc2's advanced containers,
  enabling independent third-party readers, writers, and verifiers.
* **Native C container implementations:** Bring containers currently available
  only in Python-Blosc2 directly into C-Blosc2 with guaranteed cross-platform
  compatibility:

  * **DictStore:** Hierarchical / dictionary store for organizing arrays,
    tables, and metadata.
  * **EmbedStore:** Portable, embedded container storage (including framed
    representations).
  * **CTable:** Compressed columnar table format with typed schemas and
    segment indexing.

* **Open governance:** Review and endorse all format specifications through
  open community discussions and the reactivated **Blosc Steering Council**.

2. Intelligent Slicing & Fast Data Skipping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Advanced multidimensional slicing in C:** Implement strided slices,
  boolean masks, and sophisticated indexing directly in the C core.
* **Array & columnar indexing with chunk pruning:** Add native indexing and
  metadata inspection so C-Blosc2 can prune (skip) irrelevant compressed chunks
  during queries and slices, reducing both I/O and memory consumption on large
  datasets.

3. Computing Directly on Compressed Data (Zero Dependencies)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Embedded ``miniexpr`` engine:** Embed the lightweight, high-performance
  ``miniexpr`` math engine directly into C-Blosc2.
* **Streaming evaluation on compressed data:** Evaluate mathematical
  expressions and table queries (e.g. ``temperature > 30``) directly as
  cache-sized blocks stream through the CPU, avoiding full decompression into
  RAM—completely self-contained in C with **zero external runtime
  dependencies** (no Python or NumPy required).

4. Built-in Data Encryption & Hardened Security
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Transparent software encryption:** Introduce native encryption in C-Blosc2
  so datasets can be encrypted and authenticated at rest.
* **Continuous fuzzing & static analysis:** Expand continuous fuzz testing with
  **Google OSS-Fuzz** and integrate static analysis into CI to catch memory
  errors, undefined behavior, and other vulnerabilities early.
* **Security governance:** Establish a formal vulnerability disclosure policy
  (``SECURITY.md``) with standardized CVE reporting workflows and a documented,
  risk-based security review process.

Cross-Cutting Deliverables
--------------------------

* **Compatibility & downstream integration testing:** Expand CI matrices and
  run cross-version compatibility and integration test suites against major
  downstream libraries (such as PyTables, ``hdf5plugin``, Zarr/``numcodecs``,
  and DKFZ's nnU-Net) prior to releases.
* **Documentation & contributor onboarding:** Overhaul the C-Blosc2
  documentation with a comprehensive C API reference, clear tutorials,
  in-depth technical articles, and a welcoming onboarding guide for new
  contributors.

Longer-Term / Community Goals
-----------------------------

These items remain on the broader C-Blosc2 wishlist beyond the STA contract
scope; contributions are welcome:

* **Checksums in frames:** Per-chunk, per-index, and per-metalayer checksums
  (e.g. xxHash32/xxHash64) to detect and localize frame corruption.
* **Multiple index chunks in frames:** Overcome the single
  `index chunk <https://github.com/Blosc/c-blosc2/blob/main/README_CFRAME_FORMAT.rst#chunks>`_
  limit (~256 million chunks per frame at the 2 GB chunk limit).
* **ACID / journaling frames:** Build on the 3.2 growth-SWMR and optional file
  locking mechanisms with journaling for crash-safe atomic updates.
* **NUMA / multi-socket optimizations:** Thread and memory pinning (e.g. via
  ``libnuma``) to improve memory locality on multi-socket systems.
* **GPU offloading:** Offload compression and decompression workloads to GPUs
  (e.g. via CUDA or ROCm).
* **Modernized CPU capability detection:** Refactor the organically grown
  CPU feature detection layer (potentially leveraging
  `google/cpu_features <https://github.com/google/cpu_features>`_).

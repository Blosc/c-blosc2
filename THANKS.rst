Thanks
======

* Valentin Haenel did a terrific work implementing the support for the Snappy compression, fixing typos and improving docs and the plotting script.

* Thibault North, with ideas from Oscar Villellas, contributed a way to call Blosc from different threads in a safe way.  Christopher
  Speller introduced contexts so that a global lock is not necessary anymore.

* The CMake support was initially contributed by Thibault North, and Antonio Valentino and Mark Wiebe made great enhancements to it.

* Christopher Speller also introduced the two new '_ctx' calls to avoid the use of the blosc_init() and blosc_destroy().

* Jack Pappas contributed important portability enhancements, specially runtime and cross-platform detection of SSE2/AVX2 as well as high precision timers (HPET) for the benchmark program.

* @littlezhou implemented the AVX2 version of shuffle routines.

* Julian Taylor contributed a way to detect AVX2 in runtime and calling the appropriate routines only if the underlying hardware supports it.

* Lucian Marc provided the support for ARM/NEON for the shuffle filter.

* Jerome Kieffer contributed support for PowerPC/ALTIVEC for the shuffle/bitshuffle filter.

* Alberto Sabater, for his great efforts on producing really nice Blosc2 docs, among other aspects.

* Kiyo Masui for relicensing his bitshuffle project for allowing the inclusion of part of his code in Blosc.

* Aleix Alcacer for his implementation of mutable super-chunks, multiple variable length metalayers and many other things.

* Oscar Guiñón for the optimization of reading a (sparse) set of blocks of a chunk in parallel.

* Nathan Moinvaziri for his outstanding work on the security side of the things via `fuzzer testing <https://google.github.io/oss-fuzz/>`_.

* Marta Iborra for her implementation of sparse storage for persistent super-chunks and her attention to detail in many other aspects of the library.

* Dimitri Papadopoulos for an extensive set of improvements in documentation and code.

Support for Apple arm64
=======================

Since November 2020 Apple is releasing Mac boxes using CPUs with its own
arm64 implementation.  Currently, you can compile C-Blosc2 with:

.. code-block:: console

    mkdir build
    cd build
    CC="clang -arch arm64" cmake ..

The NEON extensions are used automatically in this case.

Here it is a couple of benchmarks on BloscLZ and LZ4 codecs running on a MacBook Air
with a new M1 arm64 processor:

.. code-block:: console

    (base) francesc@Francescs-MacBook-Air build % bench/b2bench blosclz
    Blosc version: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)
    List of supported compressors in this build: blosclz,lz4,lz4hc,zlib,zstd
    Supported compression libraries:
      BloscLZ: 2.3.0
      LZ4: 1.9.1
      Zlib: 10.0.3
      Zstd: 1.4.5
    Using compressor: blosclz
    Using shuffle type: shuffle
    Running suite: single
    --> 4, 4194304, 4, 19, blosclz, shuffle
    ********************** Run info ******************************
    Blosc version: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)
    Using synthetic data with 19 significant bits (out of 32)
    Dataset size: 4194304 bytes	Type size: 4 bytes
    Working set: 256.0 MB		Number of threads: 4
    ********************** Running benchmarks *********************
    memcpy(write):		  327.0 us, 12234.1 MB/s
    memcpy(read):		  127.1 us, 31460.5 MB/s
    Compression level: 0
    comp(write):	  136.6 us, 29274.7 MB/s	  Final bytes: 4194336  Ratio: 1.00
    decomp(read):	  101.8 us, 39281.3 MB/s	  OK
    Compression level: 1
    comp(write):	  443.7 us, 9014.3 MB/s	  Final bytes: 1102576  Ratio: 3.80
    decomp(read):	   99.4 us, 40242.1 MB/s	  OK
    Compression level: 2
    comp(write):	  414.2 us, 9656.0 MB/s	  Final bytes: 1083528  Ratio: 3.87
    decomp(read):	   99.6 us, 40178.6 MB/s	  OK
    Compression level: 3
    comp(write):	  451.0 us, 8869.1 MB/s	  Final bytes: 1083528  Ratio: 3.87
    decomp(read):	  102.3 us, 39107.8 MB/s	  OK
    Compression level: 4
    comp(write):	  568.7 us, 7033.2 MB/s	  Final bytes: 165194  Ratio: 25.39
    decomp(read):	  135.8 us, 29447.2 MB/s	  OK
    Compression level: 5
    comp(write):	  565.2 us, 7076.6 MB/s	  Final bytes: 165194  Ratio: 25.39
    decomp(read):	  135.5 us, 29512.3 MB/s	  OK
    Compression level: 6
    comp(write):	  523.5 us, 7640.9 MB/s	  Final bytes: 141642  Ratio: 29.61
    decomp(read):	  138.2 us, 28943.9 MB/s	  OK
    Compression level: 7
    comp(write):	  520.4 us, 7687.0 MB/s	  Final bytes: 138042  Ratio: 30.38
    decomp(read):	  137.3 us, 29136.4 MB/s	  OK
    Compression level: 8
    comp(write):	  524.1 us, 7632.7 MB/s	  Final bytes: 137610  Ratio: 30.48
    decomp(read):	  138.4 us, 28893.1 MB/s	  OK
    Compression level: 9
    comp(write):	  534.4 us, 7485.3 MB/s	  Final bytes: 73254  Ratio: 57.26
    decomp(read):	  127.5 us, 31371.5 MB/s	  OK

    Round-trip compr/decompr on 7.5 GB
    Elapsed time:	    1.2 s, 13694.7 MB/s

    (base) francesc@Francescs-MacBook-Air build % bench/b2bench lz4
    Blosc version: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)
    List of supported compressors in this build: blosclz,lz4,lz4hc,zlib,zstd
    Supported compression libraries:
      BloscLZ: 2.3.0
      LZ4: 1.9.1
      Zlib: 10.0.3
      Zstd: 1.4.5
    Using compressor: lz4
    Using shuffle type: shuffle
    Running suite: single
    --> 4, 4194304, 4, 19, lz4, shuffle
    ********************** Run info ******************************
    Blosc version: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)
    Using synthetic data with 19 significant bits (out of 32)
    Dataset size: 4194304 bytes	Type size: 4 bytes
    Working set: 256.0 MB		Number of threads: 4
    ********************** Running benchmarks *********************
    memcpy(write):		  331.2 us, 12076.7 MB/s
    memcpy(read):		  134.7 us, 29689.2 MB/s
    Compression level: 0
    comp(write):	  132.2 us, 30259.8 MB/s	  Final bytes: 4194336  Ratio: 1.00
    decomp(read):	  109.0 us, 36709.8 MB/s	  OK
    Compression level: 1
    comp(write):	  589.3 us, 6787.9 MB/s	  Final bytes: 991408  Ratio: 4.23
    decomp(read):	  244.4 us, 16367.1 MB/s	  OK
    Compression level: 2
    comp(write):	  658.0 us, 6078.7 MB/s	  Final bytes: 877344  Ratio: 4.78
    decomp(read):	  313.3 us, 12765.8 MB/s	  OK
    Compression level: 3
    comp(write):	  670.3 us, 5967.1 MB/s	  Final bytes: 786514  Ratio: 5.33
    decomp(read):	  312.4 us, 12804.1 MB/s	  OK
    Compression level: 4
    comp(write):	  420.5 us, 9512.9 MB/s	  Final bytes: 428754  Ratio: 9.78
    decomp(read):	  244.8 us, 16340.0 MB/s	  OK
    Compression level: 5
    comp(write):	  419.5 us, 9536.0 MB/s	  Final bytes: 428754  Ratio: 9.78
    decomp(read):	  245.2 us, 16311.3 MB/s	  OK
    Compression level: 6
    comp(write):	  413.5 us, 9673.5 MB/s	  Final bytes: 379116  Ratio: 11.06
    decomp(read):	  239.3 us, 16713.1 MB/s	  OK
    Compression level: 7
    comp(write):	  414.2 us, 9656.7 MB/s	  Final bytes: 379116  Ratio: 11.06
    decomp(read):	  240.0 us, 16666.0 MB/s	  OK
    Compression level: 8
    comp(write):	  413.2 us, 9680.5 MB/s	  Final bytes: 379116  Ratio: 11.06
    decomp(read):	  237.7 us, 16830.1 MB/s	  OK
    Compression level: 9
    comp(write):	  414.3 us, 9654.1 MB/s	  Final bytes: 379116  Ratio: 11.06
    decomp(read):	  239.8 us, 16684.0 MB/s	  OK

    Round-trip compr/decompr on 7.5 GB
    Elapsed time:	    1.4 s, 11720.1 MB/s

Yes, the speed is really good on the new Apple architecture!


ARM environment for Ubuntu Intel
================================

In order to create executable code for a platform other than the one on
which the compiler is running we can use cross compilers. In this case we
want to create executables for ARM platforms but execute them on
Intel platform with Ubuntu. The only package that is necessary
(at least in Ubuntu 15.04) is gcc-arm-linux-gnueabihf.

Once this is installed the way to compile an example is:

.. code-block:: console

  $ arm-linux-gnueabihf-gcc -static  -mfpu=neon -flax-vector-conversions -DSHUFFLE_NEON_ENABLED -O3 simple.c ../blosc/blosc.c ../blosc/blosclz.c ../blosc/shuffle.c ../blosc/shuffle-generic.c ../blosc/bitshuffle-generic.c ../blosc/shuffle-neon.c ../blosc/bitshuffle-neon.c -I../blosc -o simple -lpthread

Another example for running a bench with the cross compiler:

.. code-block:: console

  $ arm-linux-gnueabihf-gcc -static  -mfpu=neon -flax-vector-conversions -DSHUFFLE_NEON_ENABLED -O3 bench.c  ../blosc/blosc.c ../blosc/blosclz.c ../blosc/shuffle.c ../blosc/shuffle-generic.c ../blosc/bitshuffle-generic.c ../blosc/shuffle-neon.c ../blosc/bitshuffle-neon.c -I../blosc -o bench -lpthread

In these cases the NEON flags are: `-mfpu=neon -flax-vector-conversions`

This is explained in detail in: https://linux-sunxi.org/Toolchain

This way you can develop and debug applications for ARM on intel machines as if
you were in ARM platforms.

Benchmark for ODROID-XU3
========================

This is a benchmark to compare the speeds between the NEON and the generic
implementation.

::

    CPU: ARMv7 Processor rev 3 (v7l)
    Compiler: gcc, version gcc-4.8.real
    Optimizations: -O3
    OS: Ubuntu 14.04 trusty

• NEON implementation results:

::

    Blosc version: 2.0.0a1 ($Date:: 2015-07-30 #$)
    List of supported compressors in this build: blosclz
    Supported compression libraries:
      BloscLZ: 1.0.5
      LZ4: unknown
      Zlib: unknown
    Using compressor: blosclz
    Using shuffle type: shuffle
    Running suite: suite
    --> 1, 2097152, 8, 19, blosclz, shuffle
    ********************** Run info ******************************
    Blosc version: 2.0.0a1 ($Date:: 2015-07-30 #$)
    Using synthetic data with 19 significant bits (out of 32)
    Dataset size: 2097152 bytes	Type size: 8 bytes
    Working set: 64.0 MB		Number of threads: 1
    ********************** Running benchmarks *********************
    memcpy(write):		 2265.1 us, 883.0 MB/s
    memcpy(read):		 1196.9 us, 1671.0 MB/s
    Compression level: 0
    comp(write):	  984.8 us, 2030.9 MB/s	  Final bytes: 2097168  Ratio: 1.00
    decomp(read):	 1202.8 us, 1662.8 MB/s	  OK
    Compression level: 1
    comp(write):	 5863.7 us, 341.1 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 1014.1 us, 1972.3 MB/s	  OK
    Compression level: 2
    comp(write):	 6229.8 us, 321.0 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 1013.4 us, 1973.5 MB/s	  OK
    Compression level: 3
    comp(write):	 6603.8 us, 302.9 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 1012.9 us, 1974.4 MB/s	  OK
    Compression level: 4
    comp(write):	 6792.6 us, 294.4 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	  983.5 us, 2033.6 MB/s	  OK
    Compression level: 5
    comp(write):	 8598.3 us, 232.6 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	  983.4 us, 2033.9 MB/s	  OK
    Compression level: 6
    comp(write):	 9866.2 us, 202.7 MB/s	  Final bytes: 546320  Ratio: 3.84
    decomp(read):	 1079.1 us, 1853.3 MB/s	  OK
    Compression level: 7
    comp(write):	 9334.9 us, 214.2 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 1959.4 us, 1020.7 MB/s	  OK
    Compression level: 8
    comp(write):	 9221.1 us, 216.9 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 1972.1 us, 1014.2 MB/s	  OK
    Compression level: 9
    comp(write):	 8452.0 us, 236.6 MB/s	  Final bytes: 153160  Ratio: 13.69
    decomp(read):	 2780.0 us, 719.4 MB/s	  OK
    --> 2, 2097152, 8, 19, blosclz, shuffle
    ********************** Run info ******************************
    Blosc version: 2.0.0a1 ($Date:: 2015-07-30 #$)
    Using synthetic data with 19 significant bits (out of 32)
    Dataset size: 2097152 bytes	Type size: 8 bytes
    Working set: 64.0 MB		Number of threads: 2
    ********************** Running benchmarks *********************
    memcpy(write):		 2258.0 us, 885.8 MB/s
    memcpy(read):		 1194.9 us, 1673.7 MB/s
    Compression level: 0
    comp(write):	  831.2 us, 2406.0 MB/s	  Final bytes: 2097168  Ratio: 1.00
    decomp(read):	 1162.6 us, 1720.3 MB/s	  OK
    Compression level: 1
    comp(write):	 2975.1 us, 672.2 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	  738.4 us, 2708.4 MB/s	  OK
    Compression level: 2
    comp(write):	 3156.8 us, 633.6 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	  738.7 us, 2707.5 MB/s	  OK
    Compression level: 3
    comp(write):	 3347.0 us, 597.6 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	  749.6 us, 2668.1 MB/s	  OK
    Compression level: 4
    comp(write):	 3486.2 us, 573.7 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	  745.1 us, 2684.4 MB/s	  OK
    Compression level: 5
    comp(write):	 4488.5 us, 445.6 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	  725.7 us, 2755.9 MB/s	  OK
    Compression level: 6
    comp(write):	 4998.5 us, 400.1 MB/s	  Final bytes: 546320  Ratio: 3.84
    decomp(read):	  796.8 us, 2510.2 MB/s	  OK
    Compression level: 7
    comp(write):	 4780.3 us, 418.4 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 1383.9 us, 1445.1 MB/s	  OK
    Compression level: 8
    comp(write):	 4778.6 us, 418.5 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 1398.0 us, 1430.6 MB/s	  OK
    Compression level: 9
    comp(write):	 5884.6 us, 339.9 MB/s	  Final bytes: 153160  Ratio: 13.69
    decomp(read):	 2647.7 us, 755.4 MB/s	  OK

    Round-trip compr/decompr on 3.8 GB
    Elapsed time:	   13.9 s, 609.1 MB/s

• Generic implementation results:

::

    Blosc version: 2.0.0a1 ($Date:: 2015-07-30 #$)
    List of supported compressors in this build: blosclz
    Supported compression libraries:
      BloscLZ: 1.0.5
      LZ4: unknown
      Zlib: unknown
    Using compressor: blosclz
    Using shuffle type: shuffle
    Running suite: suite
    --> 1, 2097152, 8, 19, blosclz, shuffle
    ********************** Run info ******************************
    Blosc version: 2.0.0a1 ($Date:: 2015-07-30 #$)
    Using synthetic data with 19 significant bits (out of 32)
    Dataset size: 2097152 bytes	Type size: 8 bytes
    Working set: 64.0 MB		Number of threads: 1
    ********************** Running benchmarks *********************
    memcpy(write):		 2194.1 us, 911.5 MB/s
    memcpy(read):		 1170.8 us, 1708.2 MB/s
    Compression level: 0
    comp(write):	  896.2 us, 2231.7 MB/s	  Final bytes: 2097168  Ratio: 1.00
    decomp(read):	 1179.3 us, 1695.9 MB/s	  OK
    Compression level: 1
    comp(write):	 7534.4 us, 265.4 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 4117.1 us, 485.8 MB/s	  OK
    Compression level: 2
    comp(write):	 7895.6 us, 253.3 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 4106.7 us, 487.0 MB/s	  OK
    Compression level: 3
    comp(write):	 8262.2 us, 242.1 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 4113.9 us, 486.2 MB/s	  OK
    Compression level: 4
    comp(write):	 8495.6 us, 235.4 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	 4042.6 us, 494.7 MB/s	  OK
    Compression level: 5
    comp(write):	 10321.0 us, 193.8 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	 4033.9 us, 495.8 MB/s	  OK
    Compression level: 6
    comp(write):	 11675.3 us, 171.3 MB/s	  Final bytes: 546320  Ratio: 3.84
    decomp(read):	 4096.4 us, 488.2 MB/s	  OK
    Compression level: 7
    comp(write):	 10193.0 us, 196.2 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 7150.9 us, 279.7 MB/s	  OK
    Compression level: 8
    comp(write):	 10192.3 us, 196.2 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 7167.7 us, 279.0 MB/s	  OK
    Compression level: 9
    comp(write):	 10418.7 us, 192.0 MB/s	  Final bytes: 153160  Ratio: 13.69
    decomp(read):	 7870.8 us, 254.1 MB/s	  OK
    --> 2, 2097152, 8, 19, blosclz, shuffle
    ********************** Run info ******************************
    Blosc version: 2.0.0a1 ($Date:: 2015-07-30 #$)
    Using synthetic data with 19 significant bits (out of 32)
    Dataset size: 2097152 bytes	Type size: 8 bytes
    Working set: 64.0 MB		Number of threads: 2
    ********************** Running benchmarks *********************
    memcpy(write):		 2243.1 us, 891.6 MB/s
    memcpy(read):		 1219.9 us, 1639.4 MB/s
    Compression level: 0
    comp(write):	  846.8 us, 2361.9 MB/s	  Final bytes: 2097168  Ratio: 1.00
    decomp(read):	 1182.1 us, 1691.9 MB/s	  OK
    Compression level: 1
    comp(write):	 3867.6 us, 517.1 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 2162.8 us, 924.7 MB/s	  OK
    Compression level: 2
    comp(write):	 4054.2 us, 493.3 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 2156.6 us, 927.4 MB/s	  OK
    Compression level: 3
    comp(write):	 4241.2 us, 471.6 MB/s	  Final bytes: 584976  Ratio: 3.59
    decomp(read):	 2169.6 us, 921.8 MB/s	  OK
    Compression level: 4
    comp(write):	 4377.1 us, 456.9 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	 7556.9 us, 264.7 MB/s	  OK
    Compression level: 5
    comp(write):	 5276.3 us, 379.1 MB/s	  Final bytes: 557840  Ratio: 3.76
    decomp(read):	 7556.7 us, 264.7 MB/s	  OK
    Compression level: 6
    comp(write):	 6026.8 us, 331.9 MB/s	  Final bytes: 546320  Ratio: 3.84
    decomp(read):	 3108.2 us, 643.5 MB/s	  OK
    Compression level: 7
    comp(write):	 5877.0 us, 340.3 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 3668.5 us, 545.2 MB/s	  OK
    Compression level: 8
    comp(write):	 5882.0 us, 340.0 MB/s	  Final bytes: 216528  Ratio: 9.69
    decomp(read):	 3531.2 us, 566.4 MB/s	  OK
    Compression level: 9
    comp(write):	 7621.3 us, 262.4 MB/s	  Final bytes: 153160  Ratio: 13.69
    decomp(read):	 4978.3 us, 401.7 MB/s	  OK

    Round-trip compr/decompr on 3.8 GB
    Elapsed time:	   21.9 s, 385.0 MB/s

We have achieved to implement shuffle NEON instructions for ARM that are twice
as fast as the generic implementation.

Enjoy developing for ARM!

Lucian Marc

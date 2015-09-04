ARM enviroment for Ubuntu Intel
================================

In order to create executable code for a platform other than the one on 
which the compiler is running we can use cross compilers. In this case we
want to create executables for ARM platforms but execute them on
Intel platform with Ubuntu. The only package that is necessary 
(at least in Ubuntu 15.04) is gcc-arm-linux-gnueabihf.

Once this is installed the way to compile an example is:

.. code-block:: console

  $ arm-linux-gnueabihf-gcc -static  -mfpu=neon -flax-vector-conversions -DSHUFFLE_NEON_ENABLED -O3 simple.c ../blosc/blosc.c ../blosc/blosclz.c ../blosc/shuffle.c ../blosc/shuffle-generic.c ../blosc/bitshuffle-generic.c ../blosc/shuffle-neon.c ../blosc/bitshuffle-neon.c -I../blosc -o simple -lpthread

Another example for runnig a bench with the cross compliler: 

.. code-block:: console

  $ arm-linux-gnueabihf-gcc -static  -mfpu=neon -flax-vector-conversions -DSHUFFLE_NEON_ENABLED -O3 bench.c  ../blosc/blosc.c ../blosc/blosclz.c ../blosc/shuffle.c ../blosc/shuffle-generic.c ../blosc/bitshuffle-generic.c ../blosc/shuffle-neon.c ../blosc/bitshuffle-neon.c -I../blosc -o bench -lpthread

In these cases the NEON flags are: `-mfpu=neon -flax-vector-conversions`

This is explained in detail in: http://linux-sunxi.org/Toolchain

This way you can develop and debug aplications for ARM on intel machines as if 
you were in ARM platforms.

Enjoy developing for ARM!

Lucian Marc

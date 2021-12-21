ZFP: a multidimensional lossy codec
=============================================================================

Given a 4-dim array, *ZFP* is a compressor of lossy data compression mainly designed for multidimensional floating point datasets.

Plugin motivation
--------------------

Enabling the support for a lossy codec like ZFP in C-Blosc2 allows for much better compression ratios at the expense of loosing some precision.  At the same time, and if executed carefully, it can allow for a third level partition for Caterva (a light-weight layer for multidimensional data on top of C-Blosc2) so that slicing can be done even more efficiently with this codec.

Plugin usage
-------------------

The codec consists of an encoder called *blosc2_zfp_acc_compress()* to codify data and
a decoder called *blosc2_zfp_acc_decompress()* to recover the original data.

This plugin only works from 1 to 4-dim datasets of 4 or 8 bytes items (floats or doubles),
so if user tries to work with other type of dataset, the codec will return an error value.

The parameters used by *ZFP* are the ones specified in the *blosc2_codec*
structure of *blosc2.h*.
Furthermore, depending on the *ZFP* mode wanted to use, user must specify the parameter meta.

- BLOSC_CODEC_ZFP_FIXED_RATE: meta must be a number between 1 and 100. It is called *rate* and represents the size that the compressed cells must have based on the input cell size. For example, if the cell size is 2000 bytes and meta = 50, the output cell size will be 50% of 2000 = 1000 bytes.
- BLOSC_CODEC_ZFP_FIXED_ACCURACY: meta is used as absolute error in truncation. If user do not want the compiler to show a warning, a cast to unsigned integer of 1 byte must be done before calling the codec. For example, if meta = -2, each value loss must be less than or equal to 10^(-2) = 0,01. Then, if 23,0567 is a value of the original input, after compressing and decompressing this input with meta = -2, this value must be between 23,0467 and 23,0667.

To use one of the compression modes, user must choose its corresponding identifier as blosc2_cparams.compcode. For example:

    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_ACCURACY;

Available codecs IDs (including ZFP modes) are defined at:
https://github.com/Blosc/c-blosc2/blob/main/include/blosc2/codecs-registry.h

Plugin behaviour
-------------------

In order to compress n-dimensional arrays of floating-point data, ZFP partitions them into cells of size 4<sup>n</sup> so, in a 3-dim dataset, cellshape will be 4x4x4.
Then, depending on the compression mode, each block is compressed or decompressed individually into a fixed- or variable-length bit string, and these bit strings are concatenated into a single stream of bits.


- BLOSC_CODEC_ZFP_FIXED_RATE: each cell is compressed with a fixed output size given by the parameter *zfp_stream.maxbits*. This number of compressed bits per cell is amortized over the cell size to give a *rate*:

      rate = maxbits / 4^n

  This rate can be changed using the function *zfp_stream_set_rate()*.


- BLOSC_CODEC_ZFP_FIXED_ACCURACY: each cell is compressed using truncation with an absolute error tolerance. It can be changed using *zfp_stream_set_accuracy()*.

For more info you can see ZFP official documentation:
https://zfp.readthedocs.io/en/release0.5.4/index.html

And the offical repo:
https://github.com/LLNL/zfp

Advantages and disadvantages
------------------------------

The main advantage of *ZFP* in front of most of the codecs is that this one
considers datasets multidimensionality and takes advantage of it instead of
processing all data as serial. The difference between it and *NDLZ* is that this codec uses lossy data compression (which implies better ratios) and lets user to work with floating-point datasets.
Furthermore, *ZFP* implements different interesting modes that bring Blosc and Caterva new possibilities.

The main disadvantage of *ZFP* is that it is only useful for 2 to 4-dim datsets. It can also be used for 1-dim datasets but then it loses all its advantages.

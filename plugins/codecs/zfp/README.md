ZFP: a multidimensional lossy codec
=============================================================================

*ZFP* is a codec for lossy data compression, designed for being mainly used in multidimensional (up to 4-dim) floating point datasets.

Plugin motivation
--------------------

A lossy codec like ZFP allows for much better compression ratios at the expense of loosing some precision in floating point data.  At the same time, and if executed carefully, it can tentatively allow for a third level partition for Caterva (a light-weight layer for multidimensional data on top of C-Blosc2) so that slicing could be done even more efficiently with this codec.

Plugin usage
-------------------

The codec consists of an encoder called `blosc2_zfp_acc_compress()` to codify data and
a decoder called `blosc2_zfp_acc_decompress()` to recover the original data.

This plugin only works with 1 to 4-dim datasets of floats or doubles, so if one tries to work with another type of dataset, it will return an error value.

The parameters used by *ZFP* are the ones specified in the `blosc2_codec` structure in the `blosc2.h` header.
Furthermore, *ZFP* allows to work in two modes, BLOSC_CODEC_ZFP_FIXED_RATE and BLOSC_CODEC_ZFP_FIXED_ACCURACY, each of one can be fine-tuned via the `meta` parameter as follows:

- BLOSC_CODEC_ZFP_FIXED_RATE: `meta` must be a number between 1 and 100. It is called *rate* and represents the size that the compressed cells must have based on the input cell size. For example, if the cell size is 2000 bytes and meta = 50, the output cell size will be 50% of 2000 = 1000 bytes.
- BLOSC_CODEC_ZFP_FIXED_ACCURACY: `meta` is used as absolute error in truncation.  For example, if meta = -2, each value loss must be less than or equal to 10^(-2) = 0,01. Then, if 23,0567 is a value of the original input, after compressing and decompressing this input with meta = -2, this value must be between 23,0467 and 23,0667. As `meta` is unsigned in the codec plugin interface, if user does not want the compiler to show a warning, a cast to unsigned integer of 1 byte must be done before calling the plugin.

To choose one of the compression modes, user must use its corresponding identifier as `blosc2_cparams.compcode`. For example:

    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_ACCURACY;
    
or:

    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;

Remember that all available codec IDs (including ZFP modes) are defined at:
https://github.com/Blosc/c-blosc2/blob/main/include/blosc2/codecs-registry.h


How ZFP works
-------------------

In order to compress n-dimensional arrays of floating-point data, ZFP partitions them into cells of size 4^n so; for example, in a 3-dim dataset, the cellshape will be 4x4x4.
Then, depending on the compression mode, each block is compressed or decompressed individually into a fixed- or variable-length bit string, and these bit strings are concatenated into a single stream of bits.

For more info you can see ZFP official documentation:
https://zfp.readthedocs.io/en/latest/

And the offical repo:
https://github.com/LLNL/zfp

Advantages and disadvantages
------------------------------

The main advantage of *ZFP* when compared with others is that *ZFP*
makes use of the multidimensionality of datasets and takes advantage of this instead of
processing all data as serial.

For example, the difference between *ZFP* and *NDLZ* is that *ZFP* codec uses lossy data compression (which implies better ratios) and lets user to work with floating-point datasets.
Furthermore, *ZFP* implements different compression modes that brings interesting new possibilities.

The main disadvantage of *ZFP* is that it is mainlly useful for 2 to 4-dim datasets only. It can also be used for 1-dim datasets but then it loses most of its advantages (i.e. using spacial locality to better reduce data entropy).

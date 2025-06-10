ZFP: a multidimensional lossy codec
=============================================================================

*ZFP* is a codec for lossy data compression, designed for being mainly used in multidimensional (up to 4-dim) floating point datasets.

Plugin motivation
--------------------

A lossy codec like ZFP allows for much better compression ratios at the expense of losing some precision in floating point data.  For a discussion on how it works and specially, how it performs, see our blog at: https://www.blosc.org/posts/support-lossy-zfp/.

Plugin usage
-------------------

The codec consists of different encoders to codify data and decoders to recover the original data, located at `blosc2-zfp.c`.

This plugin only works with 1 to 4-dim datasets of floats or doubles, so if one tries to work with another type of dataset, it will return an error value. Also, the blocksize requires to be a minimum of 4 items for 1 dim, 4x4 for 2 dim, 4x4x4 for 3 dim and 4x4x4x4 for 4 dim.  Finally, the ZFP codecs do interpret the *values*, so they are meant to be used *without any shuffle filter* (that would break byte/bit ordering, and hence, changing the values before they would reach the ZFP codec).

The parameters used by *ZFP* are the ones specified in the `blosc2_codec` structure in the `blosc2.h` header.
Furthermore, *ZFP* allows to work in three modes, BLOSC_CODEC_ZFP_FIXED_ACCURACY, BLOSC_CODEC_ZFP_FIXED_PRECISION and BLOSC_CODEC_ZFP_FIXED_RATE, each of one can be fine-tuned via the `meta` parameter as follows:

- BLOSC_CODEC_ZFP_FIXED_ACCURACY: `meta` is used as absolute error in truncation.  For example, if meta = -2, each value loss must be less than or equal to 10^(-2) = 0,01. Then, if 23,0567 is a value of the original input, after compressing and decompressing this input with meta = -2, this value must be between 23,0467 and 23,0667. As `meta` is unsigned in the codec plugin interface, if user does not want the compiler to show a warning, a cast to a `uint8_t` must be done before calling the plugin.
- BLOSC_CODEC_ZFP_FIXED_PRECISION: `meta` must be a number between 1 and ZFP_MAX_PREC. It is called *precision* and represents the maximum number of bit planes encoded. This is, for each input value, the number of most significant bits that will be encoded. For more info, see:
  https://zfp.readthedocs.io/en/latest/faq.html#q-relerr
- BLOSC_CODEC_ZFP_FIXED_RATE: `meta` must be a number between 1 and 100. It is called *ratio* and represents the size that the compressed cells must have based on the input cell size. For example, if the cell size is 2000 bytes and meta = 50, the output cell size will be 50% of 2000 = 1000 bytes.

To choose one of the compression modes, user must use its corresponding identifier as `blosc2_cparams.compcode`. For example:

    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_ACCURACY;

or:

    cparams.compcode = BLOSC_CODEC_ZFP_FIXED_PRECISION;    

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

And the official repo:
https://github.com/LLNL/zfp

Advantages and disadvantages
------------------------------

The main advantage of *ZFP* when compared with others is that *ZFP* makes use of the multidimensionality of datasets and takes advantage of this instead of processing all data as serial.

For example, the difference between *ZFP* and *NDLZ* is that *ZFP* codec uses lossy data compression (which implies better ratios) and lets user to work with floating-point datasets.  Furthermore, *ZFP* implements different compression modes that brings interesting new possibilities.

The main disadvantage of *ZFP* is that it is mainly useful for 2 to 4-dim datasets only. It can also be used for 1-dim datasets, but then it loses most of its advantages (i.e. using spatial locality to better reduce data entropy).

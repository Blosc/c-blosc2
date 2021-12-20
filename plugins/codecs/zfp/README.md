ZFP: a multidimensional lossy codec
=============================================================================

Given a 4-dim array, *ZFP* is a compressor of lossy data compression mainly designed for multidimensional floating point datasets.

Plugin motivation
--------------------

Enabling the support for a lossy codec like ZFP in C-Blosc2 will allow for much better compression ratios at the expense of loosing some precision.  At the same time, and if executed carefully, it can allow for a third level partition for Caterva (a light-weight layer for multidimensional data on top of C-Blosc2) so that slicing can be done even more efficiently with this codec.

Plugin usage
-------------------

The codec consists of an encoder called *blosc2_zfp_compress()* to codify data and
a decoder called *blosc2_zfp_decompress()* to recover the original data.

This plugin only works from 1 to 4-dim datasets of 4 or 8 bytes items (floats or doubles),
so if user tries to work with other type of dataset, the codec will return an error value.

The parameters used by *ZFP* are the ones specified in the *blosc2_codec*
structure of *blosc2.h*.
Furthermore, depending on the *ZFP* mode wanted to use, user must specify the parameter meta.

- BLOSC_CODEC_ZFP_FIXED_RATE: meta must be a number between 1 and 100. It represents the size that the compressed buffer must have based on the input size. For example, if the input size is 2000 bytes and meta = 50, the output size will be 50% of 2000 = 1000 bytes.
- BLOSC_CODEC_ZFP_FIXED_ACCURACY: meta is used as absolute error in truncation. For example, if meta = -2, each value loss must be less than or equal to 10^(-2) = 0,01. Then, if 23,0567 is a value of the original input, after compressing and decompressing this input with meta = -2, this value must be between 23,0467 and 23,0667.

Plugin behaviour
-------------------


Advantages and disadvantages
------------------------------

The main advantage of *NDLZ* in front of most of the codecs is that this one
considers dataset multidimensionality and takes advantage of it instead of
processing all data as serial.

The main disadvantage of *NDLZ* is that it is only useful for 2-dim datsets
and at the moment other more developed
codecs that do not consider multidimensionality obtain better results
(times and ratios) for 2-dim datasets.

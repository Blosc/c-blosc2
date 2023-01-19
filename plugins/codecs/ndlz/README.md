NDLZ: a multidimensional lossless codec
=============================================================================

Given a 2-dim array or matrix, *NDLZ* is a compressor based on the Lempel-Ziv algorithm of lossless data compression.

Plugin motivation
--------------------

*NDLZ* was created in order to search for patterns repetitions in multidimensional cells using a multidimensional blocking machinery.

Plugin usage
-------------------

The codec consists of an encoder called *ndlz_compress()* to codify data and
a decoder called *ndlz_decompress()* to recover the original data.

The parameters used by *NDLZ* are the ones specified in the *blosc2_codec*
structure of *blosc2.h*.
Furthermore, since *NDLZ* goes through dataset blocks dividing them into fixed size cells,
user must specify the parameter meta as 4 to use cells of size 4x4 or
8 to use 8x8 cells. If user tries to use other value for meta, the codec
will return an error value.

NDLZ only works for 2-dim datasets of 1 byte items (typesize = 1),
so if you want to use it for a dataset with bigger typesize then you
must activate SHUFFLE filter and splitting mode.

Plugin behaviour
-------------------

This codec is meant to leverage multidimensionality for getting
better compression ratios.  The idea is to look for similarities
in places that are closer in a euclidean metric, not the typical
linear one.

First *NDLZ* goes through dataset blocks dividing them into fixed size cells.
Then, for each cell the codec searches for data coincidences with previous
cells in order to copy only references to those cells instead of copying
the full current cell.

To understand how the compressor and decompressor work it is important to
learn about the compressed block format. An *NDLZ* compressed block is
composed of a not-compressed byte called token and some 2 bytes values
called offsets.

The token is divided in two fields. The first field is composed of the 2 first bits of the token and gives important
information about the cells and rows couples matches.
The high-bit of the field is activated when there exists repeated information (there are matches) and there exists offset.
If it is activated, the other field indicates special patterns of matches, and if not we have to look at the second bit.
If it is activated (token = 01000000), this means that the whole cell is composed of the same element, and if not
(token = 00000000) there is not repeated information and the whole cell is literally copied.

The offsets are references to previous literal copies that match with the
data that is being evaluated at the moment.

Otherwise, it is important to know that there exist different hash tables which store the references to the literal copies of cells and rows in their hash position.

Advantages and disadvantages
------------------------------

The main advantage of *NDLZ* when compared with most of the codecs is that this one
considers dataset multidimensionality and takes advantage of it instead of
processing all data as serial.

The main disadvantage of *NDLZ* is that it is only useful for 2-dim datasets
and at the moment it gets worse results (times and ratios) than other, more developed codecs
that do not consider multidimensionality, at least in our limited testing.

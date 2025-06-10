NDMEAN: a multidimensional filter for lossy compression
=============================================================================

Given an n-dim array or matrix, *NDMEAN* is a filter based on *NDCELL*
(https://github.com/Blosc/c-blosc2/tree/main/plugins/filters/ndcell/README.rst)
that not only divides data into multidimensional cells so
that a codec compress cell by cell, but also replaces each cell elements by
the cell mean.

Plugin motivation
--------------------

*NDMEAN* was created in order to add another depth level to *NDCELL*, taking
advantage of multidimensionality and making each cell items equal to earn
better compression ratios for the codecs.

Plugin usage
-------------------

The codec consists of an encoder called *ndmean_forward()* to reorder data and
a decoder called *ndmean_backward()* to recover the original order of data
but not the original data (lossy compression).

The parameters used by *NDMEAN* are the ones specified in the *blosc2_filter*
structure of *blosc2.h*.
Furthermore, since *NDMEAN* goes through dataset blocks dividing them into fixed size cells,
user must specify the parameter meta as the cellshape, so if in a
3-dim dataset user specifies meta = 4, then cellshape will be 4x4x4. If user tries to use other value for meta, the codec
will return an error value.

NDMEAN only works for float or double datasets (depending on the typesize), so if it is used 
for a different data type it may return an error code. 

Plugin behaviour
-------------------

This filter is meant first to order the data so that when the
dataset is traversed the elements of a cell are all in a row.
Furthermore, *NDMEAN* replaces cell elements by the cell mean.



    ------------------------                  -----------------------------
    | 0 1 | 1 2 | 2 3 | 3 5 |                 | 1 1 1 1 1 1 | 2 2 2 2 2 2 |
    | 2 0 | 3 1 | 4 2 | 3 5 |                 -----------------------------
    | 1 2 | 2 3 | 3 4 | 5 3 | NDMEAN encoder  | 3 3 3 3 3 3 | 4 4 4 4 4 4 |
    -------------------------      ------>    -----------------------------
    | 1 3 | 1 2 | 2 4 | 4 8 |                 | 2 2 2 2 2 2 | 1 1 1 1 1 1 |
    | 3 1 | 0 1 | 1 5 | 7 5 |                 -----------------------------
    | 1 3 | 0 2 | 0 6 | 4 8 |                 | 3 3 3 3 3 3 | 6 6 6 6 6 6 |
    -------------------------                 -----------------------------



Advantages and disadvantages
------------------------------

The particularity of *NDMEAN* is that this filter not only
considers datasets multidimensionality and takes advantage of it instead
of processing all data as serial, but also makes cell data easier to
compress.

The main disadvantage of *NDMEAN* is that it changes the original data
and can not recover it (lossy compression).

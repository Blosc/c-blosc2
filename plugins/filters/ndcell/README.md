NDCELL: a multidimensional filter for lossless compression
=============================================================================

Given an n-dim array or matrix, *NDCELL* is a filter based on the *NDLZ* codec
that groups data in multidimensional cells, reordering them so that the codec
can find better repetition patterns on a cell by cell basis.

Plugin motivation
--------------------

*NDCELL* was created in order to make easy for codecs to search for pattern
repetitions in multidimensional datasets using the Blosc2 NDim blocking machinery.

Plugin usage
-------------------

The codec consists of an encoder called *ndcell_forward()* to reorder data and
a decoder called *ndcell_backward()* to recover the original data.

The parameters used by *NDCELL* are the ones specified in the *blosc2_filter*
structure of *blosc2.h*.
Furthermore, since *NDCELL* goes through dataset blocks dividing them into fixed size cells,
user must specify the parameter meta as the cellshape, so if in a
3-dim dataset user specifies meta = 4, then cellshape will be 4x4x4. 

Plugin behaviour
-------------------

This filter is meant to leverage multidimensionality for helping codecs to get
better compression ratios. The idea is to order the data so that when the
dataset is traversed the elements of a cell are all in a row.

    ------------------------                  -----------------------------
    | 0 1 | 2 3 | 4 5 | 6 7 |                 |   0 1 8 9   |  2 3 10 11  |
    | 8 9 |10 11|12 13|14 15| NDCELL encoder  -----------------------------
    -------------------------      ------>    |  4 5 12 13  |  6 7 14 15  |
    |16 17|18 19|20 21|22 23|                 -----------------------------
    |24 25|26 27|28 29|30 31|                 | 16 17 24 25 | 18 19 26 27 |
    -------------------------                 -----------------------------
                                              | 20 21 28 29 | 22 23 30 31 |
                                              -----------------------------

Advantages and disadvantages
------------------------------

The advantage of *NDCELL* is that it makes codecs finding duplicacies in
multidimensional datasets.

The main disadvantage of *NDCELL* is that it only works for multidimensional
datasets, and not all codecs can leverage it (only the fastest ones).

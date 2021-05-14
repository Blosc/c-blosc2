NDCELL: a multidimensional filter for lossless compression
=============================================================================

Given an n-dim array or matrix, *NDCELL* is a filter based on the codec *NDLZ*
that divides data into multidimensional cells, reordering the dataset so
that the codec compress cell by cell.

Plugin motivation
--------------------

*NDCELL* was created in order to make easy for codecs to search for patterns repetitions in multidimensional datasets using the Caterva blocking machinery.

Plugin behaviour
-------------------

This filter is meant to leverage multidimensionality for helping codecs to get
better compression ratios. The idea is to order the data so that when the
dataset is traversed the elements of a cell are all in a row.

::

    ------------------------                  -------------------------
    |     |     |     |     |                 |     0     |     1      |
    |  0  |  1  |  2  |  3  |                 -------------------------
    |     |     |     |     | NDCELL encoder  |     2     |     3      |
    -------------------------      ------>    -------------------------
    |     |     |     |     |                 |     4     |     5      |
    |  4  |  5  |  6  |  7  |                 -------------------------
    |     |     |     |     |                 |     6     |     7      |
    -------------------------                 -------------------------

*NDCELL* goes through dataset blocks dividing them into fixed size cells,
so user must specify the parameter meta as the cellshape, so if in a
3-dim dataset user specifies meta = 4, then cellshape will be 4x4x4.

Advantages and disadvantages
------------------------------

The particularity of *NDCELL* is that this filter
considers datasets multidimensionality and takes advantage of it instead
of processing all data as serial.

The main disadvantage of *NDCELL* is that only a few codecs are benefied
by it and only for multidimensional datasets.









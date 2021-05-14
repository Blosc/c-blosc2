NDMEAN: a multidimensional filter for lossy compression
=============================================================================

Given an n-dim array or matrix, *NDMEAN* is a filter based on *NDCELL*
that not only divides data into multidimensional cells so
that a codec compress cell by cell, but also replaces each cell elements by
the cell mean.

Plugin motivation
--------------------

*NDMEAN* was created in order to add another depth level to *NDCELL*, taking
advantage of multidimensionality and making each cell items equal to earn
better compression ratios for the codecs.

Plugin behaviour
-------------------

This filter is meant first to order the data so that when the
dataset is traversed the elements of a cell are all in a row.
Furthermore, *NDMEAN* replaces cell elements by the cell mean.

::

    ------------------------                  -------------------------
    | 1 2 | 1 2 | 1 2 | 1 2 |                 |2 2 2 2 2 2|2 2 2 2 2 2|
    | 3 1 | 3 1 | 3 1 | 3 1 |                 -------------------------
    | 2 3 | 2 3 | 2 3 | 2 3 | NDMEAN encoder  |2 2 2 2 2 2|2 2 2 2 2 2|
    -------------------------      ------>    -------------------------
    | 1 2 | 1 2 | 1 2 | 1 2 |                 |2 2 2 2 2 2|2 2 2 2 2 2|
    | 3 1 | 3 1 | 3 1 | 3 1 |                 -------------------------
    | 2 3 | 2 3 | 2 3 | 2 3 |                 |2 2 2 2 2 2|2 2 2 2 2 2|
    -------------------------                 -------------------------

*NDMEAN* goes through dataset blocks dividing them into fixed size cells,
so user must specify the parameter meta as the cellshape, so if in a
3-dim dataset user specifies meta = 4, then cellshape will be 4x4x4.

Advantages and disadvantages
------------------------------

The particularity of *NDMEAN* is that this filter not only
considers datasets multidimensionality and takes advantage of it instead
of processing all data as serial, but also makes cell data easier to
compress.

The main disadvantage of *NDMEAN* is that it changes the original data and
only a few codecs are benefied by it and only for multidimensional
datasets.









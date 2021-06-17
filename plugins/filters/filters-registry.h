/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

enum {
    BLOSC_FILTER_NDCELL = 32,
    BLOSC_FILTER_NDMEAN = 33,
};

void register_filters();

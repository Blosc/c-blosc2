/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

enum {
    BLOSC_CODEC_NDLZ = 32,
    BLOSC_CODEC_ZFP_FIXED_ACCURACY = 33,
    BLOSC_CODEC_ZFP_FIXED_PRECISION = 34,
    BLOSC_CODEC_ZFP_FIXED_RATE = 35,
};

void register_codecs(void);

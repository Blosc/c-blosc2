/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <blosc-private.h>
#include "blosc2/codecs-registry.h"
#include "ndlz/ndlz.h"
#include "zfp/blosc2-zfp.h"

void register_codecs() {

    blosc2_codec ndlz;
    ndlz.compcode = BLOSC_CODEC_NDLZ;
    ndlz.compver = 1;
    ndlz.complib = BLOSC_CODEC_NDLZ;
    ndlz.encoder = ndlz_compress;
    ndlz.decoder = ndlz_decompress;
    ndlz.compname = "ndlz";
    register_codec_private(&ndlz);

    blosc2_codec zfp;
    zfp.compcode = BLOSC_CODEC_ZFP;
    zfp.compver = 1;
    zfp.complib = BLOSC_CODEC_ZFP;
    zfp.encoder = blosc2_zfp_compress;
    zfp.decoder = blosc2_zfp_decompress;
    zfp.compname = "zfp";
    register_codec_private(&zfp);
}
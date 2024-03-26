/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "blosc2/codecs-registry.h"
#include "ndlz/ndlz.h"
#include "zfp/blosc2-zfp.h"
#include "blosc-private.h"
#include "blosc2.h"

void register_codecs(void) {

  blosc2_codec ndlz;
  ndlz.compcode = BLOSC_CODEC_NDLZ;
  ndlz.version = 1;
  ndlz.complib = BLOSC_CODEC_NDLZ;
  ndlz.encoder = &ndlz_compress;
  ndlz.decoder = &ndlz_decompress;
  ndlz.compname = "ndlz";
  register_codec_private(&ndlz);

  blosc2_codec zfp_acc;
  zfp_acc.compcode = BLOSC_CODEC_ZFP_FIXED_ACCURACY;
  zfp_acc.version = 1;
  zfp_acc.complib = BLOSC_CODEC_ZFP_FIXED_ACCURACY;
  zfp_acc.encoder = &zfp_acc_compress;
  zfp_acc.decoder = &zfp_acc_decompress;
  zfp_acc.compname = "zfp_acc";
  register_codec_private(&zfp_acc);

  blosc2_codec zfp_prec;
  zfp_prec.compcode = BLOSC_CODEC_ZFP_FIXED_PRECISION;
  zfp_prec.version = 1;
  zfp_prec.complib = BLOSC_CODEC_ZFP_FIXED_PRECISION;
  zfp_prec.encoder = &zfp_prec_compress;
  zfp_prec.decoder = &zfp_prec_decompress;
  zfp_prec.compname = "zfp_prec";
  register_codec_private(&zfp_prec);

  blosc2_codec zfp_rate;
  zfp_rate.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  zfp_rate.version = 1;
  zfp_rate.complib = BLOSC_CODEC_ZFP_FIXED_RATE;
  zfp_rate.encoder = &zfp_rate_compress;
  zfp_rate.decoder = &zfp_rate_decompress;
  zfp_rate.compname = "zfp_rate";
  register_codec_private(&zfp_rate);

  blosc2_codec openhtj2k;
  openhtj2k.compcode = BLOSC_CODEC_OPENHTJ2K;
  openhtj2k.version = 1;
  openhtj2k.complib = BLOSC_CODEC_OPENHTJ2K;
  openhtj2k.encoder = NULL;
  openhtj2k.decoder = NULL;
  openhtj2k.compname = "openhtj2k";
  register_codec_private(&openhtj2k);

  blosc2_codec grok;
  grok.compcode = BLOSC_CODEC_GROK;
  grok.version = 1;
  grok.complib = BLOSC_CODEC_GROK;
  grok.encoder = NULL;
  grok.decoder = NULL;
  grok.compname = "grok";
  register_codec_private(&grok);
}

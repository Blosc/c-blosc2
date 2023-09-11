/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_BLOSC2_CODECS_REGISTRY_H
#define BLOSC_BLOSC2_CODECS_REGISTRY_H

#include "blosc2.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BLOSC_CODEC_NDLZ = 32,
    BLOSC_CODEC_ZFP_FIXED_ACCURACY = 33,
    BLOSC_CODEC_ZFP_FIXED_PRECISION = 34,
    BLOSC_CODEC_ZFP_FIXED_RATE = 35,
    BLOSC_CODEC_OPENHTJ2K = 36,
};

void register_codecs(void);

// For dynamically loaded codecs
typedef struct {
    char *encoder;
    char *decoder;
} codec_info;

// Silence unused codec_info typedef warning
static codec_info codec_info_defaults BLOSC_ATTRIBUTE_UNUSED = {0};

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_BLOSC2_CODECS_REGISTRY_H */

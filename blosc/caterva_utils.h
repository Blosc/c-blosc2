/*
 * Copyright (C) 2018-present Francesc Alted, Aleix Alcacer.
 * Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef CATERVA_CATERVA_UTILS_H_
#define CATERVA_CATERVA_UTILS_H_

#include <caterva.h>
#include <../plugins/plugin_utils.h>

#ifdef __cplusplus
extern "C" {
#endif


int caterva_copy_buffer(int8_t ndim,
                        uint8_t itemsize,
                        void *src, const int64_t *src_pad_shape,
                        int64_t *src_start, const int64_t *src_stop,
                        void *dst, const int64_t *dst_pad_shape,
                        int64_t *dst_start);

int create_blosc_params(caterva_ctx_t *ctx,
                        caterva_params_t *params,
                        caterva_storage_t *storage,
                        blosc2_cparams *cparams,
                        blosc2_dparams *dparams,
                        blosc2_storage *b_storage);

int caterva_config_from_schunk(caterva_ctx_t *ctx, blosc2_schunk *sc, caterva_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif  // CATERVA_CATERVA_UTILS_H_

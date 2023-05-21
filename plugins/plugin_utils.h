/*
  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#ifndef BLOSC_PLUGINS_PLUGIN_UTILS_H
#define BLOSC_PLUGINS_PLUGIN_UTILS_H

#include <stdint.h>

int32_t deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape);

#endif /* BLOSC_PLUGINS_PLUGIN_UTILS_H */

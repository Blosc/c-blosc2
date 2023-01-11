/*
  Copyright (C) 2021 The Blosc Developers
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

int32_t deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape);

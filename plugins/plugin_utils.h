/*
  Copyright (C) 2021 The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

void swap_store(void *dest, const void *pa, int size);

int32_t deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape);

void index_unidim_to_multidim(uint8_t ndim, int64_t *shape, int64_t i, int64_t *index);

void index_multidim_to_unidim(const int64_t *index, int8_t ndim, const int64_t *strides, int64_t *i);

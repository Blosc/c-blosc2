/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_BLOSC2_FILTERS_REGISTRY_H
#define BLOSC_BLOSC2_FILTERS_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BLOSC_FILTER_NDCELL = 32,
    BLOSC_FILTER_NDMEAN = 33,
    BLOSC_FILTER_BYTEDELTA_BUGGY = 34, // buggy version. See #524
    BLOSC_FILTER_BYTEDELTA = 35,  // fixed version
};

void register_filters(void);

// For dynamically loaded filters
typedef struct {
    char *forward;
    char *backward;
} filter_info;

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_BLOSC2_FILTERS_REGISTRY_H */

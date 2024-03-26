/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "blosc2/filters-registry.h"
#include "ndmean/ndmean.h"
#include "ndcell/ndcell.h"
#include "bytedelta/bytedelta.h"
#include "int_trunc/int_trunc.h"
#include "blosc-private.h"
#include "blosc2.h"

void register_filters(void) {

  blosc2_filter ndcell;
  ndcell.id = BLOSC_FILTER_NDCELL;
  ndcell.name = "ndcell";
  ndcell.version = 1;
  ndcell.forward = &ndcell_forward;
  ndcell.backward = &ndcell_backward;
  register_filter_private(&ndcell);

  blosc2_filter ndmean;
  ndmean.id = BLOSC_FILTER_NDMEAN;
  ndmean.name = "ndmean";
  ndmean.version = 1;
  ndmean.forward = &ndmean_forward;
  ndmean.backward = &ndmean_backward;
  register_filter_private(&ndmean);

  // Buggy version. See #524
  blosc2_filter bytedelta_buggy;
  bytedelta_buggy.id = BLOSC_FILTER_BYTEDELTA_BUGGY;
  bytedelta_buggy.name = "bytedelta_buggy";
  bytedelta_buggy.version = 1;
  bytedelta_buggy.forward = &bytedelta_forward_buggy;
  bytedelta_buggy.backward = &bytedelta_backward_buggy;
  register_filter_private(&bytedelta_buggy);

  // Fixed version. See #524
  blosc2_filter bytedelta;
  bytedelta.id = BLOSC_FILTER_BYTEDELTA;
  bytedelta.name = "bytedelta";
  bytedelta.version = 1;
  bytedelta.forward = &bytedelta_forward;
  bytedelta.backward = &bytedelta_backward;
  register_filter_private(&bytedelta);

  blosc2_filter int_trunc;
  int_trunc.id = BLOSC_FILTER_INT_TRUNC;
  int_trunc.name = "int_trunc";
  int_trunc.version = 1;
  int_trunc.forward = &int_trunc_forward;
  int_trunc.backward = &int_trunc_backward;
  register_filter_private(&int_trunc);

}

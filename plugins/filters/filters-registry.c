/*
  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org> 
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "blosc2/filters-registry.h"
#include "ndmean/ndmean.h"
#include "ndcell/ndcell.h"
#include "bytedelta/bytedelta.h"
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

  blosc2_filter bytedelta;
  bytedelta.id = BLOSC_FILTER_BYTEDELTA;
  bytedelta.name = "bytedelta";
  bytedelta.forward = &bytedelta_forward;
  bytedelta.backward = &bytedelta_backward;
  register_filter_private(&bytedelta);

}

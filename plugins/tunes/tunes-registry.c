/*
  Copyright (C) 2021 The Blosc Developers
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <blosc-private.h>
#include "blosc2/tunes-registry.h"

void register_tunes(void) {

  blosc2_tune btune;
  btune.id = BLOSC_BTUNE;
  btune.name = "btune";
  btune.init = NULL;
  btune.next_cparams = NULL;
  btune.next_blocksize = NULL;
  btune.update = NULL;
  btune.free = NULL;

  register_tune_private(&btune);
}

/*
  Copyright (C) 2021 The Blosc Developers
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "blosc2/tuners-registry.h"
#include "blosc-private.h"
#include "blosc2.h"

#include <stddef.h>

void register_tuners(void) {

  blosc2_tuner btune;
  btune.id = BLOSC_BTUNE;
  btune.name = "btune";
  btune.init = NULL;
  btune.next_cparams = NULL;
  btune.next_blocksize = NULL;
  btune.update = NULL;
  btune.free = NULL;

  register_tuner_private(&btune);
}

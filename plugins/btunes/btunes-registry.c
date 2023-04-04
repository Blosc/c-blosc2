/*
  Copyright (C) 2021 The Blosc Developers
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <blosc-private.h>
#include "blosc2/btunes-registry.h"

void register_btunes(void) {

  blosc2_btune btune;
  btune.id = 32;
  btune.name = "btune";
  btune.btune_init = NULL;
  btune.btune_next_cparams = NULL;
  btune.btune_next_blocksize = NULL;
  btune.btune_update = NULL;
  btune.btune_free = NULL;

  register_btune_private(&btune);
}

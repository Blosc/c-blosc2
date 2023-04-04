/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

enum {
    BLOSC_BTUNE = 32,

};

void register_btunes(void);

// For dynamically loaded btunes
typedef struct {
    char *btune_init;
    char *btune_next_blocksize;
    char *btune_next_cparams;
    char *btune_update;
    char *btune_free;
}btune_info;

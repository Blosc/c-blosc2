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

void register_tuners(void);

// For dynamically loaded tunes
typedef struct {
    char *init;
    char *next_blocksize;
    char *next_cparams;
    char *update;
    char *free;
} tuner_info;

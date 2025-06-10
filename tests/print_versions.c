/*********************************************************************
  Print versions for Blosc and all its internal compressors.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

 *********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "blosc2.h"


int main(int argc, char* argv[]) {

  char* name = NULL, * version = NULL;
  int ret;

  printf("Blosc version: %s (%s)\n", BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  printf("List of supported compressors in this build: %s\n",
         blosc_list_compressors());

  printf("Supported compression libraries:\n");
  ret = blosc_get_complib_info("blosclz", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  ret = blosc_get_complib_info("lz4", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  ret = blosc_get_complib_info("zlib", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);
  ret = blosc_get_complib_info("zstd", &name, &version);
  if (ret >= 0) printf("  %s: %s\n", name, version);

  return (0);
}

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Broken dynamic plugins for test_plugin_info, built three times: without
  `info` (PLUGIN_INFO_NONE), with an `info` that names no functions
  (PLUGIN_INFO_NULL) and with an `info` that names a function the library
  does not have (PLUGIN_INFO_MISSING).  The same library serves as a
  filter, a codec and a tuner: their `info` structs are all lists of
  function names (2 for filters and codecs, 5 for tuners).

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stddef.h>

#if defined(_WIN32)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

PLUGIN_EXPORT int plugin_info_function(void) {
  return 0;
}

#if defined(PLUGIN_INFO_NULL)
PLUGIN_EXPORT char *info[5] = {NULL};
#elif defined(PLUGIN_INFO_MISSING)
/* The first function (filter forward, codec encoder, tuner init) exists */
PLUGIN_EXPORT char *info[5] = {"plugin_info_function", "plugin_info_missing", "plugin_info_missing",
                               "plugin_info_missing", "plugin_info_missing"};
#endif

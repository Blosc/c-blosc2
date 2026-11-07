/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  A dynamic plugin for test_plugin_threads, built twice: with
  PLUGIN_THREADS_CODEC as a codec (byte run-length coding) and without it as
  a filter (adds 1 to every byte).  c-blosc2 finds its functions through the
  exported `info`.  Loading it takes 50 ms, so that all worker threads of a
  job arrive while it loads.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdint.h>
#include "blosc2.h"

#if defined(_WIN32)
#include <windows.h>
#define PLUGIN_EXPORT __declspec(dllexport)

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)instance;
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    Sleep(50);
  }
  return TRUE;
}
#else
#include <unistd.h>
#define PLUGIN_EXPORT __attribute__((visibility("default")))

__attribute__((constructor)) static void slow_load(void) {
  usleep(50 * 1000);
}
#endif

#if defined(PLUGIN_THREADS_CODEC)
#include "blosc2/codecs-registry.h"

PLUGIN_EXPORT int plugin_threads_encoder(const uint8_t *input, int32_t input_len, uint8_t *output,
                                         int32_t output_len, uint8_t meta, blosc2_cparams *cparams,
                                         const void *chunk) {
  (void)meta;
  (void)cparams;
  (void)chunk;
  int32_t out = 0;
  for (int32_t i = 0; i < input_len;) {
    int32_t run = 1;
    while (i + run < input_len && run < 255 && input[i + run] == input[i]) {
      run++;
    }
    if (out + 2 > output_len) {
      return 0;  /* does not compress */
    }
    output[out++] = (uint8_t)run;
    output[out++] = input[i];
    i += run;
  }
  return out;
}

PLUGIN_EXPORT int plugin_threads_decoder(const uint8_t *input, int32_t input_len, uint8_t *output,
                                         int32_t output_len, uint8_t meta, blosc2_dparams *dparams,
                                         const void *chunk) {
  (void)meta;
  (void)dparams;
  (void)chunk;
  int32_t out = 0;
  for (int32_t i = 0; i + 1 < input_len; i += 2) {
    if (out + input[i] > output_len) {
      return BLOSC2_ERROR_WRITE_BUFFER;
    }
    for (int32_t j = 0; j < input[i]; j++) {
      output[out++] = input[i + 1];
    }
  }
  return out;
}

PLUGIN_EXPORT codec_info info = {"plugin_threads_encoder", "plugin_threads_decoder"};

#else
#include "blosc2/filters-registry.h"

PLUGIN_EXPORT int plugin_threads_forward(const uint8_t *src, uint8_t *dest, int32_t size, uint8_t meta,
                                         blosc2_cparams *cparams, uint8_t id) {
  (void)meta;
  (void)cparams;
  (void)id;
  for (int32_t i = 0; i < size; i++) {
    dest[i] = (uint8_t)(src[i] + 1);
  }
  return BLOSC2_ERROR_SUCCESS;
}

PLUGIN_EXPORT int plugin_threads_backward(const uint8_t *src, uint8_t *dest, int32_t size, uint8_t meta,
                                          blosc2_dparams *dparams, uint8_t id) {
  (void)meta;
  (void)dparams;
  (void)id;
  for (int32_t i = 0; i < size; i++) {
    dest[i] = (uint8_t)(src[i] - 1);
  }
  return BLOSC2_ERROR_SUCCESS;
}

PLUGIN_EXPORT filter_info info = {"plugin_threads_forward", "plugin_threads_backward"};
#endif

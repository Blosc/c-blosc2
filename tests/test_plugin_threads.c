/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Dynamic plugins (codecs and filters registered without functions, like
  grok) are loaded on first use, i.e. by all worker threads of the first job
  at once.  Each library must be loaded once, and every thread must find its
  functions complete: before, each worker loaded it again (one dlopen(), or
  one Python process to find it, per thread) and wrote the function pointers
  while the others read them.  The plugins here (plugin_threads_lib.c) take
  50 ms to load, so that all workers arrive meanwhile; the test counts how
  often each library was opened by its reference count.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#define _DARWIN_C_SOURCE  /* RTLD_NOLOAD on macOS */
#include <stdio.h>
#include <string.h>

#include "test_common.h"

#if defined(_WIN32)
#include <windows.h>
#define LIBRARY_NAME(name) "blosc2_" name ".dll"
#else
#include <dlfcn.h>
#define LIBRARY_NAME(name) "libblosc2_" name ".so"
#endif

#define NTHREADS 8
#define NBYTES (64 * 1024)
#define PLUGIN_ID 244

/* Takes one more reference to the library if it is loaded */
static void *reference_loaded(const char *name) {
#if defined(_WIN32)
  HMODULE handle;
  return GetModuleHandleExA(0, name, &handle) ? (void *)handle : NULL;
#else
  return dlopen(name, RTLD_LAZY | RTLD_NOLOAD);
#endif
}

static void release(void *handle) {
#if defined(_WIN32)
  FreeLibrary((HMODULE)handle);
#else
  dlclose(handle);
#endif
}

/* How often the library was opened (by c-blosc2, which never closes it after
   a successful load); unloads it.  -1 if it cannot be counted. */
static int count_and_unload(const char *name) {
  int count = 0;
  void *handle;
  while ((handle = reference_loaded(name)) != NULL) {
    release(handle);
    release(handle);
    if (++count > 1000) {
      return -1;
    }
  }
  return count;
}

/* Registry entries without functions, as for grok: the libraries are loaded
   on first use.  blosc2_init() empties the registry again. */
static int register_plugins(void) {
  blosc2_codec codec = {PLUGIN_ID, "plugin_threads_codec", PLUGIN_ID, 1, NULL, NULL};
  blosc2_filter filter = {PLUGIN_ID, "plugin_threads_filter", 1, NULL, NULL};
  return (blosc2_register_codec(&codec) < 0 || blosc2_register_filter(&filter) < 0) ? -1 : 0;
}

static int check_loaded_once(const char *when) {
  int ncodec = count_and_unload(LIBRARY_NAME("plugin_threads_codec"));
  int nfilter = count_and_unload(LIBRARY_NAME("plugin_threads_filter"));
  if (ncodec == -1 || nfilter == -1) {
    printf("%s: cannot count the loads of the plugin libraries, not checked\n", when);
    return 0;
  }
  if (ncodec != 1 || nfilter != 1) {
    printf("%s: codec library loaded %d times, filter library %d times (instead of once)\n", when, ncodec,
           nfilter);
    return 1;
  }
  return 0;
}

int main(void) {
  uint8_t *src = malloc(NBYTES);
  uint8_t *dest = malloc(NBYTES);
  uint8_t *chunk = malloc(NBYTES + BLOSC2_MAX_OVERHEAD);
  int failed = 0;
  for (int i = 0; i < NBYTES; i++) {
    src[i] = (uint8_t)(i / 300);  /* runs, for the run-length codec */
  }

  /* Compression: its worker threads load both plugins */
  blosc2_init();
  if (register_plugins() < 0) {
    printf("Cannot register the plugins\n");
    return 1;
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = PLUGIN_ID;
  cparams.typesize = 1;
  cparams.nthreads = NTHREADS;
  cparams.blocksize = 1024;  /* 64 blocks */
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = PLUGIN_ID;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int cbytes = blosc2_compress_ctx(cctx, src, NBYTES, chunk, NBYTES + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  if (cbytes <= 0 || cbytes >= NBYTES / 2) {
    printf("Compression with the plugins failed: %d\n", cbytes);
    return 1;
  }
  failed |= check_loaded_once("compression");

  /* Decompression, with new registry entries: its worker threads load them */
  blosc2_destroy();
  blosc2_init();
  if (register_plugins() < 0) {
    printf("Cannot register the plugins again\n");
    return 1;
  }
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  int dbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, dest, NBYTES);
  blosc2_free_ctx(dctx);
  if (dbytes != NBYTES || memcmp(src, dest, NBYTES) != 0) {
    printf("Decompression with the plugins failed: %d\n", dbytes);
    return 1;
  }
  failed |= check_loaded_once("decompression");
  blosc2_destroy();

  free(src);
  free(dest);
  free(chunk);
  if (failed) {
    printf("test_plugin_threads FAILED\n");
    return 1;
  }
  printf("test_plugin_threads: the worker threads loaded each plugin once: OK\n");
  return 0;
}

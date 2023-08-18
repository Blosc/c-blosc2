/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef BLOSC_BLOSC_PRIVATE_H
#define BLOSC_BLOSC_PRIVATE_H

#include "blosc2/blosc2-common.h"
#include "blosc2.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

/*********************************************************************

  Utility functions meant to be used internally.

*********************************************************************/

#define to_little(dest, src, itemsize)    endian_handler(true, dest, src, itemsize)
#define from_little(dest, src, itemsize)  endian_handler(true, dest, src, itemsize)
#define to_big(dest, src, itemsize)       endian_handler(false, dest, src, itemsize)
#define from_big(dest, src, itemsize)     endian_handler(false, dest, src, itemsize)


// Return true if platform is little endian; else false
static bool is_little_endian(void) {
  static const int i = 1;
  char* p = (char*)&i;

  if (p[0] == 1) {
    return true;
  }
  else {
    return false;
  }
}


static inline void endian_handler(bool little, void *dest, const void *pa, int size)
{
  bool little_endian = is_little_endian();
  if (little_endian == little) {
    memcpy(dest, pa, size);
  }
  else {
    uint8_t* pa_ = (uint8_t*)pa;
    uint8_t pa2_[8];
    switch (size) {
      case 8:
        pa2_[0] = pa_[7];
        pa2_[1] = pa_[6];
        pa2_[2] = pa_[5];
        pa2_[3] = pa_[4];
        pa2_[4] = pa_[3];
        pa2_[5] = pa_[2];
        pa2_[6] = pa_[1];
        pa2_[7] = pa_[0];
        break;
      case 4:
        pa2_[0] = pa_[3];
        pa2_[1] = pa_[2];
        pa2_[2] = pa_[1];
        pa2_[3] = pa_[0];
        break;
      case 2:
        pa2_[0] = pa_[1];
        pa2_[1] = pa_[0];
        break;
      case 1:
        pa2_[0] = pa_[0];
        break;
      default:
        BLOSC_TRACE_ERROR("Unhandled size: %d.", size);
    }
    memcpy(dest, pa2_, size);
  }
}

/* Copy 4 bytes from @p *pa to int32_t, changing endianness if necessary. */
static inline int32_t sw32_(const void* pa) {
  int32_t idest;

  bool little_endian = is_little_endian();
  if (little_endian) {
    idest = *(int32_t *)pa;
  }
  else {
#if defined (__GNUC__)
    return __builtin_bswap32(*(unsigned int *)pa);
#elif defined (_MSC_VER) /* Visual Studio */
    return _byteswap_ulong(*(unsigned int *)pa);
#else
    uint8_t *dest = (uint8_t *)&idest;
    dest[0] = pa_[3];
    dest[1] = pa_[2];
    dest[2] = pa_[1];
    dest[3] = pa_[0];
#endif
  }
  return idest;
}

/* Copy 4 bytes from int32_t to @p *dest, changing endianness if necessary. */
static inline void _sw32(void* dest, int32_t a) {
  uint8_t* dest_ = (uint8_t*)dest;
  uint8_t* pa = (uint8_t*)&a;

  bool little_endian = is_little_endian();
  if (little_endian) {
    *(int32_t *)dest_ = a;
  }
  else {
#if defined (__GNUC__)
    *(int32_t *)dest_ = __builtin_bswap32(*(unsigned int *)pa);
#elif defined (_MSC_VER) /* Visual Studio */
    *(int32_t *)dest_ = _byteswap_ulong(*(unsigned int *)pa);
#else
    dest_[0] = pa[3];
    dest_[1] = pa[2];
    dest_[2] = pa[1];
    dest_[3] = pa[0];
#endif
  }
}

/* Reverse swap bits in a 32-bit integer */
static inline int32_t bswap32_(int32_t a) {
#if defined (__GNUC__)
  return __builtin_bswap32(a);

#elif defined (_MSC_VER) /* Visual Studio */
  return _byteswap_ulong(a);
#else
  a = ((a & 0x000000FF) << 24) |
      ((a & 0x0000FF00) <<  8) |
      ((a & 0x00FF0000) >>  8) |
      ((a & 0xFF000000) >> 24);
  return a;
#endif
}

/**
 * @brief Register a filter in Blosc.
 *
 * @param filter The filter to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_filter_private(blosc2_filter *filter);

/**
 * @brief Register a codec in Blosc.
 *
 * @param codec The codec to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_codec_private(blosc2_codec *codec);


/**
 * @brief Register a tune in Blosc.
 *
 * @param tune The tune to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_tuner_private(blosc2_tuner *tuner);

int fill_tuner(blosc2_tuner *tuner);

extern blosc2_tuner g_tuners[256];
extern int g_ntuners;


#if defined(_WIN32)
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#define RTLD_LAZY   0x000
#define popen _popen
#define pclose _pclose

static struct {
    long lasterror;
    const char *err_rutin;
}
var = {
    0,
    NULL
};

static inline void *dlopen (const char *filename, int flags) {
  HINSTANCE hInst;
  hInst = LoadLibrary(filename);
  if (hInst==NULL) {
    var.lasterror = GetLastError();
    var.err_rutin = "dlopen";
  }

  return hInst;
}

static inline void *dlsym(void *handle, const char *name) {
  FARPROC fp;
  fp = GetProcAddress((HINSTANCE)handle, name);
  if (!fp) {
    var.lasterror = GetLastError ();
    var.err_rutin = "dlsym";
  }
  return (void *)(intptr_t)fp;
}

static inline int dlclose(void *handle) {
  bool ok = FreeLibrary((HINSTANCE)handle);
  if (!ok) {
    var.lasterror = GetLastError();
    var.err_rutin = "dlclose";
    return BLOSC2_ERROR_FAILURE;
  }
  return BLOSC2_ERROR_SUCCESS;
}

static inline const char *dlerror (void) {
  static char errstr [88];
  if (var.lasterror) {
      sprintf (errstr, "%s error #%ld", var.err_rutin, var.lasterror);
      return errstr;
  } else {
      return NULL;
  }
}
#else
#include <dlfcn.h>
#endif


static inline void* load_lib(char *plugin_name, char *libpath) {
  char python_cmd[PATH_MAX] = {0};
  sprintf(python_cmd, "python -c \"import blosc2_%s; blosc2_%s.print_libpath()\"", plugin_name, plugin_name);
  FILE *fp = popen(python_cmd, "r");
  if (fp == NULL) {
    BLOSC_TRACE_ERROR("Could not run python");
    return NULL;
  }
  if (fgets(libpath, PATH_MAX, fp) == NULL) {
    BLOSC_TRACE_ERROR("Could not read python output");
    pclose(fp);
    return NULL;
  }
  pclose(fp);
  if (strlen(libpath) == 0) {
    BLOSC_TRACE_ERROR("Could not find plugin libpath");
    return NULL;
  }
  void* loaded_lib;
  BLOSC_TRACE_INFO("libpath for plugin blosc2_%s: %s\n", plugin_name, libpath);
  loaded_lib = dlopen(libpath, RTLD_LAZY);
  if (loaded_lib == NULL) {
    BLOSC_TRACE_ERROR("Attempt to load plugin in path '%s' failed with error: %s",
                      libpath, dlerror());
  }
  return loaded_lib;
}

#endif /* BLOSC_BLOSC_PRIVATE_H */

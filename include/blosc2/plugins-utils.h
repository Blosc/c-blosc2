/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#if defined(_WIN32)
#include <windows.h>
#define PATH_MAX MAX_PATH
#define RTLD_LAZY   0x000
#define popen _popen
#define pclose _pclose

void *dlopen (const char *filename, int flags) {
  HINSTANCE hInst;
  hInst = LoadLibrary(filename);

  return hInst;
}

void *dlsym(void *handle, const char *name) {
  FARPROC fp;
  fp = GetProcAddress ((HINSTANCE)handle, name);
  return (void *)(intptr_t)fp;
}

void dlclose(void *handle) {
  FreeLibrary ((HINSTANCE)handle);
}
#else
#include <dlfcn.h>
#endif


static inline void* load_lib(char *plugin_name, char *path) {
  char python_path[PATH_MAX] = {0};
  FILE *fp = popen("python -c \"exec(\\\"import sys\\npaths=sys.path\\nfor p in paths:\\n\\tif 'site-packages' in p:"
                   "\\n \\t\\tprint(p+'/', end='')\\n \\t\\tbreak\\\")\"", "r");
  fgets(python_path, PATH_MAX, fp);
  pclose(fp);

  if (strlen(python_path) == 0) {
    BLOSC_TRACE_ERROR("Could not find python path");
    return NULL;
  }
#if defined(_WIN32)
    sprintf(path, "%s%s/lib%s.dll", python_path, plugin_name, plugin_name);
#else
  sprintf(path, "%sblosc2_%s/libblosc2_%s.so", python_path, plugin_name, plugin_name);
  void* loaded_lib = dlopen(path, RTLD_LAZY);
  if (loaded_lib != NULL) {
    return loaded_lib;
  }
  sprintf(path, "%sblosc2_%s/libblosc2_%s.dylib", python_path, plugin_name, plugin_name);
#endif

  return dlopen(path, RTLD_LAZY);
}

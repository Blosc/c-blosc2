/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include <blosc2.h>

#if defined(_WIN32)
#include <windows.h>
#define PATH_MAX MAX_PATH
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

void *dlopen (const char *filename, int flags) {
  HINSTANCE hInst;
  hInst = LoadLibrary(filename);
  if (hInst==NULL) {
    var.lasterror = GetLastError();
    var.err_rutin = "dlopen";
  }

  return hInst;
}

void *dlsym(void *handle, const char *name) {
  FARPROC fp;
  fp = GetProcAddress((HINSTANCE)handle, name);
  if (!fp) {
    var.lasterror = GetLastError ();
    var.err_rutin = "dlsym";
  }
  return (void *)(intptr_t)fp;
}

int dlclose(void *handle) {
  bool ok = FreeLibrary((HINSTANCE)handle);
  if (!ok) {
    var.lasterror = GetLastError();
    var.err_rutin = "dlclose";
    return BLOSC2_ERROR_FAILURE;
  }
  return BLOSC2_ERROR_SUCCESS;
}

const char *dlerror (void) {
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


static inline void* load_lib(char *plugin_name, char *path) {
  char python_cmd[PATH_MAX] = {0};
  char python_path[PATH_MAX] = {0};
  sprintf(python_cmd, "python -c \"import blosc2_%s; print(blosc2_%s.__path__[0], end='')\"", plugin_name, plugin_name);
  FILE *fp = popen(python_cmd, "r");
  if (fp == NULL) {
    BLOSC_TRACE_ERROR("Could not run python");
    return NULL;
  }
  if (fgets(python_path, PATH_MAX, fp) == NULL) {
    BLOSC_TRACE_ERROR("Could not read python output");
    return NULL;
  }  BLOSC_TRACE_WARNING("python path for plugin blosc2_%s: %s\n", plugin_name, python_path);
  pclose(fp);

  if (strlen(python_path) == 0) {
    BLOSC_TRACE_ERROR("Could not find python path");
    return NULL;
  }
  void* loaded_lib;
#if defined(_WIN32)
    sprintf(path, "%s/libblosc2_%s.dll", python_path, plugin_name);
#else
  sprintf(path, "%s/libblosc2_%s.so", python_path, plugin_name);
  BLOSC_TRACE_WARNING("Trying first path: %s\n", path);
  loaded_lib = dlopen(path, RTLD_LAZY);
  if (loaded_lib != NULL) {
    return loaded_lib;
  }
#endif
  BLOSC_TRACE_WARNING("First attempt loading library %s. Trying 2nd path", dlerror());

  sprintf(path, "%s/libblosc2_%s.dylib", python_path, plugin_name);
  BLOSC_TRACE_WARNING("Trying second path: %s\n", path);
  loaded_lib = dlopen(path, RTLD_LAZY);
  if (loaded_lib == NULL) {
    BLOSC_TRACE_ERROR("Second attempt loading library %s", dlerror());
  }
  return loaded_lib;
}

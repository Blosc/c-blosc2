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
    return NULL;
  }
  pclose(fp);
  if (strlen(libpath) == 0) {
    BLOSC_TRACE_ERROR("Could not find plugin libpath");
    return NULL;
  }
  void* loaded_lib;
  BLOSC_TRACE_WARNING("libpath for plugin blosc2_%s: %s\n", plugin_name, libpath);
  loaded_lib = dlopen(libpath, RTLD_LAZY);
  if (loaded_lib == NULL) {
    BLOSC_TRACE_ERROR("Attempt to load plugin in path '%s' failed with error: %s",
                      libpath, dlerror());
  }
  return loaded_lib;
}

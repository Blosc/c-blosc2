/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include "blosc2.h"

#if defined(_WIN32)
  #include <windows.h>
  #include <malloc.h>
  #include <io.h>

  int blosc2_remove_dir(const char* dir_path) {
    char last_char = dir_path[strlen(dir_path) - 1];
    char* path;
    if (last_char != '\\' || last_char != '/') {
      path = malloc(strlen(dir_path) + 2 + 1);
      sprintf(path, "%s\\*", dir_path);
    }
    else {
      path = malloc(strlen(dir_path) + 1 + 1);
      strcpy(path, dir_path);
      strcat(path, "*");
    }
    char* fname;
    struct _finddata_t cfile;

    intptr_t file = _findfirst(path, &cfile);

    if (file == -1) {
      BLOSC_TRACE_ERROR("Could not open the file.");
      return -1;
    }
    int ret;

    while ( _findnext(file, &cfile) == 0) {
      if (strcmp(".", cfile.name) == 0 || strcmp("..", cfile.name) == 0) {
        continue;
      }
      fname = malloc(strlen(dir_path) + 1 + strlen(cfile.name) + 1);
      sprintf(fname, "%s\\%s", dir_path, cfile.name);

      ret = remove(fname);
      free(fname);
      if (ret < 0) {
        BLOSC_TRACE_ERROR("Could not remove file %s", fname);
        free(path);
        _findclose(file);
        return -1;
      }
    }

    rmdir(dir_path);
    free(path);
    _findclose(file);
    return 0;
  }

#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <dirent.h>

/* Function needed for removing each time the directory */
int blosc2_remove_dir(const char* dir_path) {
  char last_char = dir_path[strlen(dir_path) - 1];
  char* path;
  if (last_char != '\\' || last_char != '/'){
    path = malloc(strlen(dir_path) + 1 + 1);
    sprintf(path, "%s/", dir_path);
  }
  else {
    path = malloc(strlen(dir_path) + 1);
    strcpy(path, dir_path);
  }

  DIR* dr = opendir(path);
  struct stat statbuf;
  if (dr == NULL) {
    BLOSC_TRACE_ERROR("No file or directory found.");
    free(path);
    return -1;
  }
  struct dirent *de;
  int ret;
  char* fname;
  while ((de = readdir(dr)) != NULL) {
    fname = malloc(strlen(path) + strlen(de->d_name) + 1);
    sprintf(fname, "%s%s", path, de->d_name);
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
      free(fname);
      continue;
    }
    if (!stat(fname, &statbuf)) {
      ret = unlink(fname);
      if (ret < 0) {
        BLOSC_TRACE_ERROR("Could not remove file %s", fname);
        free(fname);
        closedir(dr);
        free(path);
        return -1;
      }
    }
    free(fname);
  }
  closedir(dr);
  rmdir(path);
  free(path);
  return 0;
}

#endif  /* _WIN32 */





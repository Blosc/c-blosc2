/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc2.h"

#include <sys/stat.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#if defined(_WIN32) || defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>
  #include <io.h>
  #include <direct.h>   /* <- add this for _rmdir */

  int blosc2_remove_dir(const char* dir_path) {
    char* path;
    if (dir_path == NULL || dir_path[0] == '\0') {
      BLOSC_TRACE_ERROR("Invalid directory path");
      return BLOSC2_ERROR_INVALID_PARAM;
    }
    size_t dir_len = strlen(dir_path);
    char last_char = dir_path[dir_len - 1];
    size_t path_len = 0;
    if (last_char != '\\' && last_char != '/') {
      if (dir_len > SIZE_MAX - 3) {
        BLOSC_TRACE_ERROR("Directory path is too long");
        return BLOSC2_ERROR_INVALID_PARAM;
      }
      path_len = dir_len + 3;
      path = malloc(path_len);
      if (path == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
      }
      snprintf(path, path_len, "%s\\*", dir_path);
    }
    else {
      if (dir_len > SIZE_MAX - 2) {
        BLOSC_TRACE_ERROR("Directory path is too long");
        return BLOSC2_ERROR_INVALID_PARAM;
      }
      path_len = dir_len + 2;
      path = malloc(path_len);
      if (path == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
      }
      snprintf(path, path_len, "%s*", dir_path);
    }
    char* fname;
    struct _finddata_t cfile;

    intptr_t file = _findfirst(path, &cfile);
    free(path);

    if (file == -1) {
      BLOSC_TRACE_ERROR("Could not open the file.");
      return BLOSC2_ERROR_FILE_OPEN;
    }
    int ret;

    while ( _findnext(file, &cfile) == 0) {
      if (strcmp(".", cfile.name) == 0 || strcmp("..", cfile.name) == 0) {
        continue;
      }
      size_t name_len = strlen(cfile.name);
      if (dir_len > SIZE_MAX - name_len - 2) {
        BLOSC_TRACE_ERROR("File path is too long");
        _findclose(file);
        return BLOSC2_ERROR_INVALID_PARAM;
      }
      fname = malloc(dir_len + name_len + 2);
      if (fname == NULL) {
        _findclose(file);
        return BLOSC2_ERROR_MEMORY_ALLOC;
      }
      snprintf(fname, dir_len + name_len + 2, "%s\\%s", dir_path, cfile.name);

      ret = remove(fname);
      if (ret < 0) {
        BLOSC_TRACE_ERROR("Could not remove file %s", fname);
        free(fname);
        _findclose(file);
        return BLOSC2_ERROR_FAILURE;
      }
      free(fname);
    }

    /* remove the directory */
    if (_rmdir(dir_path) != 0) {
      BLOSC_TRACE_ERROR("Could not remove directory %s (errno=%d)", dir_path, errno);
      _findclose(file);
      return BLOSC2_ERROR_FAILURE;
    }
    _findclose(file);
    return BLOSC2_ERROR_SUCCESS;
  }

#else
  #include <dirent.h>
  #include <unistd.h>

/* Return the directory path with the '/' at the end */
char* blosc2_normalize_dirpath(const char* dir_path) {
  if (dir_path == NULL || dir_path[0] == '\0') {
    errno = EINVAL;
    return NULL;
  }
  size_t dir_len = strlen(dir_path);
  if (dir_len == 0) {
    errno = EINVAL;
    return NULL;
  }
  char last_char = dir_path[dir_len - 1];
  char* path;
  if (last_char != '\\' && last_char != '/') {
    if (dir_len > SIZE_MAX - 2) {
      BLOSC_TRACE_ERROR("Directory path is too long");
      return NULL;
    }
    path = malloc(dir_len + 2);
    if (path == NULL) {
      return NULL;
    }
    snprintf(path, dir_len + 2, "%s/", dir_path);
  }
  else {
    if (dir_len > SIZE_MAX - 1) {
      BLOSC_TRACE_ERROR("Directory path is too long");
      return NULL;
    }
    path = malloc(dir_len + 1);
    if (path == NULL) {
      return NULL;
    }
    snprintf(path, dir_len + 1, "%s", dir_path);
  }
  return path;
}

/* Function needed for removing each time the directory */
int blosc2_remove_dir(const char* dir_path) {
  char* path = blosc2_normalize_dirpath(dir_path);
  if (path == NULL) {
    if (errno == ENOMEM) {
      return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  DIR* dr = opendir(path);
  struct stat statbuf;
  if (dr == NULL) {
    BLOSC_TRACE_ERROR("No file or directory found.");
    free(path);
    return BLOSC2_ERROR_NOT_FOUND;
  }
  struct dirent *de;
  int ret;
  char* fname;
  while ((de = readdir(dr)) != NULL) {
    size_t path_len = strlen(path);
    size_t name_len = strlen(de->d_name);
    if (path_len > SIZE_MAX - name_len - 1) {
      BLOSC_TRACE_ERROR("File path is too long");
      closedir(dr);
      free(path);
      return BLOSC2_ERROR_INVALID_PARAM;
    }
    fname = malloc(path_len + name_len + 1);
    if (fname == NULL) {
      closedir(dr);
      free(path);
      return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    snprintf(fname, path_len + name_len + 1, "%s%s", path, de->d_name);
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
        return BLOSC2_ERROR_FAILURE;
      }
    }
    free(fname);
  }
  closedir(dr);
  rmdir(path);
  free(path);
  return BLOSC2_ERROR_SUCCESS;
}

#endif  /* _WIN32 */

int blosc2_remove_urlpath(const char* urlpath){
  if (urlpath != NULL) {
    struct stat statbuf;
    if (stat(urlpath, &statbuf) != 0){
      if (errno == ENOENT) {
        // Path does not exist
        return BLOSC2_ERROR_SUCCESS;
      }
      BLOSC_TRACE_ERROR("Could not access %s", urlpath);
      return BLOSC2_ERROR_FAILURE;
    }
    if ((statbuf.st_mode & S_IFDIR) != 0) {
      return blosc2_remove_dir(urlpath);
    }
    if (remove(urlpath) < 0) {
      BLOSC_TRACE_ERROR("Could not remove %s", urlpath);
      return BLOSC2_ERROR_FILE_REMOVE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}

int blosc2_rename_urlpath(char* old_urlpath, char* new_urlpath){
  if (old_urlpath != NULL && new_urlpath != NULL) {
    struct stat statbuf;
    if (stat(old_urlpath, &statbuf) != 0) {
      BLOSC_TRACE_ERROR("Could not access %s", old_urlpath);
      return BLOSC2_ERROR_FAILURE;
    }
    int ret = rename(old_urlpath, new_urlpath);
    if (ret < 0) {
      BLOSC_TRACE_ERROR("Could not rename %s to %s", old_urlpath, new_urlpath);
      return BLOSC2_ERROR_FAILURE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}

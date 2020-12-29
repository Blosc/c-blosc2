/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "blosc2.h"


/* Function needed for removing each time the directory */
int remove_dir(const char *dir_path) {
  char last_char = dir_path[strlen(dir_path) - 1];
  char* path = malloc(strlen(dir_path) + 1);
  strcpy(path, dir_path);
  if (last_char != '\\' || last_char != '/'){
    free(path);
    path = malloc(strlen(dir_path) + 1 + 1);
    sprintf(path, "%s/", dir_path);
  }

  DIR *dr = opendir(path);
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
    sprintf(fname,"%s%s",path,de->d_name);
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

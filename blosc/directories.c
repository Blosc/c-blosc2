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
int remove_dir(const char *path) {
  DIR *dr = opendir(path);
  struct stat statbuf;
  if (dr == NULL) {
    BLOSC_TRACE_ERROR("No file or directory found.");
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
        return -1;
      }
    }
    free(fname);
  }
  closedir(dr);
  rmdir(path);
  return 0;
}

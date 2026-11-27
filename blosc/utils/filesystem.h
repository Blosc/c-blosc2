/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/
#ifndef BLOSC_FILESYSTEM_H
#define BLOSC_FILESYSTEM_H

#include <blosc2.h>

#if defined(_WIN32) || defined(__MINGW32__)
#if defined(__MINGW32__)
#include <dirent.h>
#include <libgen.h>
#endif
#include <stdlib.h>
#include <direct.h>
#include <io.h>
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#define STAT(filename, buf) _stat(filename, buf)
#define GETPID() _getpid()
#define OPEN(path) _open((path), _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE)
#define FSYNC(fd) _commit(fd)
#define CLOSE(fd) _close(fd)
#else
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#define MKDIR(path) mkdir(path, 0700)
#define STAT(filename, buf) stat(filename, buf)
#define GETPID() getpid()
#define OPEN(path) open((path), O_RDWR | O_CREAT | O_EXCL )
#define FSYNC(fd) fsync(fd)
#define CLOSE(fd) close(fd);
#endif

int fs_mkdir(const char* dir_path);
int fs_path_join(const char* base, const char* relative, char** out);
int fs_create_tmp_file(const char* path, char** out);
int fs_create_tmp_dir(char** ptmpdir_path);
int fs_remove_dir_recursive(const char* dir_path);
int fs_create_dir_recursive(const char* dir_path);
int fs_create_dir_recursive_check_extension_paths(const char* dir_path);

int fs_sync_parent_dir(const char* path);
int fs_atomic_schunk_write(const char* path, blosc2_schunk* schunk);

int fs_realpath(const char* path, char** out_path);

#endif //BLOSC_FILESYSTEM_H

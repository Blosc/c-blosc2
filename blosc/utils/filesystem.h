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
#include <dirent.h>


//char* blosc2_normalize_dirpath(const char* dir_path);
int fs_mkdir(const char* dir_path);
int fs_create_tmp_dir(char** ptmpdir_path);
int fs_remove_dir_recursive(const char* dir_path);
int fs_create_dir_recursive(const char* dir_path);
int fs_create_dir_recursive_check_extension_paths(const char* dir_path);

int fs_sync_parent_dir(const char* path);
int fs_atomic_schunk_write(const char* path, blosc2_schunk* schunk, bool create_tmp);

int fs_realpath(const char* path, char** out_path);

#endif //BLOSC_FILESYSTEM_H

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
int blosc2_mkdir(const char* dir_path);
int blosc2_create_tmp_dir(char** ptmpdir_path);
int blosc2_remove_dir_recursive(const char* dir_path);
int blosc2_create_dir_recursive(const char* dir_path);
int blosc2_create_dir_recursive_check_extension_paths(const char* dir_path);

int blosc2_sync_parent_dir(const char* path);
int blosc2_atomic_schunk_write(const char* path, blosc2_schunk* schunk);

int blosc2_path_verify_rel(const char* path);
const char* blosc2_path_join(const char* p, const char* q);
int blosc2_path_atomic_replace(const char* tmp, const char* dest); /* Atomically replaces two files */
int blosc2_realpath(const char* path, char** out_path);

#endif //BLOSC_FILESYSTEM_H

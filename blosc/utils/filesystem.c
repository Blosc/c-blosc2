/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "filesystem.h"

#include <blosc2.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>

#include <string_manipulation.h>

#if defined(_WIN32) || defined (__MINGW32__)
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#define STAT(filename, buf) _stat(filename, buf)
typedef struct _stat struct stat;
#else
#define MKDIR(path) mkdir(path, 0700)
#define STAT(filename, buf) stat(filename, buf)
#endif

int blosc2_mkdir(const char* path) {
    if (path == NULL) {
        BLOSC_TRACE_ERROR("Path cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (path[0] == '\0') {
        BLOSC_TRACE_ERROR("Path cannot be equal to the empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (MKDIR(path) != 0) {
        int err = errno;
        if (err == EEXIST) {
            BLOSC_TRACE_ERROR("%s already exists in the filesystem", path);
        }
        return BLOSC2_ERROR_FAILURE;
    }
    return BLOSC2_ERROR_SUCCESS;
}

static char* blosc2_normalize_dirpath(const char* dir_path) {
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

static int s_remove_dir_recursive(const char* dir_path) {
    const char* path = blosc2_normalize_dirpath(dir_path);
    if (path == NULL) {
        if (errno == ENOMEM) {
            return BLOSC2_ERROR_MEMORY_ALLOC;
        }
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    DIR* dr = opendir(path);
    struct stat statbuf;
    if (dr == NULL) {
        BLOSC_TRACE_ERROR("No file or directory found at %s", path);
        free((void*)path);
        return BLOSC2_ERROR_NOT_FOUND;
    }
    struct dirent *de;
    int ret;
    char* fname;
    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        size_t path_len = strlen(path);
        size_t name_len = strlen(de->d_name);
        size_t full_path_len = path_len + name_len;
        if (path_len > SIZE_MAX - name_len - 1) {
            BLOSC_TRACE_ERROR("File path is too long");
            closedir(dr);
            free((void*)path);
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        fname = malloc(full_path_len + 1);
        if (fname == NULL) {
            BLOSC_TRACE_ERROR("Could not allocate memory to remove recursively %s", dir_path);
            closedir(dr);
            free((void*)path);
            return BLOSC2_ERROR_MEMORY_ALLOC;
        }
        snprintf(fname, full_path_len + 1, "%s%s", path, de->d_name);
        if (!stat(fname, &statbuf)) {

            if (S_ISDIR(statbuf.st_mode)) {
                // Recursive descent
                s_remove_dir_recursive(fname);
            } else {
                ret = remove(fname);
                if (ret < 0) {
                    int err = errno;
                    switch (err) {
                    case ENOENT:
                        BLOSC_TRACE_ERROR("Could not find route %s", (const char*)fname);
                        break;
                    case EACCES:
                        BLOSC_TRACE_ERROR("Write permission was unexpectedly revoked or is being used by another process at %s", (const char*)fname);
                    case ENAMETOOLONG:
                        BLOSC_TRACE_ERROR("Filename %s was too long", (const char*)fname);
                    default:
                        BLOSC_TRACE_ERROR("Could not remove file %s", fname);
                    }
                    free(fname);
                    free((void*)path);
                    closedir(dr);
                    return BLOSC2_ERROR_FAILURE;
                }
            }
        }
        free(fname);
    }
    closedir(dr);
    rmdir(path);
    free((void*)path);
    return BLOSC2_ERROR_SUCCESS;

}

int blosc2_remove_dir_recursive(const char* dir_path) {
    if (dir_path == NULL) {
        BLOSC_TRACE_ERROR("dir_path cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (dir_path[0] == '\0') {
        BLOSC_TRACE_ERROR("dir_path cannot be an empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    return s_remove_dir_recursive(dir_path);
}

int blosc2_create_dir_recursive(const char* dir_path) {
    if (dir_path == NULL) {
        BLOSC_TRACE_ERROR("dir_path cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (dir_path[0] == '\0') {
        BLOSC_TRACE_ERROR("dir_path cannot be an empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    size_t dir_path_size = strlen(dir_path) + 1;
    if (dir_path_size >= PATH_MAX) {
        BLOSC_TRACE_ERROR("Fiel ");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    char tmp[PATH_MAX];
    memcpy(tmp, dir_path, dir_path_size);

    const char* ptr_last_slash = strrchr(tmp, '/');
    char* ptr_iter = tmp;
    struct stat st = {0};

    while (ptr_iter <= ptr_last_slash) {
        if (*ptr_iter == '/') {
            *ptr_iter = '\0';

            if (stat(tmp, &st) != 0) {
                if (errno == ENOENT) {
                    mkdir(tmp, 0700);
                } else {
                    BLOSC_TRACE_ERROR("Could not create directory");
                    return BLOSC2_ERROR_FILE_OPEN;
                }
            }

            *ptr_iter = '/';
        }
        ptr_iter += 1;
    }
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_create_dir_recursive_check_extension_paths(const char* dir_path) {
    if (dir_path == NULL) {
        BLOSC_TRACE_ERROR("dir_path cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (dir_path[0] == '\0') {
        BLOSC_TRACE_ERROR("dir_path cannot be an empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    uint8_t tmp[PATH_MAX];
    memcpy(tmp, dir_path, strlen((const char*)dir_path) + 1);

    uint8_t* ptr_last_slash = (uint8_t*) strrchr((const char*)tmp, '/');
    uint8_t* ptr_last = tmp;

    struct stat st = {0};
    bool abort = false;
    while (ptr_last <= ptr_last_slash && !abort) {

        if (*ptr_last == '/') {
            *ptr_last = '\0';

            // Check extension
            if (strrchr(tmp, '.') != NULL) {
                abort = true;
            } else if (stat((const char*)tmp, &st) != 0) {
                if (errno == ENOENT) {
                    mkdir((const char*)tmp, 0700);
                } else {
                    BLOSC_TRACE_ERROR("Could not create directory");
                    return BLOSC2_ERROR_FILE_OPEN;
                }
            }

            *ptr_last = '/';
            if (abort) break;
        }

        ptr_last += 1;
    }

    // Abort resets the directories created
    if (abort) {

        const uint8_t* first_slash = (const uint8_t*) strchr((const char*)tmp, '/');
        while (ptr_last > first_slash) {
            if (*ptr_last == '/') {
                *ptr_last = '\0';
                remove((const char*)tmp);
                *ptr_last = '/';
            }

            ptr_last -= 1;
        }

        return BLOSC2_ERROR_INVALID_PARAM;
    }

    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_create_tmp_dir(char** ptmpdir_path) {
    if (ptmpdir_path == NULL) {
        BLOSC_TRACE_ERROR("ptmpdir_path cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *ptmpdir_path = NULL;

    const char* template = ".b2.XXXXXX";
    size_t template_length = strlen(template);
    char* path = malloc(template_length + 1);
    if (path == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate buffer for the creation of a temporal directory");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(path, template, template_length + 1);

    if (mkdtemp(path) == NULL) {
        int err = errno;
        free(path);
        switch (err) {
        case ENOSPC:
        case EDQUOT: {
            BLOSC_TRACE_ERROR("There is no space left to create the directory");
            return BLOSC2_ERROR_FILE_WRITE;
        }
        default: {
            BLOSC_TRACE_ERROR("Unexpected error, could not create temporal directory");
            return BLOSC2_ERROR_FAILURE;
        }
        }
    }

    char* abs_path = realpath(path, NULL);
    if (abs_path == NULL) {
        int err = errno;
        rmdir(path);
        free(path);
        BLOSC_TRACE_ERROR("Could not resolve temporary directory path: %s", strerror(err));
        return BLOSC2_ERROR_FAILURE;
    }
    free(path);
    *ptmpdir_path = abs_path;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_create_tmp_file(const char* localpath, char** tmp_path) {
    if (localpath == NULL || tmp_path == NULL) {
        BLOSC_TRACE_ERROR("Function arguments cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *tmp_path = NULL;
    if (localpath[0] == '\0') {
        BLOSC_TRACE_ERROR("Path cannot be the empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    size_t tmp_path_size = strlen(localpath) + sizeof(".XXXXXX");
    char* tmpl = malloc(tmp_path_size);
    if (tmpl == NULL) {
        BLOSC_TRACE_ERROR("Memory allocation error when creating a temporary file");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    snprintf(tmpl, tmp_path_size, "%s.XXXXXX", localpath);

#if defined(HAVE_MKOSTEMP) && defined(O_CLOEXEC)
    int fd = mkostemp(tmpl, O_CLOEXEC);
#else
    int fd = mkstemp(tmpl);
#endif

    if (fd < 0) {
        int err = errno;
        free(tmpl);
        switch (err) {
        case ENOSPC:
        case EDQUOT:
            BLOSC_TRACE_ERROR("No space left to create temporary file");
            return BLOSC2_ERROR_FILE_WRITE;
        case ENAMETOOLONG:
            BLOSC_TRACE_ERROR("Path too long to create temporary file");
            return BLOSC2_ERROR_INVALID_PARAM;
        default:
            BLOSC_TRACE_ERROR("Could not create temporary file: %s", strerror(err));
            return BLOSC2_ERROR_FILE_OPEN;
        }
    }

#if !(defined(HAVE_MKOSTEMP) && defined(O_CLOEXEC))
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        int err = errno;
        close(fd);
        unlink(tmpl);
        free(tmpl);
        BLOSC_TRACE_ERROR("Could not set FD_CLOEXEC: %s", strerror(err));
    }
#endif

    *tmp_path = tmpl;
    return fd;
}

int blosc2_sync_parent_dir(const char* path) {
    char* buf = NULL;
    size_t buf_len = strlen(path) + 1;
    buf = malloc(buf_len);
    if (buf == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory buffer");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(buf, path, buf_len);
    int dirfd = -1;
    dirfd = open(dirname(buf), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0 ) {
        free(buf);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    bool retry_fsync = true;
    int error_code = BLOSC2_ERROR_SUCCESS;
    while (retry_fsync) {
        retry_fsync = false;
        if (fsync(dirfd) < 0) {
            int err = errno;
            switch (err) {
            case EINTR:
                BLOSC_TRACE_ERROR("A signal has interrupted the fsync operation, retrying at file %s ...", path);
                retry_fsync = true;
                break;
            case EIO:
                BLOSC_TRACE_ERROR("Error during synchronization of parent directory of %s", path);
                error_code = BLOSC2_ERROR_FILE_OPEN;
                break;
            case ENOSPC:
                BLOSC_TRACE_ERROR("Disk space was exhausted while syncing %s", path);
                error_code = BLOSC2_ERROR_FILE_OPEN;
                break;
            }
        }
    }
    close(dirfd);
    free(buf);

    return error_code;
}

int blosc2_atomic_schunk_write(const char* path, blosc2_schunk* schunk) {
    const char* tmp_file_path = NULL;
    int error_code = 0;
    bool tmp_exists = true;

    int fd = blosc2_create_tmp_file(path, (char**)&tmp_file_path);
    if (fd < 0) {
        BLOSC_TRACE_ERROR("Could not create temporal file at %s", path);
        error_code = fd;
        goto cleanup;
    }


    if (blosc2_schunk_to_file(schunk, tmp_file_path) < 0) {
        BLOSC_TRACE_ERROR("Could not write schunk to temporal file at %s", tmp_file_path);
        error_code = BLOSC2_ERROR_FILE_WRITE;
        goto cleanup;
    }

    bool retry_fsync = true;
    while (retry_fsync) {
        retry_fsync = false;
        if (fsync(fd) < 0) {
            int err = errno;
            switch (err) {
            case EINTR:
                BLOSC_TRACE_ERROR("A signal has interrupted the fsync operation, retrying at file %s ...", tmp_file_path);
                retry_fsync = true;
                break;
            case EIO:
                BLOSC_TRACE_ERROR("Error during synchronization in file %s", tmp_file_path);
                error_code = BLOSC2_ERROR_FILE_WRITE;
                goto cleanup;
            case ENOSPC:
                BLOSC_TRACE_ERROR("Disk space was exhausted while syncing %s", tmp_file_path);
                error_code = BLOSC2_ERROR_FILE_WRITE;
                goto cleanup;
            }
        }
    }

    if (close(fd) < 0) {
        fd = -1;
        error_code = BLOSC2_ERROR_FILE_WRITE;
        goto cleanup;
    }
    fd = -1;

    if (rename(tmp_file_path, path) < 0) {
        error_code = BLOSC2_ERROR_FILE_WRITE;
        goto cleanup;
    }
    tmp_exists = false;
    error_code = blosc2_sync_parent_dir(path);

cleanup:
    if (fd >= 0) close(fd);
    if (tmp_exists) remove(tmp_file_path);
    free((void*)tmp_file_path);
    return error_code;
}

int blosc2_realpath(const char* path, char** out_path) {
    if (path == NULL || out_path == NULL) {
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *out_path = NULL;

    char* real = realpath((const char*)path, NULL);
    if (real == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    *out_path = real;
    return BLOSC2_ERROR_SUCCESS;
}
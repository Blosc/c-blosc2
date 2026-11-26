/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "dict-store-private.h"
#include <embed-store/embed-store-private.h>
#include <utils/filesystem.h>
#include <utils/key.h>
#include <utils/string_manipulation.h>
#include <utils/ziputils.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <b2nd.h>
#include <fcntl.h>

typedef blosc2_dict_store dict_store;

/* == Static functions (and some constants) == */

static const uint8_t b2e_name[] = "embed.b2e"; // Used to identify the EmbedStore in the filesystem
static const uint8_t b2z_extension[] = "b2z";
static const uint8_t b2d_extension[] = "b2d";
static const uint8_t schunk_extension[] = "b2f";
static const uint8_t b2nd_extension[] = "b2nd";

// Enum used to specify if a value was inserted in the EmbedStore successfully
enum {
    EMBED_INSERTED = 1,
    EMBED_NOT_INSERTED = 2,
};

/* # Dict functions */

static int s_dict_store_alloc(dict_store** dstore, cb_free_value free_value) {
    dict_store *ds = calloc(1, sizeof(dict_store));
    if (ds == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    int rc = hash_map_init(&ds->map, sizeof(b2_ds_value), free_value);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not init hash map");
        free(ds);
        return rc;
    }
    *dstore = ds;

    return rc;
}

static int s_dict_store_alloc_urlpath(dict_store** dstore, const uint8_t* urlpath, b2_ds_storage_type storage_type, blosc2_store_open_mode mode, cb_free_value cb) {
    *dstore = NULL;
    dict_store *pds = NULL;
    int error_code = s_dict_store_alloc(&pds, cb);
    if (error_code < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory for blosc2_dict_store structure");
        return error_code;
    }
    error_code = string_duplicate(urlpath, (uint8_t**)&pds->urlpath);
    if (error_code < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory to duplicate string");
        blosc2_dict_store_free(pds);
        return error_code;
    }
    pds->urlpath_length = strlen((const char*)pds->urlpath);
    pds->mode = mode;
    pds->storage_type = storage_type;
    *dstore = pds;
    return error_code;
}

static void s_blosc2_store_value_set_local(blosc2_store_value* item, blosc2_schunk* schunk) {
    item->tag = BLOSC2_STORE_VALUE_SCHUNK;
    item->schunk = schunk;
}

static void s_b2_ds_value_free_dir(void* ptr) {
    b2_ds_value* value = (b2_ds_value*)ptr;
    free(value->f_dir.localpath);
}

// # END Dict functions

static bool s_schunk_is_ndarray(blosc2_schunk *schunk) {
    return blosc2_meta_exists(schunk, "b2nd") >= 0;
}

/* Extension used to persist a schunk on disk: ndarrays have their own. */
static const uint8_t* s_schunk_extension(const blosc2_schunk* schunk) {
    return s_schunk_is_ndarray((blosc2_schunk*)schunk) ? b2nd_extension : schunk_extension;
}

/* Return a malloc-ed copy of `key` with the schunk's extension appended. */
static int s_add_extension_to_key(const uint8_t* key, blosc2_schunk* container, uint8_t** pkey_with_extension, size_t* length) {
    int rc = string_append_extension(key, s_schunk_extension(container), pkey_with_extension);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not add extension to key");
        return rc;
    }
    if (length != NULL) {
        *length = strlen((const char*)*pkey_with_extension);
    }
    return BLOSC2_ERROR_SUCCESS;
}

/* Resolve `key` to its canonical form. When `key` is already canonical, *out
   borrows it and *owned is set to NULL; otherwise *out points to a malloc-ed
   copy stored in *owned that the caller must release. Returns 0 on success. */
static int s_resolve_key(const uint8_t* key, const uint8_t** out, uint8_t** owned) {
    *owned = NULL;
    if (key_is_canonical(key)) {
        *out = key;
        return BLOSC2_ERROR_SUCCESS;
    }
    int rc = key_normalize_copy(key, owned);
    if (rc < 0) {
        return rc;
    }
    *out = *owned;
    return BLOSC2_ERROR_SUCCESS;
}

/* Return a malloc-ed canonical copy of `key`, owned by the caller. */
static int s_canonical_key_copy(const uint8_t* key, uint8_t** out) {
    if (key_is_canonical(key)) {
        return string_duplicate(key, out);
    }
    return key_normalize_copy(key, out);
}

static b2_ds_storage_type s_get_storage_type_from_extension(const uint8_t* urlpath) {
    if (string_has_extension(urlpath, b2z_extension)) {
        return B2_DS_B2Z;
    }
    if (string_has_extension(urlpath, b2d_extension)) {
        return B2_DS_B2D;
    }
    return -1;
}

/* Base directory where the keys of this dict_store live on disk: the temporary
   directory for b2z, the store's own directory for b2d. */
static const uint8_t* s_storage_prefix(const dict_store* ds) {
    return (ds->storage_type == B2_DS_B2Z) ? ds->tmpdir : ds->urlpath;
}

/* Resolve `relative` against the store's base directory. The caller owns *out. */
static int s_storage_path(const dict_store* ds, const char* relative, char** out) {
    return fs_path_join((const char*)s_storage_prefix(ds), relative, out);
}

static int s_inode_exists(const uint8_t* path) {
    struct stat st;
    if (stat((const char*)path, &st) == 0) {
        return BLOSC2_ERROR_SUCCESS;
    }
    switch (errno) {
        case ENOENT:
        case ENOTDIR:
            return BLOSC2_ERROR_NOT_FOUND;
        default:
            BLOSC_TRACE_ERROR("Something went wrong in s_inode_exists");
            return BLOSC2_ERROR_FAILURE;
    }
}

static int s_zip_write_all_pairs(const dict_store *ds, zip_archive *za) {
    assert(ds != NULL && za != NULL);

    int rc = BLOSC2_ERROR_SUCCESS;
    hash_map_iter iter = {0};
    hash_map_iter* piter = &iter;
    hash_map_iter_init(&iter, &ds->map);
    while (hash_map_iter_next(piter)) {
        const uint8_t* key = hash_map_iter_get_key(piter);
        b2_ds_value* value = hash_map_iter_get_value(piter);
        blosc2_schunk* schunk = value->f_mem.schunk;

        uint8_t* concat = NULL;
        rc = s_add_extension_to_key(key, schunk, &concat, NULL);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not allocate memory to set extension to key");
            break;
        }

        uint8_t* frame_buffer = NULL;
        bool needs_free = false;
        int64_t frame_len = blosc2_schunk_to_buffer(schunk, &frame_buffer, &needs_free);
        if (frame_len < 0) {
            BLOSC_TRACE_ERROR("Could not convert schunk to cframe");
            rc = (int)frame_len;
            free(concat);
            break;
        }
        bool ok = ziputils_writer_add_entry(za, concat, frame_buffer, (size_t)frame_len);
        if (needs_free) free(frame_buffer);
        free(concat);
        if (!ok) {
            BLOSC_TRACE_ERROR("Could not add cframe to zip archive");
            rc = BLOSC2_ERROR_WRITE_BUFFER;
            break;
        }
    }

    return rc;
}

// abs_relative is the relative path from the top most directory, the first directory in the recursion chain
// parent_dir_fd is the file descriptor for the immediate parent dir
// A function frame has no ownership of the parameters parent_dir_fd and abs_relative
static int s_tree_walker(int parent_dir_fd, const char* abs_relative, bool* pembed_found) {

    int fd = openat(parent_dir_fd, abs_relative, O_DIRECTORY | O_CLOEXEC | O_RDONLY);
    if (fd < 0) {
        BLOSC_TRACE_ERROR("Could not open directory at relative %s", abs_relative);
        return BLOSC2_ERROR_FILE_OPEN;
    }
    DIR* dir = fdopendir(fd);
    if (dir == NULL) {
        BLOSC_TRACE_ERROR("Could not fdopendir at relative %s", abs_relative);
        close(fd);
    }
    fd = dirfd(dir); // Just in case...
    int rc = BLOSC2_ERROR_SUCCESS;
    struct dirent* entry;
    while ( (entry = readdir(dir)) != NULL ) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        const char* filename = entry->d_name;
        char* new_abs_relative = NULL;
        rc = fs_path_join(abs_relative, filename, &new_abs_relative);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not join paths");
            break;
        }

        struct stat st;
        if (fstatat(fd, filename, &st, AT_SYMLINK_NOFOLLOW) < 0) {
            BLOSC_TRACE_ERROR("Could not make fstatat");
            rc = BLOSC2_ERROR_FILE_READ;
            free(new_abs_relative);
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            rc = s_tree_walker(fd, new_abs_relative, pembed_found);
            if (rc < 0) {
                free(new_abs_relative);
            }
        } else if (S_ISREG(st.st_mode)) {
            // Insert new entry and take ownership of abs_relative.
            
        } else {
            free(new_abs_relative);
        }
    }
    closedir(dir); // Also closes the file descriptor
    return rc;
}

// (todo): This is not platform independent
static int s_b2d_fill_pairs_recursive(dict_store *ds, const uint8_t* urlpath, size_t urlpath_len, bool _embed_found) {
    // ds is valid and urlpath is a directory

    bool embed_found = _embed_found;
    DIR *d = NULL;
    d = opendir((const char*)urlpath);
    if (d == NULL) {
        BLOSC_TRACE_ERROR("Could not open the directory %s", (const char*)urlpath);
        return BLOSC2_ERROR_FILE_OPEN;
    }
    int error_code = BLOSC2_ERROR_SUCCESS;
    struct dirent *entry = NULL;
    while ( (entry = readdir(d)) != NULL ) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        const char* filename = entry->d_name;
        size_t filename_len = strlen(filename);
        uint8_t* concat = NULL;
        size_t concat_len = filename_len + urlpath_len + 1;
        concat = malloc(concat_len + 1);
        if (concat == NULL) {
            BLOSC_TRACE_ERROR("Could not allocate memory to fill entries in b2d");
            error_code = BLOSC2_ERROR_MEMORY_ALLOC;
            break;
        }
        snprintf((char*)concat, concat_len + 1, "%s/%s", (const char*)urlpath, (const char*)filename);

        struct stat st = {0};
        stat((const char*)concat, &st);
        if (S_ISDIR(st.st_mode)) {
            error_code = s_b2d_fill_pairs_recursive(ds, concat, concat_len, embed_found);
            free((void*)concat);
            if (error_code < 0) {
                // Here no BLOSC_TRACE_ERROR should be declared. In case of error, a big, BIG trace could be printed
                return error_code;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Get filename

            if (!embed_found && strcmp((const char*)b2e_name, filename) == 0) {
                embed_found = true;
                blosc2_embed_store* estore = NULL;
                error_code = blosc2_embed_store_open(&estore, concat, ds->mode);
                if (error_code < 0) {
                    BLOSC_TRACE_ERROR("Could not open EmbedStore in B2D");
                    free(concat);
                    return error_code;
                }
                free(concat);
                ds->estore = estore;
                continue;
            }

            b2_ds_value v = {0};
            const uint8_t* first_bar = (const uint8_t*) strchr((const char*)concat, '/');
            error_code = string_duplicate(first_bar + 1, (uint8_t**)&(v.f_dir.localpath));
            free((void*)concat);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not duplicate string");
                break;
            }
            v.f_dir.localpath_length = strlen((const char*)v.f_dir.localpath);

            uint8_t* key_extensionless = NULL;
            error_code = string_remove_extension(v.f_dir.localpath, &key_extensionless);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not allocate memory to remove extension");
                free((void*)v.f_dir.localpath);
                break;
            }

            uint8_t* key = NULL;
            error_code = key_normalize_copy(key_extensionless, &key);
            free((void*)key_extensionless);
            if (error_code != 0) {
                free((void*)v.f_dir.localpath);
                break;
            }

            bool needs_free = false;
            error_code = hash_map_put(&ds->map, key, &v, &needs_free);
            if (needs_free) {
                free(key);
            }
            if (error_code < 0) {
                BLOSC_TRACE_ERROR("Could not set pair in hash_map");
                break;
            }
        }
    }
    closedir(d);
    return error_code;
}

static int s_b2d_fill_pairs(dict_store *ds, const uint8_t* urlpath) {
    // ds is not null and valid

    struct stat st = {0};
    if (stat((const char*)urlpath, &st) != 0) {
        return BLOSC2_ERROR_FILE_OPEN;
    }
    if (!S_ISDIR(st.st_mode)) {
        return BLOSC2_ERROR_FILE_OPEN;
    }

    return s_b2d_fill_pairs_recursive(ds, urlpath, strlen((const char*)urlpath), false);
}

// (note) If the nomenclature of the function is confuse, here there is an explanation
// s means static; open refers to the blosc2_dict_store function that it supports; read refers to the open mode and
// b2z refers to the storage type
static int s_open_rdonly_b2z(dict_store **ds, const uint8_t* urlpath) {
    // Preconditions
    //      - ds != NULL
    //      - urlpath is not NULL and a valid storage point to a b2z file

    zip_archive *za = NULL;
    zip_archive_iter *iter = NULL;
    dict_store *pds = NULL;
    int error_code = BLOSC2_ERROR_SUCCESS;

    za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for zip_archive struct");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    if (!ziputils_reader_init_file(za, urlpath)) {
        BLOSC_TRACE_ERROR("Could not init zip_archive reader");
        ziputils_archive_free(za);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    iter = ziputils_reader_init_iter(za);
    if (iter == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for the zip_archive_iter structure");
        ziputils_archive_free(za);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, BLOSC2_STORE_OPEN_MODE_RDONLY, NULL);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    bool embed_found = false;
    while (ziputils_reader_iter_next(iter)) {
        uint8_t *filename = NULL;
        size_t filename_size = 0;
        b2_ds_value val = {0};

        // NOTE: Filename in malloc-ed
        error_code = ziputils_reader_iter_get_filename(iter, &filename, &filename_size);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not obtain filename from b2z");
            break;
        }

        val.f_zip.offset = (int64_t)ziputils_reader_iter_get_offset(iter);
        val.f_zip.length = ziputils_reader_iter_get_length(iter);

        // Detect the EmbedStore
        if (!embed_found && strcmp((const char*)filename, (const char*)b2e_name) == 0) {
            embed_found = true;
            // Now, open EmbedStore
            blosc2_embed_store* estore = NULL;
            error_code = blosc2_embed_store_open_offset(&estore, urlpath, BLOSC2_STORE_OPEN_MODE_RDONLY, val.f_zip.offset);
            if (error_code < 0) {
                BLOSC_TRACE_ERROR("The EmbedStore was detected, but it couldn't be opened by blosc2_embed_store_open_offset");
                free(filename);
                break;
            }
            pds->estore = estore;
            free(filename);
            continue;
        }

        // Filename comes with extension, we need an extensionless version
        error_code = string_remove_extension_inplace(filename);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not allocate memory to remoev extension");
            break;
        }

        bool needs_free = false;
        error_code = hash_map_put(&pds->map, filename, &val, &needs_free);
        if (needs_free) {
            free(filename);
        }
        if (error_code < 0) {
            BLOSC_TRACE_ERROR("Could not put element in hash_map");
            break;
        }
    }

    if (error_code != 0) {
        blosc2_dict_store_free(pds);
    } else {
        *ds = pds;
    }

    ziputils_reader_iter_free(iter);
    ziputils_reader_end(za);
    ziputils_archive_free(za);
    return error_code;
}

static int s_open_rdonly_b2d(dict_store **ds, const uint8_t* urlpath) {
    // Preconditions
    //      - ds != NULL
    //      - urlpath is not NULL and a valid storage point to a b2z file

    dict_store *pds = NULL;
    int error_code = BLOSC2_ERROR_SUCCESS;
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, BLOSC2_STORE_OPEN_MODE_RDONLY, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = s_b2d_fill_pairs(pds, urlpath);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not fill entries from %s", (const char*)urlpath);
        blosc2_dict_store_free(pds);
        return error_code;
    }

    *ds = pds;
    return error_code;
}


static int s_open_wronly_b2z(dict_store **ds, const uint8_t* urlpath) {

    dict_store *pds = NULL;
    uint8_t* tmpdir = NULL;
    int error_code = BLOSC2_ERROR_SUCCESS;

    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, BLOSC2_STORE_OPEN_MODE_WRONLY, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = fs_create_tmp_dir((char**)&tmpdir);
    if (error_code != 0) {
        blosc2_dict_store_free(pds);
        return error_code;
    }
    pds->tmpdir = tmpdir;
    pds->tmpdir_length = strlen((const char*)tmpdir);
    *ds = pds;
    return error_code;
}

static int s_open_wronly_b2d(dict_store **ds, const uint8_t* urlpath) {

    if (mkdir((const char*)urlpath, 0700) != 0) {
        BLOSC_TRACE_ERROR("Could not create directory at %s", (const char*)urlpath);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    dict_store *pds = NULL;
    int error_code = BLOSC2_ERROR_SUCCESS;
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, BLOSC2_STORE_OPEN_MODE_WRONLY, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        rmdir((const char*)urlpath);
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    *ds = pds;
    return BLOSC2_ERROR_SUCCESS;
}


static int s_open_rdwr_b2z(dict_store **ds, const uint8_t* urlpath) {

    bool free_pds = true;
    dict_store *pds = NULL;
    bool remove_tmpdir = true;
    uint8_t* tmpdir = NULL;
    size_t tmpdir_length = 0;
    zip_archive *za = NULL;
    zip_archive_iter *iter = NULL;

    int error_code = BLOSC2_ERROR_SUCCESS;
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, BLOSC2_STORE_OPEN_MODE_RDWR, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = fs_create_tmp_dir((char**)&tmpdir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not create temporal directory at %s", (const char*)urlpath);
        blosc2_dict_store_free(pds);
        return error_code;
    }
    tmpdir_length = strlen((const char*)tmpdir);

    za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate new zip_archive");
        blosc2_dict_store_free(pds);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    if (!ziputils_reader_init_file(za, urlpath)) {
        BLOSC_TRACE_ERROR("Could not initialize zip_archive reader where urlpath is %s", (const char*)urlpath);
        error_code = BLOSC2_ERROR_FILE_OPEN;
        goto cleanup;
    }
    iter = ziputils_reader_init_iter(za);
    if (iter == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for zip_archive_iter");
        error_code = BLOSC2_ERROR_MEMORY_ALLOC;
        goto cleanup;
    }

    char* full_path = NULL;
    while (ziputils_reader_iter_next(iter)) {
        b2_ds_value val = {0};
        uint8_t* key_with_extension = NULL;
        uint8_t* key = NULL;

        error_code = ziputils_reader_iter_get_filename(iter, &val.f_dir.localpath, &val.f_dir.localpath_length);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not obtain filename from archive");
            if (val.f_dir.localpath != NULL) free((void*)val.f_dir.localpath);
            goto cleanup;
        }
        error_code = fs_path_join((const char*)tmpdir, (const char*)val.f_dir.localpath, &full_path);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not build path for %s", (const char*)val.f_dir.localpath);
            free((void*)val.f_dir.localpath);
            goto cleanup;
        }

        error_code = key_normalize_copy(val.f_dir.localpath, &key_with_extension);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not process and validate key %s", (const char*)val.f_dir.localpath);
            free((void*)val.f_dir.localpath);
            if (key_with_extension != NULL) free((void*)key_with_extension);
            goto cleanup;
        }
        error_code = string_remove_extension(key_with_extension, (uint8_t**)&key);
        free((void*)key_with_extension);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not remove extension");
            free((void*)val.f_dir.localpath);
            error_code = BLOSC2_ERROR_MEMORY_ALLOC;
            goto cleanup;
        }

        size_t file_index = ziputils_reader_iter_get_file_index(iter);
        fs_create_dir_recursive((const char*)full_path);
        if (!ziputils_reader_extract_to_file(za, (const uint8_t*)full_path, file_index)) {
            BLOSC_TRACE_ERROR("Could not extract file in archive to file in filesystem");
            free((void*)key);
            free((void*)val.f_dir.localpath);
            goto cleanup;
        }

        bool needs_free = false;
        error_code = hash_map_put(&pds->map, key, &val, &needs_free);
        if (needs_free) {
            free((void*)key);
        }
        if (error_code < 0) {
            BLOSC_TRACE_ERROR("Coudl not set element in hash_map");
            goto cleanup;
        }
        free(full_path);
        full_path = NULL;
    }
    pds->tmpdir = tmpdir;
    pds->tmpdir_length = tmpdir_length;

    free_pds = false;
    remove_tmpdir = false;
    *ds = pds;
cleanup:
    if (free_pds) {
        *ds = NULL;
        blosc2_dict_store_free(pds);
    }
    if (full_path != NULL) free(full_path);
    if (remove_tmpdir) {
        fs_remove_dir_recursive((const char*)tmpdir);
    }
    if (iter != NULL) ziputils_reader_iter_free(iter);
    ziputils_reader_end(za);
    ziputils_archive_free(za);
    return error_code;
}

static int s_open_rdwr_b2d(dict_store **ds, const uint8_t* urlpath) {
    // Iterate through the directory and store keys with the localpath

    *ds = NULL;
    struct stat st = {0};
    if (stat((const char*)urlpath, &st) != 0) {
        BLOSC_TRACE_ERROR("Could not make stat over %s", (const char*)urlpath);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    if (!S_ISDIR(st.st_mode)) {
        BLOSC_TRACE_ERROR("%s is not a directory", (const char*)urlpath);
        return BLOSC2_ERROR_FILE_OPEN;
    }
    int error_code = BLOSC2_ERROR_SUCCESS;
    dict_store *pds = NULL;
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, BLOSC2_STORE_OPEN_MODE_RDWR, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = s_b2d_fill_pairs(pds, pds->urlpath);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not fill entries from directory");
        blosc2_dict_store_free(pds);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    *ds = pds;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_open(dict_store** ds, const uint8_t* urlpath, b2_ds_storage_type storage_type, blosc2_store_open_mode mode) {
    // Preconditions
    //      - ds != NULL
    //      - urlpath is not NULL and points to a valid storage location
    //      - storage_type is either B2_DS_B2Z or B2_DS_B2D

    // The existence policy depends on the mode: RDONLY/RDWR require the path to
    // exist, while WRONLY requires it not to.
    int inode = s_inode_exists(urlpath);
    switch (mode) {
        case BLOSC2_STORE_OPEN_MODE_RDONLY:
        case BLOSC2_STORE_OPEN_MODE_RDWR:
            if (inode != BLOSC2_ERROR_SUCCESS) {
                BLOSC_TRACE_ERROR("Cannot open %s because it does not exist", (const char*)urlpath);
                return BLOSC2_ERROR_FILE_OPEN;
            }
            break;
        case BLOSC2_STORE_OPEN_MODE_WRONLY:
            if (inode == BLOSC2_ERROR_SUCCESS) {
                BLOSC_TRACE_ERROR("Cannot open %s because it already exists", (const char*)urlpath);
                return BLOSC2_ERROR_FILE_OPEN;
            }
            break;
        default:
            return BLOSC2_ERROR_INVALID_PARAM;
    }

    if (storage_type == B2_DS_B2Z) {
        switch (mode) {
            case BLOSC2_STORE_OPEN_MODE_RDONLY: return s_open_rdonly_b2z(ds, urlpath);
            case BLOSC2_STORE_OPEN_MODE_WRONLY: return s_open_wronly_b2z(ds, urlpath);
            case BLOSC2_STORE_OPEN_MODE_RDWR:   return s_open_rdwr_b2z(ds, urlpath);
            default: break;
        }
    } else if (storage_type == B2_DS_B2D) {
        switch (mode) {
            case BLOSC2_STORE_OPEN_MODE_RDONLY: return s_open_rdonly_b2d(ds, urlpath);
            case BLOSC2_STORE_OPEN_MODE_WRONLY: return s_open_wronly_b2d(ds, urlpath);
            case BLOSC2_STORE_OPEN_MODE_RDWR:   return s_open_rdwr_b2d(ds, urlpath);
            default: break;
        }
    }
    return BLOSC2_ERROR_INVALID_PARAM;
}

// (note): s_get_mem_sparse is implemented directly in the switch statement in blosc2_dict_store_get
static int s_get_mem_contiguous(const dict_store *ds, b2_ds_value* value, blosc2_store_value* item) {
    assert(ds != NULL && value != NULL && item != NULL);
    assert(ds->storage_type == B2_DS_MEMORY_CONTIGUOUS);
    assert(ds->mode == BLOSC2_STORE_OPEN_MODE_RDONLY);

    int64_t offset = value->f_zip.offset;
    size_t length = value->f_zip.length;
    uint8_t* ptr = &ds->urlpath[offset];
    blosc2_schunk* schunk = blosc2_schunk_from_buffer(ptr, (int64_t)length, true);
    if (schunk == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    s_blosc2_store_value_set_local(item, schunk);
    return BLOSC2_ERROR_SUCCESS;
}

static int s_get_b2x(dict_store *ds, b2_ds_value* value, blosc2_store_value* item) {
    assert(ds != NULL && value != NULL && value != NULL && item != NULL);
    assert(ds->mode == BLOSC2_STORE_OPEN_MODE_RDONLY || ds->mode == BLOSC2_STORE_OPEN_MODE_RDWR);
    assert(ds->storage_type == B2_DS_B2Z || ds->storage_type == B2_DS_B2D);

    bool free_path = false;
    int64_t offset = 0;
    uint8_t* path = NULL;

    switch (ds->mode) {
    case BLOSC2_STORE_OPEN_MODE_RDONLY:
        if (ds->storage_type == B2_DS_B2Z) {
            offset = value->f_zip.offset;
            path = ds->urlpath;
            break;
        }
        /* Intentional fallthrough */
    case BLOSC2_STORE_OPEN_MODE_RDWR:{
        char* concat = NULL;
        int rc = s_storage_path(ds, (const char*)value->f_dir.localpath, &concat);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not build path for the requested key");
            return rc;
        }
        free_path = true;
        path = (uint8_t*)concat;
        break;
    }
    default:
        BLOSC_TRACE_ERROR("Invalid open mode found");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    blosc2_schunk* schunk = blosc2_schunk_open_offset((const char*)path, offset);
    int error_code = BLOSC2_ERROR_SUCCESS;
    if (schunk == NULL) {
        BLOSC_TRACE_ERROR("Could not open schunk at file %s with offset %ld", (const char*)path, offset);
        error_code = BLOSC2_ERROR_FILE_OPEN;
    } else {
        s_blosc2_store_value_set_local(item, schunk);
    }
    if (free_path) free((void*)path);
    return error_code;
}

static int s_get_from_value(dict_store* ds, b2_ds_value* value, blosc2_store_value* item) {
    assert(ds != NULL && value != NULL && item != NULL);
    assert(ds->mode != BLOSC2_STORE_OPEN_MODE_WRONLY);

    int rc = BLOSC2_ERROR_SUCCESS;
    switch (ds->storage_type) {
    case B2_DS_MEMORY_SPARSE:
        s_blosc2_store_value_set_local(item, value->f_mem.schunk);
        break;
    case B2_DS_MEMORY_CONTIGUOUS:
        rc = s_get_mem_contiguous(ds, value, item);
        break;
    case B2_DS_B2Z:
    case B2_DS_B2D:
        rc = s_get_b2x(ds, value, item);
        break;
    default:
        BLOSC_TRACE_ERROR("Invalid storage_type found");
        rc = BLOSC2_ERROR_INVALID_PARAM;
    }

    return rc;
}

static int s_set_b2x(dict_store* ds, uint8_t* key, blosc2_schunk* schunk, bool* free_key) {
    assert(ds != NULL || key != NULL || schunk != NULL || free_key != NULL);
    assert(ds->mode != BLOSC2_STORE_OPEN_MODE_RDONLY);
    assert(ds->storage_type == B2_DS_B2Z || ds->storage_type == B2_DS_B2D);
    // Also, key is malloc-ed

    *free_key = true;

    int rc = BLOSC2_ERROR_SUCCESS;

    uint8_t* localpath = NULL;
    size_t localpath_length = 0;

    b2_ds_value* value = hash_map_get(&ds->map, key);
    bool owns_localpath = value == NULL;
    if (value != NULL) {
        localpath = value->f_dir.localpath;
        localpath_length = value->f_dir.localpath_length;
    } else {
        rc = s_add_extension_to_key(key, schunk, &localpath, &localpath_length);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not add extension to %s", (const char*)key);
            goto cleanup;
        }
    }

    // Build full path
    char* full_path = NULL;
    rc = s_storage_path(ds, (const char*)localpath, &full_path);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory for path concatenation");
        goto cleanup;
    }
    rc = fs_create_dir_recursive((const char*)full_path);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not create recursively the directory");
        free(full_path);
        goto cleanup;
    }
    if (fs_atomic_schunk_write((const char*)full_path, schunk) < 0) {
        BLOSC_TRACE_ERROR("Could not write atomically the schunk to %s", (const char*)full_path);
        free(full_path);
        rc = BLOSC2_ERROR_FILE_WRITE;
        goto cleanup;
    }
    free(full_path);
    if (value == NULL) {
        b2_ds_value v = {0};
        v.f_dir.localpath = localpath;
        v.f_dir.localpath_length = localpath_length;
        rc = hash_map_put(&ds->map, key, &v, free_key);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not set hash");
            goto cleanup;
        }
        owns_localpath = false;
    }
cleanup:
    if (owns_localpath) free(localpath);
    return rc;
}

static int s_to_b2z_from_mem(dict_store *ds, const uint8_t* destination_urlpath) {
    assert(ds != NULL && destination_urlpath != NULL);

    zip_archive *za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could allocate memory for zip archive");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    if(!ziputils_writer_init_file(za, destination_urlpath)) {
        ziputils_archive_free(za);
        BLOSC_TRACE_ERROR("Could not initialize zip writer");
        return BLOSC2_ERROR_FILE_OPEN;
    }

    int error_code = s_zip_write_all_pairs(ds, za);

    if (error_code == 0) {
        ziputils_writer_finalize_file(za);
    }
    ziputils_writer_end(za);
    ziputils_archive_free(za);
    return error_code;
}

static int s_to_b2z_from_b2d(dict_store *ds, const uint8_t* destination_urlpath) {
    assert(ds != NULL && destination_urlpath != NULL);

    if (!ziputils_archive_directory(ds->urlpath, destination_urlpath)) {
        BLOSC_TRACE_ERROR("Could not archive directory %s from b2z", (const char*)destination_urlpath);
        return BLOSC2_ERROR_FAILURE;
    }
    return BLOSC2_ERROR_SUCCESS;
}

static int s_to_b2d_from_memory(dict_store *ds, const uint8_t* destination_urlpath) {
    assert(ds != NULL && destination_urlpath != NULL);

    int rc = BLOSC2_ERROR_SUCCESS;
    hash_map_iter iter = {0};
    hash_map_iter* piter= &iter;
    hash_map_iter_init(piter, &ds->map);

    while (hash_map_iter_next(piter)) {
        const uint8_t* key = hash_map_iter_get_key(piter);
        b2_ds_value* value = hash_map_iter_get_value(piter);
        blosc2_schunk* schunk = value->f_mem.schunk;

        uint8_t* key_with_extension = NULL;
        rc = s_add_extension_to_key(key, schunk, &key_with_extension, NULL);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not add extension to key");
            break;
        }

        char* full_path = NULL;
        rc = fs_path_join((const char*)destination_urlpath, (const char*)key_with_extension, &full_path);
        free(key_with_extension);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not build destination path");
            break;
        }
        rc = fs_create_dir_recursive((const char*)full_path);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not create directory at %s", (const char*)full_path);
            free(full_path);
            break;
        }
        if (blosc2_schunk_to_file(schunk, (const char*)full_path) < 0) {
            BLOSC_TRACE_ERROR("Could not write schunk to %s", (const char*)full_path);
            free(full_path);
            rc = BLOSC2_ERROR_FILE_WRITE;
            break;
        }
        free(full_path);
    }
    if (rc < 0) {
        BLOSC_TRACE_ERROR("There was an error creating a b2d from memory");
        if (s_inode_exists(destination_urlpath) != 0) {
            fs_remove_dir_recursive((const char*)destination_urlpath);
        }
    }
    return rc;
}

static int s_to_b2d_from_b2z(dict_store *ds, const uint8_t* destination_urlpath) {
    assert(ds != NULL && destination_urlpath != NULL);

    int error_code = BLOSC2_ERROR_SUCCESS;

    switch (ds->mode) {
        case BLOSC2_STORE_OPEN_MODE_RDONLY: {
            BLOSC_TRACE_ERROR("Cannot write to the filesystem in RDONLY mode");
            error_code = BLOSC2_ERROR_FILE_WRITE;
            break;
        }
        case BLOSC2_STORE_OPEN_MODE_WRONLY:
        case BLOSC2_STORE_OPEN_MODE_RDWR: {
            hash_map_iter iter = {0};
            hash_map_iter* piter = &iter;
            hash_map_iter_init(piter, &ds->map);

            const uint8_t* tmpdir = ds->tmpdir;

            while (hash_map_iter_next(piter)) {
                b2_ds_value* value = hash_map_iter_get_value(piter);
                const uint8_t* localpath = value->f_dir.localpath;

                char* full_path_origin = NULL;
                error_code = fs_path_join((const char*)tmpdir, (const char*)localpath, &full_path_origin);
                if (error_code != 0) {
                    BLOSC_TRACE_ERROR("Could not build the source path");
                    return error_code;
                }
                char* full_path_destination = NULL;
                error_code = fs_path_join((const char*)destination_urlpath, (const char*)localpath, &full_path_destination);
                if (error_code != 0) {
                    BLOSC_TRACE_ERROR("Could not build the destination path");
                    free(full_path_origin);
                    return error_code;
                }
                int error = fs_create_dir_recursive((const char*)full_path_destination);
                if (error != 0) {
                    BLOSC_TRACE_ERROR("Could not create directories at %s", (const char*)full_path_destination);
                    free(full_path_origin);
                    free(full_path_destination);
                    return BLOSC2_ERROR_FAILURE;
                }

                blosc2_schunk* schunk = blosc2_schunk_open((const char*)full_path_origin);
                if (schunk == NULL) {
                    BLOSC_TRACE_ERROR("Could not read schunk from %s", (const char*)full_path_origin);
                    free(full_path_origin);
                    free(full_path_destination);
                    error_code = BLOSC2_ERROR_FILE_READ;
                    break;
                }
                blosc2_schunk_to_file(schunk, (const char*)full_path_destination);
                blosc2_schunk_free(schunk); // Free memory allocated
                free(full_path_origin);
                free(full_path_destination);
            }
            break;
        }
    }

    if (error_code != 0) {
        error_code = s_inode_exists(destination_urlpath);
        if (error_code == BLOSC2_ERROR_SUCCESS) {
            fs_remove_dir_recursive((const char*)destination_urlpath);
        } else if (error_code == BLOSC2_ERROR_NOT_FOUND) {
            error_code = BLOSC2_ERROR_SUCCESS;
        } else {
            BLOSC_TRACE_ERROR("There was an error while requesting filesystem data (lstat)");
        }
    } else {
        BLOSC_TRACE_ERROR("There was an error while converting the b2d to a b2z");
    }

    return error_code;
}

static int s_remove_b2x(dict_store* ds, const uint8_t* key) {
    assert(ds != NULL && key != NULL);
    assert(ds->mode != BLOSC2_STORE_OPEN_MODE_RDONLY);
    assert(ds->storage_type == B2_DS_B2Z || ds->storage_type == B2_DS_B2D);

    b2_ds_value* value = hash_map_get(&ds->map, key);
    if (value == NULL) {
        // The key was not found, so there is nothing to remove
        return BLOSC2_ERROR_SUCCESS;
    }

    uint8_t* localpath = value->f_dir.localpath;

    char* full_path = NULL;
    int error_code = s_storage_path(ds, (const char*)localpath, &full_path);
    if (error_code != BLOSC2_ERROR_SUCCESS) {
        BLOSC_TRACE_ERROR("Could not allocate memory to create path");
        return error_code;
    }

    if (blosc2_remove_urlpath(full_path) != 0) {
        BLOSC_TRACE_ERROR("Could not remove %s", full_path);
        error_code = BLOSC2_ERROR_FILE_REMOVE;
    }

    free((void*)full_path);
    if (error_code == BLOSC2_ERROR_SUCCESS) {
        // Only drop the in-memory entry once the file is gone, so that a failed
        // removal leaves both the index and the filesystem consistent. This also
        // frees the value (and its localpath) through the hash_map's free_value
        // callback.
        hash_map_remove(&ds->map, key);
    }
    return error_code;
}

static int s_persist_embed_store( blosc2_embed_store* estore, const uint8_t* urlpath, size_t urlpath_length ) {
    assert(estore != NULL);
    assert(urlpath != NULL);

    (void)urlpath_length;  // no longer needed now that the path is built with fs_path_join

    int rc = blosc2_embed_store_close(estore);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not close EmbedStore");
        return rc;
    }
    char* concat = NULL;
    rc = fs_path_join((const char*)urlpath, (const char*)b2e_name, &concat);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory to concat paths");
        return rc;
    }

    int64_t fileframe_size = blosc2_embed_store_to_file(estore, (const uint8_t*)concat);
    if (fileframe_size < 0) {
        BLOSC_TRACE_ERROR("There was an error while persisting the EmbedStore to %s", (const char*)concat);
        rc = (int)fileframe_size;
    }
    free(concat);

    return rc;
}

static int s_close_b2z( dict_store *ds ) {
    assert(ds != NULL);
    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (ds->mode) {
        case BLOSC2_STORE_OPEN_MODE_RDONLY: return BLOSC2_ERROR_SUCCESS;
        case BLOSC2_STORE_OPEN_MODE_WRONLY:
        case BLOSC2_STORE_OPEN_MODE_RDWR: {
            // Compress temporal directory
            if (ds->estore != NULL) {
                error_code = s_persist_embed_store(ds->estore, ds->tmpdir, ds->tmpdir_length);
                if (error_code < 0) {
                    BLOSC_TRACE_ERROR("Could not persist EmbedStore when closing B2Z");
                    break;
                }
            }
            if (s_inode_exists(ds->urlpath) == BLOSC2_ERROR_SUCCESS) {
                if (blosc2_remove_urlpath((const char*)ds->urlpath) != 0) {
                    BLOSC_TRACE_ERROR("Could not remove file at %s", (const char*)ds->urlpath);
                    return BLOSC2_ERROR_FILE_REMOVE;
                }
            }
            if(!ziputils_archive_directory(ds->tmpdir, ds->urlpath)) {
                BLOSC_TRACE_ERROR("Could not archive directory");
                return BLOSC2_ERROR_FILE_WRITE;
            }
            // Remove temporal directory
            error_code = fs_remove_dir_recursive((const char*)ds->tmpdir);
            break;
        }
        default: return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not close B2Z");
    }
    return error_code;
}

static int s_close_b2d(dict_store* ds) {
    assert(ds != NULL);
    int rc = BLOSC2_ERROR_SUCCESS;
    if ((ds->mode == BLOSC2_STORE_OPEN_MODE_WRONLY || ds->mode == BLOSC2_STORE_OPEN_MODE_RDWR) && ds->estore != NULL) {
        rc = s_persist_embed_store(ds->estore, ds->urlpath, ds->urlpath_length);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not persist EmbedStore while closing B2D");
        }
    }
    return rc;
}

static int s_try_set_value_in_embed(blosc2_embed_store* estore, int64_t threshold, const uint8_t* key, blosc2_store_value* value) {
    assert(estore != NULL);
    assert(value != NULL);
    // Threshold may be <= 0. In that case, we are wasting time

    switch (value->tag) {
    case BLOSC2_STORE_VALUE_SCHUNK:{
        blosc2_schunk* schunk = value->schunk;
        int64_t nbytes = schunk->nbytes;
        if (nbytes < threshold) {
            break;

        }
        return EMBED_NOT_INSERTED;
    }
    case BLOSC2_STORE_VALUE_REMOTE:{
        // It is always inserted
        break;
    }
    default:
        BLOSC_TRACE_ERROR("An unexpected tag was found on value");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int rc = blosc2_embed_store_set(estore, key, value);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("There was an error while inserting SChunk to DictStore's inner EmbedStore with key %s", (const char*)key);
        return rc;
    }
    return EMBED_INSERTED;
}

/* == END Static functions (and some constants) == */

int blosc2_dict_store_new(blosc2_dict_store** dstore, const uint8_t* urlpath, blosc2_embed_store* estore, int64_t threshold) {
    if (dstore == NULL) {
        BLOSC_TRACE_ERROR("blosc2_dict_store pointer cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *dstore = NULL;

    b2_ds_storage_type storage_type = B2_DS_MEMORY_SPARSE;
    blosc2_store_open_mode open_mode = BLOSC2_STORE_OPEN_MODE_RDWR;
    cb_free_value cb = NULL;

    if (urlpath != NULL) {
        if (!string_has_extension(urlpath, b2z_extension) && !string_has_extension(urlpath, b2d_extension)) {
            BLOSC_TRACE_ERROR("urlpath must contain an extension, either b2z or b2d");
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        if (urlpath[0] == '\0' || s_inode_exists(urlpath) == BLOSC2_ERROR_SUCCESS) {
            BLOSC_TRACE_ERROR("urlpath cannot be equal to the empty string");
            return BLOSC2_ERROR_INVALID_PARAM;
        }

        storage_type = s_get_storage_type_from_extension(urlpath);
        if ((int)storage_type == -1) {
            BLOSC_TRACE_ERROR("Could not infer storage type from urlpath, a valid extension must be provided");
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        if (storage_type == B2_DS_B2Z || storage_type == B2_DS_B2D) {
            cb = s_b2_ds_value_free_dir;
        }
    }

    dict_store* ds = NULL;
    int rc = BLOSC2_ERROR_SUCCESS;
    if (urlpath != NULL) {
        rc = s_dict_store_alloc_urlpath(&ds, urlpath, storage_type, open_mode, cb);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not allocate memory for DictStore");
            return rc;
        }
        if (storage_type == B2_DS_B2Z) {
            uint8_t* tmpdir = NULL;
            if ((rc = fs_create_tmp_dir((char**)&tmpdir)) != 0) {
                BLOSC_TRACE_ERROR("Could not create temporal directory");
                blosc2_dict_store_free(ds);
                return rc;
            }
            ds->tmpdir = tmpdir;
            ds->tmpdir_length = strlen((const char*)tmpdir);
        } else {
            rc = fs_mkdir((const char*)urlpath);
            if (rc < 0) {
                BLOSC_TRACE_ERROR("Could not create directory at %s", (const char*)urlpath);
                blosc2_dict_store_free(ds);
                return rc;
            }
        }
    } else {
        rc = s_dict_store_alloc(&ds, cb);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not allocate memory for DictStore");
            return rc;
        }
        ds->storage_type = storage_type;
        ds->mode = open_mode;
    }

    if (estore != NULL) {
        ds->estore = estore;
    } else if (threshold >= 0) {
        // Create new EmbedStore
        blosc2_embed_store* new_estore = NULL;
        rc = blosc2_embed_store_new(&new_estore, NULL, BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE, BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not create EmbedStore for the DicStore");
            blosc2_dict_store_free(ds);
            return rc;
        }
        ds->estore = new_estore;
    } // else ds->estore == NULL (calloc does that for us)
    ds->threshold = threshold;

    *dstore = ds;
    return rc;
}

int blosc2_dict_store_open(blosc2_dict_store** dstore, const uint8_t* urlpath, blosc2_store_open_mode open_mode) {
    if (dstore == NULL || urlpath == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (urlpath[0] == '\0') {
        BLOSC_TRACE_ERROR("urlpath must be different from the empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    // Check extension
    b2_ds_storage_type storage_type = s_get_storage_type_from_extension(urlpath);
    if ((int)storage_type == -1) { // error
        BLOSC_TRACE_ERROR("Could not infer the storage type from the urlpath's extension");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = s_open(dstore, urlpath, storage_type, open_mode);

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not open %s", (const char*)urlpath);
    }
    return error_code;
}

int blosc2_dict_store_close(blosc2_dict_store *dstore) {
    if (dstore == NULL) {
        BLOSC_TRACE_ERROR("blosc2_dict_store pointer must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    switch (dstore->storage_type) {
        case B2_DS_MEMORY_CONTIGUOUS:
        case B2_DS_MEMORY_SPARSE: {
            return BLOSC2_ERROR_SUCCESS;
        }
        case B2_DS_B2Z: {
            int error_code = s_close_b2z(dstore);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not close B2Z DictStore");
            }
            return error_code;
        }
        case B2_DS_B2D: {
            int error_code = s_close_b2d(dstore);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not close B2D DictStore");
            }
            return error_code;
        }
        default: {
            BLOSC_TRACE_ERROR("DictStore was neither in memory, nor a B2Z storage, nor a B2D storage");
            return BLOSC2_ERROR_INVALID_PARAM;
        }
    }
}

void blosc2_dict_store_free(blosc2_dict_store* dstore) {
    assert(dstore != NULL);
    hash_map_free_struct(&dstore->map);
    if (dstore->urlpath != NULL && dstore->storage_type != B2_DS_MEMORY_CONTIGUOUS) free(dstore->urlpath);
    if (dstore->tmpdir != NULL) free(dstore->tmpdir);
    if (dstore->estore != NULL) blosc2_embed_store_free(dstore->estore); // Remember that dstore->estore could be NULL
    free(dstore);
}

size_t blosc2_dict_store_get_length(blosc2_dict_store *dstore) {
    assert(dstore != NULL);
    size_t length = dstore->map.length;
    if (dstore->estore != NULL) {
        length += blosc2_embed_store_get_length(dstore->estore);
    }
    return length;
}

size_t blosc2_dict_store_get_capacity(blosc2_dict_store *dstore) {
    assert(dstore != NULL);
    return dstore->map.capacity;
}

blosc2_store_open_mode blosc2_dict_store_get_open_mode(blosc2_dict_store *dstore) {
    assert(dstore != NULL);
    return dstore->mode;
}

int blosc2_dict_store_get_storage_type(blosc2_dict_store *dstore) {
    assert(dstore != NULL);
    return dstore->storage_type;
}

bool blosc2_dict_store_contains_key(blosc2_dict_store *dstore, const uint8_t* key) {
    if (dstore == NULL || key == NULL) return false;

    /* Keep the same key handling as get()/set()/remove(): the EmbedStore is
       queried with the raw key, and the dict_store map with the canonical one. */
    if (dstore->estore != NULL && blosc2_embed_store_contains_key(dstore->estore, key)) {
        return true;
    }

    const uint8_t* canonical_key = NULL;
    uint8_t* owned_key = NULL;
    if (s_resolve_key(key, &canonical_key, &owned_key) != BLOSC2_ERROR_SUCCESS) {
        return false;
    }
    bool contains = hash_map_contains_key(&dstore->map, canonical_key);
    free(owned_key);
    return contains;
}

int blosc2_dict_store_to_b2z(blosc2_dict_store *dstore, const uint8_t* destination_urlpath) {
    if (dstore == NULL || destination_urlpath == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (!string_has_extension(destination_urlpath, (const uint8_t*)"b2z")) {
        BLOSC_TRACE_ERROR("destination_urlpath does not contain b2z as an extension");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (s_inode_exists(destination_urlpath) == 0) {
        BLOSC_TRACE_ERROR("%s already exists in the filesystem, aborting...", (const char*)destination_urlpath);
        return BLOSC2_ERROR_FILE_WRITE;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (dstore->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            error_code = s_to_b2z_from_mem(dstore, destination_urlpath);
            break;
        }
        case B2_DS_B2Z: {
            BLOSC_TRACE_ERROR("Cannot make a to_b2z from a b2z");
            return BLOSC2_ERROR_FAILURE;
        }
        case B2_DS_B2D: {
            error_code = s_to_b2z_from_b2d(dstore, destination_urlpath);
            break;
        }
        default:
            BLOSC_TRACE_ERROR("Unexpected storage type found");
            error_code = BLOSC2_ERROR_INVALID_PARAM;
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not dump to b2z");
    }
    return error_code;
}

int blosc2_dict_store_to_b2d(blosc2_dict_store *dstore, const uint8_t* destination_urlpath) {
    if (dstore == NULL || destination_urlpath == NULL) {
        BLOSC_TRACE_ERROR("Every parameter must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (!string_has_extension(destination_urlpath, b2d_extension)) {
        BLOSC_TRACE_ERROR("destination_urlpath does not contain b2d as an extension");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (s_inode_exists(destination_urlpath) == 0) {
        BLOSC_TRACE_ERROR("%s already exists in the filesystem, aborting...", (const char*)destination_urlpath);
        return BLOSC2_ERROR_FILE_WRITE;
    }
    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (dstore->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            error_code = s_to_b2d_from_memory(dstore, destination_urlpath);
            break;
        }
        case B2_DS_B2Z: {
            error_code = s_to_b2d_from_b2z(dstore, destination_urlpath);
            break;
        }
        case B2_DS_B2D: {
            error_code = BLOSC2_ERROR_FAILURE;
            BLOSC_TRACE_ERROR("Cannot make a to_b2z from a b2z");
            break;
        }
        default:
            BLOSC_TRACE_ERROR("Unexpected storage_type found");
            error_code = BLOSC2_ERROR_INVALID_PARAM;
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Cannot make a to_b2d from a b2d");
    }
    return error_code;
}

int blosc2_dict_store_to_buffer(blosc2_dict_store *dstore, void** pbuffer, size_t* psize) {
    if (dstore == NULL || pbuffer == NULL) {
        BLOSC_TRACE_ERROR("ds cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (dstore->storage_type == B2_DS_B2D || dstore->storage_type == B2_DS_B2Z || dstore->storage_type == B2_DS_MEMORY_CONTIGUOUS) {
        BLOSC_TRACE_ERROR("This function is only supported for B2_DS_MEMORY storage type");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *pbuffer = NULL;
    *psize = 0;

    zip_archive *za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for zip_archive");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    if (!ziputils_writer_init_heap(za)) {
        BLOSC_TRACE_ERROR("Could not init zip heap writer");
        ziputils_archive_free(za);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    int error_code = s_zip_write_all_pairs(dstore, za);

    if (error_code == BLOSC2_ERROR_SUCCESS) {
        if (!ziputils_writer_finalize_heap(za, pbuffer, psize)) {
            BLOSC_TRACE_ERROR("Could not finalize zip archive");
            error_code = BLOSC2_ERROR_MEMORY_ALLOC;
        }
    }

    ziputils_writer_end(za);
    ziputils_archive_free(za);
    return error_code;
}

int blosc2_dict_store_from_buffer(blosc2_dict_store** dstore, void* buffer, size_t size) {
    if (dstore == NULL || buffer == NULL) {
        BLOSC_TRACE_ERROR("Cannot pass NULL arguments to this function");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (size == 0) {
        BLOSC_TRACE_ERROR("Size must be greater than 0");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *dstore = NULL;
    bool free_dict_store = true;

    zip_archive *za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for zip_archive");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    if (!ziputils_reader_init_heap(za, buffer, size)) {
        BLOSC_TRACE_ERROR("Could not init zip_archive reader from buffer");
        ziputils_archive_free(za);
        return BLOSC2_ERROR_FAILURE;
    }
    // Iterate though zip_archive
    zip_archive_iter *iter = ziputils_reader_init_iter(za);
    if (iter == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for zip_archive_iter");
        ziputils_archive_free(za);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    dict_store *ds = NULL;
    int error_code = s_dict_store_alloc(&ds, NULL);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate space for the blosc2_dict_store structure");
        free_dict_store = false;
        goto cleanup;
    }
    ds->storage_type = B2_DS_MEMORY_CONTIGUOUS;
    ds->mode = BLOSC2_STORE_OPEN_MODE_RDONLY;

    while (ziputils_reader_iter_next(iter)) {
        uint8_t* filename = NULL; // Filename is malloc-ed
        size_t filename_length = 0;
        b2_ds_value val = {0};

        error_code = ziputils_reader_iter_get_filename(iter, &filename, &filename_length);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not allocate space for zip filename");
            goto cleanup;
        }

        error_code = string_remove_extension_inplace(filename);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not remove extension to %s", (const char*)filename);
            free(filename);
            goto cleanup;
        }

        size_t out_length = 0;
        error_code = key_normalize_inplace(filename, &out_length);
        // filename is not NULL
        if (error_code == BLOSC2_ERROR_INVALID_PARAM) {
            BLOSC_TRACE_ERROR("Parameter is not valid, ignoring...");
            error_code = BLOSC2_ERROR_SUCCESS;
            free(filename);
            continue;
        }

        val.f_zip.offset = (int64_t)ziputils_reader_iter_get_offset(iter);
        val.f_zip.length = ziputils_reader_iter_get_length(iter);

        bool needs_free = false;
        error_code = hash_map_put(&ds->map, filename, &val, &needs_free);
        if (needs_free) free(filename);
        if (error_code < 0) {
            BLOSC_TRACE_ERROR("Could not put item in hash_map");
            goto cleanup;
        }
    }

    ds->urlpath = buffer;
    free_dict_store = false;
    *dstore = ds;

cleanup:
    ziputils_reader_iter_free(iter);
    ziputils_reader_end(za);
    ziputils_archive_free(za);
    if (free_dict_store) blosc2_dict_store_free(ds);

    return error_code;
}

int blosc2_dict_store_get(blosc2_dict_store* dstore, const uint8_t* key, blosc2_store_value* item) {
    if (dstore == NULL || key == NULL || item == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (dstore->mode == BLOSC2_STORE_OPEN_MODE_WRONLY) {
        BLOSC_TRACE_ERROR("Cannot access items in WRONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    // EmbedStore
    if (dstore->estore != NULL) {
        error_code = blosc2_embed_store_get(dstore->estore, key, item);
        if (error_code == BLOSC2_ERROR_SUCCESS) {
            return BLOSC2_ERROR_SUCCESS;
        }
        if (error_code != BLOSC2_ERROR_NOT_FOUND && error_code < 0) {
            BLOSC_TRACE_ERROR("Element could not be retrieved from EmbedStore");
            return error_code;
        }
        // The element was not found.
    }
    // EmbedStore END

    const uint8_t* canonical_key = NULL;
    uint8_t* owned_key = NULL;
    error_code = s_resolve_key(key, &canonical_key, &owned_key);
    if (error_code < 0) {
        BLOSC_TRACE_ERROR("Could not normalize key");
        return error_code;
    }
    b2_ds_value* value = hash_map_get(&dstore->map, canonical_key);
    free(owned_key);

    if (value == NULL) {
        return BLOSC2_ERROR_NOT_FOUND;
    }

    error_code = s_get_from_value(dstore, value, item);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve schunk with key: %s", (const char*)key);
    }
    return error_code;
}

int blosc2_dict_store_set( blosc2_dict_store* ds, const uint8_t* key, blosc2_store_value* value ) {
    if (value == NULL || ds == NULL || key == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->mode == BLOSC2_STORE_OPEN_MODE_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot call set when open_mode is RDONLY");
        return BLOSC2_ERROR_FAILURE;
    }

    // EmbedStore
    int error_code = BLOSC2_ERROR_SUCCESS;
    if (ds->estore != NULL) {
        error_code = s_try_set_value_in_embed(ds->estore, ds->threshold, key, value);
        if (error_code < 0) {
            BLOSC_TRACE_ERROR("The element was suitable for insertion, but it couldn't be inserted");
            return error_code;
        }
        if (error_code == EMBED_INSERTED) {
            return BLOSC2_ERROR_SUCCESS;
        }
        // EMBED_NOT_INSERTED
    } else if (value->tag == BLOSC2_STORE_VALUE_REMOTE) {
        BLOSC_TRACE_ERROR("Cannot estore remote value if no EmbedStore was created for this DictStore");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    // EmbedStore END

    if (key[0] == '\0') {
        BLOSC_TRACE_ERROR("key cannot be equal to the empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    // The key stored in the map is a malloc-ed canonical string owned by the map
    uint8_t* processed_key = NULL;
    error_code = s_canonical_key_copy(key, &processed_key);
    if (error_code != 0) {
        return error_code;
    }
    bool free_processed = true;

    blosc2_schunk* container = value->schunk;
    // Try insert it in the DictStore
    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            b2_ds_value v = {0};
            v.f_mem.schunk = value->schunk;
            error_code = hash_map_put(&ds->map, processed_key, &v, &free_processed);
            break;
        }
        case B2_DS_B2Z:
        case B2_DS_B2D: {
            error_code = s_set_b2x(ds, processed_key, container, &free_processed);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not set schunk with key: %s", (const char*)key);
            }
            break;
        }
        default: error_code = BLOSC2_ERROR_INVALID_PARAM;
    }

    if (free_processed) { free(processed_key); }
    return error_code;
}

int64_t blosc2_dict_store_get_threshold(blosc2_dict_store* dstore) {
    assert(dstore != NULL);
    return dstore->threshold;
}

int blosc2_dict_store_set_threshold(blosc2_dict_store* dstore, int64_t threshold) {
    if (dstore == NULL) {
        BLOSC_TRACE_ERROR("dstore cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    // Threshold can be <= 0, in that case, it is assumed that the user does not want to use the EmbedStore anymore.
    dstore->threshold = threshold;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_dict_store_remove( blosc2_dict_store* ds, const uint8_t* key ) {
    if (ds == NULL || key == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->mode == BLOSC2_STORE_OPEN_MODE_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot remove elements in RDONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    // EmbedStore
    if (ds->estore != NULL && blosc2_embed_store_contains_key(ds->estore, key)) {
        error_code = blosc2_embed_store_remove(ds->estore, key);
        if (error_code < 0) {
            BLOSC_TRACE_ERROR("Item was found in the DictStore's inner EmbedStore, but could not be removed");
        }
        return error_code;
    }
    // EmbedStore END

    const uint8_t* canonical_key = NULL;
    uint8_t* owned_key = NULL;
    error_code = s_resolve_key(key, &canonical_key, &owned_key);
    if (error_code != BLOSC2_ERROR_SUCCESS) {
        BLOSC_TRACE_ERROR("Could not process and validate key");
        return error_code;
    }

    switch (ds->storage_type) {
        case B2_DS_B2Z:
        case B2_DS_B2D:
            error_code = s_remove_b2x(ds, canonical_key);
            if (error_code < 0) {
                BLOSC_TRACE_ERROR("Could not remove element with key  %s", (const char*)key);
            }
            break;
        case B2_DS_MEMORY_SPARSE:
            // This returns true if deleted, false if not. It cannot fail in MEMORY_SPARSE
            hash_map_remove(&ds->map, canonical_key);
            break;
        default:
            BLOSC_TRACE_ERROR("Invalid storage_type detected");
            error_code = BLOSC2_ERROR_INVALID_PARAM;
    }

    free(owned_key);
    return error_code;
}

/* Iterator functions */
blosc2_dict_store_iter* blosc2_dict_store_iter_new( blosc2_dict_store *ds ) {
    if (ds == NULL) {
        BLOSC_TRACE_ERROR("ds must not be equal to NULL");
        return NULL;
    }

    blosc2_dict_store_iter* iter = calloc(1, sizeof(blosc2_dict_store_iter));
    if (iter == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store_iter");
        return NULL;
    }

    if (ds->estore != NULL) {
        priv_embed_store_iter_init(&iter->embed_iter, ds->estore);
        iter->state = B2_DS_ITER_EMBED;
    } else {
        hash_map_iter_init(&iter->map_iter, &ds->map),
        iter->state = B2_DS_ITER_DICT;
    }
    iter->dstore = ds;
    return iter;
}

void blosc2_dict_store_iter_free( blosc2_dict_store_iter* iter ) {
    assert(iter != NULL);
    free(iter);
}

bool blosc2_dict_store_iter_next( blosc2_dict_store_iter *iter ) {
    assert(iter != NULL);
    bool next = false;
    if (iter->state == B2_DS_ITER_EMBED) {
        next = blosc2_embed_store_iter_next(&iter->embed_iter);
        if (!next) {
            iter->state = B2_DS_ITER_DICT;
            memset(&iter->map_iter, 0, sizeof(iter->map_iter));
            hash_map_iter_init(&iter->map_iter, &iter->dstore->map);
            next = hash_map_iter_next(&iter->map_iter);
        }
    } else {
        next = hash_map_iter_next(&iter->map_iter);
    }
    return next;
}

const uint8_t* blosc2_dict_store_iter_get_key( blosc2_dict_store_iter *iter ) {
    assert(iter != NULL);
    if (iter->state == B2_DS_ITER_EMBED) {
        return blosc2_embed_store_iter_get_key(&iter->embed_iter);
    }
    return hash_map_iter_get_key(&iter->map_iter);
}

int blosc2_dict_store_iter_get_container( blosc2_dict_store_iter *iter, blosc2_store_value* item ) {
    if (iter == NULL || item == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (iter->dstore->mode == BLOSC2_STORE_OPEN_MODE_WRONLY) {
        BLOSC_TRACE_ERROR("Cannot retrieve container from iterator in WRONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int rc = BLOSC2_ERROR_SUCCESS;
    if (iter->state == B2_DS_ITER_EMBED) {
        rc = blosc2_embed_store_iter_get_container(&iter->embed_iter, item);
    } else {
        b2_ds_value* value = hash_map_iter_get_value(&iter->map_iter);
        rc = s_get_from_value(iter->dstore, value, item);
    }
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not retrieve item");
    }
    return rc;
}

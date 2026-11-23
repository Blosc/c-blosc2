/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <blosc2/dict-store.h>
#include <utils/filesystem.h>
#include <utils/hash.h>
#include <utils/key.h>
#include <utils/string_manipulation.h>
#include <utils/ziputils.h>
#include "dict-store-private.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <b2nd.h>

typedef blosc2_dict_store dict_store;
typedef b2_ds_value value; // Recall that this is an union in C

/* == Static functions (and some constants) == */

static const uint8_t* b2z_extension = (const uint8_t*)"b2z";
//static const size_t b2z_extension_len = 3;
static const uint8_t* b2d_extension = (const uint8_t*)"b2d";
//static const size_t b2d_extension_len = 3;
static const uint8_t* schunk_extension = (const uint8_t*)"cframe";
static const size_t schunk_extension_len = 6;
static const uint8_t* b2nd_extension = (const uint8_t*)"b2nd";
static const size_t b2nd_extension_len = 3;

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

static int s_dict_store_alloc_urlpath(dict_store** dstore, const uint8_t* urlpath, b2_ds_storage_type storage_type, b2_ds_open_mode open_mode, cb_free_value cb) {
    *dstore = NULL;
    dict_store *pds = NULL;
    int error_code = s_dict_store_alloc(&pds, cb);
    if (error_code < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory for blosc2_dict_store structure");
        return error_code;
    }
    error_code = blosc2_string_duplicate(urlpath, (uint8_t**)&pds->urlpath);
    if (error_code < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory to duplicate string");
        blosc2_dict_store_free(pds);
        return error_code;
    }
    pds->urlpath_length = strlen((const char*)pds->urlpath);
    pds->open_mode = open_mode;
    pds->storage_type = storage_type;
    *dstore = pds;
    return error_code;
}

static void s_b2_ds_item_set_local(b2_ds_item* item, blosc2_schunk* schunk) {
    item->tag = B2_DS_ITEM_LOCAL;
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

static int s_add_extension_to_key( const uint8_t* key, blosc2_schunk *container, uint8_t** pkey_with_extension, size_t* length ) {

    uint8_t* key_with_extension = NULL;
    const uint8_t* extension = NULL;
    size_t extension_length = 0;

    if (s_schunk_is_ndarray(container)) {
        extension = b2nd_extension;
        extension_length = b2nd_extension_len;
    } else {
        extension = schunk_extension;
        extension_length = schunk_extension_len;
    }

    size_t key_length = strlen((const char*)key);
    size_t key_with_extension_length = key_length + extension_length + 2;
    key_with_extension = malloc(key_with_extension_length);
    if (key_with_extension == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate buffer to add extension to key");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    snprintf((char*)key_with_extension, key_with_extension_length, "%s.%s", (const char*)key, (const char*)extension);

    *pkey_with_extension = key_with_extension;
    if (length) *length = key_with_extension_length - 1;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_get_storage_type_from_extension(const uint8_t* urlpath) {
    if (blosc2_string_has_extension(urlpath, b2z_extension)) {
        return B2_DS_B2Z;
    }
    if (blosc2_string_has_extension(urlpath, b2d_extension)) {
        return B2_DS_B2D;
    }
    return -1;
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

// (todo): This is not platform independent
static int s_b2d_fill_pairs_recursive(dict_store *ds, const uint8_t* urlpath, size_t urlpath_len) {
    // ds is valid and urlpath is a directory

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
            s_b2d_fill_pairs_recursive(ds, concat, concat_len);
            free((void*)concat);
        } else if (S_ISREG(st.st_mode)) {
            // Get filename
            value v = {0};
            const uint8_t* first_bar = (const uint8_t*) strchr((const char*)concat, '/');
            //uint8_t* localpath = NULL;
            error_code = blosc2_string_duplicate(first_bar + 1, (uint8_t**)&(v.f_dir.localpath));
            free((void*)concat);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not duplicate string");
                break;
            }
            v.f_dir.localpath_length = strlen((const char*)v.f_dir.localpath);

            uint8_t* key_extensionless = NULL;
            error_code = blosc2_string_remove_extension(v.f_dir.localpath, &key_extensionless);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not allocate memory to remove extension");
                free((void*)v.f_dir.localpath);
                break;
            }

            uint8_t* key = NULL;
            error_code = blosc2_key_normalize_copy(key_extensionless, &key);
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

    return s_b2d_fill_pairs_recursive(ds, urlpath, strlen((const char*)urlpath));
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

    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, B2_DS_RDONLY, NULL);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    while (ziputils_reader_iter_next(iter)) {
        uint8_t *filename = NULL;
        uint8_t* filename_without_extension = NULL;
        size_t filename_size = 0;
        value val = {0};

        error_code = ziputils_reader_iter_get_filename(iter, &filename, &filename_size);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not obtain filename from b2z");
            break;
        }

        // Filename comes with extension, we need an extensionless version
        error_code = blosc2_string_remove_extension(filename, &filename_without_extension);
        free((void*)filename);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not allocate memory to remoev extension");
            break;
        }

        val.f_zip.offset = (int64_t)ziputils_reader_iter_get_offset(iter);
        val.f_zip.length = ziputils_reader_iter_get_length(iter);

        bool needs_free = false;
        error_code = hash_map_put(&pds->map, filename_without_extension, &val, &needs_free);
        if (needs_free) {
            free((void*)filename_without_extension);
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, B2_DS_RDONLY, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = s_b2d_fill_pairs(pds, urlpath);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not fill entries from %s", (const char*)urlpath);
        blosc2_dict_store_free(pds);
    }

    *ds = pds;
    return error_code;
}

static int s_open_rdonly(dict_store **ds, const uint8_t* urlpath, int storage_type) {
    // Preconditions
    //      - ds != NULL
    //      - urlpath is not NULL and a valid storage point
    //      - storage_type is either B2_DS_B2Z or B2_DS_B2D

    if (s_inode_exists(urlpath) != 0) {
        return BLOSC2_ERROR_FILE_OPEN;
    }

    switch (storage_type) {
        case B2_DS_B2Z: {
            return s_open_rdonly_b2z(ds, urlpath);
        }

        case B2_DS_B2D: {
            return s_open_rdonly_b2d(ds, urlpath);
        }

        default: return BLOSC2_ERROR_INVALID_PARAM;
    }
}

static int s_open_wronly_b2z(dict_store **ds, const uint8_t* urlpath) {

    dict_store *pds = NULL;
    uint8_t* tmpdir = NULL;
    int error_code = BLOSC2_ERROR_SUCCESS;

    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, B2_DS_WRONLY, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = blosc2_create_tmp_dir((char**)&tmpdir);
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, B2_DS_WRONLY, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        rmdir((const char*)urlpath);
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    *ds = pds;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_open_wronly(dict_store **ds, const uint8_t* urlpath, int storage_type) {
    // Preconditions
    //      - ds != NULL
    //      - urlpath is not NULL and a valid storage point
    //      - storage_type is either B2_DS_B2Z or B2_DS_B2D

    if (s_inode_exists(urlpath) == 0) {
        BLOSC_TRACE_ERROR("Cannot open blosc2_dict_store when urlpath already points to an existing file %s", (const char*)urlpath);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    switch (storage_type) {
        case B2_DS_B2Z: {
            return s_open_wronly_b2z(ds, urlpath);
        }

        case B2_DS_B2D: {
            return s_open_wronly_b2d(ds, urlpath);
        }

        default: return BLOSC2_ERROR_INVALID_PARAM;
    }
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, B2_DS_RDWR, s_b2_ds_value_free_dir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = blosc2_create_tmp_dir((char**)&tmpdir);
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

    uint8_t full_path[PATH_MAX];
    const size_t full_path_length = PATH_MAX;
    while (ziputils_reader_iter_next(iter)) {
        value val = {0};
        uint8_t* key_with_extension = NULL;
        uint8_t* key = NULL;

        error_code = ziputils_reader_iter_get_filename(iter, &val.f_dir.localpath, &val.f_dir.localpath_length);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not obtain filename from archive");
            if (val.f_dir.localpath != NULL) free((void*)val.f_dir.localpath);
            goto cleanup;
        }
        if (val.f_dir.localpath_length + tmpdir_length + 1 >= full_path_length) {
            BLOSC_TRACE_ERROR("Maximum size reached for %s", (const char*)val.f_dir.localpath);
            free((void*)val.f_dir.localpath);
            error_code = BLOSC2_ERROR_MAX_BUFSIZE_EXCEEDED;
            goto cleanup;
        }
        snprintf((char*)full_path, full_path_length, "%s/%s", (const char*)tmpdir, (const char*)val.f_dir.localpath);

        error_code = blosc2_key_normalize_copy(val.f_dir.localpath, &key_with_extension);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not process and validate key %s", (const char*)val.f_dir.localpath);
            free((void*)val.f_dir.localpath);
            if (key_with_extension != NULL) free((void*)key_with_extension);
            goto cleanup;
        }
        error_code = blosc2_string_remove_extension(key_with_extension, (uint8_t**)&key);
        free((void*)key_with_extension);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not remove extension");
            free((void*)val.f_dir.localpath);
            error_code = BLOSC2_ERROR_MEMORY_ALLOC;
            goto cleanup;
        }

        size_t file_index = ziputils_reader_iter_get_file_index(iter);
        blosc2_create_dir_recursive((const char*)full_path);
        if (!ziputils_reader_extract_to_file(za, full_path, file_index)) {
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
    if (remove_tmpdir) {
        blosc2_remove_dir_recursive((const char*)tmpdir);
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, B2_DS_RDWR, s_b2_ds_value_free_dir);
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

static int s_open_rdwr(dict_store **ds, const uint8_t* urlpath, int storage_type) {
    // Preconditions
    //      - ds != NULL
    //      - urlpath is not NULL and a valid storage point
    //      - storage_type is either B2_DS_B2Z or B2_DS_B2D

    if (s_inode_exists(urlpath) != 0) {
        BLOSC_TRACE_ERROR("Can't open in RDWD mode when %s does not exist", (const char*)urlpath);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    switch (storage_type) {
        case B2_DS_B2Z: {
            return s_open_rdwr_b2z(ds, urlpath);
        }

        case B2_DS_B2D: {
            return s_open_rdwr_b2d(ds, urlpath);
        }

        default: return BLOSC2_ERROR_INVALID_PARAM;
    }
}

// (note): s_get_mem_sparse is implemented directly in the switch statement in blosc2_dict_store_get
static int s_get_mem_contiguous(const dict_store *ds, b2_ds_value* value, b2_ds_item* item) {
    assert(ds != NULL && value != NULL && item != NULL);
    assert(ds->storage_type == B2_DS_MEMORY_CONTIGUOUS);
    assert(ds->open_mode == B2_DS_RDONLY);

    int64_t offset = value->f_zip.offset;
    size_t length = value->f_zip.length;
    uint8_t* ptr = &ds->urlpath[offset];
    blosc2_schunk* schunk = blosc2_schunk_from_buffer(ptr, (int64_t)length, true);
    if (schunk == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    s_b2_ds_item_set_local(item, schunk);
    return BLOSC2_ERROR_SUCCESS;
}

static int s_get_b2x(dict_store *ds, b2_ds_value* value, b2_ds_item* item) {
    assert(ds != NULL && value != NULL && value != NULL && item != NULL);
    assert(ds->open_mode == B2_DS_RDONLY || ds->open_mode == B2_DS_RDWR);
    assert(ds->storage_type == B2_DS_B2Z || ds->storage_type == B2_DS_B2D);

    bool free_path = false;
    int64_t offset = 0;
    uint8_t* path = NULL;

    switch (ds->open_mode) {
    case B2_DS_RDONLY:
        if (ds->storage_type == B2_DS_B2Z) {
            offset = value->f_zip.offset;
            path = ds->urlpath;
            break;
        }
        /* Intentional fallthrough */
    case B2_DS_RDWR:{
        uint8_t* prefix;
        size_t prefix_length;
        if (ds->storage_type == B2_DS_B2Z) {
            prefix = ds->tmpdir;
            prefix_length = ds->tmpdir_length;
        } else {
            prefix = ds->urlpath;
            prefix_length = ds->urlpath_length;
        }

        uint8_t* localpath = value->f_dir.localpath;
        size_t localpath_length = value->f_dir.localpath_length;
        size_t concat_size = prefix_length + localpath_length + 2;
        uint8_t* concat = malloc(concat_size);
        if (concat == NULL) {
            BLOSC_TRACE_ERROR("Could not allocate memory for path joining");
            return BLOSC2_ERROR_MEMORY_ALLOC;
        }
        snprintf((char*)concat, concat_size, "%s/%s", (const char*)prefix, (const char*)localpath);
        free_path = true;
        path = concat;
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
        s_b2_ds_item_set_local(item, schunk);
    }
    if (free_path) free((void*)path);
    return error_code;
}

static int s_get_from_value(dict_store* ds, b2_ds_value* value, b2_ds_item* item) {
    assert(ds != NULL && value != NULL && item != NULL);
    assert(ds->open_mode != B2_DS_WRONLY);

    int rc = BLOSC2_ERROR_SUCCESS;
    switch (ds->storage_type) {
    case B2_DS_MEMORY_SPARSE:
        s_b2_ds_item_set_local(item, value->f_mem.schunk);
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
    assert(ds->open_mode != B2_DS_RDONLY);
    assert(ds->storage_type == B2_DS_B2Z || ds->storage_type == B2_DS_B2D);
    // Also, key is malloc-ed

    *free_key = true;

    int rc = BLOSC2_ERROR_SUCCESS;
    uint8_t* prefix = NULL;
    size_t prefix_length = 0;
    if (ds->storage_type == B2_DS_B2Z) {
        prefix = ds->tmpdir;
        prefix_length = ds->tmpdir_length;
    } else {
        prefix = ds->urlpath;
        prefix_length = ds->urlpath_length;
    }

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
    size_t full_path_size = prefix_length + localpath_length + 2;
    uint8_t* full_path = malloc(full_path_size);
    if (full_path == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for path concatenation");
        rc = BLOSC2_ERROR_MEMORY_ALLOC;
        goto cleanup;
    }
    snprintf((char*)full_path, full_path_size, "%s/%s", (const char*)prefix, (const char*)localpath);
    rc = blosc2_create_dir_recursive((const char*)full_path);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not create recursively the directory");
        free(full_path);
        goto cleanup;
    }
    if (blosc2_atomic_schunk_write((const char*)full_path, schunk) < 0) {
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
    size_t destination_urlpath_length = strlen((const char*)destination_urlpath);
    size_t full_path_size = destination_urlpath_length + 1;
    uint8_t* full_path = NULL;

    while (hash_map_iter_next(piter)) {
        const uint8_t* key = hash_map_iter_get_key(piter);
        b2_ds_value* value = hash_map_iter_get_value(piter);
        blosc2_schunk* schunk = value->f_mem.schunk;

        uint8_t* key_with_extension = NULL;
        size_t key_with_extension_length = 0;
        rc = s_add_extension_to_key(key, schunk, &key_with_extension, &key_with_extension_length);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not add extension to key");
            break;
        }

        size_t new_full_path_size = key_with_extension_length + destination_urlpath_length + 2;
        if (new_full_path_size > full_path_size) {
            if (full_path != NULL) free(full_path);
            full_path_size = new_full_path_size;
            full_path = malloc(full_path_size);
            if (full_path == NULL) {
                BLOSC_TRACE_ERROR("Could not allocate memory for path concatenation");
                free(key_with_extension);
                rc = BLOSC2_ERROR_MEMORY_ALLOC;
                break;
            }
        }
        snprintf((char*)full_path, full_path_size, "%s/%s", (const char*)destination_urlpath, (const char*)key_with_extension);
        free(key_with_extension);
        rc = blosc2_create_dir_recursive((const char*)full_path);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not create directory at %s", (const char*)full_path);
            free(full_path);
        }
        if (blosc2_schunk_to_file(schunk, (const char*)full_path) < 0) {
            BLOSC_TRACE_ERROR("Could not write schunk to %s", (const char*)full_path);
            free(full_path);
            rc = BLOSC2_ERROR_FILE_WRITE;
            break;
        }
    }
    if (rc < 0) {
        BLOSC_TRACE_ERROR("There was an error creating a b2d from memory");
        if (s_inode_exists(destination_urlpath) != 0) {
            blosc2_remove_dir_recursive((const char*)destination_urlpath);
        }
    }
    if (full_path != NULL) free(full_path);
    return rc;
}

static int s_to_b2d_from_b2z(dict_store *ds, const uint8_t* destination_urlpath) {
    assert(ds != NULL && destination_urlpath != NULL);

    int error_code = BLOSC2_ERROR_SUCCESS;

    switch (ds->open_mode) {
        case B2_DS_RDONLY: {
            BLOSC_TRACE_ERROR("Cannot write to the filesystem in RDONLY mode");
            error_code = BLOSC2_ERROR_FILE_WRITE;
            break;
        }
        case B2_DS_WRONLY:
        case B2_DS_RDWR: {
            hash_map_iter iter = {0};
            hash_map_iter* piter = &iter;
            hash_map_iter_init(piter, &ds->map);

            const uint8_t* tmpdir = ds->tmpdir;
            uint8_t full_path_origin[PATH_MAX];
            uint8_t full_path_destination[PATH_MAX];

            while (hash_map_iter_next(piter)) {
                b2_ds_value* value = hash_map_iter_get_value(piter);
                const uint8_t* localpath = value->f_dir.localpath;
                if (snprintf((char*)full_path_origin, PATH_MAX, "%s/%s", (const char*)tmpdir, (const char*)localpath) < 0) {
                    BLOSC_TRACE_ERROR("The resultant string's length was greater than %d", PATH_MAX);
                    return BLOSC2_ERROR_MAX_BUFSIZE_EXCEEDED;
                }
                if (snprintf((char*)full_path_destination, PATH_MAX, "%s/%s", (const char*)destination_urlpath, (const char*)localpath) < 0) {
                    BLOSC_TRACE_ERROR("The resultant string's length was greater than %d", PATH_MAX);
                    return BLOSC2_ERROR_MAX_BUFSIZE_EXCEEDED;
                }
                int error = blosc2_create_dir_recursive((const char*)full_path_destination);
                if (error != 0) {
                    BLOSC_TRACE_ERROR("Could not create directories at %s", (const char*)full_path_destination);
                    return BLOSC2_ERROR_FAILURE;
                }

                blosc2_schunk* schunk = blosc2_schunk_open((const char*)full_path_origin);
                if (schunk == NULL) {
                    BLOSC_TRACE_ERROR("Could not read schunk from %s", (const char*)full_path_origin);
                    error_code = BLOSC2_ERROR_FILE_READ;
                    break;
                }
                blosc2_schunk_to_file(schunk, (const char*)full_path_destination);
                blosc2_schunk_free(schunk); // Free memory allocated
            }
            break;
        }
    }

    if (error_code != 0) {
        if (s_inode_exists(destination_urlpath) != 0) {
            blosc2_remove_dir_recursive((const char*)destination_urlpath);
        }
    } else {
        BLOSC_TRACE_ERROR("There was an error while converting the b2d to a b2z");
    }

    return error_code;
}

static int s_remove_b2x(dict_store* ds, const uint8_t* key) {
    assert(ds != NULL && key != NULL);
    assert(ds->open_mode != B2_DS_RDONLY);
    assert(ds->storage_type == B2_DS_B2Z || ds->storage_type == B2_DS_B2D);

    b2_ds_value* value = hash_map_get(&ds->map, key);
    uint8_t* prefix;
    size_t prefix_length;
    if (ds->storage_type == B2_DS_B2Z) {
        prefix = ds->tmpdir;
        prefix_length = ds->tmpdir_length;
    } else {
        prefix = ds->urlpath;
        prefix_length = ds->urlpath_length;
    }

    uint8_t* localpath = value->f_dir.localpath;
    size_t localpath_length = value->f_dir.localpath_length;

    char* full_path = NULL;
    size_t full_path_size = prefix_length + localpath_length + 2; // +1 for the / and +1 for NUL
    full_path = malloc(full_path_size);
    if (full_path == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory to create path");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    snprintf((char*)full_path, full_path_size, "%s/%s", (const char*)prefix, (const char*)localpath);

    struct stat st = {0};
    int error_code = BLOSC2_ERROR_SUCCESS;
    if (stat(full_path, &st) != 0) {
        BLOSC_TRACE_ERROR("Could not make stat over %s", full_path);
        error_code = BLOSC2_ERROR_FILE_REMOVE;
    }
    if ((st.st_mode & S_IFMT) != S_IFREG) {
        BLOSC_TRACE_ERROR("%s is not a regular file", full_path);
        error_code = BLOSC2_ERROR_FILE_REMOVE;
    }
    if (remove(full_path) != 0) {
        BLOSC_TRACE_ERROR("Could not remove %s", full_path);
        error_code = BLOSC2_ERROR_FILE_REMOVE;
    }

    free((void*)full_path);
    return error_code;
}

static int s_close_b2z( dict_store *ds ) {
    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (ds->open_mode) {
        case B2_DS_RDONLY: return BLOSC2_ERROR_SUCCESS;
        case B2_DS_WRONLY:
        case B2_DS_RDWR: {
            // Compress temporal directory
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
            error_code = blosc2_remove_dir_recursive((const char*)ds->tmpdir);
            break;
        }
        default: return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not close b2z");
    }
    return error_code;
}

/* == END Static functions (and some constants) == */

int blosc2_dict_store_new( blosc2_dict_store **pds, const uint8_t* urlpath ) {
    if (pds == NULL) {
        BLOSC_TRACE_ERROR("blosc2_dict_store pointer cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *pds = NULL;

    b2_ds_storage_type storage_type = B2_DS_MEMORY_SPARSE;
    b2_ds_open_mode open_mode = B2_DS_RDWR;
    cb_free_value cb = NULL;

    if (urlpath != NULL) {
        if (!blosc2_string_has_extension(urlpath, b2z_extension) && !blosc2_string_has_extension(urlpath, b2d_extension)) {
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
            if ((rc = blosc2_create_tmp_dir((char**)&tmpdir)) != 0) {
                BLOSC_TRACE_ERROR("Could not create temporal directory");
                blosc2_dict_store_free(ds);
                return rc;
            }
            ds->tmpdir = tmpdir;
            ds->tmpdir_length = strlen((const char*)tmpdir);
        } else {
            rc = blosc2_mkdir((const char*)urlpath);
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
        ds->open_mode = open_mode;
    }
    *pds = ds;
    return rc;
}

int blosc2_dict_store_open(blosc2_dict_store** pds, const uint8_t* urlpath, b2_ds_open_mode open_mode) {
    if (pds == NULL || urlpath == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (urlpath[0] == '\0') {
        BLOSC_TRACE_ERROR("urlpath must be different from the empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    // Check extension
    int storage_type = s_get_storage_type_from_extension(urlpath);
    if (storage_type == -1) { // error
        BLOSC_TRACE_ERROR("Could not infer the storage type from the urlpath's extension");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (open_mode) {
        case B2_DS_RDONLY: {
            error_code = s_open_rdonly(pds, urlpath, storage_type);
            break;
        }
        case B2_DS_WRONLY: {
            error_code = s_open_wronly(pds, urlpath, storage_type);
            break;
        }
        case B2_DS_RDWR: {
            error_code = s_open_rdwr(pds, urlpath, storage_type);
            break;
        }
        default: {
            error_code = BLOSC2_ERROR_INVALID_PARAM;
            break;
        }
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not open %s", (const char*)urlpath);
    }
    return error_code;
}

int blosc2_dict_store_close( blosc2_dict_store *ds ) {
    if (ds == NULL) {
        BLOSC_TRACE_ERROR("blosc2_dict_store pointer must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            return BLOSC2_ERROR_SUCCESS;
        }
        case B2_DS_B2Z: {
            int error_code = s_close_b2z(ds);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not close dict store");
            }
            return error_code;
        }
        case B2_DS_B2D: {
            return BLOSC2_ERROR_SUCCESS;
        }
        default: {
            BLOSC_TRACE_ERROR("Dict store was neither in memory, nor a b2z storage, nor a b2d storage");
            return BLOSC2_ERROR_INVALID_PARAM;
        }
    }
}

void blosc2_dict_store_free( blosc2_dict_store* dstore ) {
    assert(dstore != NULL);
    hash_map_free_struct(&dstore->map);
    if (dstore->urlpath != NULL) free(dstore->urlpath);
    if (dstore->tmpdir != NULL) free(dstore->tmpdir);
    free(dstore);
}

size_t blosc2_dict_store_get_length( blosc2_dict_store *ds ) {
    return ds->map.length;
}

size_t blosc2_dict_store_get_capacity( blosc2_dict_store *ds ) {
    return ds->map.capacity;
}

b2_ds_open_mode blosc2_dict_store_get_open_mode( blosc2_dict_store *ds ) {
    return ds->open_mode;
}

int blosc2_dict_store_get_storage_type( blosc2_dict_store *ds ) {
    return ds->storage_type;
}

bool blosc2_dict_store_contains_key( blosc2_dict_store *ds, const uint8_t* key ) {
    if (ds == NULL || key == NULL) return false;

    /* Keep the same key handling as get()/set()/remove(): canonical keys are
       searched as-is, non-canonical ones are normalized first. */
    if (blosc2_key_is_canonical(key)) {
        return hash_map_contains_key(&ds->map, key);
    }

    uint8_t* normalized_key = NULL;
    if (blosc2_key_normalize_copy(key, &normalized_key) != BLOSC2_ERROR_SUCCESS) {
        return false;
    }
    bool contains = hash_map_contains_key(&ds->map, normalized_key);
    free(normalized_key);
    return contains;
}

int blosc2_dict_store_to_b2z( blosc2_dict_store *ds, const uint8_t* destination_urlpath ) {

    if (ds == NULL || destination_urlpath == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (!blosc2_string_has_extension(destination_urlpath, (const uint8_t*)"b2z")) {
        BLOSC_TRACE_ERROR("destination_urlpath does not contain b2z as an extension");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (s_inode_exists(destination_urlpath) == 0) {
        BLOSC_TRACE_ERROR("%s already exists in the filesystem, aborting...", (const char*)destination_urlpath);
        return BLOSC2_ERROR_FILE_WRITE;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            error_code = s_to_b2z_from_mem(ds, destination_urlpath);
            break;
        }
        case B2_DS_B2Z: {
            BLOSC_TRACE_ERROR("Cannot make a to_b2z from a b2z");
            return BLOSC2_ERROR_FAILURE;
        }
        case B2_DS_B2D: {
            error_code = s_to_b2z_from_b2d(ds, destination_urlpath);
            break;
        }
        default: error_code = BLOSC2_ERROR_FAILURE;
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not dump to b2z");
    }
    return error_code;
}

int blosc2_dict_store_to_b2d( blosc2_dict_store *ds, const uint8_t* destination_urlpath ) {
    if (ds == NULL || destination_urlpath == NULL) {
        BLOSC_TRACE_ERROR("Every parameter must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (!blosc2_string_has_extension(destination_urlpath, b2d_extension)) {
        BLOSC_TRACE_ERROR("destination_urlpath does not contain b2d as an extension");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (s_inode_exists(destination_urlpath) == 0) {
        BLOSC_TRACE_ERROR("%s already exists in the filesystem, aborting...", (const char*)destination_urlpath);
        return BLOSC2_ERROR_FILE_WRITE;
    }
    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            error_code = s_to_b2d_from_memory(ds, destination_urlpath);
            break;
        }
        case B2_DS_B2Z: {
            error_code = s_to_b2d_from_b2z(ds, destination_urlpath);
            break;
        }
        case B2_DS_B2D: {
            error_code = BLOSC2_ERROR_FAILURE;
            BLOSC_TRACE_ERROR("Cannot make a to_b2z from a b2z");
            break;
        }
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Cannot make a to_b2d from a b2d");
    }
    return error_code;
}

int blosc2_dict_store_to_buffer( blosc2_dict_store *ds, void** pbuffer, size_t* psize ) {
    if (ds == NULL || pbuffer == NULL) {
        BLOSC_TRACE_ERROR("ds cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->storage_type == B2_DS_B2D || ds->storage_type == B2_DS_B2Z) {
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

    int error_code = s_zip_write_all_pairs(ds, za);

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

int blosc2_dict_store_from_buffer(blosc2_dict_store** pds, void* buffer, size_t size) {
    if (pds == NULL || buffer == NULL) {
        BLOSC_TRACE_ERROR("Cannot pass NULL arguments to this function");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (size == 0) {
        BLOSC_TRACE_ERROR("Size must be greater than 0");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *pds = NULL;
    bool free_dict_store = true;

    zip_archive *za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for zip_archive");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    if (!ziputils_reader_init_heap(za, buffer, size)) {
        BLOSC_TRACE_ERROR("Could not init zip_archive reader from buffer");
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
    ds->open_mode = B2_DS_RDONLY;

    while (ziputils_reader_iter_next(iter)) {
        uint8_t* filename = NULL; // Filename is malloc-ed
        size_t filename_length = 0;
        value val = {0};

        error_code = ziputils_reader_iter_get_filename(iter, &filename, &filename_length);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not allocate space for zip filename");
            goto cleanup;
        }

        error_code = blosc2_string_remove_extension_inplace(filename);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not remove extension to %s", (const char*)filename);
            free(filename);
            goto cleanup;
        }

        size_t out_length = 0;
        error_code = blosc2_key_normalize_inplace(filename, &out_length);
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
    *pds = ds;

cleanup:
    ziputils_reader_iter_free(iter);
    ziputils_reader_end(za);
    ziputils_archive_free(za);
    if (free_dict_store) blosc2_dict_store_free(ds);

    return error_code;
}

int blosc2_dict_store_get( blosc2_dict_store* ds, const uint8_t* key, b2_ds_item* item ) {
    if (ds == NULL || key == NULL || item == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->open_mode == B2_DS_WRONLY) {
        BLOSC_TRACE_ERROR("Cannot access items in WRONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    b2_ds_value* value = NULL;
    if (!blosc2_key_is_canonical(key)) {
        uint8_t* processed_key = NULL;
        error_code = blosc2_key_normalize_copy(key, &processed_key);
        if (error_code != 0) {
            return error_code;
        }
        value = hash_map_get(&ds->map, processed_key);
        free(processed_key);
    } else {
        value = hash_map_get(&ds->map, key);
    }

    if (value == NULL) {
        return BLOSC2_ERROR_NOT_FOUND;
    }

    error_code = s_get_from_value(ds, value, item);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve schunk with key: %s", (const char*)key);
    }
    return error_code;
}

int blosc2_dict_store_set( blosc2_dict_store* ds, const uint8_t* key, blosc2_schunk* container ) {
    if (container == NULL || ds == NULL || key == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->open_mode == B2_DS_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot call set when open_mode is RDONLY");
        return BLOSC2_ERROR_FAILURE;
    }
    if (key[0] == '\0') {
        BLOSC_TRACE_ERROR("key cannot be equal to the empty string");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    // The stripped key is a malloc(ed) string that belongs to the map
    uint8_t* processed_key = NULL;
    if (!blosc2_key_is_canonical(key)) {
        error_code = blosc2_key_normalize_copy(key, &processed_key);
        if (error_code != 0) {
            return error_code;
        }
    } else {
        error_code = blosc2_string_duplicate(key, &processed_key);
        if (error_code != 0) {
            return error_code;
        }
    }
    bool free_processed = true;

    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            value v = {0};
            v.f_mem.schunk = container;
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

int blosc2_dict_store_remove( blosc2_dict_store* ds, const uint8_t* key ) {
    if (ds == NULL || key == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->open_mode == B2_DS_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot remove elements in RDONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    };

    uint8_t* processed_key;
    int processed_key_status = blosc2_key_normalize_copy(key, &processed_key);
    if (processed_key_status != BLOSC2_ERROR_SUCCESS) {
        BLOSC_TRACE_ERROR("Could not process and validate key");
        return processed_key_status;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (ds->storage_type) {
    case B2_DS_B2Z:
    case B2_DS_B2D:
        error_code = s_remove_b2x(ds, processed_key);
        if (error_code < 0) {
            BLOSC_TRACE_ERROR("Could not remove element with key  %s", (const char*)key);
        }
        break;
    case B2_DS_MEMORY_SPARSE:
        error_code = hash_map_remove(&ds->map, processed_key);
        break;
    default:
        BLOSC_TRACE_ERROR("Invalid storage_type detected");
        error_code = BLOSC2_ERROR_INVALID_PARAM;
    }

    free((void*)processed_key);
    return error_code;
}

/* Iterator functions */
blosc2_dict_store_iter* blosc2_dict_store_iter_new( blosc2_dict_store *ds ) {
    if (ds == NULL) {
        BLOSC_TRACE_ERROR("ds must not be equal to NULL");
        return NULL;
    }

    blosc2_dict_store_iter* iter = calloc(1, sizeof(blosc2_dict_store_iter));
    hash_map_iter_init(&iter->map_iter, &ds->map);
    iter->dstore = ds;
    return iter;
}

void blosc2_dict_store_iter_free( blosc2_dict_store_iter* iter ) {
    assert(iter != NULL);
    free(iter);
}

bool blosc2_dict_store_iter_next( blosc2_dict_store_iter *iter ) {
    assert(iter != NULL);
    return hash_map_iter_next(&iter->map_iter);
}

const uint8_t* blosc2_dict_store_iter_get_key( blosc2_dict_store_iter *iter ) {
    assert(iter != NULL);
    return hash_map_iter_get_key(&iter->map_iter);
}

int blosc2_dict_store_iter_get_container( blosc2_dict_store_iter *iter, b2_ds_item* item ) {
    if (iter == NULL || item == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (iter->dstore->open_mode == B2_DS_WRONLY) {
        BLOSC_TRACE_ERROR("Cannot retrieve container from iterator in WRONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    b2_ds_value* value = hash_map_iter_get_value(&iter->map_iter);
    int rc = s_get_from_value(iter->dstore, value, item);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not retrieve item");
    }
    return rc;
}
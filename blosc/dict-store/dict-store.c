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
typedef b2_ds_pair pair;
typedef b2_ds_value value; // Recall that this is an union in C

/* == Static functions (and some constants) == */

// Used to mark tombstones
static const uint8_t tombstone_sentinel;
#define TOMBSTONE (&tombstone_sentinel)

static const uint8_t* b2z_extension = (const uint8_t*)"b2z";
//static const size_t b2z_extension_len = 3;
static const uint8_t* b2d_extension = (const uint8_t*)"b2d";
//static const size_t b2d_extension_len = 3;
static const uint8_t* schunk_extension = (const uint8_t*)"cframe";
static const size_t schunk_extension_len = 6;
static const uint8_t* b2nd_extension = (const uint8_t*)"b2nd";
static const size_t b2nd_extension_len = 3;

/* # Dict functions */

// (note): Creates a zero initialized struct
static int s_dict_store_alloc(dict_store **ds) {
    dict_store *pds = calloc(1, sizeof(dict_store));
    if (pds == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    pds->pairs = calloc(BLOSC2_DICT_STORE_INITIAL_CAPACITY, sizeof(pair));
    if (pds->pairs == NULL) {
        free(pds);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    pds->capacity = BLOSC2_DICT_STORE_INITIAL_CAPACITY;
    *ds = pds;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_dict_store_alloc_urlpath(dict_store **ds, const uint8_t* urlpath, int storage_type, b2_ds_open_mode open_mode) {
    *ds = NULL;
    dict_store *pds = NULL;
    int error_code = s_dict_store_alloc(&pds);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory for blosc2_dict_store structure");
        return error_code;
    }
    error_code = blosc2_string_duplicate(urlpath, (uint8_t**)&pds->urlpath);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory to duplicate string");
        free(pds->pairs);
        free(pds);
        return error_code;
    }
    pds->urlpath_length = strlen((const char*)pds->urlpath);
    pds->open_mode = open_mode;
    pds->storage_type = storage_type;
    *ds = pds;
    return error_code;
}

// (note): This "simple free", does not free individual pairs. It assumes that ds->pairs does not contain any pointer
static void s_dict_store_simple_free(dict_store *ds) {
    free(ds->pairs);
    free((void*)ds->urlpath);
    free(ds);
}

static bool s_dict_pair_is_live(const pair* p) {
    return p->key != NULL && p->key != TOMBSTONE;
}

static bool s_dict_pair_is_tombstone(const pair* p) {
    return p->key == TOMBSTONE;
}

static bool s_dict_pair_is_empty(const pair* p) {
    return p->key == NULL;
}

static bool s_dict_set_pair_hash(
    pair* pairs,
    size_t capacity,
    const uint8_t* key,
    b2_ds_value *ds_value,
    size_t* p_length, // If NULL, then a resize is being performed
    size_t* p_tombstones,
    uint64_t hash
) {
    size_t index = (size_t)(hash % capacity);
    pair *first_tomb = NULL;
    pair *p = &pairs[index];

    while (!s_dict_pair_is_empty(p)) {
        if (s_dict_pair_is_tombstone(p)) {
            if (first_tomb == NULL) first_tomb = p;
        } else if (p->hash == hash && strcmp((const char*)p->key, (const char*)key) == 0) {
            p->value = *ds_value;
            return false;
        }

        index += 1;
        if (index >= capacity) index = 0;
        p = &pairs[index];
    }

    if (first_tomb != NULL) {
        p = first_tomb;
        if (p_tombstones != NULL) *p_tombstones -= 1;
    }
    if (p_length != NULL) *p_length += 1;

    p->key = key;
    p->hash = hash;
    p->value = *ds_value;
    return true;
}

// Returns true if the key was setted. That way, the user code can check whether it needs to free the key
// or not
static bool s_dict_set_pair(
    pair* pairs,
    size_t capacity,
    const uint8_t* key,
    b2_ds_value *ds_value,
    size_t* p_length, // If NULL, then a resize is being performed
    size_t* p_tombstones
) {
    uint64_t hash = hash_fnv_1a((const char*)key);
    return s_dict_set_pair_hash(pairs, capacity, key, ds_value, p_length, p_tombstones, hash);
}

static pair* s_dict_find_pair(dict_store *ds, const uint8_t* key) {
    uint64_t hash = hash_fnv_1a((const char*)key);
    size_t index = (size_t)(hash % ds->capacity);

    for (size_t i = 0; i < ds->capacity; i += 1) {
        pair *p = &ds->pairs[index];
        if (s_dict_pair_is_empty(p)) return NULL;
        if (s_dict_pair_is_live(p) && p->hash == hash && strcmp((const char*)p->key, (const char*)key) == 0) {
            return p;
        }
        index += 1;
        if (index >= ds->capacity) index = 0;
    }
    return NULL;
}

static bool s_dict_needs_resize(dict_store *ds) {
    return (float)(ds->length + ds->tombstones) >= BLOSC2_DICT_STORE_LOAD_FACTOR * (float)(ds->capacity);
}

static bool s_dict_resize(dict_store* ds) {
    size_t new_capacity = ds->capacity;

    if ((float)ds->length >= (float)ds->capacity * BLOSC2_DICT_STORE_LOAD_FACTOR / 2.0f) {
        new_capacity = ds->capacity * BLOSC2_DICT_STORE_RESIZE_FACTOR;
        if (new_capacity < ds->capacity) return false; // Overflow
    }

    pair *new_pairs = (pair*)calloc(new_capacity, sizeof(pair));
    if (new_pairs == NULL) return false;

    for (size_t i = 0; i < ds->capacity; i += 1) {
        pair *p = &ds->pairs[i];
        if (s_dict_pair_is_live(p)) {
            s_dict_set_pair_hash(new_pairs, new_capacity, p->key, &p->value, NULL, NULL, p->hash);
        }
    }

    free(ds->pairs);
    ds->pairs = new_pairs;
    ds->capacity = new_capacity;
    ds->tombstones = 0;
    return true;
}

static int s_dict_store_put(dict_store *ds, const uint8_t* key, value *val, bool *needs_free) {
    if (s_dict_needs_resize(ds)) {
        if (!s_dict_resize(ds)) {
            BLOSC_TRACE_ERROR("Could not resize blosc2_dict_store, aborting...");
            return BLOSC2_ERROR_MEMORY_ALLOC;
        }
    }

    *needs_free = !s_dict_set_pair(ds->pairs, ds->capacity, key, val, &ds->length, &ds->tombstones);
    return BLOSC2_ERROR_SUCCESS;
}

static void s_dict_free_keys(dict_store *ds) {
    for (size_t i = 0; i < ds->capacity; i+=1) {
        pair *p = &ds->pairs[i];
        if (s_dict_pair_is_live(p)) {
            free((void*)p->key);
        }
    }
}

static void s_dict_free_keys_and_localpaths(dict_store *ds) {
    for (size_t i = 0; i < ds->capacity; i+=1) {
        pair *p = &ds->pairs[i];
        if (s_dict_pair_is_live(p)) {
            free((void*)p->key);
            free((void*)p->value.f_dir.localpath);
        }
    }
}

static void s_dict_free_b2d(dict_store *ds) {
    s_dict_free_keys_and_localpaths(ds);
    free((void*)ds->urlpath);
    free(ds->pairs);
    free(ds);
}

static void s_dict_free_b2z(dict_store *ds) {
    switch (ds->open_mode) {
    case B2_DS_RDONLY: {
        s_dict_free_keys(ds);
        free((void*)ds->urlpath);
        break;
    }

    // (note): Write, Append is the same thing
    case B2_DS_WRONLY:
    case B2_DS_RDWR: {
        s_dict_free_keys_and_localpaths(ds);
        free((void*)ds->urlpath);
        if (ds->tmpdir) free((void*)ds->tmpdir);
        break;
    }
    }
    free(ds->pairs);
    free(ds);
}

static void s_dict_free_mem(dict_store *ds) {
    s_dict_free_keys(ds);
    free(ds->pairs);
    free(ds);
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

static int s_zip_write_all_pairs(dict_store *ds, zip_archive *za) {
    // Preconditions:
    //              - ds is valid
    //              - za is valid and in writer mode

    int error_code = BLOSC2_ERROR_SUCCESS;
    for (size_t i = 0; i < ds->capacity; i+=1) {
        pair *p = &ds->pairs[i];
        if (s_dict_pair_is_live(p) && p->value.f_mem.schunk != NULL) {

            uint8_t* concat = NULL;
            int64_t frame_len = 0;
            uint8_t* frame_buffer = NULL;
            bool needs_free = false;

            error_code = s_add_extension_to_key(p->key, p->value.f_mem.schunk, &concat, NULL);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not allocate memory to set extension to key");
                error_code = BLOSC2_ERROR_MEMORY_ALLOC;
                break;
            }

            frame_len = blosc2_schunk_to_buffer(p->value.f_mem.schunk, &frame_buffer, &needs_free);
            if (frame_len < 0) {
                BLOSC_TRACE_ERROR("Could not convert schunk to cframe");
                error_code = (int)frame_len;
                free((void*)concat);
                break;
            }
            bool ok = ziputils_writer_add_entry(za, concat, (void*)frame_buffer, (size_t)frame_len);
            if (needs_free) {
                free((void*)frame_buffer);
            }
            if (!ok) {
                BLOSC_TRACE_ERROR("Could not add cframe to zip archive");
                error_code = BLOSC2_ERROR_WRITE_BUFFER;
                free(concat);
                break;
            }
            free((void*)concat);
        }
    }

    return error_code;
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
            error_code = s_dict_store_put(ds, key, &v, &needs_free);
            if (needs_free) {
                free(key);
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

    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, B2_DS_RDONLY);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    while (ziputils_reader_iter_next(iter)) {
        const uint8_t *filename = NULL;
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
        error_code = s_dict_store_put(pds, filename_without_extension, &val, &needs_free);
        if (needs_free) {
            free((void*)filename_without_extension);
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, B2_DS_RDONLY);
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
    const uint8_t* tmpdir = NULL;
    int error_code = BLOSC2_ERROR_SUCCESS;

    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, B2_DS_WRONLY);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = blosc2_create_tmp_dir((char**)&tmpdir);
    if (error_code != 0) {
        s_dict_store_simple_free(pds);
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, B2_DS_WRONLY);
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

    //if (!ziputils_validate_file(urlpath)) {
    //    return BLOSC2_ERROR_FILE_OPEN;
    //}

    bool free_pds = true;
    dict_store *pds = NULL;
    bool remove_tmpdir = true;
    const uint8_t* tmpdir = NULL;
    size_t tmpdir_length = 0;
    zip_archive *za = NULL;
    zip_archive_iter *iter = NULL;

    int error_code = BLOSC2_ERROR_SUCCESS;
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2Z, B2_DS_RDWR);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate blosc2_dict_store");
        return error_code;
    }

    error_code = blosc2_create_tmp_dir((char**)&tmpdir);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not create temporal directory at %s", (const char*)urlpath);
        s_dict_store_simple_free(pds);
        return error_code;
    }
    tmpdir_length = strlen((const char*)tmpdir);

    za = ziputils_archive_new();
    if (za == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate new zip_archive");
        s_dict_store_simple_free(pds);
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
        const uint8_t* key = NULL;

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
        error_code = s_dict_store_put(pds, key, &val, &needs_free);
        if (needs_free) {
            free((void*)key);
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
    error_code = s_dict_store_alloc_urlpath(&pds, urlpath, B2_DS_B2D, B2_DS_RDWR);
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
static int s_get_mem_contiguous(dict_store *ds, const pair* p, blosc2_schunk **pcontainer) {
    // Preconditions
    //      - ds is valid. If open_mode is write or append, then tmpdir is not NULL and valid and tmpdir_length > 0
    //      - p is not null and belongs in ds->pairs, that means, that it is not required for this function to check
    //      if the pair exists
    //      - pcontainer is not NULL

    int64_t offset = p->value.f_zip.offset;
    size_t length = p->value.f_zip.length;
    const uint8_t* ptr = &ds->urlpath[offset];
    blosc2_schunk *schunk = blosc2_schunk_from_buffer((uint8_t*)ptr, (int64_t)length, true);
    if (schunk == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    *pcontainer = schunk;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_get_b2x(dict_store *ds, const pair* p, const uint8_t* prefix, size_t prefix_length, blosc2_schunk **pcontainer) {
    // Preconditions
    //      - ds is valid. If open_mode is write or append, then tmpdir is not NULL and valid and tmpdir_length > 0 Also, dict_store is either B2Z or B2D
    //          also, dict_store is either RDONLY or RDWR
    //      - p is not null and belongs in ds->pairs, that means, that it is not required for this function to check
    //      if the pair exists
    //      - prefix is not null
    //      - prefix_length > 0
    //      - pcontainer is not NULL

    bool free_path = false;
    const uint8_t* path = prefix;
    int64_t offset = 0;

    switch (ds->open_mode) {
        case B2_DS_RDONLY: {
            if (ds->storage_type == B2_DS_B2Z) {
                offset = p->value.f_zip.offset;
                break;
            }
            // ds->storage_type == B2_DS_B2Z
            const uint8_t* localpath = p->value.f_dir.localpath;
            size_t localpath_len = p->value.f_dir.localpath_length;

            // TODO: Maybe, do better path joining
            size_t concat_len = prefix_length + localpath_len + 2;
            uint8_t* concat = NULL;
            concat = malloc(concat_len);
            if (concat == NULL) {
                BLOSC_TRACE_ERROR("Could not allocate memory");
                return BLOSC2_ERROR_MEMORY_ALLOC;
            }
            snprintf((char*)concat, concat_len, "%s/%s", (const char*)prefix, (const char*)localpath);
            free_path = true;
            path = concat;
            break;
        }
        case B2_DS_RDWR: {
            const uint8_t* localpath = p->value.f_dir.localpath;
            size_t localpath_len = p->value.f_dir.localpath_length;

            // TODO: Maybe, do better path joining
            size_t concat_len = prefix_length + localpath_len + 2;
            uint8_t* concat = NULL;
            concat = malloc(concat_len);
            if (concat == NULL) {
                BLOSC_TRACE_ERROR("Could not allocate memory");
                return BLOSC2_ERROR_MEMORY_ALLOC;
            }
            snprintf((char*)concat, concat_len, "%s/%s", (const char*)prefix, (const char*)localpath);
            free_path = true;
            path = concat;
            break;
        }
        default: return BLOSC2_ERROR_INVALID_PARAM;
    }

    *pcontainer = blosc2_schunk_open_offset((const char*)path, offset);
    int error_code = BLOSC2_ERROR_SUCCESS;
    if (*pcontainer == NULL) {
        BLOSC_TRACE_ERROR("Could not open schunk at file %s with offset %ld", (const char*)path, offset);
        error_code = BLOSC2_ERROR_FILE_OPEN;
    }
    if (free_path) free((void*)path);

    return error_code;
}

static int s_get_from_pair(dict_store *ds, pair *p, blosc2_schunk **pcontainer) {
    int error_code = BLOSC2_ERROR_SUCCESS;
    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE: {
            *pcontainer = p->value.f_mem.schunk;
            break;
        }
        case B2_DS_MEMORY_CONTIGUOUS: {
            error_code = s_get_mem_contiguous(ds, p, pcontainer);
            break;
        }
        case B2_DS_B2Z: {
            const uint8_t* prefix = NULL;
            size_t prefix_length = 0;
            if (ds->open_mode != B2_DS_RDONLY) {
                prefix = ds->tmpdir;
                prefix_length = ds->tmpdir_length;
            } else {
                prefix = ds->urlpath;
                prefix_length = ds->urlpath_length;
            }
            error_code = s_get_b2x(ds, p, prefix, prefix_length, pcontainer);
            break;
        }
        case B2_DS_B2D: {
            error_code = s_get_b2x(ds, p, ds->urlpath, ds->urlpath_length, pcontainer);
            break;
        }
    }
    return error_code;
}

static int s_set_b2x(dict_store *ds, const uint8_t* key, blosc2_schunk *container, bool *free_key) {
    // Preconditions
    //          - ds is valid and storage type is b2z
    //          - key is already processed
    //          - container is not NULL
    //          - free_key is not null
    // (note): There is not warranty that ds[key] exists. Get functions receive this privilege, set functions do not

    *free_key = true;

    if (ds->open_mode == B2_DS_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot call set in RDONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    int error_code = BLOSC2_ERROR_SUCCESS;
    pair *p = s_dict_find_pair(ds, key);

    const uint8_t* prefix_path = NULL;
    const uint8_t* localpath = NULL;
    size_t localpath_length = 0;
    uint8_t full_path[PATH_MAX];

    bool owns_localpath = p == NULL; // Used later to free memory

    if (ds->storage_type == B2_DS_B2Z) {
        prefix_path = ds->tmpdir;
    } else {
        prefix_path = ds->urlpath;
    }

    // 1. Obtain localpath (new pair: build it; existing pair: it belongs to the dict)
    if (p == NULL) {
        error_code = s_add_extension_to_key(key, container, (uint8_t**)&localpath, &localpath_length);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not add extension to %s", (const char*)key);
            goto cleanup;
        }
    } else {
        localpath = p->value.f_dir.localpath;
        localpath_length = p->value.f_dir.localpath_length;
    }

    // 2. Build full path = tmpdir + "/" + localpath
    if (ds->tmpdir_length + localpath_length + 1 >= PATH_MAX) goto cleanup;
    snprintf((char*)full_path, PATH_MAX, "%s/%s", (const char*)prefix_path, (const char*)localpath);
    blosc2_create_dir_recursive((const char*)full_path);

    // 3. Write the file (the file goes first, the registration last)
    if (blosc2_atomic_schunk_write((const char*)full_path, container) != 0) {
        BLOSC_TRACE_ERROR("Could not write atomically the schunk to %s", (const char*)full_path);
        error_code = BLOSC2_ERROR_FILE_WRITE;
        goto cleanup;
    }

    // 4. Register the new pair; the dict now owns localpath
    if (p == NULL) {
        value v = {0};
        v.f_dir.localpath = localpath;
        v.f_dir.localpath_length = localpath_length;
        error_code = s_dict_store_put(ds, key, &v, free_key);
        owns_localpath = false;
    }

cleanup:
    if (owns_localpath)
        free((void*)localpath);
    return error_code;
}

static int s_to_b2z_from_mem(dict_store *ds, const uint8_t* destination_urlpath) {
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
    if (!ziputils_archive_directory(ds->urlpath, destination_urlpath)) {
        BLOSC_TRACE_ERROR("Could not archive directory %s from b2z", (const char*)destination_urlpath);
        return BLOSC2_ERROR_FAILURE;
    }
    return BLOSC2_ERROR_SUCCESS;
}

static int s_to_b2d_from_memory(dict_store *ds, const uint8_t* destination_urlpath) {
    // Preconditions:
    //          - ds is valid
    //          - destination_urlpath is not null and it does not exist in the filesystem

    int error_code = BLOSC2_ERROR_SUCCESS;
    blosc2_dict_store_iter iter = {0};
    iter.ds = ds;

    uint8_t full_path[PATH_MAX];
    const size_t full_path_length = PATH_MAX;
    while (blosc2_dict_store_iter_next(&iter)) {
        const uint8_t* key = iter.p->key;
        blosc2_schunk *schunk = iter.p->value.f_mem.schunk;
        // Add extension to key
        const uint8_t* key_with_extension = NULL;
        size_t key_with_extension_length = 0;
        error_code = s_add_extension_to_key(key, schunk, (uint8_t**)&key_with_extension, &key_with_extension_length);
        if (error_code != 0) {
            if (key_with_extension == NULL) free((void*)key_with_extension);
            break;
        }
        if (key_with_extension_length >= PATH_MAX) {
            BLOSC_TRACE_ERROR("Key with extension (%s) is larger or equal than %d", (const char*)key_with_extension, PATH_MAX);
            free((void*)key_with_extension);
            error_code = BLOSC2_ERROR_MAX_BUFSIZE_EXCEEDED;
            break;
        }
        snprintf((char*)full_path, full_path_length, "%s/%s", (const char*)destination_urlpath, (const char*)key_with_extension);
        error_code = blosc2_create_dir_recursive((const char*)full_path);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not create directory at %s", (const char*)full_path);
            free((void*)key_with_extension);
            break;
        }

        if (blosc2_schunk_to_file(schunk, (const char*)full_path) < 0) {
            BLOSC_TRACE_ERROR("Could not write schunk to %s", (const char*)full_path);
            free((void*)key_with_extension);
            error_code = BLOSC2_ERROR_FILE_WRITE;
            break;
        }
        free((void*)key_with_extension);
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("There was an error creating a b2d from memory");
        // Remove destination directory
        if (s_inode_exists(destination_urlpath) != 0) {
            blosc2_remove_dir_recursive((const char*)destination_urlpath);
        }
    }

    return error_code;
}

static int s_to_b2d_from_b2z(dict_store *ds, const uint8_t* destination_urlpath) {
    int error_code = BLOSC2_ERROR_SUCCESS;

    switch (ds->open_mode) {
        case B2_DS_RDONLY: {
            BLOSC_TRACE_ERROR("Cannot write to the filesystem in RDONLY mode");
            error_code = BLOSC2_ERROR_FILE_WRITE;
            break;
        }
        case B2_DS_WRONLY:
        case B2_DS_RDWR: {
            blosc2_dict_store_iter iter = {0};
            iter.ds = ds;
            const uint8_t* tmpdir = ds->tmpdir;
            uint8_t full_path_origin[PATH_MAX];
            uint8_t full_path_destination[PATH_MAX];

            while (blosc2_dict_store_iter_next(&iter)) {
                const uint8_t* localpath = iter.p->value.f_dir.localpath;
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

static int s_remove_b2x(pair *p, const uint8_t* prefix, size_t prefix_length) {
    // Preconditions: ds is either in write or rdwr mode and, storage type is either B2Z or B2D.

    const uint8_t* localpath = p->value.f_dir.localpath;
    size_t localpath_length = p->value.f_dir.localpath_length;
    const char* full_path = NULL;
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

    int storage_type = B2_DS_MEMORY_SPARSE;
    b2_ds_open_mode open_mode = B2_DS_RDWR;

    if (pds == NULL) {
        BLOSC_TRACE_ERROR("blosc2_dict_store pointer cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *pds = NULL;
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
        if (storage_type == -1) {
            BLOSC_TRACE_ERROR("Could not infer storage type from urlpath, a valid extension must be provided");
            return BLOSC2_ERROR_INVALID_PARAM;
        }
    }

    dict_store *ds;
    int error_code = s_dict_store_alloc(&ds);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory for a blosc2_dict_store");
        return error_code;
    }

    if (urlpath != NULL) {
        error_code = blosc2_string_duplicate(urlpath, (uint8_t**)&ds->urlpath);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not allocate memory to duplicate string");
            free(ds->pairs);
            free(ds);
            return error_code;
        }
        ds->urlpath_length = strlen((const char*)urlpath);

        if (storage_type == B2_DS_B2Z) {
            const uint8_t* tmpdir = NULL;
            if (blosc2_create_tmp_dir((char**)&tmpdir) != 0) {
                BLOSC_TRACE_ERROR("Could not create temporal directory");
                s_dict_store_simple_free(ds);
                return BLOSC2_ERROR_FAILURE;
            }
            ds->tmpdir = tmpdir;
            ds->tmpdir_length = strlen((const char*)tmpdir);
        } else {
            error_code = blosc2_mkdir((const char*)urlpath);
            if (error_code != 0) {
                BLOSC_TRACE_ERROR("Could not create directory at %s", (const char*)urlpath);
                s_dict_store_simple_free(ds);
                return error_code;
            }
        }
    }
    ds->storage_type = storage_type;
    ds->open_mode = open_mode;

    *pds = ds;
    return BLOSC2_ERROR_SUCCESS;
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

void blosc2_dict_store_free( blosc2_dict_store *ds ) {
    if (ds == NULL) return;

    switch (ds->storage_type) {
        case B2_DS_MEMORY_SPARSE:
        case B2_DS_MEMORY_CONTIGUOUS: {
            s_dict_free_mem(ds);
            break;
        }
        case B2_DS_B2Z: {
            s_dict_free_b2z(ds);
            break;
        }
        case B2_DS_B2D: {
            s_dict_free_b2d(ds);
            break;
        }
    }
}

size_t blosc2_dict_store_get_length( blosc2_dict_store *ds ) {
    return ds->length;
}

size_t blosc2_dict_store_get_capacity( blosc2_dict_store *ds ) {
    return ds->capacity;
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
        return s_dict_find_pair(ds, key) != NULL;
    }

    uint8_t* normalized_key = NULL;
    if (blosc2_key_normalize_copy(key, &normalized_key) != BLOSC2_ERROR_SUCCESS) {
        return false;
    }
    bool contains = s_dict_find_pair(ds, normalized_key) != NULL;
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
    int error_code = s_dict_store_alloc(&ds);
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

        error_code = ziputils_reader_iter_get_filename(iter, (const uint8_t**)&filename, &filename_length);
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
        error_code = s_dict_store_put(ds, filename, &val, &needs_free);
        if (needs_free) free(filename);
    }

    ds->urlpath = (const uint8_t*)buffer;
    free_dict_store = false;
    *pds = ds;

cleanup:
    ziputils_reader_iter_free(iter);
    ziputils_reader_end(za);
    ziputils_archive_free(za);
    if (free_dict_store) s_dict_free_mem(ds);

    return error_code;
}

int blosc2_dict_store_get( blosc2_dict_store* ds, const uint8_t* key, blosc2_schunk** pcontainer ) {
    if (ds == NULL || key == NULL || pcontainer == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (ds->open_mode == B2_DS_WRONLY) {
        BLOSC_TRACE_ERROR("Cannot access items in WRONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *pcontainer = NULL;

    int error_code = BLOSC2_ERROR_SUCCESS;

    pair *p = NULL;
    uint8_t* processed_key = NULL;
    if (!blosc2_key_is_canonical(key)) {
        error_code = blosc2_key_normalize_copy(key, &processed_key);
        if (error_code != 0) {
            return error_code;
        }
        p = s_dict_find_pair(ds, processed_key);
    } else {
        p = s_dict_find_pair(ds, key);
    }

    if (p != NULL) {
        error_code = s_get_from_pair(ds, p, pcontainer);
    } else {
        error_code = BLOSC2_ERROR_NOT_FOUND;
    }

    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve schunk with key: %s", (const char*)key);
    }
    if (processed_key != NULL) free(processed_key);
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
            error_code = s_dict_store_put(ds, processed_key, &v, &free_processed);
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

    pair *p = s_dict_find_pair(ds, processed_key);
    int error_code = BLOSC2_ERROR_SUCCESS;
    if (p != NULL) {
        switch (ds->storage_type) {
            case B2_DS_B2Z: {
                error_code = s_remove_b2x(p, ds->tmpdir, ds->tmpdir_length);
                if (error_code != 0) {
                    BLOSC_TRACE_ERROR("Could not remove element with key: %s", (const char*)key);
                    break;
                }
                free((void*)p->value.f_dir.localpath);
                p->value.f_dir.localpath = NULL;
                p->value.f_dir.localpath_length = 0;
                break;
            }
            case B2_DS_B2D: {
                error_code = s_remove_b2x(p, ds->urlpath, ds->urlpath_length);
                if (error_code != 0) {
                    BLOSC_TRACE_ERROR("Could not remove element with key: %s", (const char*)key);
                    break;
                }
                free((void*)p->value.f_dir.localpath);
                p->value.f_dir.localpath = NULL;
                p->value.f_dir.localpath_length = 0;
                break;
            }
        }
        if (error_code == 0) {
            free((void*) p->key);
            p->key = TOMBSTONE;
            p->hash = 0;
            memset(&p->value, 0, sizeof(p->value));
            ds->length -= 1;
            ds->tombstones += 1;
        }
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
    if (!iter) return NULL;
    iter->ds = ds;
    return iter;
}

void blosc2_dict_store_iter_free( blosc2_dict_store_iter* iter ) {
    if (iter != NULL) free(iter);
}

bool blosc2_dict_store_iter_next( blosc2_dict_store_iter *iter ) {
    dict_store *ds = iter->ds;
    size_t index = iter->index;

    bool found = false;
    for (; index < ds->capacity; index += 1) {
        pair *p = &ds->pairs[index];
        if (s_dict_pair_is_live(p)) { // A pair was found
            iter->p = p;
            index += 1; // Set to the next index so it does not always return the same pair
            found = true;
            break;
        }
    }

    iter->index = index;
    return found;
}

const uint8_t* blosc2_dict_store_iter_get_key( blosc2_dict_store_iter *iter ) {
    return iter->p->key;
}

int blosc2_dict_store_iter_get_container( blosc2_dict_store_iter *iter, blosc2_schunk** pcontainer ) {
    if (iter == NULL || pcontainer == NULL) {
        BLOSC_TRACE_ERROR("All parameters must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (iter->ds->open_mode == B2_DS_WRONLY) {
        BLOSC_TRACE_ERROR("Cannot retrieve container from iterator in WRONLY mode");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *pcontainer = NULL;

    int error_code = BLOSC2_ERROR_SUCCESS;
    error_code = s_get_from_pair(iter->ds, iter->p, pcontainer);
    if (error_code != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve container from iterator");
    }
    return error_code;
}
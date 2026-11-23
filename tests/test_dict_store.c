/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "context.h"
#include "test_common.h"
#include <blosc2/dict-store.h>
#include <blosc2.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

int tests_run = 0;

#define SCHUNKS_SIZE 40
//#define END_TEST printf("(%d) TEST SUCCESS\n", tests_run)
#define END_TEST

static blosc2_dict_store *ds = NULL;

static blosc2_cparams cparams;
static blosc2_dparams dparams;
static blosc2_io io;
static blosc2_storage b2_storage = {0};

static blosc2_schunk* b2_schunks[SCHUNKS_SIZE];

/* By default, files created by a test are removed in the teardown. Tests that
   need to preserve an archive for the next test set this flag to true. */
static bool keep_file = false;

static const char* paths[SCHUNKS_SIZE] = {
    "/archive/one/data0",
    "/archive/raw/data1",
    "/archive/raw/data2",
    "/archive/processed/chunk0",
    "/archive/processed/chunk1",
    "/archive/processed/chunk2",
    "/archive/public/arr0",
    "/archive/public/arr1",
    "/archive/public/arr3",
    "/archive/extra",
    "/archive_new/one/data0",
    "/archive_new/raw/data1",
    "/archive_new/raw/data2",
    "/archive_new/processed/chunk0",
    "/archive_new/processed/chunk1",
    "/archive_new/processed/chunk2",
    "/archive_new/public/arr0",
    "/archive_new/public/arr1",
    "/archive_new/public/arr3",
    "/archive_new/extra",
    "/archive_third/one/data0",
    "/archive_third/raw/data1",
    "/archive_third/raw/data2",
    "/archive_third/processed/chunk0",
    "/archive_third/processed/chunk1",
    "/archive_third/processed/chunk2",
    "/archive_third/public/arr0",
    "/archive_third/public/arr1",
    "/archive_third/public/arr3",
    "/archive_third/extra",
    "/archive_fourth/one/data0",
    "/archive_fourth/raw/data1",
    "/archive_fourth/raw/data2",
    "/archive_fourth/processed/chunk0",
    "/archive_fourth/processed/chunk1",
    "/archive_fourth/processed/chunk2",
    "/archive_fourth/public/arr0",
    "/archive_fourth/public/arr1",
    "/archive_fourth/public/arr3",
    "/archive_fourth/extra",
};

static const uint8_t* archive_path = (const uint8_t*)"archive.b2z";
static const uint8_t* wrong_archive_path = (const uint8_t*)"archive.zip";

static const uint8_t* dir_path = (const uint8_t*)"dir.b2d";
static const uint8_t* wrong_dir_path = (const uint8_t*)"dir";

static uint8_t buffer[SCHUNKS_SIZE][SCHUNKS_SIZE * SCHUNKS_SIZE];

/* == Static functions == */

static bool s_exists_inode(const uint8_t* path) {
    struct stat buf;
    if (stat((const char*)path, &buf) == 0) {
        return true;
    }
    return false;
}

static int remove_dir_recursive(const uint8_t* path) {
    const char* dirname = (const char*)path;
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        // "--" evita que un nombre que empiece por '-' se interprete como opción
        execlp("rm", "rm", "-rf", "--", dirname, (char*)NULL);
        _exit(127);   // solo se llega aquí si exec falla
    }

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static void s_setup_schunks(void) {

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        for (size_t j = 0; j < SCHUNKS_SIZE * SCHUNKS_SIZE; j+=1) {
            buffer[i][j] = j;
        }
    }

    cparams = blosc2_get_blosc2_cparams_defaults();
    dparams = blosc2_get_blosc2_dparams_defaults();
    io = blosc2_get_blosc2_io_defaults();

    b2_storage.contiguous = true;
    b2_storage.cparams = &cparams;
    b2_storage.dparams = &dparams;
    b2_storage.io = &io;

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_schunk* pschunk = blosc2_schunk_new(&b2_storage);
        blosc2_schunk_append_buffer(pschunk, buffer[i], SCHUNKS_SIZE * SCHUNKS_SIZE);
        b2_schunks[i] = pschunk;
    }
}

static void s_teardown(void) {
    if (ds != NULL) {
        blosc2_dict_store_free(ds);
        ds = NULL;
    }
    if (!keep_file) {
        remove((const char*)archive_path);
    }
    keep_file = false;
}

static int s_generate_b2z(const uint8_t* destination_urlpath) {
    int error_code = BLOSC2_ERROR_SUCCESS;
    blosc2_dict_store *store = NULL;

    remove_dir_recursive(dir_path);

    error_code = blosc2_dict_store_new(&store, dir_path);
    if (error_code != 0) {
        return error_code;
    }

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        error_code = blosc2_dict_store_set(store, (const uint8_t*)paths[i], b2_schunks[i]);
        if (error_code != 0) {
            blosc2_dict_store_free(store);
            remove_dir_recursive(dir_path);
            return error_code;
        }
    }

    error_code = blosc2_dict_store_to_b2z(store, destination_urlpath);
    blosc2_dict_store_free(store);
    remove_dir_recursive(dir_path);
    return error_code;
}

static int s_generate_b2d(const uint8_t* urlpath) {
    int error_code = BLOSC2_ERROR_SUCCESS;
    blosc2_dict_store *store = NULL;

    remove_dir_recursive(urlpath);

    error_code = blosc2_dict_store_new(&store, urlpath);
    if (error_code != 0) {
        return error_code;
    }

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        error_code = blosc2_dict_store_set(store, (const uint8_t*)paths[i], b2_schunks[i]);
        if (error_code != 0) {
            blosc2_dict_store_free(store);
            remove_dir_recursive(urlpath);
            return error_code;
        }
    }

    error_code = blosc2_dict_store_close(store);
    blosc2_dict_store_free(store);
    return error_code;
}

/* == END Static functions == */

static char* test_dict_store_new_memory(void) {
    mu_assert("ERROR: Returned success when error was expected",
        blosc2_dict_store_new(NULL, NULL) != 0);

    mu_assert("ERROR: Returned failure when success was expected",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);
    mu_assert("ERROR: A new in-memory dict_store must be empty",
        blosc2_dict_store_get_length(ds) == 0);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_set_memory(void) {
    mu_assert("ERROR: Expected a NULL pointer error when ds is NULL",
        blosc2_dict_store_set(ds, (const uint8_t*)paths[0], NULL) == BLOSC2_ERROR_NULL_POINTER);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    mu_assert("ERROR: Expected an error when the container is NULL",
        blosc2_dict_store_set(ds, (const uint8_t*)"/some/path", NULL) != 0);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }

    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    blosc2_dict_store_iter* iter = blosc2_dict_store_iter_new(ds);
    mu_assert("ERROR: Could not create a dict_store iterator", iter != NULL);

    bool next = blosc2_dict_store_iter_next(iter);
    while (next) {
        const uint8_t* key = blosc2_dict_store_iter_get_key(iter);
        mu_assert("ERROR: The iterator returned a NULL key", key != NULL);

        next = blosc2_dict_store_iter_next(iter);
    }
    blosc2_dict_store_iter_free(iter);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_get_memory(void) {
    mu_assert("ERROR: Expected an error when the output container is NULL",
        blosc2_dict_store_get(ds, (const uint8_t*)paths[0], NULL) != 0);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    for (int i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }

    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    b2_ds_item item = {0};
    for (int i = 0; i < SCHUNKS_SIZE; i+=1) {
        const uint8_t* key_to_get = (const uint8_t*)paths[i];
        mu_assert("ERROR: Could not get a schunk from the dict_store",
            blosc2_dict_store_get(ds, key_to_get, &item) == 0);
        mu_assert("ERROR: The returned container is not the expected schunk",
            item.schunk == b2_schunks[i]);
    }

    mu_assert("ERROR: The dict_store length changed after get operations",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_remove_memory(void) {
    mu_assert("ERROR: Expected an error when ds is NULL",
        blosc2_dict_store_remove(ds, (const uint8_t*)paths[0]) != 0);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    for (int i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }

    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    for (int i = 0; i < SCHUNKS_SIZE; i+=1) {
        const uint8_t* key_to_remove = (const uint8_t*)paths[i];
        blosc2_dict_store_remove(ds, key_to_remove);
        size_t length = blosc2_dict_store_get_length(ds);
        size_t expected_length = SCHUNKS_SIZE - i - 1;
        mu_assert("ERROR: Unexpected dict_store length after removing a key",
            length == expected_length);
    }

    mu_assert("ERROR: The dict_store must be empty after removing all the keys",
        blosc2_dict_store_get_length(ds) == 0);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_to_b2z_memory(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    for (int i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);

        b2_ds_item item = {0};
        mu_assert("ERROR: Could not get a schunk from the dict_store",
            blosc2_dict_store_get(ds, (const uint8_t*)paths[i], &item) == 0);
        mu_assert("ERROR: The returned container is NULL", item.schunk != NULL);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Expected a NULL pointer error when the destination is NULL",
        blosc2_dict_store_to_b2z(ds, NULL) == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: Expected an invalid parameter error with a non-b2z destination",
        blosc2_dict_store_to_b2z(ds, wrong_archive_path) == BLOSC2_ERROR_INVALID_PARAM);
    mu_assert("ERROR: Could not dump the in-memory dict_store to b2z",
        blosc2_dict_store_to_b2z(ds, archive_path) == 0);
    mu_assert("ERROR: The b2z archive was not created", s_exists_inode(archive_path));

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_new_b2z(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);
    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_set_b2z(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);

    /* Keep the archive for test_dict_store_open_b2z */
    keep_file = true;

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_open_b2z(void) {
    if (!s_exists_inode(archive_path)) {
        mu_assert("ERROR: Could not generate the b2z archive",
            s_generate_b2z(archive_path) == 0);
    }

    /* RDONLY */
    mu_assert("ERROR: Could not open the b2z archive in RDONLY mode",
        blosc2_dict_store_open(&ds, archive_path, B2_DS_RDONLY) == 0);
    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        b2_ds_item item = {0};
        mu_assert("ERROR: Could not get a schunk from the b2z dict_store",
            blosc2_dict_store_get(ds, (const uint8_t*)paths[i], &item) == 0);
        mu_assert("ERROR: The returned container is NULL", item.schunk != NULL);
        blosc2_schunk_free(item.schunk);
    }

    mu_assert("ERROR: set() must fail when the dict_store was opened in RDONLY mode",
        blosc2_dict_store_set(ds, wrong_archive_path, b2_schunks[0]) != 0);
    mu_assert("ERROR: close() must succeed in RDONLY mode",
        blosc2_dict_store_close(ds) == 0);

    blosc2_dict_store_free(ds);
    ds = NULL;

    /* WRONLY */
    mu_assert("ERROR: open() must fail when the archive already exists in WRONLY mode",
        blosc2_dict_store_open(&ds, archive_path, B2_DS_WRONLY) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed open", ds == NULL);

    remove((const char*)archive_path);

    mu_assert("ERROR: Could not open a new archive in WRONLY mode",
        blosc2_dict_store_open(&ds, archive_path, B2_DS_WRONLY) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);
    mu_assert("ERROR: A newly opened WRONLY dict_store must be empty",
        blosc2_dict_store_get_length(ds) == 0);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in a WRONLY dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
        mu_assert("ERROR: Unexpected dict_store length after setting a schunk",
            blosc2_dict_store_get_length(ds) == i+1);
    }

    b2_ds_item item = {0};
    mu_assert("ERROR: get() must fail when the dict_store was opened in WRONLY mode",
        blosc2_dict_store_get(ds, (const uint8_t*)paths[0], &item) != 0);
    mu_assert("ERROR: The returned container must be NULL in WRONLY mode", item.schunk == NULL);

    mu_assert("ERROR: Could not close the WRONLY dict_store",
        blosc2_dict_store_close(ds) == 0);
    mu_assert("ERROR: The b2z archive was not created", s_exists_inode(archive_path));

    blosc2_dict_store_free(ds);
    ds = NULL;

    /* RDWR */
    mu_assert("ERROR: open() must fail with a non-existing archive in RDWR mode",
        blosc2_dict_store_open(&ds, wrong_archive_path, B2_DS_RDWR) != 0);

    mu_assert("ERROR: Could not open the b2z archive in RDWR mode",
        blosc2_dict_store_open(&ds, archive_path, B2_DS_RDWR) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);
    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    blosc2_dict_store_iter *iter = blosc2_dict_store_iter_new(ds);
    mu_assert("ERROR: Could not create a dict_store iterator", iter != NULL);
    while (blosc2_dict_store_iter_next(iter)) {
        item = (b2_ds_item){0};
        mu_assert("ERROR: Could not get the container from the iterator",
            blosc2_dict_store_iter_get_container(iter, &item) == 0);
        mu_assert("ERROR: The iterator returned a NULL container", item.schunk != NULL);
        blosc2_schunk_free(item.schunk);
    }
    blosc2_dict_store_iter_free(iter);

    for (size_t i = SCHUNKS_SIZE; i > 0; i-=1) {
        mu_assert("ERROR: Could not overwrite a schunk in a RDWR dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[SCHUNKS_SIZE - i], b2_schunks[i - 1]) == 0);
    }
    mu_assert("ERROR: The dict_store length changed after overwriting entries",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not close the RDWR dict_store",
        blosc2_dict_store_close(ds) == 0);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_new_b2d(void) {
    remove_dir_recursive(dir_path);

    mu_assert("ERROR: new() must fail when the path has no b2d extension",
        blosc2_dict_store_new(&ds, wrong_dir_path) != 0);

    mu_assert("ERROR: Could not create a b2d dict_store",
        blosc2_dict_store_new(&ds, dir_path) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the b2d dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        b2_ds_item item = {0};
        mu_assert("ERROR: Could not get a schunk from the b2d dict_store",
            blosc2_dict_store_get(ds, (const uint8_t*)paths[i], &item) == 0);
        blosc2_schunk_free(item.schunk);
    }

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove_dir_recursive(dir_path);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_open_b2d(void) {
    mu_assert("ERROR: Could not generate the b2d archive",
        s_generate_b2d(dir_path) == 0);

    mu_assert("ERROR: Could not open the b2d archive in RDONLY mode",
        blosc2_dict_store_open(&ds, dir_path, B2_DS_RDONLY) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);

    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);
    mu_assert("ERROR: set() must fail when the dict_store was opened in RDONLY mode",
        blosc2_dict_store_set(ds, (const uint8_t*)paths[0], b2_schunks[0]) != 0);

    blosc2_dict_store_iter *iter = blosc2_dict_store_iter_new(ds);
    mu_assert("ERROR: Could not create a dict_store iterator", iter != NULL);
    while (blosc2_dict_store_iter_next(iter)) {
        b2_ds_item item = {0};
        mu_assert("ERROR: Could not get the container from the iterator",
            blosc2_dict_store_iter_get_container(iter, &item) == 0);
        mu_assert("ERROR: The iterator returned a NULL container", item.schunk != NULL);
        blosc2_schunk_free(item.schunk);
    }
    blosc2_dict_store_iter_free(iter);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    blosc2_dict_store_free(ds);
    ds = NULL;

    mu_assert("ERROR: open() must fail when the directory already exists in WRONLY mode",
        blosc2_dict_store_open(&ds, dir_path, B2_DS_WRONLY) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed open", ds == NULL);

    mu_assert("ERROR: Could not open the b2d archive in RDWR mode",
        blosc2_dict_store_open(&ds, dir_path, B2_DS_RDWR) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);
    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove_dir_recursive(dir_path);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_to_b2z(void) {
    /* In-memory storage -> b2z */
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Expected an invalid parameter error with a non-b2z destination",
        blosc2_dict_store_to_b2z(ds, wrong_archive_path) != 0);
    mu_assert("ERROR: Could not dump the in-memory dict_store to b2z",
        blosc2_dict_store_to_b2z(ds, archive_path) == 0);
    mu_assert("ERROR: The b2z archive was not created", s_exists_inode(archive_path));

    blosc2_dict_store_free(ds);
    ds = NULL;
    remove((const char*)archive_path);

    /* b2z storage -> b2z */
    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: to_b2z() must fail when the source is already a b2z store",
        blosc2_dict_store_to_b2z(ds, archive_path) != 0);
    mu_assert("ERROR: to_b2z() must fail with a non-b2z destination",
        blosc2_dict_store_to_b2z(ds, dir_path) != 0);

    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);
    blosc2_dict_store_free(ds);
    ds = NULL;
    remove((const char*)archive_path);

    /* b2d storage -> b2z */
    remove_dir_recursive(dir_path);

    mu_assert("ERROR: Could not create a b2d dict_store",
        blosc2_dict_store_new(&ds, dir_path) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the b2d dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: to_b2z() must fail when the source is already a b2d store",
        blosc2_dict_store_to_b2z(ds, dir_path) != 0);
    mu_assert("ERROR: Could not dump the b2d dict_store to b2z",
        blosc2_dict_store_to_b2z(ds, archive_path) == 0);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove_dir_recursive(dir_path);
    remove((const char*)archive_path);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* test_dict_store_to_b2d(void) {
    /* In-memory storage -> b2d */
    remove_dir_recursive(dir_path);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: to_b2d() must fail with a non-b2d destination",
        blosc2_dict_store_to_b2d(ds, wrong_archive_path) != 0);
    mu_assert("ERROR: to_b2d() must fail with a b2z destination",
        blosc2_dict_store_to_b2d(ds, archive_path) != 0);
    mu_assert("ERROR: Could not dump the in-memory dict_store to b2d",
        blosc2_dict_store_to_b2d(ds, dir_path) == 0);
    mu_assert("ERROR: The b2d archive was not created", s_exists_inode(dir_path));

    mu_assert("ERROR: Could not close the in-memory dict_store",
        blosc2_dict_store_close(ds) == 0);
    blosc2_dict_store_free(ds);
    ds = NULL;
    remove_dir_recursive(dir_path);

    /* b2z storage -> b2d */
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], b2_schunks[i]) == 0);
    }
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: to_b2d() must fail with a non-b2d destination",
        blosc2_dict_store_to_b2d(ds, wrong_archive_path) != 0);
    mu_assert("ERROR: to_b2d() must fail with a b2z destination",
        blosc2_dict_store_to_b2d(ds, archive_path) != 0);
    mu_assert("ERROR: Could not dump the b2z dict_store to b2d",
        blosc2_dict_store_to_b2d(ds, dir_path) == 0);

    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove((const char*)archive_path);
    remove_dir_recursive(dir_path);

    END_TEST;
    return EXIT_SUCCESS;
}

static char* all_tests(void) {
    /* Tests when the dict_store data structure only stores data in memory */
    mu_run_test(test_dict_store_new_memory);
    s_teardown();

    mu_run_test(test_dict_store_set_memory); // It also tests the iterator
    s_teardown();

    mu_run_test(test_dict_store_get_memory);
    s_teardown();

    mu_run_test(test_dict_store_remove_memory);
    s_teardown();

    mu_run_test(test_dict_store_to_b2z_memory);
    s_teardown();

    /* b2z tests */
    mu_run_test(test_dict_store_new_b2z);
    s_teardown();

    mu_run_test(test_dict_store_set_b2z);
    s_teardown();

    mu_run_test(test_dict_store_open_b2z);
    s_teardown();

    /* b2d tests */
    mu_run_test(test_dict_store_new_b2d);
    s_teardown();

    mu_run_test(test_dict_store_open_b2d);
    s_teardown();

    mu_run_test(test_dict_store_to_b2z);
    s_teardown();

    mu_run_test(test_dict_store_to_b2d);
    s_teardown();

    return EXIT_SUCCESS;
}

int main(void) {
    blosc2_init();

    remove((const char*)archive_path);
    remove_dir_recursive(dir_path);

    s_setup_schunks();

    const char* result = all_tests();
    if (result != EXIT_SUCCESS) {
        printf("(%s)\n", result);
    } else {
        printf(" ALL TESTS PASSED");
    }
    printf("\tTests run: %d\n", tests_run);

    if (ds != NULL) {
        blosc2_dict_store_free(ds);
        ds = NULL;
    }

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_schunk_free(b2_schunks[i]);
    }

    remove((const char*)archive_path);
    remove_dir_recursive(dir_path);

    blosc2_destroy();
    return 0;
}

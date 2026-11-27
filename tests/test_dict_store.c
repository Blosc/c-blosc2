/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "context.h"
#include "test_common.h"
#include <blosc2.h>
#include <stores.h>
#include "dict-store/dict-store-private.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

int tests_run = 0;

#define SCHUNKS_SIZE 40

#define RUN_TEST(func) \
    do { \
        mu_run_test(func); \
        s_teardown(); \
        printf("FINISHED TEST: '" #func "'\n"); \
    } while(0);

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
static const int64_t threshold = 64;

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

/* Insert every global paths[i] -> b2_schunks[i] pair. Returns 0 or first error. */
static int s_set_all_schunks(blosc2_dict_store* store) {
    blosc2_store_value value = {0};
    value.tag = BLOSC2_STORE_VALUE_SCHUNK;
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        value.schunk = b2_schunks[i];
        int rc = blosc2_dict_store_set(store, (const uint8_t*)paths[i], &value);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

/* Get every global path[i] (checking non-NULL); free_each releases each one. */
static int s_get_all_schunks(blosc2_dict_store* store, bool free_each) {
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        int rc = blosc2_dict_store_get(store, (const uint8_t*)paths[i], &item);
        if (rc != 0 || item.schunk == NULL) {
            return rc != 0 ? rc : BLOSC2_ERROR_FAILURE;
        }
        if (free_each) {
            blosc2_schunk_free(item.schunk);
        }
    }
    return 0;
}

/* Iterate, checking every key is non-NULL and present; writes the count. */
static int s_count_iterated(blosc2_dict_store* store, size_t* count) {
    blosc2_dict_store_iter* iter = blosc2_dict_store_iter_new(store);
    if (iter == NULL) {
        return BLOSC2_ERROR_FAILURE;
    }
    size_t n = 0;
    while (blosc2_dict_store_iter_next(iter)) {
        const uint8_t* key = blosc2_dict_store_iter_get_key(iter);
        if (key == NULL || !blosc2_dict_store_contains_key(store, key)) {
            blosc2_dict_store_iter_free(iter);
            return BLOSC2_ERROR_FAILURE;
        }
        n += 1;
    }
    blosc2_dict_store_iter_free(iter);
    *count = n;
    return 0;
}

/* Iterate, getting and releasing every container. Returns 0 or first error. */
static int s_get_all_containers_from_iter(blosc2_dict_store* store) {
    blosc2_dict_store_iter* iter = blosc2_dict_store_iter_new(store);
    if (iter == NULL) {
        return BLOSC2_ERROR_FAILURE;
    }
    int rc = 0;
    while (blosc2_dict_store_iter_next(iter)) {
        blosc2_store_value item = {0};
        rc = blosc2_dict_store_iter_get_container(iter, &item);
        if (rc != 0) {
            break;
        }
        if (item.schunk == NULL) {
            rc = BLOSC2_ERROR_FAILURE;
            break;
        }
        blosc2_schunk_free(item.schunk);
    }
    blosc2_dict_store_iter_free(iter);
    return rc;
}

static int s_generate_b2z(const uint8_t* destination_urlpath) {
    int error_code = BLOSC2_ERROR_SUCCESS;
    blosc2_dict_store *store = NULL;

    remove_dir_recursive(dir_path);

    error_code = blosc2_dict_store_new(&store, dir_path, NULL, -1);
    if (error_code != 0) {
        return error_code;
    }

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        error_code = blosc2_dict_store_set(store, (const uint8_t*)paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
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

    error_code = blosc2_dict_store_new(&store, urlpath, NULL, -1);
    if (error_code != 0) {
        return error_code;
    }

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        error_code = blosc2_dict_store_set(store, (const uint8_t*)paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
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
        blosc2_dict_store_new(NULL, NULL, NULL, -1) != 0);

    mu_assert("ERROR: Returned failure when success was expected",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);
    mu_assert("ERROR: A new in-memory dict_store must be empty",
        blosc2_dict_store_get_length(ds) == 0);


    return EXIT_SUCCESS;
}

static char* test_dict_store_set_memory(void) {
    mu_assert("ERROR: Expected a NULL pointer error when ds is NULL",
        blosc2_dict_store_set(ds, (const uint8_t*)paths[0], NULL) == BLOSC2_ERROR_NULL_POINTER);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    mu_assert("ERROR: Expected an error when the container is NULL",
        blosc2_dict_store_set(ds, (const uint8_t*)"/some/path", NULL) != 0);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);

    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    size_t iters = 0;
    mu_assert("ERROR: Could not iterate the dict_store",
        s_count_iterated(ds, &iters) == 0);
    mu_assert("ERROR: The iterator did not yield all the inserted keys",
        iters == SCHUNKS_SIZE);


    return EXIT_SUCCESS;
}

static char* test_dict_store_get_memory(void) {
    mu_assert("ERROR: Expected an error when the output container is NULL",
        blosc2_dict_store_get(ds, (const uint8_t*)paths[0], NULL) != 0);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);

    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    /* For the sparse in-memory storage the returned pointer is borrowed, so it
       must be the very same schunk that was inserted. */
    blosc2_store_value item = {0};
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        const uint8_t* key_to_get = (const uint8_t*)paths[i];
        mu_assert("ERROR: Could not get a schunk from the dict_store",
            blosc2_dict_store_get(ds, key_to_get, &item) == 0);
        mu_assert("ERROR: The returned container is not the expected schunk",
            item.schunk == b2_schunks[i]);
    }

    mu_assert("ERROR: The dict_store length changed after get operations",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);


    return EXIT_SUCCESS;
}

static char* test_dict_store_remove_memory(void) {
    mu_assert("ERROR: Expected an error when ds is NULL",
        blosc2_dict_store_remove(ds, (const uint8_t*)paths[0]) != 0);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);

    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        const uint8_t* key_to_remove = (const uint8_t*)paths[i];
        blosc2_dict_store_remove(ds, key_to_remove);
        size_t length = blosc2_dict_store_get_length(ds);
        size_t expected_length = SCHUNKS_SIZE - i - 1;
        mu_assert("ERROR: Unexpected dict_store length after removing a key",
            length == expected_length);
    }

    mu_assert("ERROR: The dict_store must be empty after removing all the keys",
        blosc2_dict_store_get_length(ds) == 0);


    return EXIT_SUCCESS;
}

static char* test_dict_store_to_b2z_memory(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);
    mu_assert("ERROR: Could not get a schunk from the dict_store",
        s_get_all_schunks(ds, false) == 0);
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Expected a NULL pointer error when the destination is NULL",
        blosc2_dict_store_to_b2z(ds, NULL) == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: Expected an invalid parameter error with a non-b2z destination",
        blosc2_dict_store_to_b2z(ds, wrong_archive_path) == BLOSC2_ERROR_INVALID_PARAM);
    mu_assert("ERROR: Could not dump the in-memory dict_store to b2z",
        blosc2_dict_store_to_b2z(ds, archive_path) == 0);
    mu_assert("ERROR: The b2z archive was not created", s_exists_inode(archive_path));


    return EXIT_SUCCESS;
}

static char* test_dict_store_new_b2z(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);
    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);


    return EXIT_SUCCESS;
}

static char* test_dict_store_set_b2z(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
        s_set_all_schunks(ds) == 0);
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);

    /* Keep the archive for test_dict_store_open_b2z */
    keep_file = true;


    return EXIT_SUCCESS;
}

static char* test_dict_store_open_b2z(void) {
    if (!s_exists_inode(archive_path)) {
        mu_assert("ERROR: Could not generate the b2z archive",
            s_generate_b2z(archive_path) == 0);
    }

    /* RDONLY */
    mu_assert("ERROR: Could not open the b2z archive in RDONLY mode",
        blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY) == 0);
    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not get the schunks from the b2z dict_store",
        s_get_all_schunks(ds, true) == 0);

    mu_assert("ERROR: set() must fail when the dict_store was opened in RDONLY mode",
        blosc2_dict_store_set(ds, wrong_archive_path, &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[0]}) != 0);
    mu_assert("ERROR: close() must succeed in RDONLY mode",
        blosc2_dict_store_close(ds) == 0);

    blosc2_dict_store_free(ds);
    ds = NULL;

    /* WRONLY */
    mu_assert("ERROR: open() must fail when the archive already exists in WRONLY mode",
        blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_WRONLY) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed open", ds == NULL);

    remove((const char*)archive_path);

    mu_assert("ERROR: Could not open a new archive in WRONLY mode",
        blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_WRONLY) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);
    mu_assert("ERROR: A newly opened WRONLY dict_store must be empty",
        blosc2_dict_store_get_length(ds) == 0);

    /* The length must grow monotonically with each new key. */
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not set a schunk in a WRONLY dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]}) == 0);
        mu_assert("ERROR: Unexpected dict_store length after setting a schunk",
            blosc2_dict_store_get_length(ds) == i+1);
    }

    blosc2_store_value item = {0};
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
        blosc2_dict_store_open(&ds, wrong_archive_path, BLOSC2_STORE_OPEN_MODE_RDWR) != 0);

    mu_assert("ERROR: Could not open the b2z archive in RDWR mode",
        blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDWR) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);
    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not iterate the b2z dict_store",
        s_get_all_containers_from_iter(ds) == 0);

    for (size_t i = SCHUNKS_SIZE; i > 0; i-=1) {
        mu_assert("ERROR: Could not overwrite a schunk in a RDWR dict_store",
            blosc2_dict_store_set(ds, (const uint8_t*)paths[SCHUNKS_SIZE - i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i - 1]}) == 0);
    }
    mu_assert("ERROR: The dict_store length changed after overwriting entries",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not close the RDWR dict_store",
        blosc2_dict_store_close(ds) == 0);


    return EXIT_SUCCESS;
}

static char* test_dict_store_new_b2d(void) {
    remove_dir_recursive(dir_path);

    mu_assert("ERROR: new() must fail when the path has no b2d extension",
        blosc2_dict_store_new(&ds, wrong_dir_path, NULL, -1) != 0);

    mu_assert("ERROR: Could not create a b2d dict_store",
        blosc2_dict_store_new(&ds, dir_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2d dict_store",
        s_set_all_schunks(ds) == 0);
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not get the schunks from the b2d dict_store",
        s_get_all_schunks(ds, true) == 0);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove_dir_recursive(dir_path);


    return EXIT_SUCCESS;
}

static char* test_dict_store_open_b2d(void) {
    mu_assert("ERROR: Could not generate the b2d archive",
        s_generate_b2d(dir_path) == 0);

    mu_assert("ERROR: Could not open the b2d archive in RDONLY mode",
        blosc2_dict_store_open(&ds, dir_path, BLOSC2_STORE_OPEN_MODE_RDONLY) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);

    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);
    mu_assert("ERROR: set() must fail when the dict_store was opened in RDONLY mode",
        blosc2_dict_store_set(ds, (const uint8_t*)paths[0], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[0]}) != 0);

    mu_assert("ERROR: Could not iterate the b2d dict_store",
        s_get_all_containers_from_iter(ds) == 0);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    blosc2_dict_store_free(ds);
    ds = NULL;

    mu_assert("ERROR: open() must fail when the directory already exists in WRONLY mode",
        blosc2_dict_store_open(&ds, dir_path, BLOSC2_STORE_OPEN_MODE_WRONLY) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed open", ds == NULL);

    mu_assert("ERROR: Could not open the b2d archive in RDWR mode",
        blosc2_dict_store_open(&ds, dir_path, BLOSC2_STORE_OPEN_MODE_RDWR) == 0);
    mu_assert("ERROR: ds is NULL after a successful open", ds != NULL);
    mu_assert("ERROR: Unexpected dict_store length after opening the archive",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove_dir_recursive(dir_path);


    return EXIT_SUCCESS;
}

static char* test_dict_store_to_b2z(void) {
    /* In-memory storage -> b2z */
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);
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
        blosc2_dict_store_new(&ds, archive_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
        s_set_all_schunks(ds) == 0);
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
        blosc2_dict_store_new(&ds, dir_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2d dict_store",
        s_set_all_schunks(ds) == 0);
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


    return EXIT_SUCCESS;
}

static char* test_dict_store_to_b2d(void) {
    /* In-memory storage -> b2d */
    remove_dir_recursive(dir_path);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);
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
        blosc2_dict_store_new(&ds, archive_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
        s_set_all_schunks(ds) == 0);
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


    return EXIT_SUCCESS;
}

static char* test_dict_store_embed_basic(void) {

    int rc = blosc2_dict_store_new(&ds, NULL, NULL, threshold);
    mu_assert("ERROR: The DictStore should have been created", rc == 0);
    mu_assert("ERROR: The DictStore's inner EmbedStore should have been created", ds->estore != NULL);
    mu_assert("ERROR: THe DictStore's threshold should be equal to the specified threshold", ds->threshold == threshold);
    mu_assert("ERROR: A newly created DictStore should have 0 length", blosc2_dict_store_get_length(ds) == 0);

    const char* urlbase = "urlbase.com";
    const char* path = "@some/path";

    blosc2_store_value remote_value = {0};
    remote_value.tag = BLOSC2_STORE_VALUE_REMOTE;
    remote_value.remote.urlbase = (uint8_t*)urlbase;
    remote_value.remote.path = (uint8_t*)path;

    rc = blosc2_dict_store_set(ds, (const uint8_t*)"/key", &remote_value);
    mu_assert("ERROR: The item should have been inserted", rc == 0);
    mu_assert("ERROR: The DictStore's length should be 1", blosc2_dict_store_get_length(ds) == 1);

    /* Every insertion is checked one by one: the length must grow with each key. */
    for (size_t i = 0; i < SCHUNKS_SIZE; i+= 1) {
        rc = blosc2_dict_store_set(ds, (const uint8_t*)paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
        mu_assert("ERROR: Could not insert SChunk into DictStore", rc == 0);
        mu_assert("ERROR: DictStore is returning a wrong length number", blosc2_dict_store_get_length(ds) == i + 2);
    }

    blosc2_store_value item = {0};
    rc = blosc2_dict_store_get(ds, (const uint8_t*)"/key", &item);
    mu_assert("ERROR: Could not get item from EmbedStore", rc == 0);
    mu_assert("ERROR: Wrong item tag", item.tag == BLOSC2_STORE_VALUE_REMOTE);
    mu_assert("ERROR: urlbase is wrong", strcmp(urlbase, (const char*)item.remote.urlbase) == 0);
    mu_assert("ERROR: path is wrong", strcmp(path, (const char*)item.remote.path) == 0);

    /* The iterator mixes the inner EmbedStore entries with the DictStore ones. */
    blosc2_dict_store_iter* iter = blosc2_dict_store_iter_new(ds);
    size_t iters = 0;
    while (blosc2_dict_store_iter_next(iter)) {
        const uint8_t* key = blosc2_dict_store_iter_get_key(iter);
        mu_assert("ERROR: Could not get key from iterator", key != NULL);
        item = (blosc2_store_value){0};
        rc = blosc2_dict_store_iter_get_container(iter, &item);
        mu_assert("ERROR: Could not get container from iterator", rc == 0);
        if (item.tag == BLOSC2_STORE_VALUE_SCHUNK && strcmp((const char*)key, "/key") == 0) {
            blosc2_schunk_free(item.schunk);
        }
        iters += 1;
    }
    blosc2_dict_store_iter_free(iter);
    mu_assert("ERROR: The blosc2_dict_store_iter did not iterate blosc2_dict_store_get_length times", blosc2_dict_store_get_length(ds) == iters);

    rc = blosc2_dict_store_remove(ds, (const uint8_t*)"/key");
    mu_assert("ERROR: There was an unexpected error while trying to remove key", rc == 0);
    mu_assert("ERROR: The expected length was not matched", blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    // Again, to show that the remove does nothing when the key was not found
    rc = blosc2_dict_store_remove(ds, (const uint8_t*)"/key");
    mu_assert("ERROR: There was an unexpected error while trying to remove key", rc == 0);
    mu_assert("ERROR: The expected length was not matched", blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        const uint8_t* some_path = (const uint8_t*)paths[i];
        rc = blosc2_dict_store_remove(ds, some_path);
        mu_assert("ERROR: Could not remove item from DictStore", rc == 0);
    }

    mu_assert("ERROR: DictStore should be empty", blosc2_dict_store_get_length(ds) == 0);

    return NULL;
}

static char* test_dict_store_embed_b2z(void) {

    int rc = blosc2_dict_store_new(&ds, archive_path, NULL, threshold);
    mu_assert("ERROR: Could not create DictStore", rc == 0);
    mu_assert("ERROR: DictStore is NULL when it shouldn't", ds != NULL);
    mu_assert("ERROR: An EmbedStore should have been created", ds->estore != NULL);
    mu_assert("ERROR: Threshold has a wrong value", threshold == ds->threshold);

    const uint8_t* urlbase = (const uint8_t*)"urlbase";
    const uint8_t* path = (const uint8_t*)"path";

    blosc2_store_value remote_value = {0};
    remote_value.tag = BLOSC2_STORE_VALUE_REMOTE;
    remote_value.remote.urlbase = (uint8_t*)urlbase;
    remote_value.remote.path = (uint8_t*)path;

    rc = blosc2_dict_store_set(ds, (const uint8_t*)"/key", &remote_value);
    mu_assert("ERROR: Could not insert remote value in DictStore", rc == 0);

    mu_assert("ERROR: Could not set SChunk in DictStore", s_set_all_schunks(ds) == 0);

    mu_assert("ERROR: DictStore's length is wrong", blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE + 1);

    rc = blosc2_dict_store_close(ds);
    mu_assert("ERROR: The DictStore B2Z failed upon closing", rc == 0);
    mu_assert("ERROR: The file was not created", s_exists_inode(archive_path));
    blosc2_dict_store_free(ds);
    ds = NULL;

    rc = blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY);
    mu_assert("ERROR: Could not open DictStore", rc == 0);
    mu_assert("ERROR: Length is wrong", blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE + 1);

    blosc2_store_value item = {0};
    rc = blosc2_dict_store_get(ds, (const uint8_t*)"/key", &item);
    mu_assert("ERROR: Could not get EmbedStore's item", rc == 0);
    mu_assert("ERROR: Wrong tag, it should be BLOSC2_STORE_VALUE_REMOTE", item.tag == BLOSC2_STORE_VALUE_REMOTE);
    mu_assert("ERROR: urlbase is wrong", strcmp((const char*)urlbase, (const char*)item.remote.urlbase) == 0);
    mu_assert("ERROR: path is wrong", strcmp((const char*)path, (const char*)item.remote.path) == 0);

    return NULL;
}

static char* test_dict_store_embed_b2d(void) {

    const uint8_t* key = (const uint8_t*)"/key";
    int rc = blosc2_dict_store_new(&ds, dir_path, NULL, threshold);
    mu_assert("ERROR: Could not create DictStore", rc == 0);
    mu_assert("ERROR: DictStore is NULL when it shouldn't", ds != NULL);
    mu_assert("ERROR: An EmbedStore should have been created", ds->estore != NULL);
    mu_assert("ERROR: Threshold has a wrong value", threshold == ds->threshold);

    const uint8_t* urlbase = (const uint8_t*)"urlbase";
    const uint8_t* path = (const uint8_t*)"path";

    blosc2_store_value value = {0};
    value.tag = BLOSC2_STORE_VALUE_REMOTE;
    value.remote.urlbase = (uint8_t*)urlbase;
    value.remote.path = (uint8_t*)path;

    rc = blosc2_dict_store_set(ds, key, &value);
    mu_assert("ERROR: Could not insert remote value in DictStore", rc == 0);

    mu_assert("ERROR: Could not set SChunk in DictStore", s_set_all_schunks(ds) == 0);
    mu_assert("ERROR: DictStore's length is wrong", blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE + 1);

    rc = blosc2_dict_store_close(ds);
    mu_assert("ERROR: Could not close DicStore B2D with inner EmbedStore", rc == 0);
    blosc2_dict_store_free(ds);

    const blosc2_store_open_mode modes[] = {
        BLOSC2_STORE_OPEN_MODE_RDONLY,
        BLOSC2_STORE_OPEN_MODE_RDWR
    };
    size_t len = sizeof(modes) / sizeof(blosc2_store_open_mode);

    for (size_t i = 0; i < len; i+=1) {
        blosc2_store_open_mode mode = modes[i];
        rc = blosc2_dict_store_open(&ds, dir_path, mode);
        mu_assert("ERROR: Could not open DictStore B2D with inner EmbedStore", rc == 0);
        mu_assert("ERROR: DictStore should have an inner EmbedStore", ds->estore != NULL);
        mu_assert("ERROR: EmbedStore should have the same open mode as DictStore", ds->mode == ds->estore->mode);
        mu_assert("ERROR: DictStore's threshold should be 0", ds->threshold == 0);
        mu_assert("ERROR: DictStore's length is wrong", blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE + 1);

        memset(&value, 0, sizeof(value));
        rc = blosc2_dict_store_get(ds, key, &value);
        mu_assert("ERROR: Could not get SChunk from the inner EmbedStore", rc == 0);
        mu_assert("ERROR: The value is invalid", value.tag == BLOSC2_STORE_VALUE_REMOTE);
        rc = blosc2_dict_store_close(ds);
        mu_assert("ERROR: Couldn't close DictStore", rc == 0);
        blosc2_dict_store_free(ds);
    }
    ds = NULL; // Put this, so teardown does not attempt to free ds
    return NULL;
}

static char* test_dict_store_introspection_memory(void) {
    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new",
        ds != NULL);

    /* A brand new in-memory dict_store has no inner EmbedStore and is empty */
    mu_assert("ERROR: A new in-memory dict_store must use B2_DS_MEMORY_SPARSE",
        blosc2_dict_store_get_storage_type(ds) == B2_DS_MEMORY_SPARSE);
    mu_assert("ERROR: A new in-memory dict_store must be opened in RDWR mode",
        blosc2_dict_store_get_open_mode(ds) == BLOSC2_STORE_OPEN_MODE_RDWR);
    mu_assert("ERROR: A new in-memory dict_store must be empty",
        blosc2_dict_store_get_length(ds) == 0);
    mu_assert("ERROR: The capacity must be greater than or equal to the length",
        blosc2_dict_store_get_capacity(ds) >= blosc2_dict_store_get_length(ds));
    mu_assert("ERROR: The initial threshold must match the one requested",
        blosc2_dict_store_get_threshold(ds) == -1);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);
    mu_assert("ERROR: Unexpected dict_store length after setting all the schunks",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE);
    mu_assert("ERROR: The capacity must be greater than or equal to the length",
        blosc2_dict_store_get_capacity(ds) >= SCHUNKS_SIZE);


    return EXIT_SUCCESS;
}

static char* test_dict_store_threshold(void) {
    mu_assert("ERROR: set_threshold() must fail when dstore is NULL",
        blosc2_dict_store_set_threshold(NULL, 128) == BLOSC2_ERROR_NULL_POINTER);

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);

    mu_assert("ERROR: The threshold should be the one requested",
        blosc2_dict_store_get_threshold(ds) == -1);
    mu_assert("ERROR: Could not update the threshold",
        blosc2_dict_store_set_threshold(ds, 0) == 0);
    mu_assert("ERROR: The threshold was not updated",
        blosc2_dict_store_get_threshold(ds) == 0);
    mu_assert("ERROR: Could not update the threshold",
        blosc2_dict_store_set_threshold(ds, 4096) == 0);
    mu_assert("ERROR: The threshold was not updated",
        blosc2_dict_store_get_threshold(ds) == 4096);
    /* A negative threshold disables the use of the inner EmbedStore */
    mu_assert("ERROR: Could not update the threshold to a negative value",
        blosc2_dict_store_set_threshold(ds, -5) == 0);
    mu_assert("ERROR: The threshold was not updated",
        blosc2_dict_store_get_threshold(ds) == -5);


    return EXIT_SUCCESS;
}

static char* test_dict_store_contains_key_memory(void) {
    mu_assert("ERROR: contains_key() must return false for a NULL dstore",
        !blosc2_dict_store_contains_key(NULL, (const uint8_t*)paths[0]));

    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);

    mu_assert("ERROR: contains_key() must return false for a NULL key",
        !blosc2_dict_store_contains_key(ds, NULL));
    mu_assert("ERROR: contains_key() must return false in an empty store",
        !blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[0]));

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: contains_key() must find an inserted key",
            blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
    }
    mu_assert("ERROR: contains_key() must return false for a missing key",
        !blosc2_dict_store_contains_key(ds, (const uint8_t*)"/missing/key"));


    return EXIT_SUCCESS;
}

static char* test_dict_store_key_normalization_memory(void) {
    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);

    mu_assert("ERROR: Could not set a schunk with a canonical key",
        blosc2_dict_store_set(ds, (const uint8_t*)"a/b/c", &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[0]}) == 0);
    mu_assert("ERROR: Could not set a schunk with another key",
        blosc2_dict_store_set(ds, (const uint8_t*)"x/y", &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[1]}) == 0);
    mu_assert("ERROR: Unexpected length after inserting the schunks",
        blosc2_dict_store_get_length(ds) == 2);

    /* Equivalent spellings must address the very same entry */
    static const uint8_t* equivalents[] = {
        (const uint8_t*)"a/b/c",
        (const uint8_t*)"/a/b/c",
        (const uint8_t*)"a//b/c",
        (const uint8_t*)"a/./b/c",
        (const uint8_t*)"//a/b/c",
    };
    for (size_t i = 0; i < ARRAY_SIZE(equivalents); i+=1) {
        mu_assert("ERROR: contains_key() failed for an equivalent key",
            blosc2_dict_store_contains_key(ds, equivalents[i]));

        blosc2_store_value item = {0};
        mu_assert("ERROR: get() failed for an equivalent key",
            blosc2_dict_store_get(ds, equivalents[i], &item) == 0);
        mu_assert("ERROR: get() returned a wrong schunk for an equivalent key",
            item.schunk == b2_schunks[0]);
    }

    mu_assert("ERROR: contains_key() failed for a leading-slash variant",
        blosc2_dict_store_contains_key(ds, (const uint8_t*)"/x/y"));
    mu_assert("ERROR: The length changed after the equivalent lookups",
        blosc2_dict_store_get_length(ds) == 2);

    /* Removing through an equivalent spelling removes the canonical entry */
    mu_assert("ERROR: remove() failed for an equivalent key",
        blosc2_dict_store_remove(ds, (const uint8_t*)"a/./b/./c") == 0);
    mu_assert("ERROR: The key was not removed",
        !blosc2_dict_store_contains_key(ds, (const uint8_t*)"a/b/c"));
    mu_assert("ERROR: Unexpected length after removing a key",
        blosc2_dict_store_get_length(ds) == 1);

    /* Invalid keys must be rejected */
    mu_assert("ERROR: set() must reject an empty key",
        blosc2_dict_store_set(ds, (const uint8_t*)"", &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[2]}) == BLOSC2_ERROR_INVALID_PARAM);
    mu_assert("ERROR: set() must reject a key that escapes the root",
        blosc2_dict_store_set(ds, (const uint8_t*)"a/../../b", &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[2]}) != 0);
    mu_assert("ERROR: contains_key() must return false for an invalid key",
        !blosc2_dict_store_contains_key(ds, (const uint8_t*)"a/../../b"));


    return EXIT_SUCCESS;
}

static char* test_dict_store_overwrite_memory(void) {
    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);

    mu_assert("ERROR: Could not insert the first schunk",
        blosc2_dict_store_set(ds, (const uint8_t*)paths[0], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[0]}) == 0);
    mu_assert("ERROR: Unexpected length after the first insertion",
        blosc2_dict_store_get_length(ds) == 1);

    /* Overwriting must keep the length and replace the value */
    mu_assert("ERROR: Could not overwrite an existing key",
        blosc2_dict_store_set(ds, (const uint8_t*)paths[0], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[1]}) == 0);
    mu_assert("ERROR: The length changed after overwriting an entry",
        blosc2_dict_store_get_length(ds) == 1);

    blosc2_store_value item = {0};
    mu_assert("ERROR: Could not get the overwritten entry",
        blosc2_dict_store_get(ds, (const uint8_t*)paths[0], &item) == 0);
    mu_assert("ERROR: The entry was not overwritten",
        item.schunk == b2_schunks[1]);


    return EXIT_SUCCESS;
}

static char* test_dict_store_iter_count_memory(void) {
    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);

    /* An empty dict_store must yield no entries */
    size_t iters = 0;
    mu_assert("ERROR: The iterator of an empty dict_store must be exhausted",
        s_count_iterated(ds, &iters) == 0);
    mu_assert("ERROR: The empty dict_store iterator must yield 0 entries",
        iters == 0);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);

    iters = 0;
    mu_assert("ERROR: Could not iterate the dict_store",
        s_count_iterated(ds, &iters) == 0);
    mu_assert("ERROR: The iterator did not yield all the inserted keys",
        iters == SCHUNKS_SIZE);


    return EXIT_SUCCESS;
}

static char* test_dict_store_invalid_params(void) {
    /* new() */
    mu_assert("ERROR: new() must fail when dstore is NULL",
        blosc2_dict_store_new(NULL, NULL, NULL, -1) != 0);
    mu_assert("ERROR: new() must fail with an empty urlpath",
        blosc2_dict_store_new(&ds, (const uint8_t*)"", NULL, -1) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed new", ds == NULL);
    mu_assert("ERROR: new() must fail with an unknown extension",
        blosc2_dict_store_new(&ds, wrong_archive_path, NULL, -1) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed new", ds == NULL);

    /* open() */
    mu_assert("ERROR: open() must fail when dstore is NULL",
        blosc2_dict_store_open(NULL, archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY) != 0);
    mu_assert("ERROR: open() must fail when urlpath is NULL",
        blosc2_dict_store_open(&ds, NULL, BLOSC2_STORE_OPEN_MODE_RDONLY) != 0);
    mu_assert("ERROR: open() must fail with an empty urlpath",
        blosc2_dict_store_open(&ds, (const uint8_t*)"", BLOSC2_STORE_OPEN_MODE_RDONLY) != 0);
    mu_assert("ERROR: open() must fail with an unknown extension",
        blosc2_dict_store_open(&ds, wrong_archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY) != 0);
    mu_assert("ERROR: open() must fail with an unknown open mode",
        blosc2_dict_store_open(&ds, archive_path, (blosc2_store_open_mode)42) != 0);
    mu_assert("ERROR: ds must remain NULL after a failed open", ds == NULL);

    /* close() */
    mu_assert("ERROR: close() must fail when dstore is NULL",
        blosc2_dict_store_close(NULL) != 0);

    /* NULL arguments for the serialization helpers */
    mu_assert("ERROR: to_b2z() must fail when dstore is NULL",
        blosc2_dict_store_to_b2z(NULL, archive_path) != 0);
    mu_assert("ERROR: to_b2d() must fail when dstore is NULL",
        blosc2_dict_store_to_b2d(NULL, dir_path) != 0);
    void* dummy_buffer = NULL;
    mu_assert("ERROR: to_buffer() must fail when dstore is NULL",
        blosc2_dict_store_to_buffer(NULL, &dummy_buffer, NULL) != 0);


    return EXIT_SUCCESS;
}

static char* test_dict_store_to_from_buffer(void) {
    mu_assert("ERROR: Could not create an in-memory dict_store",
        blosc2_dict_store_new(&ds, NULL, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the dict_store",
        s_set_all_schunks(ds) == 0);

    void* buffer = NULL;
    size_t buffer_size = 0;
    mu_assert("ERROR: to_buffer() must fail with a NULL pbuffer",
        blosc2_dict_store_to_buffer(ds, NULL, &buffer_size) == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: to_buffer() must fail with a NULL dstore",
        blosc2_dict_store_to_buffer(NULL, &buffer, &buffer_size) == BLOSC2_ERROR_NULL_POINTER);

    mu_assert("ERROR: Could not serialize the in-memory dict_store",
        blosc2_dict_store_to_buffer(ds, &buffer, &buffer_size) == 0);
    mu_assert("ERROR: The buffer is NULL after a successful to_buffer()", buffer != NULL);
    mu_assert("ERROR: The buffer size is 0 after a successful to_buffer()", buffer_size > 0);

    blosc2_dict_store* decoded = NULL;
    mu_assert("ERROR: from_buffer() must fail with a NULL dstore",
        blosc2_dict_store_from_buffer(NULL, buffer, buffer_size) == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: from_buffer() must fail with a NULL buffer",
        blosc2_dict_store_from_buffer(&decoded, NULL, buffer_size) == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: from_buffer() must fail with a size of 0",
        blosc2_dict_store_from_buffer(&decoded, buffer, 0) == BLOSC2_ERROR_INVALID_PARAM);

    uint8_t garbage[64];
    memset(garbage, 0xff, sizeof(garbage));
    decoded = NULL;
    mu_assert("ERROR: from_buffer() must fail with an invalid buffer",
        blosc2_dict_store_from_buffer(&decoded, garbage, sizeof(garbage)) != 0);
    mu_assert("ERROR: decoded must be NULL after a failed from_buffer()",
        decoded == NULL);

    mu_assert("ERROR: Could not create a dict_store from the buffer",
        blosc2_dict_store_from_buffer(&decoded, buffer, buffer_size) == 0);
    mu_assert("ERROR: decoded is NULL after a successful from_buffer()", decoded != NULL);
    mu_assert("ERROR: A decoded dict_store must use B2_DS_MEMORY_CONTIGUOUS",
        blosc2_dict_store_get_storage_type(decoded) == B2_DS_MEMORY_CONTIGUOUS);
    mu_assert("ERROR: A decoded dict_store must be opened in RDONLY mode",
        blosc2_dict_store_get_open_mode(decoded) == BLOSC2_STORE_OPEN_MODE_RDONLY);
    mu_assert("ERROR: Unexpected length after decoding the buffer",
        blosc2_dict_store_get_length(decoded) == SCHUNKS_SIZE);

    mu_assert("ERROR: set() must fail on a RDONLY decoded dict_store",
        blosc2_dict_store_set(decoded, (const uint8_t*)paths[0], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[0]}) == BLOSC2_ERROR_FAILURE);

    /* The decoded store owns freshly opened schunks, so each one is checked and
       released individually. */
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: Could not find a key in the decoded dict_store",
            blosc2_dict_store_contains_key(decoded, (const uint8_t*)paths[i]));

        blosc2_store_value item = {0};
        mu_assert("ERROR: Could not get a schunk from the decoded dict_store",
            blosc2_dict_store_get(decoded, (const uint8_t*)paths[i], &item) == 0);
        mu_assert("ERROR: The decoded container is NULL", item.schunk != NULL);
        mu_assert("ERROR: The decoded container has a wrong tag",
            item.tag == BLOSC2_STORE_VALUE_SCHUNK);
        mu_assert("ERROR: The decoded container has a wrong size",
            item.schunk->nbytes == (int64_t)(SCHUNKS_SIZE * SCHUNKS_SIZE));
        blosc2_schunk_free(item.schunk);
    }

    mu_assert("ERROR: Could not close the decoded dict_store",
        blosc2_dict_store_close(decoded) == 0);
    /* blosc2_dict_store_free() does not release the buffer for a
       B2_DS_MEMORY_CONTIGUOUS dictionary: the buffer is owned by the caller and
       must remain valid while the dictionary is alive. */
    blosc2_dict_store_free(decoded);
    free(buffer);


    return EXIT_SUCCESS;
}

static char* test_dict_store_remove_b2z(void) {
    remove((const char*)archive_path);

    mu_assert("ERROR: Could not create a b2z dict_store",
        blosc2_dict_store_new(&ds, archive_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2z dict_store",
        s_set_all_schunks(ds) == 0);

    /* remove() must delete both the backing file and the in-memory index entry */
    for (size_t i = 0; i < SCHUNKS_SIZE / 2; i+=1) {
        mu_assert("ERROR: Could not remove a key from the b2z dict_store",
            blosc2_dict_store_remove(ds, (const uint8_t*)paths[i]) == 0);
        mu_assert("ERROR: The removed key is still reported as present in the b2z dict_store",
            !blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
        blosc2_store_value item = {0};
        mu_assert("ERROR: get() must return NOT_FOUND after removing a key from the b2z dict_store",
            blosc2_dict_store_get(ds, (const uint8_t*)paths[i], &item) == BLOSC2_ERROR_NOT_FOUND);
    }
    mu_assert("ERROR: The length was not updated after removing keys from the b2z dict_store",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE - SCHUNKS_SIZE / 2);

    /* Removing a missing key must be a silent no-op */
    mu_assert("ERROR: remove() of a missing key must succeed",
        blosc2_dict_store_remove(ds, (const uint8_t*)"/does/not/exist") == 0);
    mu_assert("ERROR: The length changed after removing a missing key",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE - SCHUNKS_SIZE / 2);

    mu_assert("ERROR: Could not close the b2z dict_store",
        blosc2_dict_store_close(ds) == 0);
    blosc2_dict_store_free(ds);
    ds = NULL;

    /* Reopen and verify the removal survived a roundtrip to disk */
    mu_assert("ERROR: Could not reopen the b2z dict_store",
        blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY) == 0);
    mu_assert("ERROR: The length after reopening the b2z dict_store is wrong",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE - SCHUNKS_SIZE / 2);
    for (size_t i = 0; i < SCHUNKS_SIZE / 2; i+=1) {
        mu_assert("ERROR: A removed key is present after reopening the b2z dict_store",
            !blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
    }
    for (size_t i = SCHUNKS_SIZE / 2; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: A surviving key is missing after reopening the b2z dict_store",
            blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
    }
    mu_assert("ERROR: Could not close the reopened b2z dict_store",
        blosc2_dict_store_close(ds) == 0);


    return EXIT_SUCCESS;
}

static char* test_dict_store_remove_b2d(void) {
    remove_dir_recursive(dir_path);

    mu_assert("ERROR: Could not create a b2d dict_store",
        blosc2_dict_store_new(&ds, dir_path, NULL, -1) == 0);
    mu_assert("ERROR: ds is NULL after a successful new", ds != NULL);

    mu_assert("ERROR: Could not set a schunk in the b2d dict_store",
        s_set_all_schunks(ds) == 0);

    /* remove() must delete both the backing file and the in-memory index entry */
    for (size_t i = 0; i < SCHUNKS_SIZE / 2; i+=1) {
        mu_assert("ERROR: Could not remove a key from the b2d dict_store",
            blosc2_dict_store_remove(ds, (const uint8_t*)paths[i]) == 0);
        mu_assert("ERROR: The removed key is still reported as present in the b2d dict_store",
            !blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
        blosc2_store_value item = {0};
        mu_assert("ERROR: get() must return NOT_FOUND after removing a key from the b2d dict_store",
            blosc2_dict_store_get(ds, (const uint8_t*)paths[i], &item) == BLOSC2_ERROR_NOT_FOUND);
    }
    mu_assert("ERROR: The length was not updated after removing keys from the b2d dict_store",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE - SCHUNKS_SIZE / 2);

    /* Removing a missing key must be a silent no-op */
    mu_assert("ERROR: remove() of a missing key must succeed",
        blosc2_dict_store_remove(ds, (const uint8_t*)"/does/not/exist") == 0);
    mu_assert("ERROR: The length changed after removing a missing key",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE - SCHUNKS_SIZE / 2);

    mu_assert("ERROR: Could not close the b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    blosc2_dict_store_free(ds);
    ds = NULL;

    /* Reopen and verify the removal survived a roundtrip to disk */
    mu_assert("ERROR: Could not reopen the b2d dict_store",
        blosc2_dict_store_open(&ds, dir_path, BLOSC2_STORE_OPEN_MODE_RDONLY) == 0);
    mu_assert("ERROR: The length after reopening the b2d dict_store is wrong",
        blosc2_dict_store_get_length(ds) == SCHUNKS_SIZE - SCHUNKS_SIZE / 2);
    for (size_t i = 0; i < SCHUNKS_SIZE / 2; i+=1) {
        mu_assert("ERROR: A removed key is present after reopening the b2d dict_store",
            !blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
    }
    for (size_t i = SCHUNKS_SIZE / 2; i < SCHUNKS_SIZE; i+=1) {
        mu_assert("ERROR: A surviving key is missing after reopening the b2d dict_store",
            blosc2_dict_store_contains_key(ds, (const uint8_t*)paths[i]));
    }
    mu_assert("ERROR: Could not close the reopened b2d dict_store",
        blosc2_dict_store_close(ds) == 0);
    remove_dir_recursive(dir_path);


    return EXIT_SUCCESS;
}

static char* all_tests(void) {
    /* Tests when the dict_store data structure only stores data in memory */
    RUN_TEST(test_dict_store_new_memory);

    RUN_TEST(test_dict_store_introspection_memory); // It also tests get_capacity/get_storage_type/get_open_mode

    RUN_TEST(test_dict_store_threshold);

    RUN_TEST(test_dict_store_set_memory); // It also tests the iterator

    RUN_TEST(test_dict_store_get_memory);

    RUN_TEST(test_dict_store_contains_key_memory);

    RUN_TEST(test_dict_store_key_normalization_memory);

    RUN_TEST(test_dict_store_overwrite_memory);

    RUN_TEST(test_dict_store_iter_count_memory);

    RUN_TEST(test_dict_store_remove_memory);

    RUN_TEST(test_dict_store_to_b2z_memory);

    RUN_TEST(test_dict_store_to_from_buffer); // It also tests to_buffer/from_buffer

    RUN_TEST(test_dict_store_invalid_params);

    /* b2z tests */
    RUN_TEST(test_dict_store_new_b2z);

    RUN_TEST(test_dict_store_set_b2z);

    RUN_TEST(test_dict_store_open_b2z);

    RUN_TEST(test_dict_store_remove_b2z);

    /* b2d tests */
    RUN_TEST(test_dict_store_new_b2d);

    RUN_TEST(test_dict_store_open_b2d);

    RUN_TEST(test_dict_store_remove_b2d);

    RUN_TEST(test_dict_store_to_b2z);

    RUN_TEST(test_dict_store_to_b2d);

    RUN_TEST(test_dict_store_embed_basic);

    RUN_TEST(test_dict_store_embed_b2z);

    RUN_TEST(test_dict_store_embed_b2d);

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

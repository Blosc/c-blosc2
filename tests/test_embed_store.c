/*
Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "test_common.h"
#include <blosc2.h>
#include <blosc2/embed-store.h>
#include "../blosc/embed-store/embed-store-private.h"
#include <utils/msgpack.h>

#define KEYS_LENGTH 3
#define SCHUNK_SIZE 2048

static blosc2_embed_store* es = NULL;

static blosc2_cparams cparams;
static blosc2_dparams dparams;
static blosc2_io io;
static blosc2_storage b2_storage = {0};
static uint8_t buffer[KEYS_LENGTH][SCHUNK_SIZE];
static size_t schunk_frame_len = 0;
static uint8_t* packed = NULL;
static size_t packed_size = 0;
static const char* packed_file = "test_embed_store.msgpack";
static const char* packed_file_v1 = "test_embed_store_v1.msgpack";
static const char* packed_file_v2 = "test_embed_store_v2.b2e";
static const uint8_t* embed_store_v1 = (const uint8_t*)"test_embed_store_v1.b2e";
static int s_default_chunksize = 256; // Bytes

static const uint8_t* keys[] = {
    (const uint8_t*)"/first_key",
    (const uint8_t*)"/second_key",
    (const uint8_t*)"/caterva"
};

static const uint8_t* s_V1[] = {
    (const uint8_t*)"/first_key",
    (const uint8_t*)"/second_key",
    (const uint8_t*)"/caterva"
};
static const size_t s_V1_length = 3;

static bool s_str_in_array(const uint8_t* candidate, const uint8_t** arr, size_t arr_size) {
    for (size_t i = 0; i < arr_size; i+=1) {
        if (strcmp((const char*)candidate, (const uint8_t*)arr[i]) == 0) {
            return true;
        }
    }
    return false;
}

static blosc2_schunk* schunks[KEYS_LENGTH];

static void s_tear_down() {
    if (es != NULL) {
        blosc2_embed_store_free(es);
        es = NULL;
    }
}

#define RUN_TEST(func)  \
    do { \
        mu_run_test(func); \
        s_tear_down(); \
        printf("FINISHED TEST: '" #func "'\n"); \
    } while (0);

int tests_run = 0; //  This variable is modified by test_common, it is declared as extern in another header


static int s_open_msgpack(const char* packed_file, uint8_t** out_buffer, size_t* size) {
    int fd = open(packed_file, O_RDONLY | O_CLOEXEC );
    if (fd < 0) {
        BLOSC_TRACE_ERROR("Could not open %s", packed_file);
        return BLOSC2_ERROR_FILE_OPEN;
    }

    struct stat buf = {0};
    if (fstat(fd, &buf) != 0) {
        close(fd);
        return BLOSC2_ERROR_FILE_OPEN;
    }
    size_t file_size = buf.st_size;
    uint8_t* buff = malloc(sizeof(uint8_t) * file_size);
    if (buff == NULL) {
        close(fd);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    read(fd, buff, file_size);
    *size = file_size;
    *out_buffer = buff;
    close(fd);
    return BLOSC2_ERROR_SUCCESS;
}
static int s_write_msgpack(const char* destination_file, const uint8_t* packed_buffer, size_t packed_buffer_size) {
    FILE* file = fopen(destination_file, "wb");
    fwrite(packed_buffer, sizeof(uint8_t), packed_buffer_size, file);
    fclose(file);
    return BLOSC2_ERROR_SUCCESS;
}

static char* test_embed_store_new() {
    int rc = blosc2_embed_store_new(NULL, NULL, -1);
    mu_assert("ERROR: This operation should fail with BLOSC2_ERROR_NULL_POINTER error", rc == BLOSC2_ERROR_NULL_POINTER);
    rc = blosc2_embed_store_new(&es, NULL, -1);
    mu_assert("ERROR: Creation should have failed", rc == BLOSC2_ERROR_INVALID_PARAM);
    rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Creation should have succeed", rc == 0);
    mu_assert("ERROR: Wrong open mode detected, it should be B2_ES_RDWR", blosc2_embed_store_get_open_mode(es) == B2_ES_RDWR);
    blosc2_embed_store_free(es);
    es = NULL;
    rc = blosc2_embed_store_new(&es, &b2_storage, s_default_chunksize);
    mu_assert("ERROR: This should not work. EmbedStore's typedisze must be equal to 1", rc != 0);

    return NULL;
}

static char* test_embed_store_open_v1() {
    // TODO
    int rc = blosc2_embed_store_open(&es, embed_store_v1, B2_ES_RDONLY);
    mu_assert("ERROR: Could not open embed store", rc == 0);
    mu_assert("ERROR: embed store was no allocated", es != NULL);
    mu_assert("ERROR: Length was not the expected", blosc2_embed_store_get_length(es) == s_V1_length);

    return NULL;
}

static int s_set_elements(blosc2_embed_store* es);

// Creates (and persists to disk) a version 2 EmbedStore with the elements described by `s_set_elements`.
// The store is written using the current default version (V2), so it can later be opened with
// blosc2_embed_store_open using the version 2 deserialization path.
static int s_create_persisted_v2_store(const char* urlpath) {
    // Make sure we do not trip over a leftover store from a previous run
    remove(urlpath);

    blosc2_cparams cparams_v2 = blosc2_get_blosc2_cparams_defaults();
    cparams_v2.typesize = 1;
    blosc2_dparams dparams_v2 = blosc2_get_blosc2_dparams_defaults();
    blosc2_storage storage_v2 = {
        .contiguous = true,
        .cparams = &cparams_v2,
        .dparams = &dparams_v2,
        .io = NULL,
        .urlpath = (char*)urlpath,
    };

    blosc2_embed_store* writer = NULL;
    int rc = blosc2_embed_store_new(&writer, &storage_v2, s_default_chunksize);
    if (rc != 0 || writer == NULL) {
        return rc != 0 ? rc : BLOSC2_ERROR_FAILURE;
    }

    rc = s_set_elements(writer);
    if (rc == 0) {
        rc = blosc2_embed_store_close(writer);
    }
    // Freeing the writer flushes the backing frame to disk
    blosc2_embed_store_free(writer);
    return rc;
}

static char* test_embed_store_open_v2() {
    int rc = s_create_persisted_v2_store(packed_file_v2);
    mu_assert("ERROR: Could not create the persisted v2 embed store", rc == 0);

    rc = blosc2_embed_store_open(&es, (const uint8_t*)packed_file_v2, B2_ES_RDONLY);
    mu_assert("ERROR: Could not open the v2 embed store", rc == 0);
    mu_assert("ERROR: The opened v2 embed store was not allocated", es != NULL);
    mu_assert("ERROR: Wrong open mode detected, it should be B2_ES_RDONLY",
              blosc2_embed_store_get_open_mode(es) == B2_ES_RDONLY);
    mu_assert("ERROR: Opened v2 embed store's length is not the expected",
              blosc2_embed_store_get_length(es) == KEYS_LENGTH);

    // Every key that was stored must be reachable, and its container loadable
    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        mu_assert("ERROR: A stored key is not present in the opened v2 embed store",
                  blosc2_embed_store_contains_key(es, keys[i]));
        b2_es_item item;
        rc = blosc2_embed_store_get(es, keys[i], &item);
        mu_assert("ERROR: Could not get a value from the opened v2 embed store", rc == 0);
        mu_assert("ERROR: item tag should be local", item.tag == B2_ES_VALUE_LOCAL);
        mu_assert("ERROR: The retrieved schunk is NULL", item.schunk != NULL);
        rc = blosc2_schunk_free(item.schunk);
        mu_assert("ERROR: Could not free the retrieved schunk", rc == 0);
    }

    // Iteration must yield exactly the stored keys
    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(es);
    mu_assert("ERROR: Could not allocate an iterator over the opened v2 embed store", iter != NULL);
    size_t iterated = 0;
    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        mu_assert("ERROR: An unexpected key was found while iterating over the v2 embed store",
                  s_str_in_array(key, keys, KEYS_LENGTH));
        iterated += 1;
    }
    blosc2_embed_store_iter_free(iter);
    mu_assert("ERROR: The number of iterated keys is not the expected", iterated == KEYS_LENGTH);

    // A store opened as read-only must reject writes
    rc = blosc2_embed_store_set(es, "/new_key", schunks[0]);
    mu_assert("ERROR: set must fail on a read-only embed store", rc != 0);

    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close the opened v2 embed store", rc == 0);

    remove(packed_file_v2);
    return NULL;
}

static char* test_embed_store_open_v2_readwrite() {
    int rc = s_create_persisted_v2_store(packed_file_v2);
    mu_assert("ERROR: Could not create the persisted v2 embed store", rc == 0);

    rc = blosc2_embed_store_open(&es, (const uint8_t*)packed_file_v2, B2_ES_RDWR);
    mu_assert("ERROR: Could not open the v2 embed store in RDWR mode", rc == 0);
    mu_assert("ERROR: Wrong open mode detected, it should be B2_ES_RDWR",
              blosc2_embed_store_get_open_mode(es) == B2_ES_RDWR);

    // Writes must be accepted when the store is opened as RDWR
    rc = blosc2_embed_store_set(es, "/new_key", schunks[0]);
    mu_assert("ERROR: Could not set a new key in a RDWR-open embed store", rc == 0);
    mu_assert("ERROR: Length was not increased after setting a new key",
              blosc2_embed_store_get_length(es) == KEYS_LENGTH + 1);

    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close the RDWR v2 embed store", rc == 0);
    blosc2_embed_store_free(es);
    es = NULL;

    // Reopen in read-only mode and check that the change was persisted
    rc = blosc2_embed_store_open(&es, (const uint8_t*)packed_file_v2, B2_ES_RDONLY);
    mu_assert("ERROR: Could not reopen the v2 embed store", rc == 0);
    mu_assert("ERROR: The new key was not persisted", blosc2_embed_store_contains_key(es, "/new_key"));
    mu_assert("ERROR: The reopened v2 embed store's length is not the expected",
              blosc2_embed_store_get_length(es) == KEYS_LENGTH + 1);

    b2_es_item item;
    rc = blosc2_embed_store_get(es, "/new_key", &item);
    mu_assert("ERROR: Could not get the newly added key's value", rc == 0);
    mu_assert("ERROR: Item tag should be LOCAL", item.tag == B2_ES_VALUE_LOCAL);
    mu_assert("ERROR: The newly added key's schunk is NULL", item.schunk != NULL);
    blosc2_schunk_free(item.schunk);

    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close the reopened v2 embed store", rc == 0);
    blosc2_embed_store_free(es);
    es = NULL;

    remove(packed_file_v2);
    return NULL;
}

static char* test_embed_store_open_errors() {
    es = NULL;

    int rc = blosc2_embed_store_open(NULL, (const uint8_t*)packed_file_v2, B2_ES_RDONLY);
    mu_assert("ERROR: open with a NULL store pointer should fail with BLOSC2_ERROR_NULL_POINTER",
              rc == BLOSC2_ERROR_NULL_POINTER);

    rc = blosc2_embed_store_open(&es, NULL, B2_ES_RDONLY);
    mu_assert("ERROR: open with a NULL urlpath should fail with BLOSC2_ERROR_NULL_POINTER",
              rc == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: The store pointer should remain NULL after a failed open", es == NULL);

    rc = blosc2_embed_store_open(&es, (const uint8_t*)"whatever.b2e", (b2_es_open_mode)99);
    mu_assert("ERROR: open with an invalid open mode should fail with BLOSC2_ERROR_INVALID_PARAM",
              rc == BLOSC2_ERROR_INVALID_PARAM);
    mu_assert("ERROR: The store pointer should remain NULL after a failed open", es == NULL);

    rc = blosc2_embed_store_open(&es, (const uint8_t*)"this_file_does_not_exist.b2e", B2_ES_RDONLY);
    mu_assert("ERROR: open with a non-existent file should fail", rc < 0);
    mu_assert("ERROR: The store pointer should remain NULL after a failed open", es == NULL);

    return NULL;
}

// B2_ES_WRONLY creates a fresh store on a non-existent path. The store must be usable
// (set/close) and, crucially, reopenable afterwards, which requires the version metalayer to
// be written at creation time.
static char* test_embed_store_open_wronly() {
    const char* wronly_file = "test_embed_store_wronly.b2e";
    remove(wronly_file);

    // A fresh store can be created in WRONLY mode, and it is a writing mode
    es = NULL;
    int rc = blosc2_embed_store_open(&es, (const uint8_t*)wronly_file, B2_ES_WRONLY);
    mu_assert("ERROR: Could not create a WRONLY embed store", rc == 0);
    mu_assert("ERROR: The WRONLY embed store was not allocated", es != NULL);
    mu_assert("ERROR: Wrong open mode detected, it should be B2_ES_WRONLY",
              blosc2_embed_store_get_open_mode(es) == B2_ES_WRONLY);

    rc = blosc2_embed_store_set(es, keys[0], schunks[0]);
    mu_assert("ERROR: set must be accepted on a WRONLY embed store", rc == 0);
    mu_assert("ERROR: The WRONLY embed store's length is not the expected",
              blosc2_embed_store_get_length(es) == 1);
    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close the WRONLY embed store", rc == 0);
    blosc2_embed_store_free(es);
    es = NULL;

    // Reopening must succeed: this pins down that the version metalayer was written
    rc = blosc2_embed_store_open(&es, (const uint8_t*)wronly_file, B2_ES_RDONLY);
    mu_assert("ERROR: Could not reopen the WRONLY-created embed store", rc == 0);
    mu_assert("ERROR: The reopened embed store was not allocated", es != NULL);
    mu_assert("ERROR: The persisted key is missing after reopening",
              blosc2_embed_store_contains_key(es, keys[0]));
    mu_assert("ERROR: The reopened embed store's length is not the expected",
              blosc2_embed_store_get_length(es) == 1);
    blosc2_embed_store_free(es);
    es = NULL;

    // A second WRONLY open on the now existing file must be rejected
    rc = blosc2_embed_store_open(&es, (const uint8_t*)wronly_file, B2_ES_WRONLY);
    mu_assert("ERROR: WRONLY open must reject an existing file", rc != 0);
    mu_assert("ERROR: The store pointer should remain NULL after a failed WRONLY open", es == NULL);

    remove(wronly_file);
    return NULL;
}

static int s_set_elements(blosc2_embed_store* es) {
    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        int rc = blosc2_embed_store_set(es, keys[i], schunks[i]);
        if (rc != 0) {
            return rc;
        }
    }
    return BLOSC2_ERROR_SUCCESS;
}

static char* test_embed_store_set() {
    int rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create embed store", rc == 0);

    rc = s_set_elements(es);
    mu_assert("ERROR: Set operation failed", rc == 0);
    mu_assert("ERROR: EmbedStore's length was not setted properly", blosc2_embed_store_get_length(es) == KEYS_LENGTH);

    // Ahora, hay que comprobar el interior del EmbedStore y cerciorarnos de que los contenidos son correctos
    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(es);
    mu_assert("ERROR: Could not allocate memory for the blosc2_embed_store_iter", iter != NULL);
    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        mu_assert("ERROR: A NULL key was found while iterating", key != NULL);
        b2_es_item item;
        rc = blosc2_embed_store_iter_get_container(iter, &item);
        blosc2_schunk* schunk = item.schunk;
        mu_assert("ERROR: Could not obtain container from iterator", rc == 0);
        mu_assert("ERROR: Could not retrieve schunk from comtainer", schunk != NULL);
        blosc2_schunk_free(schunk);
    }
    blosc2_embed_store_iter_free(iter);

    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close EmbedStore", rc == 0);
    return NULL;
}

static char* test_embed_store_get() {
    int rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create embed store", rc == 0);
    mu_assert("ERROR: EmbedStore cannot be NULL", es != NULL);
    rc = s_set_elements(es);
    mu_assert("ERROR: Set operation failed", rc == 0);
    mu_assert("ERROR: EmbedStore's length was not setted properly", blosc2_embed_store_get_length(es) == KEYS_LENGTH);

    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        b2_es_item item;
        rc = blosc2_embed_store_get(es, keys[i], &item);
        mu_assert("ERROR: Could not get element", rc == 0);
        mu_assert("ERROR: Item's tag should be local", item.tag == B2_ES_VALUE_LOCAL);
        mu_assert("ERROR: Schunk pointer unexpectedly equals to NULL", item.schunk != NULL);
        rc = blosc2_schunk_free(item.schunk);
        mu_assert("ERROR: Could not free schunk", rc == 0);
    }

    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close EmbedStore", rc == 0);
    return NULL;
}

static char* test_embed_store_remove() {
    int rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create embed store", rc == 0);
    rc = s_set_elements(es);
    mu_assert("ERROR: Set operation failed", rc == 0);
    mu_assert("ERROR: EmbedStore's length was not setted properly", blosc2_embed_store_get_length(es) == KEYS_LENGTH);

    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        rc = blosc2_embed_store_remove(es, keys[i]);
        mu_assert("ERROR: Key could not be removed", rc == 0);
        mu_assert("ERROR: The length of the embed store was not properly reduced", blosc2_embed_store_get_length(es) == KEYS_LENGTH - i - 1);
    }

    rc = blosc2_embed_store_close(es);
    mu_assert("ERROR: Could not close EmbedStore", rc == 0);
    return NULL;
}

static char* test_serialize_embed_store() {
    int rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create embed store", rc == 0);

    b2_es_value val = {
        .f_local = {
            .offset = 10,
            .length = 10
        }
    };
    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        hash_map_set(&es->hm, keys[i], &val);
    }

    uint8_t* packed_buffer = NULL;
    size_t packed_size = 0;
    rc = msgpack_embed_store_serialize(es, &packed_buffer, &packed_size);
    mu_assert("ERROR: msgpack_embed_store_serialize failed unexpedtedly", rc == 0);
    //s_write_msgpack("wrote.msgpack", packed_buffer, packed_size);
    free(packed_buffer);
    return NULL;
}

static void s_print_embed_store(blosc2_embed_store* estore) {
    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(estore);

    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        b2_es_item item = {0};
        blosc2_embed_store_iter_get_container(iter, &item);
        printf("Key: '%s'\n", (const char*)key);
        if (item.tag == B2_ES_VALUE_LOCAL) {
            printf("Local value\n");
            if (item.schunk != NULL) {
                blosc2_schunk_free(item.schunk);
            }
        } else if (item.tag == B2_ES_VALUE_REMOTE) {
            printf("urlbase: '%s'\n", (const char*)item.remote.urlbase);
            printf("path: '%s'\n", (const char*)item.remote.path);
        } else {
            printf("Could not identify tag\n");
        }
    }
}

// Deserializes the canonical v2 fixture and re-serializes it, comparing the result against the
// fixture. The serialized index is a msgpack map, which is unordered, so the entry order produced
// by the hash-map iterator need not match the fixture's entry order. The comparison is therefore
// done on the encoded content (current_offset plus every key -> [offset, length]), while also
// checking that the encoded size matches exactly. This still catches encoding/value regressions
// (including the map-header serializer bug that produced a truncated buffer).
static char* test_serialize_matches_fixture() {
    blosc2_embed_store* fixture = NULL;
    int rc = blosc2_embed_store_new(&fixture, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create the fixture store", rc == 0);

    rc = msgpack_embed_store_deserialize(packed, packed_size, fixture);
    //s_print_embed_store(fixture);
    mu_assert("ERROR: Could not deserialize the fixture", rc == 0);
    mu_assert("ERROR: The fixture's current_offset should be 100", fixture->current_offset == 100);

    uint8_t* out_buffer = NULL;
    size_t out_size = 0;
    rc = msgpack_embed_store_serialize(fixture, &out_buffer, &out_size);
    //s_write_msgpack("out_buf.msgpack", out_buffer, out_size);
    mu_assert("ERROR: Could not serialize the embed store", rc == 0);
    mu_assert("ERROR: Serialized size differs from the fixture size", out_size == packed_size);

    // Parse the produced buffer back and compare its content against the fixture
    blosc2_embed_store* roundtrip = NULL;
    rc = blosc2_embed_store_new(&roundtrip, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create the roundtrip store", rc == 0);
    rc = msgpack_embed_store_deserialize(out_buffer, out_size, roundtrip);
    free(out_buffer);
    mu_assert("ERROR: Could not deserialize the re-serialized buffer", rc == 0);

    mu_assert("ERROR: current_offset does not match the fixture",
              roundtrip->current_offset == fixture->current_offset);
    mu_assert("ERROR: The number of keys does not match the fixture",
              roundtrip->hm.length == fixture->hm.length);

    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(fixture);
    mu_assert("ERROR: Could not allocate an iterator over the fixture", iter != NULL);
    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        b2_es_value* expected = (b2_es_value*)hash_map_get(&fixture->hm, key);
        b2_es_value* obtained = (b2_es_value*)hash_map_get(&roundtrip->hm, key);
        mu_assert("ERROR: A key from the fixture is missing in the re-serialized buffer", obtained != NULL);
        mu_assert("ERROR: Diverging tags found, they should be equal", expected->tag == obtained->tag);
        if (expected->tag == B2_ES_VALUE_LOCAL) {
            mu_assert("ERROR: The serialized offset does not match the fixture",
                              obtained->f_local.offset == expected->f_local.offset);
            mu_assert("ERROR: The serialized length does not match the fixture",
                      obtained->f_local.length == expected->f_local.length);
        } else if (expected->tag == B2_ES_VALUE_REMOTE) {
            mu_assert("ERROR: urlbase strings does not match", strcmp((const char*)obtained->f_remote.urlbase, (const char*)expected->f_remote.urlbase) == 0);
            mu_assert("ERROR: path strings does not match", strcmp((const char*)obtained->f_remote.path, (const char*)expected->f_remote.path) == 0);
        } else {
            mu_assert("ERROR: Wrong tag found", false);
        }
    }
    blosc2_embed_store_iter_free(iter);

    blosc2_embed_store_free(fixture);
    blosc2_embed_store_free(roundtrip);
    return NULL;
}

static char* test_deserialize_embed_store_from_file() {
    int rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create embed store", rc == 0);

    rc = msgpack_embed_store_deserialize(packed, packed_size, es);
    mu_assert("ERROR: Could not deserialize embed store", rc == 0);
    mu_assert("ERROR: EmbedStore's length must be equal to 3", blosc2_embed_store_get_length(es) == 4);

    return NULL;
}

static char* test_serialize_version() {
    uint8_t* packed_buffer = NULL;
    size_t packed_size = 0;
    int rc = msgpack_container_serialize_version(1, &packed_buffer, &packed_size);
    mu_assert("ERROR: Could not serialize version", rc == 0);
    //s_write_msgpack("wrote.msgpack", packed_buffer, packed_size);
    rc = msgpack_validate_and_return_version(packed_buffer, packed_size);
    mu_assert("ERROR: Could not validate version serializer", rc > 0);
    free(packed_buffer);
    return NULL;
}

static char* test_deserialize_embed_store_v1_from_file() {
    int rc = blosc2_embed_store_new(&es, NULL, s_default_chunksize);
    mu_assert("ERROR: Could not create EmbedStore", rc == 0);

    uint8_t* packed_v1 = NULL;
    size_t packed_v1_size = 0;
    rc = s_open_msgpack(packed_file_v1, &packed_v1, &packed_v1_size);
    mu_assert("ERROR: Could not read v1 msgpack", rc == 0);
    rc = msgpack_embed_store_deserialize_v1(packed_v1, packed_v1_size, es);
    mu_assert("ERROR: Could not deserialize msgpack v1", rc == 0);
    free(packed_v1);

    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(es);
    mu_assert("ERROR: Could not allocate iter", iter != NULL);
    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        mu_assert("ERROR: Key was not found when it was expected",
            s_str_in_array(key, s_V1, s_V1_length) == true);
    }
    blosc2_embed_store_iter_free(iter);

    return NULL;
}

// Builds an in-memory SChunk carrying a valid (current default version, V2) EmbedStore
// index. It serializes a freshly built store populated with the shared `schunks` fixtures
// and re-hydrates it from the resulting cframe. The returned SChunk owns its own frame copy
// and must be freed by the caller, unless it is handed over to
// blosc2_embed_store_from_schunk() with `copy = false`, in which case the store adopts it.
static blosc2_schunk* s_build_embed_store_schunk(void) {
    blosc2_embed_store* writer = NULL;
    int rc = blosc2_embed_store_new(&writer, NULL, s_default_chunksize);
    if (rc != 0 || writer == NULL) {
        return NULL;
    }

    rc = s_set_elements(writer);
    if (rc == 0) {
        rc = blosc2_embed_store_close(writer);
    }
    if (rc != 0) {
        blosc2_embed_store_free(writer);
        return NULL;
    }

    uint8_t* frame = NULL;
    size_t frame_size = 0;
    rc = blosc2_embed_store_to_buffer(writer, &frame, &frame_size);
    blosc2_embed_store_free(writer);
    if (rc != 0 || frame == NULL) {
        free(frame);
        return NULL;
    }

    blosc2_schunk* schunk = blosc2_schunk_from_buffer(frame, (int64_t)frame_size, true);
    free(frame);
    return schunk;
}

// Creates a plain, in-memory SChunk with the given typesize and no EmbedStore metadata. It
// is only meant to exercise the rejection paths of blosc2_embed_store_from_schunk().
static blosc2_schunk* s_make_plain_schunk(int32_t typesize) {
    blosc2_cparams cparams_plain = blosc2_get_blosc2_cparams_defaults();
    cparams_plain.typesize = typesize;
    blosc2_dparams dparams_plain = blosc2_get_blosc2_dparams_defaults();
    blosc2_io io_plain = blosc2_get_blosc2_io_defaults();
    blosc2_storage storage_plain = blosc2_get_blosc2_storage_defaults();
    storage_plain.cparams = &cparams_plain;
    storage_plain.dparams = &dparams_plain;
    storage_plain.io = &io_plain;
    storage_plain.contiguous = true;
    return blosc2_schunk_new(&storage_plain);
}

static char* test_embed_store_from_schunk_errors() {
    es = NULL;

    // NULL arguments must be rejected
    int rc = blosc2_embed_store_from_schunk(NULL, schunks[0], false);
    mu_assert("ERROR: from_schunk with a NULL store pointer should fail with BLOSC2_ERROR_NULL_POINTER",
              rc == BLOSC2_ERROR_NULL_POINTER);

    rc = blosc2_embed_store_from_schunk(&es, NULL, false);
    mu_assert("ERROR: from_schunk with a NULL schunk should fail with BLOSC2_ERROR_NULL_POINTER",
              rc == BLOSC2_ERROR_NULL_POINTER);
    mu_assert("ERROR: The store pointer should remain NULL when schunk is NULL", es == NULL);

    // A schunk with typesize == 1 but without the EmbedStore version metalayer must be rejected
    blosc2_schunk* no_meta = s_make_plain_schunk(1);
    mu_assert("ERROR: Could not create a schunk without the EmbedStore metadata", no_meta != NULL);
    rc = blosc2_embed_store_from_schunk(&es, no_meta, false);
    mu_assert("ERROR: A schunk without the EmbedStore version metalayer must be rejected with BLOSC2_ERROR_NOT_FOUND",
              rc == BLOSC2_ERROR_NOT_FOUND);
    mu_assert("ERROR: The store pointer should remain NULL when validation fails", es == NULL);

    // A schunk whose typesize is not 1 must be rejected before anything else
    blosc2_schunk* bad_typesize = s_make_plain_schunk(4);
    mu_assert("ERROR: Could not create a schunk with a non-unit typesize", bad_typesize != NULL);
    rc = blosc2_embed_store_from_schunk(&es, bad_typesize, false);
    mu_assert("ERROR: A schunk with typesize != 1 must be rejected with BLOSC2_ERROR_INVALID_PARAM",
              rc == BLOSC2_ERROR_INVALID_PARAM);
    mu_assert("ERROR: The store pointer should remain NULL when validation fails", es == NULL);

    blosc2_schunk_free(no_meta);
    blosc2_schunk_free(bad_typesize);
    return NULL;
}

// Exercises the happy path for the current default version (V2): a valid backing SChunk
// yields an EmbedStore whose index can be queried and iterated. The retrieved values must
// be loadable as well.
static char* test_embed_store_from_schunk_v2() {
    blosc2_schunk* schunk = s_build_embed_store_schunk();
    mu_assert("ERROR: Could not build an in-memory EmbedStore schunk", schunk != NULL);

    es = NULL;
    int rc = blosc2_embed_store_from_schunk(&es, schunk, false);
    mu_assert("ERROR: Could not build an EmbedStore from a valid schunk", rc == 0);
    mu_assert("ERROR: from_schunk did not allocate the EmbedStore", es != NULL);
    mu_assert("ERROR: The length of the EmbedStore built from a schunk is not the expected",
              blosc2_embed_store_get_length(es) == KEYS_LENGTH);

    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        mu_assert("ERROR: A stored key is missing from the EmbedStore built from a schunk",
                  blosc2_embed_store_contains_key(es, keys[i]));
        b2_es_item item;
        rc = blosc2_embed_store_get(es, keys[i], &item);
        mu_assert("ERROR: Could not get a value from the EmbedStore built from a schunk", rc == 0);
        mu_assert("ERROR: Item's tag should be local", item.tag == B2_ES_VALUE_LOCAL);
        mu_assert("ERROR: The value retrieved from the EmbedStore built from a schunk is NULL", item.schunk != NULL);
        rc = blosc2_schunk_free(item.schunk);
        mu_assert("ERROR: Could not free the value retrieved from the EmbedStore built from a schunk", rc == 0);
    }

    // Iteration over the reconstructed store must yield exactly the stored keys
    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(es);
    mu_assert("ERROR: Could not allocate an iterator over the reconstructed EmbedStore", iter != NULL);
    size_t iterated = 0;
    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        mu_assert("ERROR: An unexpected key was found while iterating over the reconstructed EmbedStore",
                  s_str_in_array(key, keys, KEYS_LENGTH));
        iterated += 1;
    }
    blosc2_embed_store_iter_free(iter);
    mu_assert("ERROR: The number of iterated keys is not the expected", iterated == KEYS_LENGTH);

    // The EmbedStore now owns `schunk`; s_tear_down() releases both of them
    return NULL;
}

// Exercises the happy path for the legacy V1 format by opening the persisted V1 fixture as
// a plain SChunk and converting it through blosc2_embed_store_from_schunk().
static char* test_embed_store_from_schunk_v1() {
    blosc2_schunk* schunk = blosc2_schunk_open((const char*)embed_store_v1);
    mu_assert("ERROR: Could not open the V1 fixture as a schunk", schunk != NULL);

    es = NULL;
    int rc = blosc2_embed_store_from_schunk(&es, schunk, false);
    mu_assert("ERROR: Could not build an EmbedStore from a V1 schunk", rc == 0);
    mu_assert("ERROR: from_schunk did not allocate the V1 EmbedStore", es != NULL);
    mu_assert("ERROR: The length of the V1 EmbedStore built from a schunk is not the expected",
              blosc2_embed_store_get_length(es) == s_V1_length);

    for (size_t i = 0; i < s_V1_length; i+=1) {
        mu_assert("ERROR: A V1 key is missing from the EmbedStore built from a schunk",
                  blosc2_embed_store_contains_key(es, (const uint8_t*)s_V1[i]));
    }

    // The EmbedStore now owns `schunk`; s_tear_down() releases both of them
    return NULL;
}

// blosc2_embed_store_from_schunk() either adopts the supplied SChunk (copy == false) or
// works on a private copy of it (copy == true). This test pins down both ownership
// contracts through the backing SChunk pointer:
//   * copy == false: es->schunk is the very same SChunk, so freeing the store frees it.
//   * copy == true:  es->schunk is a distinct copy; freeing the store leaves the original
//                    untouched and the caller stays responsible for it.
static char* test_embed_store_from_schunk_ownership() {
    // copy == false: the store adopts the provided schunk
    blosc2_schunk* original = s_build_embed_store_schunk();
    mu_assert("ERROR: Could not build an in-memory EmbedStore schunk", original != NULL);

    es = NULL;
    int rc = blosc2_embed_store_from_schunk(&es, original, false);
    mu_assert("ERROR: Could not build an EmbedStore from an adopted schunk", rc == 0);
    mu_assert("ERROR: from_schunk(copy=false) must adopt the provided schunk", es->schunk == original);
    blosc2_embed_store_free(es);  // this releases the adopted schunk
    es = NULL;

    // copy == true: the store works on a private copy and the caller keeps the original
    original = s_build_embed_store_schunk();
    mu_assert("ERROR: Could not build an in-memory EmbedStore schunk", original != NULL);

    es = NULL;
    rc = blosc2_embed_store_from_schunk(&es, original, true);
    mu_assert("ERROR: Could not build an EmbedStore from a copied schunk", rc == 0);
    mu_assert("ERROR: from_schunk(copy=true) must not adopt the provided schunk", es->schunk != original);
    mu_assert("ERROR: The EmbedStore built from a copied schunk is not usable",
              blosc2_embed_store_get_length(es) == KEYS_LENGTH);

    blosc2_embed_store_free(es);  // this releases the copy only
    es = NULL;
    mu_assert("ERROR: The original schunk must remain valid when copy=true",
              blosc2_schunk_frame_len(original) > 0);
    rc = blosc2_schunk_free(original);
    mu_assert("ERROR: Could not free the original schunk", rc == 0);

    return NULL;
}

static char* test_mmap(void) {

    blosc2_embed_store* estore = NULL;

    {
        blosc2_io io = blosc2_get_blosc2_io_defaults();
        io.id = BLOSC2_IO_FILESYSTEM_MMAP;
        blosc2_stdio_mmap* mmap_file = malloc(sizeof(blosc2_stdio_mmap));
        memcpy(mmap_file, &BLOSC2_STDIO_MMAP_DEFAULTS, sizeof(blosc2_stdio_mmap));
        mmap_file->needs_free = true;
        io.params = mmap_file;
        mmap_file->mode = "r";
        int rc = blosc2_embed_store_open_udio(&estore, (const uint8_t*)"test_embed_store_v1.b2e", B2_ES_RDONLY, &io);
        mu_assert("ERROR: Could not create new EmbedStore with mmap options", rc == 0);
    }
    int rc = BLOSC2_ERROR_SUCCESS;
    blosc2_embed_store_iter* iter = blosc2_embed_store_iter_new(estore);
    mu_assert("ERROR: Could not allocate memory for the blosc2_embed_store_iter", iter != NULL);

    while (blosc2_embed_store_iter_next(iter)) {
        const uint8_t* key = blosc2_embed_store_iter_get_key(iter);
        mu_assert("ERROR: NULL key retrieved", key != NULL);
        b2_es_item item = {0};
        rc = blosc2_embed_store_iter_get_container(iter, &item);
        mu_assert("ERROR: Could not retrieve container", rc == 0);
        if (item.tag == B2_ES_VALUE_LOCAL) {
            blosc2_schunk* schunk = item.schunk;
            mu_assert("ERROR: Could not obtain schunk", schunk != NULL);
            blosc2_schunk_free(schunk);
        } else if (item.tag == B2_ES_VALUE_REMOTE) {
            mu_assert("ERROR: urlbase shouldn't be NULL", item.remote.urlbase != NULL);
            mu_assert("ERROR: path shouldn't be NULL", item.remote.path != NULL);
        } else {
            mu_assert("ERROR: Invalid tag found", false);
        }
    }
    blosc2_embed_store_iter_free(iter);

    blosc2_embed_store_free(estore);

    return NULL;
}

static char* s_run_tests(void) {
    RUN_TEST(test_embed_store_new);
    RUN_TEST(test_embed_store_set);
    RUN_TEST(test_embed_store_get);
    RUN_TEST(test_embed_store_open_v1);
    RUN_TEST(test_embed_store_open_v2);
    RUN_TEST(test_embed_store_open_v2_readwrite);
    RUN_TEST(test_embed_store_open_errors);
    RUN_TEST(test_embed_store_open_wronly);
    RUN_TEST(test_embed_store_remove);
    RUN_TEST(test_serialize_embed_store);
    RUN_TEST(test_serialize_matches_fixture);
    RUN_TEST(test_deserialize_embed_store_from_file);
    RUN_TEST(test_serialize_version);
    RUN_TEST(test_deserialize_embed_store_v1_from_file);
    RUN_TEST(test_embed_store_from_schunk_errors);
    RUN_TEST(test_embed_store_from_schunk_v2);
    RUN_TEST(test_embed_store_from_schunk_v1);
    RUN_TEST(test_embed_store_from_schunk_ownership);
    RUN_TEST(test_mmap);

    return NULL;
}

static void s_init_tests(void) {
    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        for (size_t j = 0; j < SCHUNK_SIZE; j+=1) {
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

    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
        blosc2_schunk* pschunk = blosc2_schunk_new(&b2_storage);
        blosc2_schunk_append_buffer(pschunk, buffer, KEYS_LENGTH * KEYS_LENGTH);
        schunks[i] = pschunk;
    }
    schunk_frame_len = blosc2_schunk_frame_len(schunks[0]);
}

static void s_destroy_tests(void) {
    for (size_t i = 0; i < KEYS_LENGTH; i+=1) {
       blosc2_schunk_free(schunks[i]);
    }
}

int main(void) {
    blosc2_init();

    if (s_open_msgpack(packed_file, &packed, &packed_size) != 0) {
        printf("Could not open packed at %s\n", packed_file);
        return -1;
    }
    s_init_tests();

    char* result = s_run_tests();
    if (result != NULL) {
        printf("\n (%s)\n", result);
    } else {
        printf("\n ALL TESTS PASSED\n");
    }
    printf("\tTests run: %d\n", tests_run);

    s_destroy_tests();
    blosc2_destroy();
    if (packed != NULL) free(packed);
    return result != NULL;
}

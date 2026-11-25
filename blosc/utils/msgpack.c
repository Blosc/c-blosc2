/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "msgpack.h"
#include <utils/hash_map.h>
#include "blosc-private.h"
#include "embed-store/embed-store-private.h"

#include <assert.h>
#include <string.h>

#define STR_FIX_TAG (0b10100000)
#define STR_FIX_TAG_MASK (0b11100000)
#define STR_FIX_LEN_MASK (0b00011111)
#define STR_8_TAG (0b11011001)
#define STR_16_TAG (0b11011010)
#define STR_32_TAG (0b11011011)

#define ARR_FIX_TAG (0b10010000)
#define ARR_FIX_TAG_MASK (0b11110000)
#define ARR_FIX_LEN_MASK (0b00001111)
#define ARR_16_TAG (0b11011100)
#define ARR_32_TAG (0b11011101)

#define UINT_FIX_TAG (0)
#define UINT_FIX_TAG_MASK (0b10000000)
#define UINT_FIX_VALUE_MASK (0b01111111)
#define UINT_8_TAG (0b11001100)
#define UINT_16_TAG (0b11001101)
#define UINT_32_TAG (0b11001110)
#define UINT_64_TAG (0b11001111)

#define MAP_FIX_TAG (0b10000000)
#define MAP_FIX_TAG_MASK (0b11110000)
#define MAP_FIX_LEN_MASK (0b00001111)
#define MAP_16_TAG (0b11011110)
#define MAP_32_TAG (0b11011111)

#define MSGPACK_DEFAULT_BUFFER_SIZE 1024
#define MSGPACK_DEFAULT_BUFFER_REALLOC_FACTOR 2
#define MSGPACK_DEFAULT_BUFFER_LOAD_FACTOR 0.70f

static const char s_version_string[] = "version";
static const size_t s_version_string_length = sizeof(s_version_string) - 1;
static const size_t s_version_packed_length = s_version_string_length + 3;
static const char s_v1_current_offset[] = "current_offset";
static const char s_v1_embed_map[] = "embed_map";
static const char s_v1_offset[] = "offset";
static const char s_v1_length[] = "length";
static const char s_v1_urlbase[] = "urlbase";
static const char s_v1_path[] = "path";

static int s_serialize_string(const uint8_t* str, uint8_t* packed_buffer, size_t* pindex) {
    size_t packed_index = *pindex;
    size_t str_length = strlen((const char*)str);

    if (str_length <= 31) { // Fix string
        packed_buffer[packed_index] = (uint8_t)STR_FIX_TAG | (uint8_t)str_length;
        packed_index += 1;
    } else if (str_length <= 255) {
        packed_buffer[packed_index] = (uint8_t)STR_8_TAG;
        packed_index += 1;
        packed_buffer[packed_index] = (uint8_t)str_length;
        packed_index += 1;
    } else if (str_length <= 65535) {
        packed_buffer[packed_index] = (uint8_t)STR_16_TAG;
        packed_index += 1;
        uint16_t tb = 0;
        to_big(&tb, &str_length, sizeof(tb));
        *(uint16_t*)(packed_buffer + packed_index) = tb;
        packed_index += 2;
    } else if (str_length <= 2147483647u) {
        packed_buffer[packed_index] = (uint8_t)STR_32_TAG;
        packed_index += 1;
        uint32_t tb = 0;
        to_big(&tb, &str_length, sizeof(tb));
        *(uint32_t*)(packed_buffer + packed_index) = tb;
        packed_index += 4;
    } else {
        BLOSC_TRACE_ERROR("String length cannot be serialized in msgpack, too large: %lu", str_length);
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    memcpy(&packed_buffer[packed_index], str, str_length);
    packed_index += str_length;

    *pindex = packed_index;

    return BLOSC2_ERROR_SUCCESS;
}

static int s_serialize_uint64(uint64_t value, uint8_t* packed_buffer, size_t* pindex) {
    size_t packed_index = *pindex;

    packed_buffer[packed_index] = (uint8_t)UINT_64_TAG;
    packed_index += 1;
    uint64_t tb = 0;
    to_big(&tb, &value, sizeof(tb));
    *((uint64_t*)(packed_buffer + packed_index)) = tb;
    packed_index += 8;

    *pindex = packed_index;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_serialize_uint(size_t value, uint8_t* packed_buffer, size_t* pindex) {
    size_t packed_index = *pindex;

    if (value <= 127) {
        packed_buffer[packed_index] = (uint8_t)UINT_FIX_TAG | (uint8_t)value;
        packed_index += 1;
    } else if (value <= 255ul) {
        packed_buffer[packed_index] = (uint8_t)UINT_8_TAG;
        packed_index += 1;
        packed_buffer[packed_index] = (uint8_t)value;
        packed_index += 1;
    } else if (value <= 65535ul) {
        packed_buffer[packed_index] = (uint8_t)UINT_16_TAG;
        packed_index += 1;
        uint16_t tb = 0;
        to_big(&tb, &value, sizeof(tb));
        *((uint16_t*)(packed_buffer + packed_index)) = tb;
        packed_index += 2;
    } else if (value <= 4294967295ul) {
        packed_buffer[packed_index] = (uint8_t)UINT_32_TAG;
        packed_index += 1;
        uint32_t tb = 0;
        to_big(&tb, &value, sizeof(tb));
        *((uint32_t*)(packed_buffer + packed_index)) = tb;
        packed_index += 4;
    } else {
        packed_buffer[packed_index] = (uint8_t)UINT_64_TAG;
        packed_index += 1;
        uint64_t tb = 0;
        to_big(&tb, &value, sizeof(tb));
        *((uint64_t*)(packed_buffer + packed_index)) = tb;
        packed_index += 8;
    }

    *pindex = packed_index;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_serialize_embed_store_local_value(b2_es_value value, uint8_t* packed_buffer, size_t* pindex) {
    size_t packed_index = *pindex;

    packed_buffer[packed_index] = (uint8_t)ARR_FIX_TAG | (uint8_t)2;
    packed_index += 1;
    int rc = s_serialize_uint(value.f_local.offset, packed_buffer, &packed_index);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize offset");
        return rc;
    }
    rc = s_serialize_uint(value.f_local.length, packed_buffer, &packed_index);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize length");
        return rc;
    }

    *pindex = packed_index;
    return BLOSC2_ERROR_SUCCESS;
}

/*
This struct is just a commodity, a cleaner way to serialize msgpack stuff
This is not, by any means, a general purpose serializer, and it does not aim to be one

The use of this struct is justified when the serialization needs a buffer whose final size is unknown. The wrapper
functions s_msgpack_serializer_serialize manage buffer resizing. The struct just contains the metadata for that resize
to happen safely. If you already know the length of the resultant msgpack string, then just use s_serialize_*
*/
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t index; // Where to start writing the next character
    int resize_factor; // Typically x2
    float load_factor; // Typically 0.7f, standard parameters basically
}msgpack_serializer;

static int s_msgpack_serializer_init(msgpack_serializer* pserial, size_t initial_capacity, int resize_factor, float load_factor) {
    uint8_t* buffer = malloc(initial_capacity);
    if (buffer == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate msgpack_serializer buffer");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    pserial->capacity = initial_capacity;
    pserial->buffer = buffer;
    pserial->resize_factor = resize_factor;
    pserial->load_factor = load_factor;
    return BLOSC2_ERROR_SUCCESS;
}

static void s_msgpack_serializer_free_stack(msgpack_serializer* serial) {
    free(serial->buffer);
}

static bool s_msgpack_serializer_needs_resize(msgpack_serializer* serial) {
    return serial->index >= (size_t)((float)serial->capacity * serial->load_factor);
}

static int s_msgpack_serializer_resize(msgpack_serializer* serial) {
    size_t new_capacity = (size_t)(serial->capacity * serial->resize_factor);
    uint8_t* new_buffer = realloc(serial->buffer, new_capacity);
    if (new_buffer == NULL) {
        BLOSC_TRACE_ERROR("Error while resizing msgpack_serializer");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    serial->buffer = new_buffer;
    serial->capacity = new_capacity;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_msgpack_serializer_resize_conditionally(msgpack_serializer* serial) {
    int rc = BLOSC2_ERROR_SUCCESS;
    if (s_msgpack_serializer_needs_resize(serial)) {
        rc = s_msgpack_serializer_resize(serial);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not resize msgpack_serializer buffer");
            return rc;
        }
    }
    return rc;
}

static int s_msgpack_serializer_serialize_uint64(msgpack_serializer* serial, uint64_t value) {
    int rc = s_msgpack_serializer_resize_conditionally(serial);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Conditional resize failed");
        return rc;
    }
    rc = s_serialize_uint64(value, serial->buffer, &serial->index);
    return rc;
}

//static int s_msgpack_serializer_serialize_uint(msgpack_serializer* serial, size_t value) {
//    int rc = s_msgpack_serializer_resize_conditionally(serial);
//    if (rc != 0) {
//        BLOSC_TRACE_ERROR("Conditional resize failed");
//        return rc;
//    }
//    rc = s_serialize_uint(value, serial->buffer, &serial->index);
//    return rc;
//}

static int s_msgpack_serializer_serialize_string(msgpack_serializer* serial, const uint8_t* str) {
    int rc = s_msgpack_serializer_resize_conditionally(serial);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Conditional resize failed");
        return rc;
    }
    rc = s_serialize_string(str, serial->buffer, &serial->index);
    return rc;
}

static int s_msgpack_serializer_serialize_embed_store_local_value(msgpack_serializer* serial, b2_es_value value) {
    int rc = s_msgpack_serializer_resize_conditionally(serial);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Conditional resize failed");
        return rc;
    }
    rc = s_serialize_embed_store_local_value(value, serial->buffer, &serial->index);
    return rc;
}

static int s_serialize_embed_store_remote_value(b2_es_value value, uint8_t* packed_buffer, size_t* pindex) {
    size_t index = *pindex;

    packed_buffer[index] = (uint8_t)ARR_FIX_TAG | (uint8_t)2;
    index += 1;

    uint8_t* urlbase = value.f_remote.urlbase;
    uint8_t* path = value.f_remote.path;

    int rc = s_serialize_string(urlbase, packed_buffer, &index);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize urlbase for remote value");
        return rc;
    }
    rc = s_serialize_string(path, packed_buffer, &index);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not erialize path for remote value");
    }
    *pindex = index;
    return rc;
}

static int s_msgpack_serializer_serialize_embed_store_remote_value(msgpack_serializer* serial, b2_es_value value) {
    int rc = s_msgpack_serializer_resize_conditionally(serial);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Conditional resize failed");
        return rc;
    }
    rc = s_serialize_embed_store_remote_value(value, serial->buffer, &serial->index);
    return rc;
}

static int s_msgpack_serializer_serialize_embed_store_map(msgpack_serializer* serial, const hash_map* map) {
    int rc = s_msgpack_serializer_resize_conditionally(serial);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Conditional resize failed");
        return rc;
    }

    hash_map_iter iter = {0};
    hash_map_iter* piter = &iter;
    hash_map_iter_init(&iter, map);
    size_t map_length = map->length;

    uint8_t* serial_buffer = serial->buffer;
    size_t serial_index = serial->index;

    if (map_length > INT32_MAX) {
        BLOSC_TRACE_ERROR("The EmbedStore map length %lu, cannot be represented by an unsigned 32 bit integer. msgpack imposes a limitation of 32 bits to the length of the map", map_length);
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    // Serialize 32 bit map
    serial_buffer[serial_index] = (uint8_t)MAP_32_TAG;
    serial_index += 1;
    uint32_t tb = 0;
    to_big(&tb, &map_length, sizeof(tb));
    *(uint32_t*)(serial_buffer + serial_index) = tb;
    serial_index += 4;

    // Advance the serializer index past the map header so that the entries below are appended
    // right after it (and not written on top of it).
    serial->index = serial_index;

    while (hash_map_iter_next(piter)) {
        int error_code = s_msgpack_serializer_serialize_string(serial, iter.key->key);
        if (error_code != 0) {
            BLOSC_TRACE_ERROR("Could not serialize key: '%s'", (const char*)iter.key->key);
            return error_code;
        }
        b2_es_value* pvalue = iter.value;
        int rc = BLOSC2_ERROR_SUCCESS;
        if (pvalue->tag == BLOSC2_STORE_VALUE_SCHUNK) {
            rc = s_msgpack_serializer_serialize_embed_store_local_value(serial, *pvalue);
        } else if (pvalue->tag == BLOSC2_STORE_VALUE_REMOTE) {
            rc = s_msgpack_serializer_serialize_embed_store_remote_value(serial, *pvalue);
        } else {
            BLOSC_TRACE_ERROR("The value obtained does not have the right tag");
            return BLOSC2_ERROR_INVALID_HEADER;
        }
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not serialize embed_map");
            return rc;
        }
    }

    return BLOSC2_ERROR_SUCCESS;

}

// TODO: Change resize policy rules
int msgpack_embed_store_serialize(const blosc2_embed_store* es, uint8_t** pout_buffer, size_t* pout_buffer_size) {
    assert(es != NULL);
    assert(pout_buffer != NULL);
    assert(pout_buffer_size != NULL);

    msgpack_serializer serial = {0};
    msgpack_serializer* pserial = &serial;
    int rc = s_msgpack_serializer_init(pserial, MSGPACK_DEFAULT_BUFFER_SIZE, MSGPACK_DEFAULT_BUFFER_REALLOC_FACTOR, MSGPACK_DEFAULT_BUFFER_LOAD_FACTOR);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not initialize msgpack_serializer");
        return rc;
    }

    // Set array header
    serial.buffer[serial.index] = (uint8_t)ARR_FIX_TAG | (uint8_t)2;
    serial.index += 1;

    // Serialize current offset
    rc = s_msgpack_serializer_serialize_uint64(pserial, es->current_offset);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize current offset");
        s_msgpack_serializer_free_stack(pserial);
        return rc;
    }

    // Serialize hash map
    rc = s_msgpack_serializer_serialize_embed_store_map(pserial, &es->hm);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize EmbedStore's inner hash map");
        s_msgpack_serializer_free_stack(pserial);
        return rc;
    }

    *pout_buffer = serial.buffer;
    *pout_buffer_size = serial.index;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_try_deserialize_map(const uint8_t* packed, size_t* pindex, size_t* pmap_length) {
    size_t current_index = *pindex;
    uint8_t tag = packed[current_index];
    size_t length = 0;
    switch (tag) {
    case MAP_16_TAG:{
        current_index += 1;
        uint16_t fb = 0;
        from_big(&fb, &packed[current_index], 2);
        length = (size_t)fb;
        current_index += 2;
        break;
    }
    case MAP_32_TAG:{
        current_index += 1;
        uint32_t fb = 0;
        from_big(&fb, &packed[current_index], 4);
        length = (size_t)fb;
        current_index += 4;
        break;
    }
    default:
        if (MAP_FIX_TAG == (tag & MAP_FIX_TAG_MASK)) {
            length = (size_t) ( tag & MAP_FIX_LEN_MASK );
            current_index += 1;
            break;
        }
        BLOSC_TRACE_ERROR("Could not deserialize map");
        return BLOSC2_ERROR_INVALID_HEADER;
    }

    *pindex = current_index;
    *pmap_length = length;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_try_deserialize_string(const uint8_t* packed, size_t* pindex, uint8_t** pstr) {
    size_t current_index = *pindex;
    uint8_t tag = packed[current_index];
    size_t length = 0;
    switch (tag) {
    case STR_8_TAG:{ // 0xd9
        /* str 8  */
        length = (size_t)packed[current_index + 1];
        current_index += 2;
        break;
    }
    case STR_16_TAG:{ // 0xda
        /* str 16 */
        current_index += 1;
        uint16_t fb = 0;
        from_big(&fb, &packed[current_index], 2);
        length = (size_t)fb;
        current_index += 2;
        break;
    }
    case STR_32_TAG:{ // 0xdb
        /* str 32 */
        current_index += 1;
        uint32_t fb = 0;
        from_big(&fb, &packed[current_index], 4);
        length = (size_t)fb;
        current_index += 4;
        break;
    }
    default:
        if ((tag & STR_FIX_TAG_MASK) == STR_FIX_TAG) { /* fixstr */
            length = (size_t) tag & STR_FIX_LEN_MASK;
            current_index += 1;
            break;
        }
        BLOSC_TRACE_ERROR("Could not deserialize string");
        return BLOSC2_ERROR_INVALID_HEADER;
    }

    uint8_t* str = malloc(length + 1);
    if (str == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for string");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    memcpy(str, &packed[current_index], length);
    str[length] = '\0'; // Important, if not, it won't be recognized as a string
    current_index += length;
    *pindex = current_index;
    *pstr = str;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_try_deserialize_uint(const uint8_t* packed, size_t* pindex, size_t* pvalue) {
    size_t current_index = *pindex;
    uint8_t tag = packed[current_index];
    size_t value = 0;
    switch (tag) {
    case UINT_8_TAG:{
        current_index += 1;
        value = (size_t) packed[current_index];
        current_index += 1;
        break;
    }
    case UINT_16_TAG:{
        current_index += 1;
        uint16_t fb = 0;
        from_big(&fb, &packed[current_index], sizeof(fb));
        value = (size_t) fb;
        current_index += 2;
        break;
    }
    case UINT_32_TAG:{
        current_index += 1;
        uint32_t fb = 0;
        from_big(&fb, &packed[current_index], sizeof(fb));
        value = (size_t) fb;
        current_index += 4;
        break;
    }
    case UINT_64_TAG:{
        current_index += 1;
        uint64_t fb = 0;
        from_big(&fb, &packed[current_index], sizeof(fb));
        value = (size_t)fb;
        current_index += 8;
        break;
    }
    default:
        if (UINT_FIX_TAG == (tag & UINT_FIX_TAG_MASK)) {
            value = (size_t)(tag & UINT_FIX_VALUE_MASK);
            current_index += 1;
            break;
        }
        BLOSC_TRACE_ERROR("Could not deserialize uint");
        return BLOSC2_ERROR_INVALID_HEADER;
    }
    *pindex = current_index;
    *pvalue = value;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_try_deserialize_local_value(const uint8_t* packed, size_t* pindex, b2_es_value* pvalue) {
    size_t current_index = *pindex;
    int rc = s_try_deserialize_uint(packed, &current_index, (size_t*)&pvalue->f_local.offset);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize offset");
        return rc;
    }

    rc = s_try_deserialize_uint(packed, &current_index, &pvalue->f_local.length);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize length");
        return rc;
    }
    pvalue->tag = BLOSC2_STORE_VALUE_SCHUNK;

    *pindex = current_index;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_try_deserialize_remote_value(const uint8_t* packed, size_t* pindex, b2_es_value* pvalue) {
    size_t current_index = *pindex;

    int rc = s_try_deserialize_string(packed, &current_index, &pvalue->f_remote.urlbase);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize urlbase");
        return rc;
    }
    rc = s_try_deserialize_string(packed, &current_index, &pvalue->f_remote.path);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize path");
        free(pvalue->f_remote.urlbase);
        return rc;
    }
    pvalue->tag = BLOSC2_STORE_VALUE_REMOTE;

    *pindex = current_index;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_try_deserialize_value(const uint8_t* packed, size_t* pindex, b2_es_value* pvalue) {
    // Check that the tag is an array
    size_t current_index = *pindex;
    uint8_t tag = packed[current_index];
    size_t length = 0;
    switch (tag) {
    case ARR_16_TAG:{
        current_index += 1;
        uint16_t fb = 0;
        from_big(&fb, &packed[current_index], sizeof(fb));
        length = (size_t)fb;
        current_index += 2;
        break;
    }
    case ARR_32_TAG: {
        current_index += 1;
        uint32_t fb = 0;
        from_big(&fb, &packed[current_index], sizeof(fb));
        length = (size_t) fb;
        current_index += 4;
        break;
    }
    default:
        if (ARR_FIX_TAG == (tag & ARR_FIX_TAG_MASK)) {
            length = (size_t) (tag & ARR_FIX_LEN_MASK);
            current_index += 1;
            break;
        }
        BLOSC_TRACE_ERROR("Could not deserialize array");
        return BLOSC2_ERROR_INVALID_HEADER;
    }

    if (length != 2) {
        BLOSC_TRACE_ERROR("The specified array length is diffrent from 2");
        return BLOSC2_ERROR_INVALID_HEADER;
    }

    int rc = s_try_deserialize_local_value(packed, &current_index, pvalue);
    if (rc == BLOSC2_ERROR_INVALID_HEADER) {
        rc = s_try_deserialize_remote_value(packed, &current_index, pvalue);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not deserialize remote value");
            return rc;
        }
    } else if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize lcoal value");
        return rc;
    }

    *pindex = current_index;
    return BLOSC2_ERROR_SUCCESS;
}

int msgpack_embed_store_deserialize(const uint8_t* packed_buffer, size_t packed_buffer_size, blosc2_embed_store* es) {
    assert(packed_buffer != NULL);
    assert(packed_buffer_size > 0);
    assert(es != NULL);

    hash_map* map = &es->hm;
    size_t index = 0;
    size_t map_length = 0;
    if ((packed_buffer[index] & (uint8_t)ARR_FIX_TAG_MASK) != ARR_FIX_TAG || (packed_buffer[index] & (uint8_t)ARR_FIX_LEN_MASK) != (uint8_t)2 ) {
        BLOSC_TRACE_ERROR("Array tag could not be deserialized");
        return BLOSC2_ERROR_INVALID_HEADER;
    }
    index += 1;
    int rc = s_try_deserialize_uint(packed_buffer, &index, (size_t*)&es->current_offset);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not deserialize the current_offset");
        return rc;
    }
    // Check that the first tag corresponds to a map tag
    rc = s_try_deserialize_map(packed_buffer, &index, &map_length);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not deserialize map tag, the msgpack buffer may be corrupted");
        return rc;
    }

    if (index >= packed_buffer_size) {
        BLOSC_TRACE_ERROR("The msgpack deserializer exceeded the buffer size, which is %lu bytes long", packed_buffer_size);
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < map_length; i+=1) {
        // Iterate though the map and put elements
        uint8_t* str = NULL;
        rc = s_try_deserialize_string(packed_buffer, &index, &str);
        if (rc == BLOSC2_ERROR_INVALID_HEADER) {
            BLOSC_TRACE_ERROR("Could not deserialize map key, the tag was not one of a string");
            return rc;
        }
        if (rc == BLOSC2_ERROR_MEMORY_ALLOC) {
            BLOSC_TRACE_ERROR("Could not allocate space for the key");
            return rc;
        }
        // else: The result is fine
        b2_es_value val = {0};
        rc = s_try_deserialize_value(packed_buffer, &index, &val);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not deserialize value");
            free(str);
            return rc;
        }
        rc = hash_map_set(map, str, &val);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not set pair in map");
            free(str);
            return rc;
        }
        free(str);
    }

    return BLOSC2_ERROR_SUCCESS;
}

static int s_v1_try_deserialize_embed_map_value(const uint8_t* packed, size_t* pindex, b2_es_value* pvalue) {
    enum { HAS_OFFSET = 1, HAS_LENGTH = 2, HAS_URLBASE = 4, HAS_PATH = 8 };

    size_t index = *pindex;
    size_t map_length = 0;
    int rc = s_try_deserialize_map(packed, &index, &map_length);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not detect map in s_v1_try_deserialize_embed_map_value");
        return rc;
    }
    if (map_length != 2) {
        BLOSC_TRACE_ERROR("Embed Map value's length should be equal to 2");
        return BLOSC2_ERROR_INVALID_HEADER;
    }

    int seen = 0;
    size_t offset = 0, length = 0;
    uint8_t* urlbase = NULL;
    uint8_t* path = NULL;

    for (int i = 0; i < 2; i+=1) {
        uint8_t* key = NULL;
        rc = s_try_deserialize_string(packed, &index, &key);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not deserialize key in embed map value");
            goto fail;
        }

        if (!(seen & HAS_OFFSET) && strcmp((const char*)key, s_v1_offset) == 0) {
            seen |= HAS_OFFSET;
            rc = s_try_deserialize_uint(packed, &index, &offset);
        } else if (!(seen & HAS_LENGTH) && strcmp((const char*)key, s_v1_length) == 0) {
            seen |= HAS_LENGTH;
            rc = s_try_deserialize_uint(packed, &index, &length);
        } else if (!(seen & HAS_URLBASE) && strcmp((const char*)key, s_v1_urlbase) == 0) {
            seen |= HAS_URLBASE;
            rc = s_try_deserialize_string(packed, &index, &urlbase);
        } else if (!(seen & HAS_PATH) && strcmp((const char*)key, s_v1_path) == 0) {
            seen |= HAS_PATH;
            rc = s_try_deserialize_string(packed, &index, &path);
        } else {
            BLOSC_TRACE_ERROR("Unexpected or duplicated key in embed map value");
            rc = BLOSC2_ERROR_INVALID_HEADER;
        }
        free(key);
        if (rc != 0) goto fail;
    }

    if (seen == (HAS_OFFSET | HAS_LENGTH)) {
        pvalue->tag = BLOSC2_STORE_VALUE_SCHUNK;
        pvalue->f_local.offset = offset;
        pvalue->f_local.length = length;
    } else if (seen == (HAS_URLBASE | HAS_PATH)) {
        pvalue->tag = BLOSC2_STORE_VALUE_REMOTE;
        pvalue->f_remote.urlbase = urlbase;
        pvalue->f_remote.path = path;
    } else {
        BLOSC_TRACE_ERROR("Embed map must contain either 'offset'+'length' or 'urlbase'+'path'");
        rc = BLOSC2_ERROR_INVALID_HEADER;
        goto fail;
    }

    *pindex = index;
    return BLOSC2_ERROR_SUCCESS;

fail:
    if (urlbase != NULL) free(urlbase);
    if (path != NULL) free(path);
    return rc;
}

static int s_v1_try_deserialize_embed_map(const uint8_t* packed, size_t* pindex, blosc2_embed_store* es) {
    size_t index = *pindex;
    size_t map_length = 0;
    int rc = s_try_deserialize_map(packed, &index, &map_length);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not detect map in s_v1_try_deserialize_embed_map");
        return rc;
    }

    hash_map* map = &es->hm;
    for (size_t i = 0; i < map_length; i+=1) {
        uint8_t* str = NULL; // Malloced string
        rc = s_try_deserialize_string(packed, &index, &str);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not deserialize string");
            return rc;
        }
        b2_es_value value = {0};
        rc = s_v1_try_deserialize_embed_map_value(packed, &index, &value);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not deserialize value");
            free(str);
            return rc;
        }
        bool needs_free = false;
        rc = hash_map_put(map, str, &value, &needs_free);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not insert key (%s) in map", (const char*)str);
            free(str);
            priv_embed_store_free_value(&value);
            return rc;
        }
        if (needs_free) free(str);
    }
    *pindex = index;
    return rc;
}

int msgpack_embed_store_deserialize_v1(const uint8_t* packed_buffer, size_t packed_buffer_size, blosc2_embed_store* es) {
    assert(packed_buffer != NULL);
    assert(packed_buffer_size > 0);
    assert(es != NULL);

    size_t index = 0;

    // Deserialize map
    if ((packed_buffer[index] & MAP_FIX_TAG_MASK) != MAP_FIX_TAG || (packed_buffer[index] & MAP_FIX_LEN_MASK) != 2) {
        BLOSC_TRACE_ERROR("The tag was not of a fixmap or its length was not equal to 2");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    index += 1;
    size_t current_offset = 0;
    int rc = BLOSC2_ERROR_SUCCESS;

    enum { HAS_EMBED_MAP = 1, HAS_CURRENT_OFFSET = 2 };
    int seen = 0; // Bit mask
    for (int i = 0; i < 2; i+=1) {
        // Deserialize string
        uint8_t* key = NULL;
        rc = s_try_deserialize_string(packed_buffer, &index, &key);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not deserialize string");
            break;
        }
        if (!(seen & HAS_EMBED_MAP) && strcmp((const char*)key, s_v1_embed_map) == 0) {
            seen |= HAS_EMBED_MAP;
            rc = s_v1_try_deserialize_embed_map(packed_buffer, &index, es);
            if (rc != 0) {
                BLOSC_TRACE_ERROR("Could not deserialize embed_map");
            }
        } else if (!(seen & HAS_CURRENT_OFFSET) && strcmp((const char*)key, s_v1_current_offset) == 0) {
            seen |= HAS_CURRENT_OFFSET;
            // Deserialize uint
            rc = s_try_deserialize_uint(packed_buffer, &index, &current_offset);
            if (rc != 0) {
                BLOSC_TRACE_ERROR("Could not deserialize current offset");
            }
        } else {
            BLOSC_TRACE_ERROR("Unexpected key found: '%s'", (const char*)key);
            rc = BLOSC2_ERROR_INVALID_HEADER;
        }
        free(key);
        if (rc != 0) {
            break;
        }
    }

    if (rc != 0) {
        // TODO: In case of failre, the map should be fred
    } else {
        es->current_offset = current_offset;
    }
    return rc;
}

int msgpack_validate_and_return_version(const uint8_t* packed, size_t packed_size) {
    if (packed_size != s_version_packed_length) {
        BLOSC_TRACE_ERROR("msgpack packed buffer length should be of size %lu", s_version_packed_length);
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    size_t index = 0;
    if ((packed[index] & (uint8_t)MAP_FIX_TAG_MASK) != MAP_FIX_TAG || (packed[index] & (uint8_t)MAP_FIX_LEN_MASK) != 1) {
        BLOSC_TRACE_ERROR("msgpack map tag is incorrect. Ether it is not a fixmap or its length is diffrent from 2");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    index += 1;
    if ((packed[index] & (uint8_t)STR_FIX_TAG_MASK) != STR_FIX_TAG || (packed[index] & (uint8_t)STR_FIX_LEN_MASK) != s_version_string_length) {
        BLOSC_TRACE_ERROR("msgpack map tag is incorrect. Ether string tag is not a fixstr or the specified length is different from %lu", s_version_string_length);
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    index += 1;

    int rc = memcmp(&packed[index], s_version_string, s_version_string_length);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("'%s' is not equal to found string '%.*s'", s_version_string, (int)s_version_string_length, (const char*)&packed[index]);
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    index += s_version_string_length;

    if ((packed[index] & (uint8_t)UINT_FIX_TAG_MASK) != UINT_FIX_TAG) {
        BLOSC_TRACE_ERROR("fixuint tag was expected");
    }
    int value = (int)(packed[index] & (uint8_t)UINT_FIX_VALUE_MASK);
    if (value <= 0) {
        BLOSC_TRACE_ERROR("Obtained value is not correct");
    }
    return value;
}

// The serialized version
int msgpack_container_serialize_version( size_t version, uint8_t** pout_buffer, size_t* psize ) {
    assert(version <= 127);
    assert(pout_buffer != NULL);
    assert(psize != NULL);

    uint8_t* packed_buffer = malloc(s_version_packed_length);
    if (packed_buffer == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for the serialized buffer");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    size_t index = 0;
    packed_buffer[index] = (uint8_t)MAP_FIX_TAG | (uint8_t)1;
    index += 1;

    int rc = s_serialize_string((const uint8_t*)s_version_string, packed_buffer, &index);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize string");
        free(packed_buffer);
        return rc;
    }
    rc = s_serialize_uint(version, packed_buffer, &index);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize unsigned integer");
        free(packed_buffer);
        return rc;
    }

    *pout_buffer = packed_buffer;
    *psize = s_version_packed_length;
    return rc;
}

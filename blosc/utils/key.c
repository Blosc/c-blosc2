/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "key.h"

#include <blosc2.h>
#include <stdlib.h>
#include <string.h>

static const char s_invalid_key_chars[] = ":\0\n\r\t";
static const size_t s_invalid_key_chars_size = sizeof(s_invalid_key_chars) - 1;

bool key_embed_is_valid(const char* key) {
    size_t key_len = strlen(key);
    if (key_len == 0) {
        BLOSC_TRACE_ERROR("Key cannot be empty");
        return false;
    }
    if (key[0] != '/') {
        BLOSC_TRACE_ERROR("The key must always start with '/'");
        return false;
    }
    if (key_len > 1 && key[key_len - 1] == '/') {
        BLOSC_TRACE_ERROR("Unless the key is the root key, it cannot end with a '/'");
        return false;
    }
    // TODO: Maybe use a better method (hash tables?... maybe?)
    for (size_t i = 0; i < key_len; i+=1) {
        for (size_t j = 0; j < s_invalid_key_chars_size; j+=1) {
            if (key[i] == s_invalid_key_chars[j]) {
                BLOSC_TRACE_ERROR("Key cannot contain '%c'", s_invalid_key_chars[j]);
                return false;
            }
        }
    }

    return true;
}

bool key_is_canonical(const uint8_t* key) {
    if (key == NULL || key[0] == '\0') {
        return false;
    }

    size_t index = 0;
    while (true) {
        size_t component_start = index;
        while (key[index] != '\0' && key[index] != '/') {
            index += 1;
        }

        size_t component_length = index - component_start;
        if (component_length == 0) {
            /* Leading '/', an empty component ("//"), or a trailing '/' */
            return false;
        }
        if (component_length == 1 && key[component_start] == '.') {
            return false;
        }
        if (component_length == 2 &&
            key[component_start] == '.' &&
            key[component_start + 1] == '.') {
            return false;
        }

        if (key[index] == '\0') {
            return true;
        }

        index += 1;
        if (key[index] == '\0' || key[index] == '/') {
            /* Trailing '/' or empty component */
            return false;
        }
    }
}

int key_normalize_inplace(uint8_t* key, size_t* out_length) {
    if (key == NULL) {
        return BLOSC2_ERROR_NULL_POINTER;
    }

    size_t read_index = 0;
    size_t write_index = 0;

    while (key[read_index] != '\0') {
        /* Collapse all separators, including leading and repeated ones */
        while (key[read_index] == '/') {
            read_index += 1;
        }
        if (key[read_index] == '\0') {
            break;
        }

        size_t component_start = read_index;
        while (key[read_index] != '\0' && key[read_index] != '/') {
            read_index += 1;
        }
        size_t component_length = read_index - component_start;

        if (component_length == 1 && key[component_start] == '.') {
            /* "." is the current component; it does not add anything */
            continue;
        }
        if (component_length == 2 &&
            key[component_start] == '.' &&
            key[component_start + 1] == '.') {
            /* Do not resolve "..": it can escape the logical root */
            return BLOSC2_ERROR_INVALID_PARAM;
        }

        if (write_index != 0) {
            key[write_index] = '/';
            write_index += 1;
        }

        /*
         * write_index <= component_start, so memmove handles the possible
         * overlap when a leading '/' or a "." component was removed.
         */
        if (write_index != component_start) {
            memmove(key + write_index, key + component_start, component_length);
        }
        write_index += component_length;
    }

    if (write_index == 0) {
        /* "", "/", "///", "." and similar inputs do not identify a key */
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    key[write_index] = '\0';
    if (out_length != NULL) {
        *out_length = write_index;
    }
    return BLOSC2_ERROR_SUCCESS;
}

int key_normalize_copy(const uint8_t* key, uint8_t** out) {
    if (key == NULL || out == NULL) {
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *out = NULL;

    size_t key_length = strlen((const char*)key);
    uint8_t* normalized = malloc(key_length + 1);
    if (normalized == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(normalized, key, key_length + 1);

    int error_code = key_normalize_inplace(normalized, NULL);
    if (error_code != BLOSC2_ERROR_SUCCESS) {
        free(normalized);
        return error_code;
    }

    *out = normalized;
    return BLOSC2_ERROR_SUCCESS;
}

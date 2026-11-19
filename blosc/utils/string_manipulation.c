/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "string_manipulation.h"
#include <assert.h>
#include <blosc2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool blosc2_string_check_dirs_are_extensionless(const uint8_t* path) {
    assert(path != NULL);

    const char* cpath = (const char*)path;
    const char* last_slash = strrchr(cpath, '/');
    if (last_slash == NULL) {
        return true;
    }

    for (const char* p = cpath; p < last_slash; p++) {
        if (*p == '.') {
            return false;
        }
    }
    return true;
}

bool blosc2_string_has_char(const uint8_t* str, char character) {
    assert(str != NULL);

    return character != '\0' && strchr((const char*)str, character) != NULL;
}

bool blosc2_string_has_extension(const uint8_t* str, const uint8_t* extension) {
    assert(str != NULL);
    assert(extension != NULL && extension[0] != '\0');

    const char* last_dot = strrchr((const char*)str, '.');
    if (last_dot == NULL) {
        return false;
    }
    return strcmp(last_dot + 1, (const char*)extension) == 0;
}

int blosc2_string_strip_set(const uint8_t* str, const uint8_t* strip_set, uint8_t** out) {
    assert(str != NULL);
    assert(strip_set != NULL && strip_set[0] != '\0');
    assert(out != NULL);

    const char* cstr = (const char*)str;
    const char* set = (const char*)strip_set;

    const char* first = cstr;
    while (*first != '\0' && strchr(set, *first) != NULL) {
        first += 1;
    }
    const char* end = cstr + strlen(cstr);
    while (end > first && strchr(set, *(end - 1)) != NULL) {
        end -= 1;
    }

    size_t stripped_size = (size_t)(end - first);
    uint8_t* stripped_string = malloc(stripped_size + 1);
    if (stripped_string == NULL) {
        *out = NULL;
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(stripped_string, first, stripped_size);
    stripped_string[stripped_size] = '\0';

    *out = stripped_string;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_string_strip(const uint8_t* str, uint8_t** out) {
    static const uint8_t whitespace[] = " ";
    return blosc2_string_strip_set(str, whitespace, out);
}

int blosc2_string_duplicate(const uint8_t* str, uint8_t** out) {
    assert(str != NULL);
    assert(out != NULL);

    size_t str_size = strlen((const char*)str) + 1;
    uint8_t* dup = malloc(str_size);
    if (dup == NULL) {
        *out = NULL;
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(dup, str, str_size);

    *out = dup;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_string_get_extension(const uint8_t* str, uint8_t** out) {
    assert(str != NULL);
    assert(out != NULL);

    const char* last_dot = strrchr((const char*)str, '.');
    if (last_dot == NULL) {
        *out = NULL;
        return BLOSC2_ERROR_NOT_FOUND;
    }

    /* Extensión sin el punto, más el terminador. */
    size_t extension_size = strlen(last_dot + 1) + 1;
    uint8_t* extension = malloc(extension_size);
    if (extension == NULL) {
        *out = NULL;
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(extension, last_dot + 1, extension_size);

    *out = extension;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_string_append_extension(const uint8_t* str, const uint8_t* extension, uint8_t** out) {
    assert(str != NULL);
    assert(extension != NULL && extension[0] != '\0');
    assert(out != NULL);

    /* str + '.' + extension + '\0' */
    size_t str_with_extension_size = strlen((const char*)str) + strlen((const char*)extension) + 2;
    uint8_t* str_with_extension = malloc(str_with_extension_size);
    if (str_with_extension == NULL) {
        *out = NULL;
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    snprintf((char*)str_with_extension, str_with_extension_size, "%s.%s", (const char*)str, (const char*)extension);

    *out = str_with_extension;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_string_remove_extension_inplace(uint8_t* str) {
    assert(str != NULL);
    if (str[0] == '\0') return BLOSC2_ERROR_INVALID_PARAM;

    uint8_t* last_dot = (uint8_t*) strrchr((const char*)str, '.');
    if (last_dot == NULL) return BLOSC2_ERROR_SUCCESS;
    *last_dot = '\0';

    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_string_remove_extension(const uint8_t* str, uint8_t** out) {
    assert(str != NULL);
    assert(out != NULL);
    if (str[0] == '\0') return BLOSC2_ERROR_INVALID_PARAM;
    *out = NULL;

    size_t str_size = strlen((const char*)str) + 1;
    uint8_t* out_str = malloc(str_size);
    if (out_str == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(out_str, str, str_size);

    int rc = blosc2_string_remove_extension_inplace(out_str);
    if (rc != BLOSC2_ERROR_SUCCESS) {
        free(out_str);
        return rc;
    }
    *out = out_str;
    return BLOSC2_ERROR_SUCCESS;
}

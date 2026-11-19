/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_STRING_MANIPULATION_H
#define BLOSC_STRING_MANIPULATION_H

#include <stdint.h>
#include <stdbool.h>

bool blosc2_string_check_dirs_are_extensionless(const uint8_t* path);

bool blosc2_string_has_char(const uint8_t* str, char character);

bool blosc2_string_has_extension(const uint8_t* str, const uint8_t* extension);

int blosc2_string_strip(const uint8_t* str, uint8_t** out);

int blosc2_string_strip_set(const uint8_t* str, const uint8_t* strip_set, uint8_t** out);

int blosc2_string_duplicate(const uint8_t* str, uint8_t** out);

int blosc2_string_get_extension(const uint8_t* str, uint8_t** out);

int blosc2_string_append_extension(const uint8_t* str, const uint8_t* extension, uint8_t** out);

int blosc2_string_remove_extension_inplace(uint8_t* str);

int blosc2_string_remove_extension(const uint8_t* str, uint8_t** out);

#endif //BLOSC_STRING_MANIPULATION_H

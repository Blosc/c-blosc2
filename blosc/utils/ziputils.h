/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_ZIPUTILS_H
#define BLOSC_ZIPUTILS_H

/*designed as a state machine. It wraps a ZIP archive backend and it does not offer Deflate
functionality; this is in purpose, though. Compression is achieved using the Blosc2 compressor, something that
Ziputils is not in charge of. This wrapper was designed around blosc2_dict_store and its main purpose is to reduce
as much as possible disk access.
*/

#include "miniz.h"
#include <stdbool.h>

#define ZIP_LDH_SIGNATURE        0x04034b50u
#define ZIP_LDH_SIZE             30
#define ZIP_LDH_FILENAME_LEN_OFS 26
#define ZIP_LDH_EXTRA_LEN_OFS    28

typedef struct {
  mz_zip_archive archive;
}zip_archive;

typedef struct {
  mz_zip_archive_file_stat stat;
  mz_zip_archive *archive;
  size_t total_entries;
  size_t consumed_entries;
  size_t index;
}zip_archive_iter;

/* This just allocates a zeroed zip_archive struct, it does not perform any disk access */
zip_archive* ziputils_archive_new();
void ziputils_archive_free( zip_archive* za );

bool ziputils_archive_directory( const uint8_t* dir_path, const uint8_t* dest_path );

bool ziputils_validate_file(const uint8_t* archive_path);
bool ziputils_validate_heap(void* cbuffer, size_t cbuffer_size);

bool ziputils_writer_init_file( zip_archive* za, const uint8_t* archive_path );
bool ziputils_writer_init_heap( zip_archive* za );
bool ziputils_writer_add_entry( zip_archive* za, const uint8_t* file_name, void* incoming_buffer, size_t buffer_size );
bool ziputils_writer_finalize_file( zip_archive* za );
bool ziputils_writer_finalize_heap( zip_archive* za, void** buffer, size_t* size );
void ziputils_writer_end( zip_archive* za );

/* The archive path must exist */
bool ziputils_reader_init_file( zip_archive* za, const uint8_t* archive_path );
bool ziputils_reader_init_heap( zip_archive* za, void* buffer, size_t buffer_size );
bool ziputils_reader_extract_to_file( zip_archive *za, const uint8_t* dest_filename, size_t file_index );
void ziputils_reader_end( zip_archive* za );

/* The following functions are for the archive file iterator */
zip_archive_iter* ziputils_reader_init_iter( zip_archive *za );
void ziputils_reader_iter_free( zip_archive_iter *iter );
bool ziputils_reader_iter_next( zip_archive_iter *iter );
bool ziputils_reader_iter_is_dir( zip_archive_iter *iter );

/* pfilename_length must be diffrent from null. It returns the size of the buffer and (using pfilename_length) the size of
  the filename buffer including the null terminator '\0'. It does not alter iter. The buffer returned needs to be fred by the user
  using the stdlib.h free function, in other words, ziputils does not own the buffer returned
  Returns NULL on failure
*/
int ziputils_reader_iter_get_filename( zip_archive_iter *iter, uint8_t** pfilename_buffer, size_t* pfilename_length );
uint64_t ziputils_reader_iter_get_offset( zip_archive_iter *iter );
size_t ziputils_reader_iter_get_length( zip_archive_iter *iter );
size_t ziputils_reader_iter_get_file_index( zip_archive_iter *iter );

#endif // BLOSC_ZIPUTILS_H

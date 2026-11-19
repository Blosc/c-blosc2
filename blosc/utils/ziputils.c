/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <blosc2.h>
#include <sys/stat.h>
#include <dirent.h>
#include "ziputils.h"

zip_archive* ziputils_archive_new() {
    return calloc(1, sizeof(zip_archive));
}

void ziputils_archive_free( zip_archive* za ) {
    if (za != NULL) {
        free(za);
    }
}

static bool add_dir(mz_zip_archive *za, const uint8_t* dir_path, size_t path_offset) {
    DIR *dir = opendir((const char*)dir_path);
    if (!dir) { return false; }
    struct dirent *e;
    while ((e = readdir(dir)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", (const char*)dir_path, e->d_name);

        struct stat st;
        if (stat(full, &st) != 0) {
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            if (!mz_zip_writer_add_file(za, full + path_offset, full, "", 0, MZ_NO_COMPRESSION)) {
                closedir(dir);
                return false;
            }
        } else if (S_ISDIR(st.st_mode)) {
            // Entrada de directorio: nombre terminado en '/' y sin datos
            if (!add_dir(za, (const uint8_t*)full, path_offset)) {
                return false;
            }
        }
    }
    closedir(dir);
    return true;
}

bool ziputils_archive_directory(const uint8_t* dir_path, const uint8_t* dest_path ) {

    if (dir_path == NULL || dest_path == NULL) { return false; }

    mz_zip_archive za;
    mz_zip_zero_struct(&za);
    mz_zip_writer_init_file(&za, (const char*)dest_path, 0);

    size_t path_offset = strlen((const char*)dir_path) + 1;
    // Traverse directory and add entries in it recursively
    bool ok = add_dir(&za, dir_path, path_offset);
    if (!ok) { return false; }
    if (!mz_zip_writer_finalize_archive(&za)) { return false; }
    mz_zip_writer_end(&za);
    return true;
}

bool ziputils_validate_file(const uint8_t* archive_path) {
    mz_zip_error error; // Not used
    return mz_zip_validate_file_archive((const char*)archive_path, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY | MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY, &error);
}

bool ziputils_validate_heap(void* cbuffer, size_t cbuffer_size) {
    mz_zip_error error = 0;
    return mz_zip_validate_mem_archive(cbuffer, cbuffer_size, MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY | MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY, &error);
}

bool ziputils_writer_init_file( zip_archive* za, const uint8_t* archive_path ) {
    return mz_zip_writer_init_file_v2(&za->archive, (const char*)archive_path, 0, MZ_ZIP_FLAG_WRITE_ZIP64);
}

bool ziputils_writer_init_heap( zip_archive* za ) {
    return mz_zip_writer_init_heap_v2(&za->archive, 0, 0, MZ_ZIP_FLAG_WRITE_ZIP64);
}

bool ziputils_writer_add_entry( zip_archive* za, const uint8_t* file_name, void* incoming_buffer, size_t buffer_size ) {
    return mz_zip_writer_add_mem(&za->archive, (const char*)file_name, incoming_buffer, buffer_size, MZ_NO_COMPRESSION);
}

bool ziputils_writer_finalize_file( zip_archive* za ) {
    return mz_zip_writer_finalize_archive(&za->archive);
}

bool ziputils_writer_finalize_heap( zip_archive* za, void** buffer, size_t* size ) {
    return mz_zip_writer_finalize_heap_archive(&za->archive, buffer, size);
}

void ziputils_writer_end( zip_archive* za ) {
    mz_zip_writer_end(&za->archive);
}

bool ziputils_writer_init_from_reader( zip_archive* za, const uint8_t* file_path ) {
    return mz_zip_writer_init_from_reader(&za->archive, (const char*)file_path);
}

/* The archive path must exist */
bool ziputils_reader_init_file( zip_archive* za, const uint8_t* archive_path ) {
    return mz_zip_reader_init_file(&za->archive, (const char*)archive_path, MZ_NO_COMPRESSION);
}

bool ziputils_reader_init_heap( zip_archive* za, void* buffer, size_t buffer_size ) {
    return mz_zip_reader_init_mem(&za->archive, buffer, buffer_size, 0);
}

bool ziputils_reader_extract_to_file( zip_archive *za, const uint8_t* dest_filename, size_t file_index ) {
    return mz_zip_reader_extract_to_file(&za->archive, file_index, (const char*)dest_filename, MZ_NO_COMPRESSION);
}

void ziputils_reader_end( zip_archive* za ) {
    mz_zip_reader_end(&za->archive);
}

/* Iterator functions */
zip_archive_iter* ziputils_reader_init_iter( zip_archive *za ) {
    zip_archive_iter *iter = NULL;
    iter = calloc(1, sizeof(zip_archive_iter));
    // (note): za must be in reader mode
    iter->archive = &za->archive;
    iter->index = 0;
    iter->consumed_entries = 0;
    iter->total_entries = mz_zip_reader_get_num_files( &za->archive );
    return iter;
}

void ziputils_reader_iter_free( zip_archive_iter *iter ) {
    free(iter);
}

bool ziputils_reader_iter_next( zip_archive_iter *iter ) {
    if (iter == NULL) return false;

    for (size_t i = iter->consumed_entries; i < iter->total_entries; i+=1) {
        bool ok = mz_zip_reader_file_stat(iter->archive, i, &iter->stat);
        if (ok && !iter->stat.m_is_directory) {
            // We are not looking for directories, we are looking for leafs
            iter->index = iter->stat.m_file_index;
            iter->consumed_entries = i + 1;
            return true;
        }
    }
    iter->consumed_entries = iter->total_entries;
    return false;
}

bool ziputils_reader_file_iter_next_with_dirs( zip_archive_iter *iter ) {
    if (iter == NULL) return false;

    for (size_t i = iter->consumed_entries; i < iter->total_entries; i+=1) {
        bool ok = mz_zip_reader_file_stat(iter->archive, i, &iter->stat);
        if (ok) {
            // We are not looking for directories, we are looking for leafs
            iter->index = iter->stat.m_file_index;
            iter->consumed_entries = i + 1;
            return true;
        }
    }
    iter->consumed_entries = iter->total_entries;
    return false;
}

bool ziputils_reader_iter_is_dir( zip_archive_iter *iter ) {
    return iter->stat.m_is_directory;
}

int ziputils_reader_iter_get_filename( zip_archive_iter *iter, const uint8_t** pfilename_buffer, size_t* pfilename_length ) {
    if (iter == NULL || pfilename_length == NULL || pfilename_buffer == NULL) return BLOSC2_ERROR_INVALID_PARAM;

    char mock = '1';
    size_t filename_length = mz_zip_reader_get_filename(iter->archive, iter->index, &mock, 0 );
    if (filename_length <= 0 || mock == '\0') return BLOSC2_ERROR_NOT_FOUND; // Look at the code of mz_zip_reader_get_filename

    // if mz_zip_reader_get_filename succeeds with the parameters shown in the previous line, returns the size of the
    // filename including the null character terminator '\0'
    const uint8_t* filename_buffer = malloc(filename_length * sizeof(uint8_t));
    if (filename_buffer == NULL) return BLOSC2_ERROR_MEMORY_ALLOC;

    filename_length = mz_zip_reader_get_filename(iter->archive, iter->index, (char*)filename_buffer, filename_length);

    *pfilename_buffer = filename_buffer;
    *pfilename_length = filename_length;
    return BLOSC2_ERROR_SUCCESS;
}

uint64_t ziputils_reader_iter_get_offset( zip_archive_iter *iter ) {

    uint8_t header[ZIP_LDH_SIZE];
    const uint64_t header_ofs = iter->stat.m_local_header_ofs;

    if (mz_zip_read_archive_data(iter->archive, header_ofs, header, sizeof(header)) != sizeof(header)) {
        return 0; // (todo): Better error handling
    }

    if (MZ_READ_LE32(header) != ZIP_LDH_SIGNATURE) {
        return 0; // (todo): Better error handling
    }

    const uint16_t filename_len = MZ_READ_LE16(header + ZIP_LDH_FILENAME_LEN_OFS);
    const uint16_t extra_len = MZ_READ_LE16(header + ZIP_LDH_EXTRA_LEN_OFS);

    return header_ofs + ZIP_LDH_SIZE + filename_len + extra_len;
}

size_t ziputils_reader_iter_get_length ( zip_archive_iter *iter ) {
    return iter->stat.m_uncomp_size; // Because in Blosc2 deflate is not used, m_uncomp_size should be equal to m_comp_size
}

size_t ziputils_reader_iter_get_file_index( zip_archive_iter *iter ) {
    return iter->index;
}

/* This file has been created from the original MINIZ library. License is provided in the LICENSES directory */

#ifndef BLOSC_MINIZ_H
#define BLOSC_MINIZ_H

/* miniz 3.1.2 store-only fork: ZIP archive API, CRC-32 and allocators.
   Deflate/Inflate and the zlib-style API have been removed. Only stored ZIP
   entries (method 0) are read; other methods are rejected unless the caller
   requests raw compressed data via MZ_ZIP_FLAG_COMPRESSED_DATA.
   Copyright 2013-2014 RAD Game Tools and Valve Software.
   Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC.
   SPDX-License-Identifier: MIT OR Unlicense. See LICENSE. */
#pragma once
/* Optional: MINIZ_NO_STDIO, MINIZ_NO_TIME, MINIZ_NO_ARCHIVE_APIS,
   MINIZ_NO_ARCHIVE_WRITING_APIS, MINIZ_NO_MALLOC. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#define MZ_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define MZ_FORCEINLINE __inline__ __attribute__((__always_inline__))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define MZ_FORCEINLINE inline
#else
#define MZ_FORCEINLINE
#endif
#if defined(__TINYC__) && (defined(__linux) || defined(__linux__))
#define MINIZ_NO_TIME
#endif
#include <stddef.h>
#if !defined(MINIZ_NO_TIME) && !defined(MINIZ_NO_ARCHIVE_APIS)
#include <time.h>
#endif
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__i386) || defined(__i486__) || defined(__i486) || defined(i386) || defined(__ia64__) || defined(__x86_64__)
#define MINIZ_X86_OR_X64_CPU 1
#else
#define MINIZ_X86_OR_X64_CPU 0
#endif
#if !defined(MINIZ_LITTLE_ENDIAN)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define MINIZ_LITTLE_ENDIAN 1
#else
#define MINIZ_LITTLE_ENDIAN 0
#endif
#else
#if MINIZ_X86_OR_X64_CPU
#define MINIZ_LITTLE_ENDIAN 1
#else
#define MINIZ_LITTLE_ENDIAN 0
#endif
#endif
#endif
#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif
#endif
#ifndef MINIZ_USE_UNALIGNED_LOADS_AND_STORES
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long mz_ulong;
 void mz_free(void *p);
#define MZ_CRC32_INIT (0)
 mz_ulong mz_crc32(mz_ulong crc, const unsigned char *ptr, size_t buf_len);
#define MZ_STORED 0
#define MZ_DEFLATED 8
typedef void *(*mz_alloc_func)(void *opaque, size_t items, size_t size);
typedef void (*mz_free_func)(void *opaque, void *address);
typedef void *(*mz_realloc_func)(void *opaque, void *address, size_t items, size_t size);
enum {
    MZ_NO_COMPRESSION = 0,
    MZ_BEST_SPEED = 1,
    MZ_BEST_COMPRESSION = 9,
    MZ_UBER_COMPRESSION = 10,
    MZ_DEFAULT_LEVEL = 6,
    MZ_DEFAULT_COMPRESSION = -1
};
#define MZ_VERSION "11.3.2"
#define MZ_VERNUM 0xB302
#define MZ_VER_MAJOR 11
#define MZ_VER_MINOR 3
#define MZ_VER_REVISION 2
#define MZ_VER_SUBREVISION 0
 const char *mz_version(void);
#ifdef __cplusplus
}
#endif
/* Shared types, allocator macros and little-endian helpers. */
typedef unsigned char mz_uint8;
typedef int16_t mz_int16;
typedef uint16_t mz_uint16;
typedef uint32_t mz_uint32;
typedef uint32_t mz_uint;
typedef int64_t mz_int64;
typedef uint64_t mz_uint64;
typedef int mz_bool;
#define MZ_FALSE (0)
#define MZ_TRUE (1)
#ifdef _MSC_VER
#define MZ_MACRO_END while (0, 0)
#else
#define MZ_MACRO_END while (0)
#endif
#ifdef MINIZ_NO_STDIO
#define MZ_FILE void *
#else
#include <stdio.h>
#define MZ_FILE FILE
#endif
#ifdef MINIZ_NO_TIME
typedef struct mz_dummy_time_t_tag {
    mz_uint32 m_dummy1;
    mz_uint32 m_dummy2;
} mz_dummy_time_t;
#define MZ_TIME_T mz_dummy_time_t
#else
#define MZ_TIME_T time_t
#endif
#define MZ_ASSERT(x) assert(x)
#ifdef MINIZ_NO_MALLOC
#define MZ_MALLOC(x) NULL
#define MZ_FREE(x) (void)x, ((void)0)
#define MZ_REALLOC(p, x) NULL
#else
#define MZ_MALLOC(x) malloc(x)
#define MZ_FREE(x) free(x)
#define MZ_REALLOC(p, x) realloc(p, x)
#endif
#define MZ_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MZ_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MZ_CLEAR_ARR(obj) memset((obj), 0, sizeof(obj))
#define MZ_CLEAR_PTR(obj) memset((obj), 0, sizeof(*obj))
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
#define MZ_READ_LE16(p) *((const mz_uint16 *)(p))
#define MZ_READ_LE32(p) *((const mz_uint32 *)(p))
#else
#define MZ_READ_LE16(p) ((mz_uint32)(((const mz_uint8 *)(p))[0]) | ((mz_uint32)(((const mz_uint8 *)(p))[1]) << 8U))
#define MZ_READ_LE32(p) ((mz_uint32)(((const mz_uint8 *)(p))[0]) | ((mz_uint32)(((const mz_uint8 *)(p))[1]) << 8U) | ((mz_uint32)(((const mz_uint8 *)(p))[2]) << 16U) | ((mz_uint32)(((const mz_uint8 *)(p))[3]) << 24U))
#endif
#define MZ_READ_LE64(p) (((mz_uint64)MZ_READ_LE32(p)) | (((mz_uint64)MZ_READ_LE32((const mz_uint8 *)(p) + sizeof(mz_uint32))) << 32U))
#ifdef __cplusplus
extern "C" {
#endif
extern  void *miniz_def_alloc_func(void *opaque, size_t items, size_t size);
extern  void miniz_def_free_func(void *opaque, void *address);
extern  void *miniz_def_realloc_func(void *opaque, void *address, size_t items, size_t size);
#define MZ_UINT16_MAX (0xFFFFU)
#define MZ_UINT32_MAX (0xFFFFFFFFU)
#ifdef __cplusplus
}
#endif
/* ZIP archive API. */
#ifndef MINIZ_NO_ARCHIVE_APIS
#ifdef __cplusplus
extern "C" {
#endif
enum {
    MZ_ZIP_MAX_IO_BUF_SIZE = 64 * 1024,
    MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE = 512,
    MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE = 512
};
typedef struct
{
    mz_uint32 m_file_index;
    mz_uint64 m_central_dir_ofs;
    mz_uint16 m_version_made_by;
    mz_uint16 m_version_needed;
    mz_uint16 m_bit_flag;
    mz_uint16 m_method;
    mz_uint32 m_crc32;
    mz_uint64 m_comp_size;
    mz_uint64 m_uncomp_size;
    mz_uint16 m_internal_attr;
    mz_uint32 m_external_attr;
    mz_uint64 m_local_header_ofs;
    mz_uint32 m_comment_size;
    mz_bool m_is_directory;
    mz_bool m_is_encrypted;
    mz_bool m_is_supported;
    char m_filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
    char m_comment[MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE];
#ifdef MINIZ_NO_TIME
    MZ_TIME_T m_padding;
#else
    MZ_TIME_T m_time;
#endif
} mz_zip_archive_file_stat;
typedef size_t (*mz_file_read_func)(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n);
typedef size_t (*mz_file_write_func)(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n);
typedef mz_bool (*mz_file_needs_keepalive)(void *pOpaque);
struct mz_zip_internal_state_tag;
typedef struct mz_zip_internal_state_tag mz_zip_internal_state;
typedef enum {
    MZ_ZIP_MODE_INVALID = 0,
    MZ_ZIP_MODE_READING = 1,
    MZ_ZIP_MODE_WRITING = 2,
    MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED = 3
} mz_zip_mode;
typedef enum {
    MZ_ZIP_FLAG_CASE_SENSITIVE = 0x0100,
    MZ_ZIP_FLAG_IGNORE_PATH = 0x0200,
    MZ_ZIP_FLAG_COMPRESSED_DATA = 0x0400, /* raw bytes, no decompression */
    MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY = 0x0800,
    MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG = 0x1000,
    MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY = 0x2000,
    MZ_ZIP_FLAG_WRITE_ZIP64 = 0x4000,
    MZ_ZIP_FLAG_WRITE_ALLOW_READING = 0x8000,
    MZ_ZIP_FLAG_ASCII_FILENAME = 0x10000,
    MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE = 0x20000,
    MZ_ZIP_FLAG_READ_ALLOW_WRITING = 0x40000
} mz_zip_flags;
typedef enum {
    MZ_ZIP_TYPE_INVALID = 0,
    MZ_ZIP_TYPE_USER,
    MZ_ZIP_TYPE_MEMORY,
    MZ_ZIP_TYPE_HEAP,
    MZ_ZIP_TYPE_FILE,
    MZ_ZIP_TYPE_CFILE,
    MZ_ZIP_TOTAL_TYPES
} mz_zip_type;
typedef enum {
    MZ_ZIP_NO_ERROR = 0,
    MZ_ZIP_UNDEFINED_ERROR,
    MZ_ZIP_TOO_MANY_FILES,
    MZ_ZIP_FILE_TOO_LARGE,
    MZ_ZIP_UNSUPPORTED_METHOD,
    MZ_ZIP_UNSUPPORTED_ENCRYPTION,
    MZ_ZIP_UNSUPPORTED_FEATURE,
    MZ_ZIP_FAILED_FINDING_CENTRAL_DIR,
    MZ_ZIP_NOT_AN_ARCHIVE,
    MZ_ZIP_INVALID_HEADER_OR_CORRUPTED,
    MZ_ZIP_UNSUPPORTED_MULTIDISK,
    MZ_ZIP_DECOMPRESSION_FAILED,
    MZ_ZIP_COMPRESSION_FAILED,
    MZ_ZIP_UNEXPECTED_DECOMPRESSED_SIZE,
    MZ_ZIP_CRC_CHECK_FAILED,
    MZ_ZIP_UNSUPPORTED_CDIR_SIZE,
    MZ_ZIP_ALLOC_FAILED,
    MZ_ZIP_FILE_OPEN_FAILED,
    MZ_ZIP_FILE_CREATE_FAILED,
    MZ_ZIP_FILE_WRITE_FAILED,
    MZ_ZIP_FILE_READ_FAILED,
    MZ_ZIP_FILE_CLOSE_FAILED,
    MZ_ZIP_FILE_SEEK_FAILED,
    MZ_ZIP_FILE_STAT_FAILED,
    MZ_ZIP_INVALID_PARAMETER,
    MZ_ZIP_INVALID_FILENAME,
    MZ_ZIP_BUF_TOO_SMALL,
    MZ_ZIP_INTERNAL_ERROR,
    MZ_ZIP_FILE_NOT_FOUND,
    MZ_ZIP_ARCHIVE_TOO_LARGE,
    MZ_ZIP_VALIDATION_FAILED,
    MZ_ZIP_WRITE_CALLBACK_FAILED,
    MZ_ZIP_TOTAL_ERRORS
} mz_zip_error;
typedef struct
{
    mz_uint64 m_archive_size;
    mz_uint64 m_central_directory_file_ofs;
    mz_uint32 m_total_files;
    mz_zip_mode m_zip_mode;
    mz_zip_type m_zip_type;
    mz_zip_error m_last_error;
    mz_uint64 m_file_offset_alignment;
    mz_alloc_func m_pAlloc;
    mz_free_func m_pFree;
    mz_realloc_func m_pRealloc;
    void *m_pAlloc_opaque;
    mz_file_read_func m_pRead;
    mz_file_write_func m_pWrite;
    mz_file_needs_keepalive m_pNeeds_keepalive;
    void *m_pIO_opaque;
    mz_zip_internal_state *m_pState;
} mz_zip_archive;
typedef struct
{
    mz_zip_archive *pZip;
    mz_uint flags;
    int status;
    mz_uint64 read_buf_size, comp_remaining, out_buf_ofs, cur_file_ofs;
    mz_zip_archive_file_stat file_stat;
    void *pRead_buf;
#ifdef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
    mz_uint padding;
#else
    mz_uint file_crc32;
#endif
} mz_zip_reader_extract_iter_state;
 mz_bool mz_zip_reader_init(mz_zip_archive *pZip, mz_uint64 size, mz_uint flags);
 mz_bool mz_zip_reader_init_mem(mz_zip_archive *pZip, const void *pMem, size_t size, mz_uint flags);
#ifndef MINIZ_NO_STDIO
 mz_bool mz_zip_reader_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint32 flags);
 mz_bool mz_zip_reader_init_file_v2(mz_zip_archive *pZip, const char *pFilename, mz_uint flags, mz_uint64 file_start_ofs, mz_uint64 archive_size);
 mz_bool mz_zip_reader_init_cfile(mz_zip_archive *pZip, MZ_FILE *pFile, mz_uint64 archive_size, mz_uint flags);
#endif
 mz_bool mz_zip_reader_end(mz_zip_archive *pZip);
 void mz_zip_zero_struct(mz_zip_archive *pZip);
 mz_zip_mode mz_zip_get_mode(mz_zip_archive *pZip);
 mz_zip_type mz_zip_get_type(mz_zip_archive *pZip);
 mz_uint mz_zip_reader_get_num_files(mz_zip_archive *pZip);
 mz_uint64 mz_zip_get_archive_size(mz_zip_archive *pZip);
 mz_uint64 mz_zip_get_archive_file_start_offset(mz_zip_archive *pZip);
 MZ_FILE *mz_zip_get_cfile(mz_zip_archive *pZip);
 size_t mz_zip_read_archive_data(mz_zip_archive *pZip, mz_uint64 file_ofs, void *pBuf, size_t n);
 mz_zip_error mz_zip_set_last_error(mz_zip_archive *pZip, mz_zip_error err_num);
 mz_zip_error mz_zip_peek_last_error(mz_zip_archive *pZip);
 mz_zip_error mz_zip_clear_last_error(mz_zip_archive *pZip);
 mz_zip_error mz_zip_get_last_error(mz_zip_archive *pZip);
 const char *mz_zip_get_error_string(mz_zip_error mz_err);
 mz_bool mz_zip_reader_is_file_a_directory(mz_zip_archive *pZip, mz_uint file_index);
 mz_bool mz_zip_reader_is_file_encrypted(mz_zip_archive *pZip, mz_uint file_index);
 mz_bool mz_zip_reader_is_file_supported(mz_zip_archive *pZip, mz_uint file_index);
 mz_uint mz_zip_reader_get_filename(mz_zip_archive *pZip, mz_uint file_index, char *pFilename, mz_uint filename_buf_size);
 int mz_zip_reader_locate_file(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags);
 mz_bool mz_zip_reader_locate_file_v2(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags, mz_uint32 *file_index);
 mz_bool mz_zip_reader_file_stat(mz_zip_archive *pZip, mz_uint file_index, mz_zip_archive_file_stat *pStat);
 mz_bool mz_zip_is_zip64(mz_zip_archive *pZip);
 size_t mz_zip_get_central_dir_size(mz_zip_archive *pZip);
 mz_bool mz_zip_reader_extract_to_mem_no_alloc(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size);
 mz_bool mz_zip_reader_extract_file_to_mem_no_alloc(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size);
 mz_bool mz_zip_reader_extract_to_mem(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags);
 mz_bool mz_zip_reader_extract_file_to_mem(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags);
 void *mz_zip_reader_extract_to_heap(mz_zip_archive *pZip, mz_uint file_index, size_t *pSize, mz_uint flags);
 void *mz_zip_reader_extract_file_to_heap(mz_zip_archive *pZip, const char *pFilename, size_t *pSize, mz_uint flags);
 mz_bool mz_zip_reader_extract_to_callback(mz_zip_archive *pZip, mz_uint file_index, mz_file_write_func pCallback, void *pOpaque, mz_uint flags);
 mz_bool mz_zip_reader_extract_file_to_callback(mz_zip_archive *pZip, const char *pFilename, mz_file_write_func pCallback, void *pOpaque, mz_uint flags);
 mz_zip_reader_extract_iter_state *mz_zip_reader_extract_iter_new(mz_zip_archive *pZip, mz_uint file_index, mz_uint flags);
 mz_zip_reader_extract_iter_state *mz_zip_reader_extract_file_iter_new(mz_zip_archive *pZip, const char *pFilename, mz_uint flags);
 size_t mz_zip_reader_extract_iter_read(mz_zip_reader_extract_iter_state *pState, void *pvBuf, size_t buf_size);
 mz_bool mz_zip_reader_extract_iter_free(mz_zip_reader_extract_iter_state *pState);
#ifndef MINIZ_NO_STDIO
 mz_bool mz_zip_reader_extract_to_file(mz_zip_archive *pZip, mz_uint file_index, const char *pDst_filename, mz_uint flags);
 mz_bool mz_zip_reader_extract_file_to_file(mz_zip_archive *pZip, const char *pArchive_filename, const char *pDst_filename, mz_uint flags);
 mz_bool mz_zip_reader_extract_to_cfile(mz_zip_archive *pZip, mz_uint file_index, MZ_FILE *File, mz_uint flags);
 mz_bool mz_zip_reader_extract_file_to_cfile(mz_zip_archive *pZip, const char *pArchive_filename, MZ_FILE *pFile, mz_uint flags);
#endif
 mz_bool mz_zip_validate_file(mz_zip_archive *pZip, mz_uint file_index, mz_uint flags);
 mz_bool mz_zip_validate_archive(mz_zip_archive *pZip, mz_uint flags);
 mz_bool mz_zip_validate_mem_archive(const void *pMem, size_t size, mz_uint flags, mz_zip_error *pErr);
#ifndef MINIZ_NO_STDIO
 mz_bool mz_zip_validate_file_archive(const char *pFilename, mz_uint flags, mz_zip_error *pErr);
#endif
 mz_bool mz_zip_end(mz_zip_archive *pZip);
#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
 mz_bool mz_zip_writer_init(mz_zip_archive *pZip, mz_uint64 existing_size);
 mz_bool mz_zip_writer_init_v2(mz_zip_archive *pZip, mz_uint64 existing_size, mz_uint flags);
 mz_bool mz_zip_writer_init_heap(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size);
 mz_bool mz_zip_writer_init_heap_v2(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size, mz_uint flags);
#ifndef MINIZ_NO_STDIO
 mz_bool mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning);
 mz_bool mz_zip_writer_init_file_v2(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning, mz_uint flags);
 mz_bool mz_zip_writer_init_cfile(mz_zip_archive *pZip, MZ_FILE *pFile, mz_uint flags);
#endif
 mz_bool mz_zip_writer_init_from_reader(mz_zip_archive *pZip, const char *pFilename);
 mz_bool mz_zip_writer_init_from_reader_v2(mz_zip_archive *pZip, const char *pFilename, mz_uint flags);
 mz_bool mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, mz_uint level_and_flags);
 mz_bool mz_zip_writer_add_mem_ex(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags,
                                              mz_uint64 uncomp_size, mz_uint32 uncomp_crc32);
 mz_bool mz_zip_writer_add_mem_ex_v2(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags,
                                                 mz_uint64 uncomp_size, mz_uint32 uncomp_crc32, MZ_TIME_T *last_modified, const char *user_extra_data_local, mz_uint user_extra_data_local_len,
                                                 const char *user_extra_data_central, mz_uint user_extra_data_central_len);
 mz_bool mz_zip_writer_add_read_buf_callback(mz_zip_archive *pZip, const char *pArchive_name, mz_file_read_func read_callback, void *callback_opaque, mz_uint64 max_size,
                                                         const MZ_TIME_T *pFile_time, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags, const char *user_extra_data_local, mz_uint user_extra_data_local_len,
                                                         const char *user_extra_data_central, mz_uint user_extra_data_central_len);
#ifndef MINIZ_NO_STDIO
 mz_bool mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags);
 mz_bool mz_zip_writer_add_cfile(mz_zip_archive *pZip, const char *pArchive_name, MZ_FILE *pSrc_file, mz_uint64 max_size,
                                             const MZ_TIME_T *pFile_time, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags, const char *user_extra_data_local, mz_uint user_extra_data_local_len,
                                             const char *user_extra_data_central, mz_uint user_extra_data_central_len);
#endif
 mz_bool mz_zip_writer_add_from_zip_reader(mz_zip_archive *pZip, mz_zip_archive *pSource_zip, mz_uint src_file_index);
 mz_bool mz_zip_writer_finalize_archive(mz_zip_archive *pZip);
 mz_bool mz_zip_writer_finalize_heap_archive(mz_zip_archive *pZip, void **ppBuf, size_t *pSize);
 mz_bool mz_zip_writer_end(mz_zip_archive *pZip);
 mz_bool mz_zip_add_mem_to_archive_file_in_place(const char *pZip_filename, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags);
 mz_bool mz_zip_add_mem_to_archive_file_in_place_v2(const char *pZip_filename, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags, mz_zip_error *pErr);
#ifndef MINIZ_NO_STDIO
 void *mz_zip_extract_archive_file_to_heap(const char *pZip_filename, const char *pArchive_name, size_t *pSize, mz_uint flags);
 void *mz_zip_extract_archive_file_to_heap_v2(const char *pZip_filename, const char *pArchive_name, const char *pComment, size_t *pSize, mz_uint flags, mz_zip_error *pErr);
#endif
#endif
#ifdef __cplusplus
}
#endif
#endif


#endif // BLOSC_MINIZ_H

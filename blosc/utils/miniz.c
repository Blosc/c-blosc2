/* This file has been created from the original MINIZ library. License is provided in the LICENSES directory */

/* ZIP64 archiver, CRC-32, allocator helpers and version string for the
   store-only miniz fork. Non-stored entries are rejected;
   MZ_ZIP_FLAG_COMPRESSED_DATA still raw-copies caller bytes.
   Copyright 2013-2014 RAD Game Tools and Valve Software.
   Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC.
   Copyright 2016 Martin Raiber.
   SPDX-License-Identifier: MIT OR Unlicense. See LICENSE. */
#include "miniz.h"
typedef unsigned char mz_validate_uint16[sizeof(mz_uint16) == 2 ? 1 : -1];
typedef unsigned char mz_validate_uint32[sizeof(mz_uint32) == 4 ? 1 : -1];
typedef unsigned char mz_validate_uint64[sizeof(mz_uint64) == 8 ? 1 : -1];
#ifdef __cplusplus
extern "C" {
#endif
mz_ulong mz_crc32(mz_ulong crc, const mz_uint8 *ptr, size_t buf_len) {
    static const mz_uint32 s_crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535,
        0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD,
        0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D,
        0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
        0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4,
        0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC,
        0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
        0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
        0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB,
        0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA,
        0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE,
        0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
        0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409,
        0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739,
        0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
        0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344, 0x8708A3D2, 0x1E01F268,
        0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0,
        0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8,
        0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF,
        0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703,
        0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
        0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
        0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE,
        0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6,
        0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D,
        0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5,
        0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605,
        0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };
    mz_uint32 crc32 = (mz_uint32)crc ^ 0xFFFFFFFF;
    const mz_uint8 *pByte_buf = (const mz_uint8 *)ptr;
    while (buf_len >= 4) {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[0]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[1]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[2]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[3]) & 0xFF];
        pByte_buf += 4;
        buf_len -= 4;
    }
    while (buf_len) {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[0]) & 0xFF];
        ++pByte_buf;
        --buf_len;
    }
    return ~crc32;
}
void mz_free(void *p) {
    MZ_FREE(p);
}
 void *miniz_def_alloc_func(void *opaque, size_t items, size_t size) {
    (void)opaque, (void)items, (void)size;
    return MZ_MALLOC(items * size);
}
 void miniz_def_free_func(void *opaque, void *address) {
    (void)opaque, (void)address;
    MZ_FREE(address);
}
 void *miniz_def_realloc_func(void *opaque, void *address, size_t items, size_t size) {
    (void)opaque, (void)address, (void)items, (void)size;
    return MZ_REALLOC(address, items * size);
}
const char *mz_version(void) {
    return MZ_VERSION;
}
#ifdef __cplusplus
}
#endif
/* ZIP archive implementation. */
#define MZ_ZIP_STATUS_DONE 0
#define MZ_ZIP_STATUS_FAILED (-1)
#ifndef MINIZ_NO_ARCHIVE_APIS
#ifdef __cplusplus
extern "C" {
#endif
#ifndef MINIZ_NO_STDIO
#include <sys/stat.h>
#if defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef __cplusplus
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static WCHAR *mz_utf8z_to_widechar(const char *str) {
    int reqChars = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    WCHAR *wStr = (WCHAR *)malloc(reqChars * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wStr, reqChars);
    return wStr;
}
static FILE *mz_fopen(const char *pFilename, const char *pMode) {
    WCHAR *wFilename = mz_utf8z_to_widechar(pFilename);
    WCHAR *wMode = mz_utf8z_to_widechar(pMode);
    FILE *pFile = NULL;
    errno_t err = _wfopen_s(&pFile, wFilename, wMode);
    free(wFilename);
    free(wMode);
    return err ? NULL : pFile;
}
static FILE *mz_freopen(const char *pPath, const char *pMode, FILE *pStream) {
    WCHAR *wPath = mz_utf8z_to_widechar(pPath);
    WCHAR *wMode = mz_utf8z_to_widechar(pMode);
    FILE *pFile = NULL;
    errno_t err = _wfreopen_s(&pFile, wPath, wMode, pStream);
    free(wPath);
    free(wMode);
    return err ? NULL : pFile;
}
#if defined(__MINGW32__)
static int mz_stat(const char *path, struct _stat *buffer) {
    WCHAR *wPath = mz_utf8z_to_widechar(path);
    int res = _wstat(wPath, buffer);
    free(wPath);
    return res;
}
#else
static int mz_stat64(const char *path, struct __stat64 *buffer) {
    WCHAR *wPath = mz_utf8z_to_widechar(path);
    int res = _wstat64(wPath, buffer);
    free(wPath);
    return res;
}
#endif
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif
#define MZ_FOPEN mz_fopen
#define MZ_FCLOSE fclose
#define MZ_FREAD fread
#define MZ_FWRITE fwrite
#define MZ_FTELL64 _ftelli64
#define MZ_FSEEK64 _fseeki64
#if defined(__MINGW32__)
#define MZ_FILE_STAT_STRUCT _stat
#define MZ_FILE_STAT mz_stat
#else
#define MZ_FILE_STAT_STRUCT _stat64
#define MZ_FILE_STAT mz_stat64
#endif
#define MZ_FFLUSH fflush
#define MZ_FREOPEN mz_freopen
#define MZ_DELETE_FILE remove
#elif defined(__WATCOMC__)
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif
#define MZ_FOPEN(f, m) fopen(f, m)
#define MZ_FCLOSE fclose
#define MZ_FREAD fread
#define MZ_FWRITE fwrite
#define MZ_FTELL64 _ftelli64
#define MZ_FSEEK64 _fseeki64
#define MZ_FILE_STAT_STRUCT stat
#define MZ_FILE_STAT stat
#define MZ_FFLUSH fflush
#define MZ_FREOPEN(f, m, s) freopen(f, m, s)
#define MZ_DELETE_FILE remove
#elif defined(__TINYC__)
#ifndef MINIZ_NO_TIME
#include <sys/utime.h>
#endif
#define MZ_FOPEN(f, m) fopen(f, m)
#define MZ_FCLOSE fclose
#define MZ_FREAD fread
#define MZ_FWRITE fwrite
#define MZ_FTELL64 ftell
#define MZ_FSEEK64 fseek
#define MZ_FILE_STAT_STRUCT stat
#define MZ_FILE_STAT stat
#define MZ_FFLUSH fflush
#define MZ_FREOPEN(f, m, s) freopen(f, m, s)
#define MZ_DELETE_FILE remove
#elif defined(__USE_LARGEFILE64)
#ifndef MINIZ_NO_TIME
#include <utime.h>
#endif
#define MZ_FOPEN(f, m) fopen64(f, m)
#define MZ_FCLOSE fclose
#define MZ_FREAD fread
#define MZ_FWRITE fwrite
#define MZ_FTELL64 ftello64
#define MZ_FSEEK64 fseeko64
#define MZ_FILE_STAT_STRUCT stat64
#define MZ_FILE_STAT stat64
#define MZ_FFLUSH fflush
#define MZ_FREOPEN(p, m, s) freopen64(p, m, s)
#define MZ_DELETE_FILE remove
#elif defined(__APPLE__) || defined(__FreeBSD__) || (defined(__linux__) && defined(__x86_64__))
#ifndef MINIZ_NO_TIME
#include <utime.h>
#endif
#define MZ_FOPEN(f, m) fopen(f, m)
#define MZ_FCLOSE fclose
#define MZ_FREAD fread
#define MZ_FWRITE fwrite
#define MZ_FTELL64 ftello
#define MZ_FSEEK64 fseeko
#define MZ_FILE_STAT_STRUCT stat
#define MZ_FILE_STAT stat
#define MZ_FFLUSH fflush
#define MZ_FREOPEN(p, m, s) freopen(p, m, s)
#define MZ_DELETE_FILE remove
#else
#pragma message("Using fopen, ftello, fseeko, stat() etc. path for file I/O - this path may not support large files.")
#ifndef MINIZ_NO_TIME
#include <utime.h>
#endif
#define MZ_FOPEN(f, m) fopen(f, m)
#define MZ_FCLOSE fclose
#define MZ_FREAD fread
#define MZ_FWRITE fwrite
#ifdef __STRICT_ANSI__
#define MZ_FTELL64 ftell
#define MZ_FSEEK64 fseek
#else
#define MZ_FTELL64 ftello
#define MZ_FSEEK64 fseeko
#endif
#define MZ_FILE_STAT_STRUCT stat
#define MZ_FILE_STAT stat
#define MZ_FFLUSH fflush
#define MZ_FREOPEN(f, m, s) freopen(f, m, s)
#define MZ_DELETE_FILE remove
#endif
#endif
#define MZ_TOLOWER(c) ((((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c))
enum {
    MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG = 0x06054b50,
    MZ_ZIP_CENTRAL_DIR_HEADER_SIG = 0x02014b50,
    MZ_ZIP_LOCAL_DIR_HEADER_SIG = 0x04034b50,
    MZ_ZIP_LOCAL_DIR_HEADER_SIZE = 30,
    MZ_ZIP_CENTRAL_DIR_HEADER_SIZE = 46,
    MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE = 22,
    MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIG = 0x06064b50,
    MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG = 0x07064b50,
    MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE = 56,
    MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE = 20,
    MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID = 0x0001,
    MZ_ZIP_DATA_DESCRIPTOR_ID = 0x08074b50,
    MZ_ZIP_DATA_DESCRIPTER_SIZE64 = 24,
    MZ_ZIP_DATA_DESCRIPTER_SIZE32 = 16,
    MZ_ZIP_CDH_SIG_OFS = 0,
    MZ_ZIP_CDH_VERSION_MADE_BY_OFS = 4,
    MZ_ZIP_CDH_VERSION_NEEDED_OFS = 6,
    MZ_ZIP_CDH_BIT_FLAG_OFS = 8,
    MZ_ZIP_CDH_METHOD_OFS = 10,
    MZ_ZIP_CDH_FILE_TIME_OFS = 12,
    MZ_ZIP_CDH_FILE_DATE_OFS = 14,
    MZ_ZIP_CDH_CRC32_OFS = 16,
    MZ_ZIP_CDH_COMPRESSED_SIZE_OFS = 20,
    MZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS = 24,
    MZ_ZIP_CDH_FILENAME_LEN_OFS = 28,
    MZ_ZIP_CDH_EXTRA_LEN_OFS = 30,
    MZ_ZIP_CDH_COMMENT_LEN_OFS = 32,
    MZ_ZIP_CDH_DISK_START_OFS = 34,
    MZ_ZIP_CDH_INTERNAL_ATTR_OFS = 36,
    MZ_ZIP_CDH_EXTERNAL_ATTR_OFS = 38,
    MZ_ZIP_CDH_LOCAL_HEADER_OFS = 42,
    MZ_ZIP_LDH_SIG_OFS = 0,
    MZ_ZIP_LDH_VERSION_NEEDED_OFS = 4,
    MZ_ZIP_LDH_BIT_FLAG_OFS = 6,
    MZ_ZIP_LDH_METHOD_OFS = 8,
    MZ_ZIP_LDH_FILE_TIME_OFS = 10,
    MZ_ZIP_LDH_FILE_DATE_OFS = 12,
    MZ_ZIP_LDH_CRC32_OFS = 14,
    MZ_ZIP_LDH_COMPRESSED_SIZE_OFS = 18,
    MZ_ZIP_LDH_DECOMPRESSED_SIZE_OFS = 22,
    MZ_ZIP_LDH_FILENAME_LEN_OFS = 26,
    MZ_ZIP_LDH_EXTRA_LEN_OFS = 28,
    MZ_ZIP_LDH_BIT_FLAG_HAS_LOCATOR = 1 << 3,
    MZ_ZIP_ECDH_SIG_OFS = 0,
    MZ_ZIP_ECDH_NUM_THIS_DISK_OFS = 4,
    MZ_ZIP_ECDH_NUM_DISK_CDIR_OFS = 6,
    MZ_ZIP_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS = 8,
    MZ_ZIP_ECDH_CDIR_TOTAL_ENTRIES_OFS = 10,
    MZ_ZIP_ECDH_CDIR_SIZE_OFS = 12,
    MZ_ZIP_ECDH_CDIR_OFS_OFS = 16,
    MZ_ZIP_ECDH_COMMENT_SIZE_OFS = 20,
    MZ_ZIP64_ECDL_SIG_OFS = 0,
    MZ_ZIP64_ECDL_NUM_DISK_CDIR_OFS = 4,
    MZ_ZIP64_ECDL_REL_OFS_TO_ZIP64_ECDR_OFS = 8,
    MZ_ZIP64_ECDL_TOTAL_NUMBER_OF_DISKS_OFS = 16,
    MZ_ZIP64_ECDH_SIG_OFS = 0,
    MZ_ZIP64_ECDH_SIZE_OF_RECORD_OFS = 4,
    MZ_ZIP64_ECDH_VERSION_MADE_BY_OFS = 12,
    MZ_ZIP64_ECDH_VERSION_NEEDED_OFS = 14,
    MZ_ZIP64_ECDH_NUM_THIS_DISK_OFS = 16,
    MZ_ZIP64_ECDH_NUM_DISK_CDIR_OFS = 20,
    MZ_ZIP64_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS = 24,
    MZ_ZIP64_ECDH_CDIR_TOTAL_ENTRIES_OFS = 32,
    MZ_ZIP64_ECDH_CDIR_SIZE_OFS = 40,
    MZ_ZIP64_ECDH_CDIR_OFS_OFS = 48,
    MZ_ZIP_VERSION_MADE_BY_DOS_FILESYSTEM_ID = 0,
    MZ_ZIP_DOS_DIR_ATTRIBUTE_BITFLAG = 0x10,
    MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED = 1,
    MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_COMPRESSED_PATCH_FLAG = 32,
    MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION = 64,
    MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_LOCAL_DIR_IS_MASKED = 8192,
    MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_UTF8 = 1 << 11
};
typedef struct
{
    void *m_p;
    size_t m_size, m_capacity;
    mz_uint m_element_size;
} mz_zip_array;
struct mz_zip_internal_state_tag {
    mz_zip_array m_central_dir;
    mz_zip_array m_central_dir_offsets;
    mz_zip_array m_sorted_central_dir_offsets;
    mz_uint32 m_init_flags;
    mz_bool m_zip64;
    mz_bool m_zip64_has_extended_info_fields;
    MZ_FILE *m_pFile;
    mz_uint64 m_file_archive_start_ofs;
    void *m_pMem;
    size_t m_mem_size;
    size_t m_mem_capacity;
};
#define MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(array_ptr, element_size) (array_ptr)->m_element_size = element_size
#if defined(DEBUG) || defined(_DEBUG)
static MZ_FORCEINLINE mz_uint mz_zip_array_range_check(const mz_zip_array *pArray, mz_uint index) {
    MZ_ASSERT(index < pArray->m_size);
    return index;
}
#define MZ_ZIP_ARRAY_ELEMENT(array_ptr, element_type, index) ((element_type *)((array_ptr)->m_p))[mz_zip_array_range_check(array_ptr, index)]
#else
#define MZ_ZIP_ARRAY_ELEMENT(array_ptr, element_type, index) ((element_type *)((array_ptr)->m_p))[index]
#endif
static MZ_FORCEINLINE void mz_zip_array_init(mz_zip_array *pArray, mz_uint32 element_size) {
    memset(pArray, 0, sizeof(mz_zip_array));
    pArray->m_element_size = element_size;
}
static MZ_FORCEINLINE void mz_zip_array_clear(mz_zip_archive *pZip, mz_zip_array *pArray) {
    pZip->m_pFree(pZip->m_pAlloc_opaque, pArray->m_p);
    memset(pArray, 0, sizeof(mz_zip_array));
}
static mz_bool mz_zip_array_ensure_capacity(mz_zip_archive *pZip, mz_zip_array *pArray, size_t min_new_capacity, mz_uint growing) {
    void *pNew_p;
    size_t new_capacity = min_new_capacity;
    MZ_ASSERT(pArray->m_element_size);
    if (pArray->m_capacity >= min_new_capacity)
        return MZ_TRUE;
    if (growing) {
        new_capacity = MZ_MAX(1, pArray->m_capacity);
        while (new_capacity < min_new_capacity)
            new_capacity *= 2;
    }
    if (NULL == (pNew_p = pZip->m_pRealloc(pZip->m_pAlloc_opaque, pArray->m_p, pArray->m_element_size, new_capacity)))
        return MZ_FALSE;
    pArray->m_p = pNew_p;
    pArray->m_capacity = new_capacity;
    return MZ_TRUE;
}
static MZ_FORCEINLINE mz_bool mz_zip_array_reserve(mz_zip_archive *pZip, mz_zip_array *pArray, size_t new_capacity, mz_uint growing) {
    if (new_capacity > pArray->m_capacity) {
        if (!mz_zip_array_ensure_capacity(pZip, pArray, new_capacity, growing))
            return MZ_FALSE;
    }
    return MZ_TRUE;
}
static MZ_FORCEINLINE mz_bool mz_zip_array_resize(mz_zip_archive *pZip, mz_zip_array *pArray, size_t new_size, mz_uint growing) {
    if (new_size > pArray->m_capacity) {
        if (!mz_zip_array_ensure_capacity(pZip, pArray, new_size, growing))
            return MZ_FALSE;
    }
    pArray->m_size = new_size;
    return MZ_TRUE;
}
static MZ_FORCEINLINE mz_bool mz_zip_array_ensure_room(mz_zip_archive *pZip, mz_zip_array *pArray, size_t n) {
    return mz_zip_array_reserve(pZip, pArray, pArray->m_size + n, MZ_TRUE);
}
static MZ_FORCEINLINE mz_bool mz_zip_array_push_back(mz_zip_archive *pZip, mz_zip_array *pArray, const void *pElements, size_t n) {
    size_t orig_size = pArray->m_size;
    if (!mz_zip_array_resize(pZip, pArray, orig_size + n, MZ_TRUE))
        return MZ_FALSE;
    if (n > 0)
        memcpy((mz_uint8 *)pArray->m_p + orig_size * pArray->m_element_size, pElements, n * pArray->m_element_size);
    return MZ_TRUE;
}
#ifndef MINIZ_NO_TIME
static MZ_TIME_T mz_zip_dos_to_time_t(int dos_time, int dos_date) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_isdst = -1;
    tm.tm_year = ((dos_date >> 9) & 127) + 1980 - 1900;
    tm.tm_mon = ((dos_date >> 5) & 15) - 1;
    tm.tm_mday = dos_date & 31;
    tm.tm_hour = (dos_time >> 11) & 31;
    tm.tm_min = (dos_time >> 5) & 63;
    tm.tm_sec = (dos_time << 1) & 62;
    return mktime(&tm);
}
#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
static void mz_zip_time_t_to_dos_time(MZ_TIME_T time, mz_uint16 *pDOS_time, mz_uint16 *pDOS_date) {
#ifdef _MSC_VER
    struct tm tm_struct;
    struct tm *tm = &tm_struct;
    errno_t err = localtime_s(tm, &time);
    if (err) {
        *pDOS_date = 0;
        *pDOS_time = 0;
        return;
    }
#else
    struct tm *tm = localtime(&time);
#endif
    *pDOS_time = (mz_uint16)(((tm->tm_hour) << 11) + ((tm->tm_min) << 5) + ((tm->tm_sec) >> 1));
    *pDOS_date = (mz_uint16)(((tm->tm_year + 1900 - 1980) << 9) + ((tm->tm_mon + 1) << 5) + tm->tm_mday);
}
#endif
#ifndef MINIZ_NO_STDIO
#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
static mz_bool mz_zip_get_file_modified_time(const char *pFilename, MZ_TIME_T *pTime) {
    struct MZ_FILE_STAT_STRUCT file_stat;
    if (MZ_FILE_STAT(pFilename, &file_stat) != 0)
        return MZ_FALSE;
    *pTime = file_stat.st_mtime;
    return MZ_TRUE;
}
#endif
static mz_bool mz_zip_set_file_times(const char *pFilename, MZ_TIME_T access_time, MZ_TIME_T modified_time) {
    struct utimbuf t;
    memset(&t, 0, sizeof(t));
    t.actime = access_time;
    t.modtime = modified_time;
    return !utime(pFilename, &t);
}
#endif
#endif
static MZ_FORCEINLINE mz_bool mz_zip_set_error(mz_zip_archive *pZip, mz_zip_error err_num) {
    if (pZip)
        pZip->m_last_error = err_num;
    return MZ_FALSE;
}
static mz_bool mz_zip_reader_init_internal(mz_zip_archive *pZip, mz_uint flags) {
    (void)flags;
    if ((!pZip) || (pZip->m_pState) || (pZip->m_zip_mode != MZ_ZIP_MODE_INVALID))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (!pZip->m_pAlloc)
        pZip->m_pAlloc = miniz_def_alloc_func;
    if (!pZip->m_pFree)
        pZip->m_pFree = miniz_def_free_func;
    if (!pZip->m_pRealloc)
        pZip->m_pRealloc = miniz_def_realloc_func;
    pZip->m_archive_size = 0;
    pZip->m_central_directory_file_ofs = 0;
    pZip->m_total_files = 0;
    pZip->m_last_error = MZ_ZIP_NO_ERROR;
    if (NULL == (pZip->m_pState = (mz_zip_internal_state *)pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, sizeof(mz_zip_internal_state))))
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    memset(pZip->m_pState, 0, sizeof(mz_zip_internal_state));
    MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir, sizeof(mz_uint8));
    MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir_offsets, sizeof(mz_uint32));
    MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_sorted_central_dir_offsets, sizeof(mz_uint32));
    pZip->m_pState->m_init_flags = flags;
    pZip->m_pState->m_zip64 = MZ_FALSE;
    pZip->m_pState->m_zip64_has_extended_info_fields = MZ_FALSE;
    pZip->m_zip_mode = MZ_ZIP_MODE_READING;
    return MZ_TRUE;
}
static MZ_FORCEINLINE mz_bool mz_zip_reader_filename_less(const mz_zip_array *pCentral_dir_array, const mz_zip_array *pCentral_dir_offsets, mz_uint l_index, mz_uint r_index) {
    const mz_uint8 *pL = &MZ_ZIP_ARRAY_ELEMENT(pCentral_dir_array, mz_uint8, MZ_ZIP_ARRAY_ELEMENT(pCentral_dir_offsets, mz_uint32, l_index)), *pE;
    const mz_uint8 *pR = &MZ_ZIP_ARRAY_ELEMENT(pCentral_dir_array, mz_uint8, MZ_ZIP_ARRAY_ELEMENT(pCentral_dir_offsets, mz_uint32, r_index));
    mz_uint l_len = MZ_READ_LE16(pL + MZ_ZIP_CDH_FILENAME_LEN_OFS), r_len = MZ_READ_LE16(pR + MZ_ZIP_CDH_FILENAME_LEN_OFS);
    mz_uint8 l = 0, r = 0;
    pL += MZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
    pR += MZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
    pE = pL + MZ_MIN(l_len, r_len);
    while (pL < pE) {
        if ((l = MZ_TOLOWER(*pL)) != (r = MZ_TOLOWER(*pR)))
            break;
        pL++;
        pR++;
    }
    return (pL == pE) ? (l_len < r_len) : (l < r);
}
#define MZ_SWAP_UINT32(a, b) \
    do {                     \
        mz_uint32 t = a;     \
        a = b;               \
        b = t;               \
    }                        \
    MZ_MACRO_END
static void mz_zip_reader_sort_central_dir_offsets_by_filename(mz_zip_archive *pZip) {
    mz_zip_internal_state *pState = pZip->m_pState;
    const mz_zip_array *pCentral_dir_offsets = &pState->m_central_dir_offsets;
    const mz_zip_array *pCentral_dir = &pState->m_central_dir;
    mz_uint32 *pIndices;
    mz_uint32 start, end;
    const mz_uint32 size = pZip->m_total_files;
    if (size <= 1U)
        return;
    pIndices = &MZ_ZIP_ARRAY_ELEMENT(&pState->m_sorted_central_dir_offsets, mz_uint32, 0);
    start = (size - 2U) >> 1U;
    for (;;) {
        mz_uint64 child, root = start;
        for (;;) {
            if ((child = (root << 1U) + 1U) >= size)
                break;
            child += (((child + 1U) < size) && (mz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets, pIndices[child], pIndices[child + 1U])));
            if (!mz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets, pIndices[root], pIndices[child]))
                break;
            MZ_SWAP_UINT32(pIndices[root], pIndices[child]);
            root = child;
        }
        if (!start)
            break;
        start--;
    }
    end = size - 1;
    while (end > 0) {
        mz_uint64 child, root = 0;
        MZ_SWAP_UINT32(pIndices[end], pIndices[0]);
        for (;;) {
            if ((child = (root << 1U) + 1U) >= end)
                break;
            child += (((child + 1U) < end) && mz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets, pIndices[child], pIndices[child + 1U]));
            if (!mz_zip_reader_filename_less(pCentral_dir, pCentral_dir_offsets, pIndices[root], pIndices[child]))
                break;
            MZ_SWAP_UINT32(pIndices[root], pIndices[child]);
            root = child;
        }
        end--;
    }
}
static mz_bool mz_zip_reader_locate_header_sig(mz_zip_archive *pZip, mz_uint32 record_sig, mz_uint32 record_size, mz_int64 *pOfs) {
    mz_int64 cur_file_ofs;
    mz_uint32 buf_u32[4096 / sizeof(mz_uint32)];
    mz_uint8 *pBuf = (mz_uint8 *)buf_u32;
    if (pZip->m_archive_size < record_size)
        return MZ_FALSE;
    cur_file_ofs = MZ_MAX((mz_int64)pZip->m_archive_size - (mz_int64)sizeof(buf_u32), 0);
    for (;;) {
        int i, n = (int)MZ_MIN(sizeof(buf_u32), pZip->m_archive_size - cur_file_ofs);
        if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pBuf, n) != (mz_uint)n)
            return MZ_FALSE;
        for (i = n - 4; i >= 0; --i) {
            mz_uint s = MZ_READ_LE32(pBuf + i);
            if (s == record_sig) {
                if ((pZip->m_archive_size - (cur_file_ofs + i)) >= record_size)
                    break;
            }
        }
        if (i >= 0) {
            cur_file_ofs += i;
            break;
        }
        if ((!cur_file_ofs) || ((pZip->m_archive_size - cur_file_ofs) >= ((mz_uint64)(MZ_UINT16_MAX) + record_size)))
            return MZ_FALSE;
        cur_file_ofs = MZ_MAX(cur_file_ofs - (mz_int64)(sizeof(buf_u32) - 3), 0);
    }
    *pOfs = cur_file_ofs;
    return MZ_TRUE;
}
static mz_bool mz_zip_reader_eocd64_valid(mz_zip_archive *pZip, uint64_t offset, uint8_t *buf) {
    if (pZip->m_pRead(pZip->m_pIO_opaque, offset, buf, MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE) == MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE) {
        if (MZ_READ_LE32(buf + MZ_ZIP64_ECDH_SIG_OFS) == MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIG) {
            return MZ_TRUE;
        }
    }
    return MZ_FALSE;
}
static mz_bool mz_zip_reader_read_central_dir(mz_zip_archive *pZip, mz_uint flags) {
    mz_uint cdir_size = 0, cdir_entries_on_this_disk = 0, num_this_disk = 0, cdir_disk_index = 0;
    mz_uint64 cdir_ofs = 0, eocd_ofs = 0, archive_ofs = 0;
    mz_int64 cur_file_ofs = 0;
    const mz_uint8 *p;
    mz_uint32 buf_u32[4096 / sizeof(mz_uint32)];
    mz_uint8 *pBuf = (mz_uint8 *)buf_u32;
    mz_bool sort_central_dir = ((flags & MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY) == 0);
    mz_uint32 zip64_end_of_central_dir_locator_u32[(MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pZip64_locator = (mz_uint8 *)zip64_end_of_central_dir_locator_u32;
    mz_uint32 zip64_end_of_central_dir_header_u32[(MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pZip64_end_of_central_dir = (mz_uint8 *)zip64_end_of_central_dir_header_u32;
    mz_uint64 zip64_end_of_central_dir_ofs = 0;
    if (pZip->m_archive_size < MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
    if (!mz_zip_reader_locate_header_sig(pZip, MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG, MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE, &cur_file_ofs))
        return mz_zip_set_error(pZip, MZ_ZIP_FAILED_FINDING_CENTRAL_DIR);
    eocd_ofs = cur_file_ofs;
    if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pBuf, MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE) != MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
    if (MZ_READ_LE32(pBuf + MZ_ZIP_ECDH_SIG_OFS) != MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG)
        return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
    if (cur_file_ofs >= (MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE + MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE)) {
        if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs - MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE, pZip64_locator, MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE) == MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE) {
            if (MZ_READ_LE32(pZip64_locator + MZ_ZIP64_ECDL_SIG_OFS) == MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG) {
                pZip->m_pState->m_zip64 = MZ_TRUE;
            }
        }
    }
    if (pZip->m_pState->m_zip64) {
        if (cur_file_ofs < MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE +
                               MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE)
            return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
        zip64_end_of_central_dir_ofs = cur_file_ofs -
                                       MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE -
                                       MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE;
        if (!mz_zip_reader_eocd64_valid(pZip, zip64_end_of_central_dir_ofs,
                                        pZip64_end_of_central_dir)) {
            zip64_end_of_central_dir_ofs = MZ_READ_LE64(
                pZip64_locator + MZ_ZIP64_ECDL_REL_OFS_TO_ZIP64_ECDR_OFS);
            if (zip64_end_of_central_dir_ofs >
                (pZip->m_archive_size - MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE))
                return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
            if (!mz_zip_reader_eocd64_valid(pZip, zip64_end_of_central_dir_ofs,
                                            pZip64_end_of_central_dir))
                return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
        }
    }
    pZip->m_total_files = MZ_READ_LE16(pBuf + MZ_ZIP_ECDH_CDIR_TOTAL_ENTRIES_OFS);
    cdir_entries_on_this_disk = MZ_READ_LE16(pBuf + MZ_ZIP_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS);
    num_this_disk = MZ_READ_LE16(pBuf + MZ_ZIP_ECDH_NUM_THIS_DISK_OFS);
    cdir_disk_index = MZ_READ_LE16(pBuf + MZ_ZIP_ECDH_NUM_DISK_CDIR_OFS);
    cdir_size = MZ_READ_LE32(pBuf + MZ_ZIP_ECDH_CDIR_SIZE_OFS);
    cdir_ofs = MZ_READ_LE32(pBuf + MZ_ZIP_ECDH_CDIR_OFS_OFS);
    if (pZip->m_pState->m_zip64) {
        mz_uint32 zip64_total_num_of_disks = MZ_READ_LE32(pZip64_locator + MZ_ZIP64_ECDL_TOTAL_NUMBER_OF_DISKS_OFS);
        mz_uint64 zip64_cdir_total_entries = MZ_READ_LE64(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_CDIR_TOTAL_ENTRIES_OFS);
        mz_uint64 zip64_cdir_total_entries_on_this_disk = MZ_READ_LE64(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS);
        mz_uint64 zip64_size_of_end_of_central_dir_record = MZ_READ_LE64(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_SIZE_OF_RECORD_OFS);
        mz_uint64 zip64_size_of_central_directory = MZ_READ_LE64(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_CDIR_SIZE_OFS);
        if (zip64_size_of_end_of_central_dir_record < (MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE - 12))
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
        if (zip64_total_num_of_disks != 1U)
            return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_MULTIDISK);
        if (zip64_cdir_total_entries > MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
        pZip->m_total_files = (mz_uint32)zip64_cdir_total_entries;
        if (zip64_cdir_total_entries_on_this_disk > MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
        cdir_entries_on_this_disk = (mz_uint32)zip64_cdir_total_entries_on_this_disk;
        if (zip64_size_of_central_directory > MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_CDIR_SIZE);
        cdir_size = (mz_uint32)zip64_size_of_central_directory;
        num_this_disk = MZ_READ_LE32(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_NUM_THIS_DISK_OFS);
        cdir_disk_index = MZ_READ_LE32(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_NUM_DISK_CDIR_OFS);
        cdir_ofs = MZ_READ_LE64(pZip64_end_of_central_dir + MZ_ZIP64_ECDH_CDIR_OFS_OFS);
    }
    if (pZip->m_total_files != cdir_entries_on_this_disk)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_MULTIDISK);
    if (((num_this_disk | cdir_disk_index) != 0) && ((num_this_disk != 1) || (cdir_disk_index != 1)))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_MULTIDISK);
    if (cdir_size < (mz_uint64)pZip->m_total_files * MZ_ZIP_CENTRAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    if (cdir_size > pZip->m_archive_size || cdir_ofs > pZip->m_archive_size - cdir_size)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    if (eocd_ofs < cdir_ofs + cdir_size)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    archive_ofs = eocd_ofs - (cdir_ofs + cdir_size);
    if (pZip->m_pState->m_zip64) {
        if (archive_ofs < MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE +
                              MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE)
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
        archive_ofs -= MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE +
                       MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE;
    }
    if ((pZip->m_zip_type == MZ_ZIP_TYPE_FILE || pZip->m_zip_type == MZ_ZIP_TYPE_CFILE ||
         pZip->m_zip_type == MZ_ZIP_TYPE_USER) &&
        pZip->m_pState->m_file_archive_start_ofs == 0) {
        pZip->m_pState->m_file_archive_start_ofs = archive_ofs;
        pZip->m_archive_size -= archive_ofs;
    }
    pZip->m_central_directory_file_ofs = cdir_ofs;
    if (pZip->m_total_files) {
        mz_uint i, n;
        if ((!mz_zip_array_resize(pZip, &pZip->m_pState->m_central_dir, cdir_size, MZ_FALSE)) ||
            (!mz_zip_array_resize(pZip, &pZip->m_pState->m_central_dir_offsets, pZip->m_total_files, MZ_FALSE)))
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        if (sort_central_dir) {
            if (!mz_zip_array_resize(pZip, &pZip->m_pState->m_sorted_central_dir_offsets, pZip->m_total_files, MZ_FALSE))
                return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        if (pZip->m_pRead(pZip->m_pIO_opaque, cdir_ofs, pZip->m_pState->m_central_dir.m_p, cdir_size) != cdir_size)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
        p = (const mz_uint8 *)pZip->m_pState->m_central_dir.m_p;
        for (n = cdir_size, i = 0; i < pZip->m_total_files; ++i) {
            mz_uint total_header_size, disk_index, bit_flags, filename_size, ext_data_size;
            mz_uint64 comp_size, decomp_size, local_header_ofs;
            if ((n < MZ_ZIP_CENTRAL_DIR_HEADER_SIZE) || (MZ_READ_LE32(p) != MZ_ZIP_CENTRAL_DIR_HEADER_SIG))
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, mz_uint32, i) = (mz_uint32)(p - (const mz_uint8 *)pZip->m_pState->m_central_dir.m_p);
            if (sort_central_dir)
                MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_sorted_central_dir_offsets, mz_uint32, i) = i;
            comp_size = MZ_READ_LE32(p + MZ_ZIP_CDH_COMPRESSED_SIZE_OFS);
            decomp_size = MZ_READ_LE32(p + MZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS);
            local_header_ofs = MZ_READ_LE32(p + MZ_ZIP_CDH_LOCAL_HEADER_OFS);
            filename_size = MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS);
            ext_data_size = MZ_READ_LE16(p + MZ_ZIP_CDH_EXTRA_LEN_OFS);
            if ((!pZip->m_pState->m_zip64_has_extended_info_fields) &&
                (ext_data_size) &&
                (MZ_MAX(MZ_MAX(comp_size, decomp_size), local_header_ofs) == MZ_UINT32_MAX)) {
                mz_uint32 extra_size_remaining = ext_data_size;
                if (extra_size_remaining) {
                    const mz_uint8 *pExtra_data;
                    void *buf = NULL;
                    if (MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size + ext_data_size > n) {
                        buf = MZ_MALLOC(ext_data_size);
                        if (buf == NULL)
                            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
                        if (pZip->m_pRead(pZip->m_pIO_opaque, cdir_ofs + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size, buf, ext_data_size) != ext_data_size) {
                            MZ_FREE(buf);
                            return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
                        }
                        pExtra_data = (mz_uint8 *)buf;
                    } else {
                        pExtra_data = p + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size;
                    }
                    do {
                        mz_uint32 field_id;
                        mz_uint32 field_data_size;
                        if (extra_size_remaining < (sizeof(mz_uint16) * 2)) {
                            MZ_FREE(buf);
                            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                        }
                        field_id = MZ_READ_LE16(pExtra_data);
                        field_data_size = MZ_READ_LE16(pExtra_data + sizeof(mz_uint16));
                        if ((field_data_size + sizeof(mz_uint16) * 2) > extra_size_remaining) {
                            MZ_FREE(buf);
                            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                        }
                        if (field_id == MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID) {
                            pZip->m_pState->m_zip64 = MZ_TRUE;
                            pZip->m_pState->m_zip64_has_extended_info_fields = MZ_TRUE;
                            break;
                        }
                        pExtra_data += sizeof(mz_uint16) * 2 + field_data_size;
                        extra_size_remaining = extra_size_remaining - sizeof(mz_uint16) * 2 - field_data_size;
                    } while (extra_size_remaining);
                    MZ_FREE(buf);
                }
            }
            if ((comp_size != MZ_UINT32_MAX) && (decomp_size != MZ_UINT32_MAX)) {
                if (((!MZ_READ_LE32(p + MZ_ZIP_CDH_METHOD_OFS)) && (decomp_size != comp_size)) || (decomp_size && !comp_size))
                    return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            }
            disk_index = MZ_READ_LE16(p + MZ_ZIP_CDH_DISK_START_OFS);
            if ((disk_index == MZ_UINT16_MAX) || ((disk_index != num_this_disk) && (disk_index != 1)))
                return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_MULTIDISK);
            if (comp_size != MZ_UINT32_MAX) {
                if (((mz_uint64)MZ_READ_LE32(p + MZ_ZIP_CDH_LOCAL_HEADER_OFS) + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + comp_size) > pZip->m_archive_size)
                    return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            }
            bit_flags = MZ_READ_LE16(p + MZ_ZIP_CDH_BIT_FLAG_OFS);
            if (bit_flags & MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_LOCAL_DIR_IS_MASKED)
                return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_ENCRYPTION);
            if ((total_header_size = MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS) + MZ_READ_LE16(p + MZ_ZIP_CDH_EXTRA_LEN_OFS) + MZ_READ_LE16(p + MZ_ZIP_CDH_COMMENT_LEN_OFS)) > n)
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            n -= total_header_size;
            p += total_header_size;
        }
    }
    if (sort_central_dir)
        mz_zip_reader_sort_central_dir_offsets_by_filename(pZip);
    return MZ_TRUE;
}
void mz_zip_zero_struct(mz_zip_archive *pZip) {
    if (pZip)
        MZ_CLEAR_PTR(pZip);
}
static mz_bool mz_zip_reader_end_internal(mz_zip_archive *pZip, mz_bool set_last_error) {
    mz_bool status = MZ_TRUE;
    if (!pZip)
        return MZ_FALSE;
    if ((!pZip->m_pState) || (!pZip->m_pAlloc) || (!pZip->m_pFree) || (pZip->m_zip_mode != MZ_ZIP_MODE_READING)) {
        if (set_last_error)
            pZip->m_last_error = MZ_ZIP_INVALID_PARAMETER;
        return MZ_FALSE;
    }
    if (pZip->m_pState) {
        mz_zip_internal_state *pState = pZip->m_pState;
        pZip->m_pState = NULL;
        mz_zip_array_clear(pZip, &pState->m_central_dir);
        mz_zip_array_clear(pZip, &pState->m_central_dir_offsets);
        mz_zip_array_clear(pZip, &pState->m_sorted_central_dir_offsets);
#ifndef MINIZ_NO_STDIO
        if (pState->m_pFile) {
            if (pZip->m_zip_type == MZ_ZIP_TYPE_FILE) {
                if (MZ_FCLOSE(pState->m_pFile) == EOF) {
                    if (set_last_error)
                        pZip->m_last_error = MZ_ZIP_FILE_CLOSE_FAILED;
                    status = MZ_FALSE;
                }
            }
            pState->m_pFile = NULL;
        }
#endif
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
    }
    pZip->m_zip_mode = MZ_ZIP_MODE_INVALID;
    return status;
}
mz_bool mz_zip_reader_end(mz_zip_archive *pZip) {
    return mz_zip_reader_end_internal(pZip, MZ_TRUE);
}
mz_bool mz_zip_reader_init(mz_zip_archive *pZip, mz_uint64 size, mz_uint flags) {
    if ((!pZip) || (!pZip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (!mz_zip_reader_init_internal(pZip, flags))
        return MZ_FALSE;
    pZip->m_zip_type = MZ_ZIP_TYPE_USER;
    pZip->m_archive_size = size;
    if (!mz_zip_reader_read_central_dir(pZip, flags)) {
        mz_zip_reader_end_internal(pZip, MZ_FALSE);
        return MZ_FALSE;
    }
    return MZ_TRUE;
}
static size_t mz_zip_mem_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
    mz_zip_archive *pZip = (mz_zip_archive *)pOpaque;
    size_t s = (file_ofs >= pZip->m_archive_size) ? 0 : (size_t)MZ_MIN(pZip->m_archive_size - file_ofs, n);
    memcpy(pBuf, (const mz_uint8 *)pZip->m_pState->m_pMem + file_ofs, s);
    return s;
}
mz_bool mz_zip_reader_init_mem(mz_zip_archive *pZip, const void *pMem, size_t size, mz_uint flags) {
    if (!pMem)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (size < MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
    if (!mz_zip_reader_init_internal(pZip, flags))
        return MZ_FALSE;
    pZip->m_zip_type = MZ_ZIP_TYPE_MEMORY;
    pZip->m_archive_size = size;
    pZip->m_pRead = mz_zip_mem_read_func;
    pZip->m_pIO_opaque = pZip;
    pZip->m_pNeeds_keepalive = NULL;
#ifdef __cplusplus
    pZip->m_pState->m_pMem = const_cast<void *>(pMem);
#else
    pZip->m_pState->m_pMem = (void *)pMem;
#endif
    pZip->m_pState->m_mem_size = size;
    if (!mz_zip_reader_read_central_dir(pZip, flags)) {
        mz_zip_reader_end_internal(pZip, MZ_FALSE);
        return MZ_FALSE;
    }
    return MZ_TRUE;
}
#ifndef MINIZ_NO_STDIO
static size_t mz_zip_file_read_func(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
    mz_zip_archive *pZip = (mz_zip_archive *)pOpaque;
    mz_int64 cur_ofs = MZ_FTELL64(pZip->m_pState->m_pFile);
    file_ofs += pZip->m_pState->m_file_archive_start_ofs;
    if (((mz_int64)file_ofs < 0) || (((cur_ofs != (mz_int64)file_ofs)) && (MZ_FSEEK64(pZip->m_pState->m_pFile, (mz_int64)file_ofs, SEEK_SET))))
        return 0;
    return MZ_FREAD(pBuf, 1, n, pZip->m_pState->m_pFile);
}
mz_bool mz_zip_reader_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint32 flags) {
    return mz_zip_reader_init_file_v2(pZip, pFilename, flags, 0, 0);
}
mz_bool mz_zip_reader_init_file_v2(mz_zip_archive *pZip, const char *pFilename, mz_uint flags, mz_uint64 file_start_ofs, mz_uint64 archive_size) {
    mz_uint64 file_size;
    MZ_FILE *pFile;
    if ((!pZip) || (!pFilename) || ((archive_size) && (archive_size < MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pFile = MZ_FOPEN(pFilename, (flags & MZ_ZIP_FLAG_READ_ALLOW_WRITING) ? "r+b" : "rb");
    if (!pFile)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_OPEN_FAILED);
    file_size = archive_size;
    if (!file_size) {
        if (MZ_FSEEK64(pFile, 0, SEEK_END)) {
            MZ_FCLOSE(pFile);
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_SEEK_FAILED);
        }
        file_size = MZ_FTELL64(pFile);
    }
    if (file_size < MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE) {
        MZ_FCLOSE(pFile);
        return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
    }
    if (!mz_zip_reader_init_internal(pZip, flags)) {
        MZ_FCLOSE(pFile);
        return MZ_FALSE;
    }
    pZip->m_zip_type = MZ_ZIP_TYPE_FILE;
    pZip->m_pRead = mz_zip_file_read_func;
    pZip->m_pIO_opaque = pZip;
    pZip->m_pState->m_pFile = pFile;
    pZip->m_archive_size = file_size;
    pZip->m_pState->m_file_archive_start_ofs = file_start_ofs;
    if (!mz_zip_reader_read_central_dir(pZip, flags)) {
        mz_zip_reader_end_internal(pZip, MZ_FALSE);
        return MZ_FALSE;
    }
    return MZ_TRUE;
}
mz_bool mz_zip_reader_init_cfile(mz_zip_archive *pZip, MZ_FILE *pFile, mz_uint64 archive_size, mz_uint flags) {
    mz_uint64 cur_file_ofs;
    if ((!pZip) || (!pFile))
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_OPEN_FAILED);
    cur_file_ofs = MZ_FTELL64(pFile);
    if (!archive_size) {
        if (MZ_FSEEK64(pFile, 0, SEEK_END))
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_SEEK_FAILED);
        archive_size = MZ_FTELL64(pFile) - cur_file_ofs;
        if (archive_size < MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
            return mz_zip_set_error(pZip, MZ_ZIP_NOT_AN_ARCHIVE);
    }
    if (!mz_zip_reader_init_internal(pZip, flags))
        return MZ_FALSE;
    pZip->m_zip_type = MZ_ZIP_TYPE_CFILE;
    pZip->m_pRead = mz_zip_file_read_func;
    pZip->m_pIO_opaque = pZip;
    pZip->m_pState->m_pFile = pFile;
    pZip->m_archive_size = archive_size;
    pZip->m_pState->m_file_archive_start_ofs = cur_file_ofs;
    if (!mz_zip_reader_read_central_dir(pZip, flags)) {
        mz_zip_reader_end_internal(pZip, MZ_FALSE);
        return MZ_FALSE;
    }
    return MZ_TRUE;
}
#endif
static MZ_FORCEINLINE const mz_uint8 *mz_zip_get_cdh(mz_zip_archive *pZip, mz_uint file_index) {
    if ((!pZip) || (!pZip->m_pState) || (file_index >= pZip->m_total_files))
        return NULL;
    return &MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir, mz_uint8, MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, mz_uint32, file_index));
}
mz_bool mz_zip_reader_is_file_encrypted(mz_zip_archive *pZip, mz_uint file_index) {
    mz_uint m_bit_flag;
    const mz_uint8 *p = mz_zip_get_cdh(pZip, file_index);
    if (!p) {
        mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        return MZ_FALSE;
    }
    m_bit_flag = MZ_READ_LE16(p + MZ_ZIP_CDH_BIT_FLAG_OFS);
    return (m_bit_flag & (MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION)) != 0;
}
mz_bool mz_zip_reader_is_file_supported(mz_zip_archive *pZip, mz_uint file_index) {
    mz_uint bit_flag;
    mz_uint method;
    const mz_uint8 *p = mz_zip_get_cdh(pZip, file_index);
    if (!p) {
        mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        return MZ_FALSE;
    }
    method = MZ_READ_LE16(p + MZ_ZIP_CDH_METHOD_OFS);
    bit_flag = MZ_READ_LE16(p + MZ_ZIP_CDH_BIT_FLAG_OFS);
    if (method != 0) {
        mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_METHOD);
        return MZ_FALSE;
    }
    if (bit_flag & (MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION)) {
        mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_ENCRYPTION);
        return MZ_FALSE;
    }
    if (bit_flag & MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_COMPRESSED_PATCH_FLAG) {
        mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_FEATURE);
        return MZ_FALSE;
    }
    return MZ_TRUE;
}
mz_bool mz_zip_reader_is_file_a_directory(mz_zip_archive *pZip, mz_uint file_index) {
    mz_uint filename_len, attribute_mapping_id, external_attr;
    const mz_uint8 *p = mz_zip_get_cdh(pZip, file_index);
    if (!p) {
        mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        return MZ_FALSE;
    }
    filename_len = MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS);
    if (filename_len) {
        if (*(p + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_len - 1) == '/')
            return MZ_TRUE;
    }
    attribute_mapping_id = MZ_READ_LE16(p + MZ_ZIP_CDH_VERSION_MADE_BY_OFS) >> 8;
    (void)attribute_mapping_id;
    external_attr = MZ_READ_LE32(p + MZ_ZIP_CDH_EXTERNAL_ATTR_OFS);
    if ((external_attr & MZ_ZIP_DOS_DIR_ATTRIBUTE_BITFLAG) != 0) {
        return MZ_TRUE;
    }
    return MZ_FALSE;
}
static mz_bool mz_zip_file_stat_internal(mz_zip_archive *pZip, mz_uint file_index, const mz_uint8 *pCentral_dir_header, mz_zip_archive_file_stat *pStat, mz_bool *pFound_zip64_extra_data) {
    mz_uint n;
    const mz_uint8 *p = pCentral_dir_header;
    if (pFound_zip64_extra_data)
        *pFound_zip64_extra_data = MZ_FALSE;
    if ((!p) || (!pStat))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pStat->m_file_index = file_index;
    pStat->m_central_dir_ofs = MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, mz_uint32, file_index);
    pStat->m_version_made_by = MZ_READ_LE16(p + MZ_ZIP_CDH_VERSION_MADE_BY_OFS);
    pStat->m_version_needed = MZ_READ_LE16(p + MZ_ZIP_CDH_VERSION_NEEDED_OFS);
    pStat->m_bit_flag = MZ_READ_LE16(p + MZ_ZIP_CDH_BIT_FLAG_OFS);
    pStat->m_method = MZ_READ_LE16(p + MZ_ZIP_CDH_METHOD_OFS);
#ifndef MINIZ_NO_TIME
    pStat->m_time = mz_zip_dos_to_time_t(MZ_READ_LE16(p + MZ_ZIP_CDH_FILE_TIME_OFS), MZ_READ_LE16(p + MZ_ZIP_CDH_FILE_DATE_OFS));
#endif
    pStat->m_crc32 = MZ_READ_LE32(p + MZ_ZIP_CDH_CRC32_OFS);
    pStat->m_comp_size = MZ_READ_LE32(p + MZ_ZIP_CDH_COMPRESSED_SIZE_OFS);
    pStat->m_uncomp_size = MZ_READ_LE32(p + MZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS);
    pStat->m_internal_attr = MZ_READ_LE16(p + MZ_ZIP_CDH_INTERNAL_ATTR_OFS);
    pStat->m_external_attr = MZ_READ_LE32(p + MZ_ZIP_CDH_EXTERNAL_ATTR_OFS);
    pStat->m_local_header_ofs = MZ_READ_LE32(p + MZ_ZIP_CDH_LOCAL_HEADER_OFS);
    n = MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS);
    n = MZ_MIN(n, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE - 1);
    memcpy(pStat->m_filename, p + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE, n);
    pStat->m_filename[n] = '\0';
    n = MZ_READ_LE16(p + MZ_ZIP_CDH_COMMENT_LEN_OFS);
    n = MZ_MIN(n, MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE - 1);
    pStat->m_comment_size = n;
    memcpy(pStat->m_comment, p + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS) + MZ_READ_LE16(p + MZ_ZIP_CDH_EXTRA_LEN_OFS), n);
    pStat->m_comment[n] = '\0';
    pStat->m_is_directory = mz_zip_reader_is_file_a_directory(pZip, file_index);
    pStat->m_is_encrypted = mz_zip_reader_is_file_encrypted(pZip, file_index);
    pStat->m_is_supported = mz_zip_reader_is_file_supported(pZip, file_index);
    if (MZ_MAX(MZ_MAX(pStat->m_comp_size, pStat->m_uncomp_size), pStat->m_local_header_ofs) == MZ_UINT32_MAX) {
        mz_uint32 extra_size_remaining = MZ_READ_LE16(p + MZ_ZIP_CDH_EXTRA_LEN_OFS);
        if (extra_size_remaining) {
            const mz_uint8 *pExtra_data = p + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS);
            do {
                mz_uint32 field_id;
                mz_uint32 field_data_size;
                if (extra_size_remaining < (sizeof(mz_uint16) * 2))
                    return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                field_id = MZ_READ_LE16(pExtra_data);
                field_data_size = MZ_READ_LE16(pExtra_data + sizeof(mz_uint16));
                if ((field_data_size + sizeof(mz_uint16) * 2) > extra_size_remaining)
                    return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                if (field_id == MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID) {
                    const mz_uint8 *pField_data = pExtra_data + sizeof(mz_uint16) * 2;
                    mz_uint32 field_data_remaining = field_data_size;
                    if (pFound_zip64_extra_data)
                        *pFound_zip64_extra_data = MZ_TRUE;
                    if (pStat->m_uncomp_size == MZ_UINT32_MAX) {
                        if (field_data_remaining < sizeof(mz_uint64))
                            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                        pStat->m_uncomp_size = MZ_READ_LE64(pField_data);
                        pField_data += sizeof(mz_uint64);
                        field_data_remaining -= sizeof(mz_uint64);
                    }
                    if (pStat->m_comp_size == MZ_UINT32_MAX) {
                        if (field_data_remaining < sizeof(mz_uint64))
                            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                        pStat->m_comp_size = MZ_READ_LE64(pField_data);
                        pField_data += sizeof(mz_uint64);
                        field_data_remaining -= sizeof(mz_uint64);
                    }
                    if (pStat->m_local_header_ofs == MZ_UINT32_MAX) {
                        if (field_data_remaining < sizeof(mz_uint64))
                            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                        pStat->m_local_header_ofs = MZ_READ_LE64(pField_data);
                        pField_data += sizeof(mz_uint64);
                        field_data_remaining -= sizeof(mz_uint64);
                    }
                    break;
                }
                pExtra_data += sizeof(mz_uint16) * 2 + field_data_size;
                extra_size_remaining = extra_size_remaining - sizeof(mz_uint16) * 2 - field_data_size;
            } while (extra_size_remaining);
        }
    }
    return MZ_TRUE;
}
static MZ_FORCEINLINE mz_bool mz_zip_string_equal(const char *pA, const char *pB, mz_uint len, mz_uint flags) {
    mz_uint i;
    if (flags & MZ_ZIP_FLAG_CASE_SENSITIVE)
        return 0 == memcmp(pA, pB, len);
    for (i = 0; i < len; ++i)
        if (MZ_TOLOWER(pA[i]) != MZ_TOLOWER(pB[i]))
            return MZ_FALSE;
    return MZ_TRUE;
}
static MZ_FORCEINLINE int mz_zip_filename_compare(const mz_zip_array *pCentral_dir_array, const mz_zip_array *pCentral_dir_offsets, mz_uint l_index, const char *pR, mz_uint r_len) {
    const mz_uint8 *pL = &MZ_ZIP_ARRAY_ELEMENT(pCentral_dir_array, mz_uint8, MZ_ZIP_ARRAY_ELEMENT(pCentral_dir_offsets, mz_uint32, l_index)), *pE;
    mz_uint l_len = MZ_READ_LE16(pL + MZ_ZIP_CDH_FILENAME_LEN_OFS);
    mz_uint8 l = 0, r = 0;
    pL += MZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
    pE = pL + MZ_MIN(l_len, r_len);
    while (pL < pE) {
        if ((l = MZ_TOLOWER(*pL)) != (r = MZ_TOLOWER(*pR)))
            break;
        pL++;
        pR++;
    }
    return (pL == pE) ? (int)(l_len - r_len) : (l - r);
}
static mz_bool mz_zip_locate_file_binary_search(mz_zip_archive *pZip, const char *pFilename, mz_uint32 *pIndex) {
    mz_zip_internal_state *pState = pZip->m_pState;
    const mz_zip_array *pCentral_dir_offsets = &pState->m_central_dir_offsets;
    const mz_zip_array *pCentral_dir = &pState->m_central_dir;
    mz_uint32 *pIndices = &MZ_ZIP_ARRAY_ELEMENT(&pState->m_sorted_central_dir_offsets, mz_uint32, 0);
    const mz_uint32 size = pZip->m_total_files;
    const mz_uint filename_len = (mz_uint)strlen(pFilename);
    if (pIndex)
        *pIndex = 0;
    if (size) {
        mz_int64 l = 0, h = (mz_int64)size - 1;
        while (l <= h) {
            mz_int64 m = l + ((h - l) >> 1);
            mz_uint32 file_index = pIndices[(mz_uint32)m];
            int comp = mz_zip_filename_compare(pCentral_dir, pCentral_dir_offsets, file_index, pFilename, filename_len);
            if (!comp) {
                if (pIndex)
                    *pIndex = file_index;
                return MZ_TRUE;
            } else if (comp < 0)
                l = m + 1;
            else
                h = m - 1;
        }
    }
    return mz_zip_set_error(pZip, MZ_ZIP_FILE_NOT_FOUND);
}
int mz_zip_reader_locate_file(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags) {
    mz_uint32 index;
    if (!mz_zip_reader_locate_file_v2(pZip, pName, pComment, flags, &index))
        return -1;
    else
        return (int)index;
}
mz_bool mz_zip_reader_locate_file_v2(mz_zip_archive *pZip, const char *pName, const char *pComment, mz_uint flags, mz_uint32 *pIndex) {
    mz_uint file_index;
    size_t name_len, comment_len;
    if (pIndex)
        *pIndex = 0;
    if ((!pZip) || (!pZip->m_pState) || (!pName))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (((pZip->m_pState->m_init_flags & MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY) == 0) &&
        (pZip->m_zip_mode == MZ_ZIP_MODE_READING) &&
        ((flags & (MZ_ZIP_FLAG_IGNORE_PATH | MZ_ZIP_FLAG_CASE_SENSITIVE)) == 0) && (!pComment) && (pZip->m_pState->m_sorted_central_dir_offsets.m_size)) {
        return mz_zip_locate_file_binary_search(pZip, pName, pIndex);
    }
    name_len = strlen(pName);
    if (name_len > MZ_UINT16_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    comment_len = pComment ? strlen(pComment) : 0;
    if (comment_len > MZ_UINT16_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    for (file_index = 0; file_index < pZip->m_total_files; file_index++) {
        const mz_uint8 *pHeader = &MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir, mz_uint8, MZ_ZIP_ARRAY_ELEMENT(&pZip->m_pState->m_central_dir_offsets, mz_uint32, file_index));
        mz_uint filename_len = MZ_READ_LE16(pHeader + MZ_ZIP_CDH_FILENAME_LEN_OFS);
        const char *pFilename = (const char *)pHeader + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE;
        if (filename_len < name_len)
            continue;
        if (comment_len) {
            mz_uint file_extra_len = MZ_READ_LE16(pHeader + MZ_ZIP_CDH_EXTRA_LEN_OFS), file_comment_len = MZ_READ_LE16(pHeader + MZ_ZIP_CDH_COMMENT_LEN_OFS);
            const char *pFile_comment = pFilename + filename_len + file_extra_len;
            if ((file_comment_len != comment_len) || (!mz_zip_string_equal(pComment, pFile_comment, file_comment_len, flags)))
                continue;
        }
        if ((flags & MZ_ZIP_FLAG_IGNORE_PATH) && (filename_len)) {
            int ofs = filename_len - 1;
            do {
                if ((pFilename[ofs] == '/') || (pFilename[ofs] == '\\') || (pFilename[ofs] == ':'))
                    break;
            } while (--ofs >= 0);
            ofs++;
            pFilename += ofs;
            filename_len -= ofs;
        }
        if ((filename_len == name_len) && (mz_zip_string_equal(pName, pFilename, filename_len, flags))) {
            if (pIndex)
                *pIndex = file_index;
            return MZ_TRUE;
        }
    }
    return mz_zip_set_error(pZip, MZ_ZIP_FILE_NOT_FOUND);
}
static mz_bool mz_zip_reader_extract_to_mem_no_alloc1(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size, const mz_zip_archive_file_stat *st) {
    mz_uint64 needed_size, cur_file_ofs;
    mz_zip_archive_file_stat file_stat;
    mz_uint32 local_header_u32[(MZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pLocal_header = (mz_uint8 *)local_header_u32;
    if ((!pZip) || (!pZip->m_pState) || ((buf_size) && (!pBuf)) || ((user_read_buf_size) && (!pUser_read_buf)) || (!pZip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (st) {
        file_stat = *st;
    } else if (!mz_zip_reader_file_stat(pZip, file_index, &file_stat))
        return MZ_FALSE;
    if ((file_stat.m_is_directory) || (!file_stat.m_comp_size))
        return MZ_TRUE;
    if (file_stat.m_bit_flag & (MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_COMPRESSED_PATCH_FLAG))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_ENCRYPTION);
    if ((!(flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) && (file_stat.m_method != 0))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_METHOD);
    needed_size = (flags & MZ_ZIP_FLAG_COMPRESSED_DATA) ? file_stat.m_comp_size : file_stat.m_uncomp_size;
    if (buf_size < needed_size)
        return mz_zip_set_error(pZip, MZ_ZIP_BUF_TOO_SMALL);
    cur_file_ofs = file_stat.m_local_header_ofs;
    if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pLocal_header, MZ_ZIP_LOCAL_DIR_HEADER_SIZE) != MZ_ZIP_LOCAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
    if (MZ_READ_LE32(pLocal_header) != MZ_ZIP_LOCAL_DIR_HEADER_SIG)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    cur_file_ofs += (mz_uint64)(MZ_ZIP_LOCAL_DIR_HEADER_SIZE) + MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_FILENAME_LEN_OFS) + MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_EXTRA_LEN_OFS);
    if ((cur_file_ofs + file_stat.m_comp_size) > pZip->m_archive_size)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pBuf, (size_t)needed_size) != needed_size)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
    if ((flags & MZ_ZIP_FLAG_COMPRESSED_DATA) == 0) {
        if (mz_crc32(MZ_CRC32_INIT, (const mz_uint8 *)pBuf, (size_t)file_stat.m_uncomp_size) != file_stat.m_crc32)
            return mz_zip_set_error(pZip, MZ_ZIP_CRC_CHECK_FAILED);
    }
#endif
    return MZ_TRUE;
}
mz_bool mz_zip_reader_extract_to_mem_no_alloc(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size) {
    return mz_zip_reader_extract_to_mem_no_alloc1(pZip, file_index, pBuf, buf_size, flags, pUser_read_buf, user_read_buf_size, NULL);
}
mz_bool mz_zip_reader_extract_file_to_mem_no_alloc(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags, void *pUser_read_buf, size_t user_read_buf_size) {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(pZip, pFilename, NULL, flags, &file_index))
        return MZ_FALSE;
    return mz_zip_reader_extract_to_mem_no_alloc1(pZip, file_index, pBuf, buf_size, flags, pUser_read_buf, user_read_buf_size, NULL);
}
mz_bool mz_zip_reader_extract_to_mem(mz_zip_archive *pZip, mz_uint file_index, void *pBuf, size_t buf_size, mz_uint flags) {
    return mz_zip_reader_extract_to_mem_no_alloc1(pZip, file_index, pBuf, buf_size, flags, NULL, 0, NULL);
}
mz_bool mz_zip_reader_extract_file_to_mem(mz_zip_archive *pZip, const char *pFilename, void *pBuf, size_t buf_size, mz_uint flags) {
    return mz_zip_reader_extract_file_to_mem_no_alloc(pZip, pFilename, pBuf, buf_size, flags, NULL, 0);
}
void *mz_zip_reader_extract_to_heap(mz_zip_archive *pZip, mz_uint file_index, size_t *pSize, mz_uint flags) {
    mz_zip_archive_file_stat file_stat;
    mz_uint64 alloc_size;
    void *pBuf;
    if (pSize)
        *pSize = 0;
    if (!mz_zip_reader_file_stat(pZip, file_index, &file_stat))
        return NULL;
    alloc_size = (flags & MZ_ZIP_FLAG_COMPRESSED_DATA) ? file_stat.m_comp_size : file_stat.m_uncomp_size;
    if (((sizeof(size_t) == sizeof(mz_uint32))) && (alloc_size > 0x7FFFFFFF)) {
        mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        return NULL;
    }
    if (NULL == (pBuf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, (size_t)alloc_size))) {
        mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        return NULL;
    }
    if (!mz_zip_reader_extract_to_mem_no_alloc1(pZip, file_index, pBuf, (size_t)alloc_size, flags, NULL, 0, &file_stat)) {
        pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
        return NULL;
    }
    if (pSize)
        *pSize = (size_t)alloc_size;
    return pBuf;
}
void *mz_zip_reader_extract_file_to_heap(mz_zip_archive *pZip, const char *pFilename, size_t *pSize, mz_uint flags) {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(pZip, pFilename, NULL, flags, &file_index)) {
        if (pSize)
            *pSize = 0;
        return MZ_FALSE;
    }
    return mz_zip_reader_extract_to_heap(pZip, file_index, pSize, flags);
}
mz_bool mz_zip_reader_extract_to_callback(mz_zip_archive *pZip, mz_uint file_index, mz_file_write_func pCallback, void *pOpaque, mz_uint flags) {
    int status = MZ_ZIP_STATUS_DONE;
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
    mz_uint file_crc32 = MZ_CRC32_INIT;
#endif
    mz_uint64 read_buf_size, read_buf_avail, comp_remaining, out_buf_ofs = 0, cur_file_ofs;
    mz_zip_archive_file_stat file_stat;
    void *pRead_buf = NULL;
    mz_uint32 local_header_u32[(MZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pLocal_header = (mz_uint8 *)local_header_u32;
    if ((!pZip) || (!pZip->m_pState) || (!pCallback) || (!pZip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (!mz_zip_reader_file_stat(pZip, file_index, &file_stat))
        return MZ_FALSE;
    if ((file_stat.m_is_directory) || (!file_stat.m_comp_size))
        return MZ_TRUE;
    if (file_stat.m_bit_flag & (MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_COMPRESSED_PATCH_FLAG))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_ENCRYPTION);
    if ((!(flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) && (file_stat.m_method != 0))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_METHOD);
    cur_file_ofs = file_stat.m_local_header_ofs;
    if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pLocal_header, MZ_ZIP_LOCAL_DIR_HEADER_SIZE) != MZ_ZIP_LOCAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
    if (MZ_READ_LE32(pLocal_header) != MZ_ZIP_LOCAL_DIR_HEADER_SIG)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    cur_file_ofs += (mz_uint64)(MZ_ZIP_LOCAL_DIR_HEADER_SIZE) + MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_FILENAME_LEN_OFS) + MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_EXTRA_LEN_OFS);
    if ((cur_file_ofs + file_stat.m_comp_size) > pZip->m_archive_size)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    if (pZip->m_pState->m_pMem) {
        pRead_buf = (mz_uint8 *)pZip->m_pState->m_pMem + cur_file_ofs;
        read_buf_size = read_buf_avail = file_stat.m_comp_size;
        comp_remaining = 0;
    } else {
        read_buf_size = MZ_MIN(file_stat.m_comp_size, (mz_uint64)MZ_ZIP_MAX_IO_BUF_SIZE);
        if (NULL == (pRead_buf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, (size_t)read_buf_size)))
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        read_buf_avail = 0;
        comp_remaining = file_stat.m_comp_size;
    }
    if (pZip->m_pState->m_pMem) {
        if (((sizeof(size_t) == sizeof(mz_uint32))) && (file_stat.m_comp_size > MZ_UINT32_MAX))
            return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        if (pCallback(pOpaque, out_buf_ofs, pRead_buf, (size_t)file_stat.m_comp_size) != file_stat.m_comp_size) {
            mz_zip_set_error(pZip, MZ_ZIP_WRITE_CALLBACK_FAILED);
            status = MZ_ZIP_STATUS_FAILED;
        } else if (!(flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) {
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
            file_crc32 = (mz_uint32)mz_crc32(file_crc32, (const mz_uint8 *)pRead_buf, (size_t)file_stat.m_comp_size);
#endif
        }
        cur_file_ofs += file_stat.m_comp_size;
        out_buf_ofs += file_stat.m_comp_size;
        comp_remaining = 0;
    } else {
        while (comp_remaining) {
            read_buf_avail = MZ_MIN(read_buf_size, comp_remaining);
            if (pZip->m_pRead(pZip->m_pIO_opaque, cur_file_ofs, pRead_buf, (size_t)read_buf_avail) != read_buf_avail) {
                mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
                status = MZ_ZIP_STATUS_FAILED;
                break;
            }
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
            if (!(flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) {
                file_crc32 = (mz_uint32)mz_crc32(file_crc32, (const mz_uint8 *)pRead_buf, (size_t)read_buf_avail);
            }
#endif
            if (pCallback(pOpaque, out_buf_ofs, pRead_buf, (size_t)read_buf_avail) != read_buf_avail) {
                mz_zip_set_error(pZip, MZ_ZIP_WRITE_CALLBACK_FAILED);
                status = MZ_ZIP_STATUS_FAILED;
                break;
            }
            cur_file_ofs += read_buf_avail;
            out_buf_ofs += read_buf_avail;
            comp_remaining -= read_buf_avail;
        }
    }
    if ((status == MZ_ZIP_STATUS_DONE) && (!(flags & MZ_ZIP_FLAG_COMPRESSED_DATA))) {
        if (out_buf_ofs != file_stat.m_uncomp_size) {
            mz_zip_set_error(pZip, MZ_ZIP_UNEXPECTED_DECOMPRESSED_SIZE);
            status = MZ_ZIP_STATUS_FAILED;
        }
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
        else if (file_crc32 != file_stat.m_crc32) {
            mz_zip_set_error(pZip, MZ_ZIP_DECOMPRESSION_FAILED);
            status = MZ_ZIP_STATUS_FAILED;
        }
#endif
    }
    if (!pZip->m_pState->m_pMem)
        pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
    return status == MZ_ZIP_STATUS_DONE;
}
mz_bool mz_zip_reader_extract_file_to_callback(mz_zip_archive *pZip, const char *pFilename, mz_file_write_func pCallback, void *pOpaque, mz_uint flags) {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(pZip, pFilename, NULL, flags, &file_index))
        return MZ_FALSE;
    return mz_zip_reader_extract_to_callback(pZip, file_index, pCallback, pOpaque, flags);
}
mz_zip_reader_extract_iter_state *mz_zip_reader_extract_iter_new(mz_zip_archive *pZip, mz_uint file_index, mz_uint flags) {
    mz_zip_reader_extract_iter_state *pState;
    mz_uint32 local_header_u32[(MZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pLocal_header = (mz_uint8 *)local_header_u32;
    if ((!pZip) || (!pZip->m_pState))
        return NULL;
    pState = (mz_zip_reader_extract_iter_state *)pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, sizeof(mz_zip_reader_extract_iter_state));
    if (!pState) {
        mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        return NULL;
    }
    if (!mz_zip_reader_file_stat(pZip, file_index, &pState->file_stat)) {
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
        return NULL;
    }
    if (pState->file_stat.m_bit_flag & (MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_IS_ENCRYPTED | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_USES_STRONG_ENCRYPTION | MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_COMPRESSED_PATCH_FLAG)) {
        mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_ENCRYPTION);
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
        return NULL;
    }
    if ((!(flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) && (pState->file_stat.m_method != 0)) {
        mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_METHOD);
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
        return NULL;
    }
    pState->pZip = pZip;
    pState->flags = flags;
    pState->status = MZ_ZIP_STATUS_DONE;
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
    pState->file_crc32 = MZ_CRC32_INIT;
#endif
    pState->out_buf_ofs = 0;
    pState->pRead_buf = NULL;
    pState->cur_file_ofs = pState->file_stat.m_local_header_ofs;
    if (pZip->m_pRead(pZip->m_pIO_opaque, pState->cur_file_ofs, pLocal_header, MZ_ZIP_LOCAL_DIR_HEADER_SIZE) != MZ_ZIP_LOCAL_DIR_HEADER_SIZE) {
        mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
        return NULL;
    }
    if (MZ_READ_LE32(pLocal_header) != MZ_ZIP_LOCAL_DIR_HEADER_SIG) {
        mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
        return NULL;
    }
    pState->cur_file_ofs += (mz_uint64)(MZ_ZIP_LOCAL_DIR_HEADER_SIZE) + MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_FILENAME_LEN_OFS) + MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_EXTRA_LEN_OFS);
    if ((pState->cur_file_ofs + pState->file_stat.m_comp_size) > pZip->m_archive_size) {
        mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
        return NULL;
    }
    if (pZip->m_pState->m_pMem) {
        pState->pRead_buf = (mz_uint8 *)pZip->m_pState->m_pMem + pState->cur_file_ofs;
        pState->read_buf_size = pState->file_stat.m_comp_size;
    } else {
        pState->read_buf_size = 0;
    }
    pState->comp_remaining = pState->file_stat.m_comp_size;
    return pState;
}
mz_zip_reader_extract_iter_state *mz_zip_reader_extract_file_iter_new(mz_zip_archive *pZip, const char *pFilename, mz_uint flags) {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(pZip, pFilename, NULL, flags, &file_index))
        return NULL;
    return mz_zip_reader_extract_iter_new(pZip, file_index, flags);
}
size_t mz_zip_reader_extract_iter_read(mz_zip_reader_extract_iter_state *pState, void *pvBuf, size_t buf_size) {
    size_t copied_to_caller = 0;
    if ((!pState) || (!pState->pZip) || (!pState->pZip->m_pState) || (!pvBuf))
        return 0;
    copied_to_caller = (size_t)MZ_MIN(buf_size, pState->comp_remaining);
    if (pState->pZip->m_pState->m_pMem) {
        memcpy(pvBuf, pState->pRead_buf, copied_to_caller);
        pState->pRead_buf = ((mz_uint8 *)pState->pRead_buf) + copied_to_caller;
    } else {
        if (pState->pZip->m_pRead(pState->pZip->m_pIO_opaque, pState->cur_file_ofs, pvBuf, copied_to_caller) != copied_to_caller) {
            mz_zip_set_error(pState->pZip, MZ_ZIP_FILE_READ_FAILED);
            pState->status = MZ_ZIP_STATUS_FAILED;
            copied_to_caller = 0;
        }
    }
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
    if (!(pState->flags & MZ_ZIP_FLAG_COMPRESSED_DATA))
        pState->file_crc32 = (mz_uint32)mz_crc32(pState->file_crc32, (const mz_uint8 *)pvBuf, copied_to_caller);
#endif
    pState->cur_file_ofs += copied_to_caller;
    pState->out_buf_ofs += copied_to_caller;
    pState->comp_remaining -= copied_to_caller;
    return copied_to_caller;
}
mz_bool mz_zip_reader_extract_iter_free(mz_zip_reader_extract_iter_state *pState) {
    int status;
    if ((!pState) || (!pState->pZip) || (!pState->pZip->m_pState))
        return MZ_FALSE;
    if ((pState->status == MZ_ZIP_STATUS_DONE) && (!(pState->flags & MZ_ZIP_FLAG_COMPRESSED_DATA))) {
        if (pState->out_buf_ofs != pState->file_stat.m_uncomp_size) {
            mz_zip_set_error(pState->pZip, MZ_ZIP_UNEXPECTED_DECOMPRESSED_SIZE);
            pState->status = MZ_ZIP_STATUS_FAILED;
        }
#ifndef MINIZ_DISABLE_ZIP_READER_CRC32_CHECKS
        else if (pState->file_crc32 != pState->file_stat.m_crc32) {
            mz_zip_set_error(pState->pZip, MZ_ZIP_DECOMPRESSION_FAILED);
            pState->status = MZ_ZIP_STATUS_FAILED;
        }
#endif
    }
    if (!pState->pZip->m_pState->m_pMem)
        pState->pZip->m_pFree(pState->pZip->m_pAlloc_opaque, pState->pRead_buf);
    status = pState->status;
    pState->pZip->m_pFree(pState->pZip->m_pAlloc_opaque, pState);
    return status == MZ_ZIP_STATUS_DONE;
}
#ifndef MINIZ_NO_STDIO
static size_t mz_zip_file_write_callback(void *pOpaque, mz_uint64 ofs, const void *pBuf, size_t n) {
    (void)ofs;
    return MZ_FWRITE(pBuf, 1, n, (MZ_FILE *)pOpaque);
}
mz_bool mz_zip_reader_extract_to_file(mz_zip_archive *pZip, mz_uint file_index, const char *pDst_filename, mz_uint flags) {
    mz_bool status;
    mz_zip_archive_file_stat file_stat;
    MZ_FILE *pFile;
    if (!mz_zip_reader_file_stat(pZip, file_index, &file_stat))
        return MZ_FALSE;
    if ((file_stat.m_is_directory) || (!file_stat.m_is_supported))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_FEATURE);
    pFile = MZ_FOPEN(pDst_filename, "wb");
    if (!pFile)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_OPEN_FAILED);
    status = mz_zip_reader_extract_to_callback(pZip, file_index, mz_zip_file_write_callback, pFile, flags);
    if (MZ_FCLOSE(pFile) == EOF) {
        if (status)
            mz_zip_set_error(pZip, MZ_ZIP_FILE_CLOSE_FAILED);
        status = MZ_FALSE;
    }
#if !defined(MINIZ_NO_TIME) && !defined(MINIZ_NO_STDIO)
    if (status)
        mz_zip_set_file_times(pDst_filename, file_stat.m_time, file_stat.m_time);
#endif
    return status;
}
mz_bool mz_zip_reader_extract_file_to_file(mz_zip_archive *pZip, const char *pArchive_filename, const char *pDst_filename, mz_uint flags) {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(pZip, pArchive_filename, NULL, flags, &file_index))
        return MZ_FALSE;
    return mz_zip_reader_extract_to_file(pZip, file_index, pDst_filename, flags);
}
mz_bool mz_zip_reader_extract_to_cfile(mz_zip_archive *pZip, mz_uint file_index, MZ_FILE *pFile, mz_uint flags) {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(pZip, file_index, &file_stat))
        return MZ_FALSE;
    if ((file_stat.m_is_directory) || (!file_stat.m_is_supported))
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_FEATURE);
    return mz_zip_reader_extract_to_callback(pZip, file_index, mz_zip_file_write_callback, pFile, flags);
}
mz_bool mz_zip_reader_extract_file_to_cfile(mz_zip_archive *pZip, const char *pArchive_filename, MZ_FILE *pFile, mz_uint flags) {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(pZip, pArchive_filename, NULL, flags, &file_index))
        return MZ_FALSE;
    return mz_zip_reader_extract_to_cfile(pZip, file_index, pFile, flags);
}
#endif
static size_t mz_zip_compute_crc32_callback(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    mz_uint32 *p = (mz_uint32 *)pOpaque;
    (void)file_ofs;
    *p = (mz_uint32)mz_crc32(*p, (const mz_uint8 *)pBuf, n);
    return n;
}
mz_bool mz_zip_validate_file(mz_zip_archive *pZip, mz_uint file_index, mz_uint flags) {
    mz_zip_archive_file_stat file_stat;
    mz_zip_internal_state *pState;
    const mz_uint8 *pCentral_dir_header;
    mz_bool found_zip64_ext_data_in_cdir = MZ_FALSE;
    mz_bool found_zip64_ext_data_in_ldir = MZ_FALSE;
    mz_uint32 local_header_u32[(MZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pLocal_header = (mz_uint8 *)local_header_u32;
    mz_uint64 local_header_ofs = 0;
    mz_uint32 local_header_filename_len, local_header_extra_len, local_header_crc32;
    mz_uint64 local_header_comp_size, local_header_uncomp_size;
    mz_uint32 uncomp_crc32 = MZ_CRC32_INIT;
    mz_bool has_data_descriptor;
    mz_uint32 local_header_bit_flags;
    mz_zip_array file_data_array;
    mz_zip_array_init(&file_data_array, 1);
    if ((!pZip) || (!pZip->m_pState) || (!pZip->m_pAlloc) || (!pZip->m_pFree) || (!pZip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (file_index > pZip->m_total_files)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pState = pZip->m_pState;
    pCentral_dir_header = mz_zip_get_cdh(pZip, file_index);
    if (!mz_zip_file_stat_internal(pZip, file_index, pCentral_dir_header, &file_stat, &found_zip64_ext_data_in_cdir))
        return MZ_FALSE;
    if ((file_stat.m_is_directory) || (!file_stat.m_uncomp_size))
        return MZ_TRUE;
    if (file_stat.m_is_encrypted)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_ENCRYPTION);
    if (file_stat.m_method != 0)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_METHOD);
    if (!file_stat.m_is_supported)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_FEATURE);
    local_header_ofs = file_stat.m_local_header_ofs;
    if (pZip->m_pRead(pZip->m_pIO_opaque, local_header_ofs, pLocal_header, MZ_ZIP_LOCAL_DIR_HEADER_SIZE) != MZ_ZIP_LOCAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
    if (MZ_READ_LE32(pLocal_header) != MZ_ZIP_LOCAL_DIR_HEADER_SIG)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    local_header_filename_len = MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_FILENAME_LEN_OFS);
    local_header_extra_len = MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_EXTRA_LEN_OFS);
    local_header_comp_size = MZ_READ_LE32(pLocal_header + MZ_ZIP_LDH_COMPRESSED_SIZE_OFS);
    local_header_uncomp_size = MZ_READ_LE32(pLocal_header + MZ_ZIP_LDH_DECOMPRESSED_SIZE_OFS);
    local_header_crc32 = MZ_READ_LE32(pLocal_header + MZ_ZIP_LDH_CRC32_OFS);
    local_header_bit_flags = MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_BIT_FLAG_OFS);
    has_data_descriptor = (local_header_bit_flags & 8) != 0;
    if (local_header_filename_len != strlen(file_stat.m_filename))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    if ((local_header_ofs + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + local_header_filename_len + local_header_extra_len + file_stat.m_comp_size) > pZip->m_archive_size)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    if (!mz_zip_array_resize(pZip, &file_data_array, MZ_MAX(local_header_filename_len, local_header_extra_len), MZ_FALSE)) {
        mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        goto handle_failure;
    }
    if (local_header_filename_len) {
        if (pZip->m_pRead(pZip->m_pIO_opaque, local_header_ofs + MZ_ZIP_LOCAL_DIR_HEADER_SIZE, file_data_array.m_p, local_header_filename_len) != local_header_filename_len) {
            mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
            goto handle_failure;
        }
        if (memcmp(file_stat.m_filename, file_data_array.m_p, local_header_filename_len) != 0) {
            mz_zip_set_error(pZip, MZ_ZIP_VALIDATION_FAILED);
            goto handle_failure;
        }
    }
    if ((local_header_extra_len) && ((local_header_comp_size == MZ_UINT32_MAX) || (local_header_uncomp_size == MZ_UINT32_MAX))) {
        mz_uint32 extra_size_remaining = local_header_extra_len;
        const mz_uint8 *pExtra_data = (const mz_uint8 *)file_data_array.m_p;
        if (pZip->m_pRead(pZip->m_pIO_opaque, local_header_ofs + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + local_header_filename_len, file_data_array.m_p, local_header_extra_len) != local_header_extra_len) {
            mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
            goto handle_failure;
        }
        do {
            mz_uint32 field_id, field_data_size, field_total_size;
            if (extra_size_remaining < (sizeof(mz_uint16) * 2)) {
                mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                goto handle_failure;
            }
            field_id = MZ_READ_LE16(pExtra_data);
            field_data_size = MZ_READ_LE16(pExtra_data + sizeof(mz_uint16));
            field_total_size = field_data_size + sizeof(mz_uint16) * 2;
            if (field_total_size > extra_size_remaining) {
                mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                goto handle_failure;
            }
            if (field_id == MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID) {
                const mz_uint8 *pSrc_field_data = pExtra_data + sizeof(mz_uint32);
                if (field_data_size < sizeof(mz_uint64) * 2) {
                    mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                    goto handle_failure;
                }
                local_header_uncomp_size = MZ_READ_LE64(pSrc_field_data);
                local_header_comp_size = MZ_READ_LE64(pSrc_field_data + sizeof(mz_uint64));
                found_zip64_ext_data_in_ldir = MZ_TRUE;
                break;
            }
            pExtra_data += field_total_size;
            extra_size_remaining -= field_total_size;
        } while (extra_size_remaining);
    }
    if ((has_data_descriptor) && (!local_header_comp_size) && (!local_header_crc32)) {
        mz_uint8 descriptor_buf[32];
        mz_bool has_id;
        const mz_uint8 *pSrc;
        mz_uint32 file_crc32;
        mz_uint64 comp_size = 0, uncomp_size = 0;
        mz_uint32 num_descriptor_uint32s = ((pState->m_zip64) || (found_zip64_ext_data_in_ldir)) ? 6 : 4;
        if (pZip->m_pRead(pZip->m_pIO_opaque, local_header_ofs + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + local_header_filename_len + local_header_extra_len + file_stat.m_comp_size, descriptor_buf, sizeof(mz_uint32) * num_descriptor_uint32s) != (sizeof(mz_uint32) * num_descriptor_uint32s)) {
            mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
            goto handle_failure;
        }
        has_id = (MZ_READ_LE32(descriptor_buf) == MZ_ZIP_DATA_DESCRIPTOR_ID);
        pSrc = has_id ? (descriptor_buf + sizeof(mz_uint32)) : descriptor_buf;
        file_crc32 = MZ_READ_LE32(pSrc);
        if ((pState->m_zip64) || (found_zip64_ext_data_in_ldir)) {
            comp_size = MZ_READ_LE64(pSrc + sizeof(mz_uint32));
            uncomp_size = MZ_READ_LE64(pSrc + sizeof(mz_uint32) + sizeof(mz_uint64));
        } else {
            comp_size = MZ_READ_LE32(pSrc + sizeof(mz_uint32));
            uncomp_size = MZ_READ_LE32(pSrc + sizeof(mz_uint32) + sizeof(mz_uint32));
        }
        if ((file_crc32 != file_stat.m_crc32) || (comp_size != file_stat.m_comp_size) || (uncomp_size != file_stat.m_uncomp_size)) {
            mz_zip_set_error(pZip, MZ_ZIP_VALIDATION_FAILED);
            goto handle_failure;
        }
    } else {
        if ((local_header_crc32 != file_stat.m_crc32) || (local_header_comp_size != file_stat.m_comp_size) || (local_header_uncomp_size != file_stat.m_uncomp_size)) {
            mz_zip_set_error(pZip, MZ_ZIP_VALIDATION_FAILED);
            goto handle_failure;
        }
    }
    mz_zip_array_clear(pZip, &file_data_array);
    if ((flags & MZ_ZIP_FLAG_VALIDATE_HEADERS_ONLY) == 0) {
        if (!mz_zip_reader_extract_to_callback(pZip, file_index, mz_zip_compute_crc32_callback, &uncomp_crc32, 0))
            return MZ_FALSE;
        if (uncomp_crc32 != file_stat.m_crc32) {
            mz_zip_set_error(pZip, MZ_ZIP_VALIDATION_FAILED);
            return MZ_FALSE;
        }
    }
    return MZ_TRUE;
handle_failure:
    mz_zip_array_clear(pZip, &file_data_array);
    return MZ_FALSE;
}
mz_bool mz_zip_validate_archive(mz_zip_archive *pZip, mz_uint flags) {
    mz_zip_internal_state *pState;
    mz_uint32 i;
    if ((!pZip) || (!pZip->m_pState) || (!pZip->m_pAlloc) || (!pZip->m_pFree) || (!pZip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pState = pZip->m_pState;
    if (!pState->m_zip64) {
        if (pZip->m_total_files > MZ_UINT16_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
        if (pZip->m_archive_size > MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
    } else {
        if (pState->m_central_dir.m_size >= MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
    }
    for (i = 0; i < pZip->m_total_files; i++) {
        if (MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_FLAG & flags) {
            mz_uint32 found_index;
            mz_zip_archive_file_stat stat;
            if (!mz_zip_reader_file_stat(pZip, i, &stat))
                return MZ_FALSE;
            if (!mz_zip_reader_locate_file_v2(pZip, stat.m_filename, NULL, 0, &found_index))
                return MZ_FALSE;
            if (found_index != i)
                return mz_zip_set_error(pZip, MZ_ZIP_VALIDATION_FAILED);
        }
        if (!mz_zip_validate_file(pZip, i, flags))
            return MZ_FALSE;
    }
    return MZ_TRUE;
}
mz_bool mz_zip_validate_mem_archive(const void *pMem, size_t size, mz_uint flags, mz_zip_error *pErr) {
    mz_bool success = MZ_TRUE;
    mz_zip_archive zip;
    mz_zip_error actual_err = MZ_ZIP_NO_ERROR;
    if ((!pMem) || (!size)) {
        if (pErr)
            *pErr = MZ_ZIP_INVALID_PARAMETER;
        return MZ_FALSE;
    }
    mz_zip_zero_struct(&zip);
    if (!mz_zip_reader_init_mem(&zip, pMem, size, flags)) {
        if (pErr)
            *pErr = zip.m_last_error;
        return MZ_FALSE;
    }
    if (!mz_zip_validate_archive(&zip, flags)) {
        actual_err = zip.m_last_error;
        success = MZ_FALSE;
    }
    if (!mz_zip_reader_end_internal(&zip, success)) {
        if (!actual_err)
            actual_err = zip.m_last_error;
        success = MZ_FALSE;
    }
    if (pErr)
        *pErr = actual_err;
    return success;
}
#ifndef MINIZ_NO_STDIO
mz_bool mz_zip_validate_file_archive(const char *pFilename, mz_uint flags, mz_zip_error *pErr) {
    mz_bool success = MZ_TRUE;
    mz_zip_archive zip;
    mz_zip_error actual_err = MZ_ZIP_NO_ERROR;
    if (!pFilename) {
        if (pErr)
            *pErr = MZ_ZIP_INVALID_PARAMETER;
        return MZ_FALSE;
    }
    mz_zip_zero_struct(&zip);
    if (!mz_zip_reader_init_file_v2(&zip, pFilename, flags, 0, 0)) {
        if (pErr)
            *pErr = zip.m_last_error;
        return MZ_FALSE;
    }
    if (!mz_zip_validate_archive(&zip, flags)) {
        actual_err = zip.m_last_error;
        success = MZ_FALSE;
    }
    if (!mz_zip_reader_end_internal(&zip, success)) {
        if (!actual_err)
            actual_err = zip.m_last_error;
        success = MZ_FALSE;
    }
    if (pErr)
        *pErr = actual_err;
    return success;
}
#endif
#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
static MZ_FORCEINLINE void mz_write_le16(mz_uint8 *p, mz_uint16 v) {
    p[0] = (mz_uint8)v;
    p[1] = (mz_uint8)(v >> 8);
}
static MZ_FORCEINLINE void mz_write_le32(mz_uint8 *p, mz_uint32 v) {
    p[0] = (mz_uint8)v;
    p[1] = (mz_uint8)(v >> 8);
    p[2] = (mz_uint8)(v >> 16);
    p[3] = (mz_uint8)(v >> 24);
}
static MZ_FORCEINLINE void mz_write_le64(mz_uint8 *p, mz_uint64 v) {
    mz_write_le32(p, (mz_uint32)v);
    mz_write_le32(p + sizeof(mz_uint32), (mz_uint32)(v >> 32));
}
#define MZ_WRITE_LE16(p, v) mz_write_le16((mz_uint8 *)(p), (mz_uint16)(v))
#define MZ_WRITE_LE32(p, v) mz_write_le32((mz_uint8 *)(p), (mz_uint32)(v))
#define MZ_WRITE_LE64(p, v) mz_write_le64((mz_uint8 *)(p), (mz_uint64)(v))
static size_t mz_zip_heap_write_func(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    mz_zip_archive *pZip = (mz_zip_archive *)pOpaque;
    mz_zip_internal_state *pState = pZip->m_pState;
    mz_uint64 new_size = MZ_MAX(file_ofs + n, pState->m_mem_size);
    if (!n)
        return 0;
    if ((sizeof(size_t) == sizeof(mz_uint32)) && (new_size > 0x7FFFFFFF)) {
        mz_zip_set_error(pZip, MZ_ZIP_FILE_TOO_LARGE);
        return 0;
    }
    if (new_size > pState->m_mem_capacity) {
        void *pNew_block;
        size_t new_capacity = MZ_MAX(64, pState->m_mem_capacity);
        while (new_capacity < new_size)
            new_capacity *= 2;
        if (NULL == (pNew_block = pZip->m_pRealloc(pZip->m_pAlloc_opaque, pState->m_pMem, 1, new_capacity))) {
            mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
            return 0;
        }
        pState->m_pMem = pNew_block;
        pState->m_mem_capacity = new_capacity;
    }
    memcpy((mz_uint8 *)pState->m_pMem + file_ofs, pBuf, n);
    pState->m_mem_size = (size_t)new_size;
    return n;
}
static mz_bool mz_zip_writer_end_internal(mz_zip_archive *pZip, mz_bool set_last_error) {
    mz_zip_internal_state *pState;
    mz_bool status = MZ_TRUE;
    if ((!pZip) || (!pZip->m_pState) || (!pZip->m_pAlloc) || (!pZip->m_pFree) || ((pZip->m_zip_mode != MZ_ZIP_MODE_WRITING) && (pZip->m_zip_mode != MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED))) {
        if (set_last_error)
            mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        return MZ_FALSE;
    }
    pState = pZip->m_pState;
    pZip->m_pState = NULL;
    mz_zip_array_clear(pZip, &pState->m_central_dir);
    mz_zip_array_clear(pZip, &pState->m_central_dir_offsets);
    mz_zip_array_clear(pZip, &pState->m_sorted_central_dir_offsets);
#ifndef MINIZ_NO_STDIO
    if (pState->m_pFile) {
        if (pZip->m_zip_type == MZ_ZIP_TYPE_FILE) {
            if (MZ_FCLOSE(pState->m_pFile) == EOF) {
                if (set_last_error)
                    mz_zip_set_error(pZip, MZ_ZIP_FILE_CLOSE_FAILED);
                status = MZ_FALSE;
            }
        }
        pState->m_pFile = NULL;
    }
#endif
    if ((pZip->m_pWrite == mz_zip_heap_write_func) && (pState->m_pMem)) {
        pZip->m_pFree(pZip->m_pAlloc_opaque, pState->m_pMem);
        pState->m_pMem = NULL;
    }
    pZip->m_pFree(pZip->m_pAlloc_opaque, pState);
    pZip->m_zip_mode = MZ_ZIP_MODE_INVALID;
    return status;
}
mz_bool mz_zip_writer_init_v2(mz_zip_archive *pZip, mz_uint64 existing_size, mz_uint flags) {
    mz_bool zip64 = (flags & MZ_ZIP_FLAG_WRITE_ZIP64) != 0;
    if ((!pZip) || (pZip->m_pState) || (!pZip->m_pWrite) || (pZip->m_zip_mode != MZ_ZIP_MODE_INVALID))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (flags & MZ_ZIP_FLAG_WRITE_ALLOW_READING) {
        if (!pZip->m_pRead)
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    }
    if (pZip->m_file_offset_alignment) {
        if (pZip->m_file_offset_alignment & (pZip->m_file_offset_alignment - 1))
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    }
    if (!pZip->m_pAlloc)
        pZip->m_pAlloc = miniz_def_alloc_func;
    if (!pZip->m_pFree)
        pZip->m_pFree = miniz_def_free_func;
    if (!pZip->m_pRealloc)
        pZip->m_pRealloc = miniz_def_realloc_func;
    pZip->m_archive_size = existing_size;
    pZip->m_central_directory_file_ofs = 0;
    pZip->m_total_files = 0;
    if (NULL == (pZip->m_pState = (mz_zip_internal_state *)pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, sizeof(mz_zip_internal_state))))
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    memset(pZip->m_pState, 0, sizeof(mz_zip_internal_state));
    MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir, sizeof(mz_uint8));
    MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_central_dir_offsets, sizeof(mz_uint32));
    MZ_ZIP_ARRAY_SET_ELEMENT_SIZE(&pZip->m_pState->m_sorted_central_dir_offsets, sizeof(mz_uint32));
    pZip->m_pState->m_zip64 = zip64;
    pZip->m_pState->m_zip64_has_extended_info_fields = zip64;
    pZip->m_zip_type = MZ_ZIP_TYPE_USER;
    pZip->m_zip_mode = MZ_ZIP_MODE_WRITING;
    return MZ_TRUE;
}
mz_bool mz_zip_writer_init(mz_zip_archive *pZip, mz_uint64 existing_size) {
    return mz_zip_writer_init_v2(pZip, existing_size, 0);
}
mz_bool mz_zip_writer_init_heap_v2(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size, mz_uint flags) {
    pZip->m_pWrite = mz_zip_heap_write_func;
    pZip->m_pNeeds_keepalive = NULL;
    if (flags & MZ_ZIP_FLAG_WRITE_ALLOW_READING)
        pZip->m_pRead = mz_zip_mem_read_func;
    pZip->m_pIO_opaque = pZip;
    if (!mz_zip_writer_init_v2(pZip, size_to_reserve_at_beginning, flags))
        return MZ_FALSE;
    pZip->m_zip_type = MZ_ZIP_TYPE_HEAP;
    if (0 != (initial_allocation_size = MZ_MAX(initial_allocation_size, size_to_reserve_at_beginning))) {
        if (NULL == (pZip->m_pState->m_pMem = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, initial_allocation_size))) {
            mz_zip_writer_end_internal(pZip, MZ_FALSE);
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        pZip->m_pState->m_mem_capacity = initial_allocation_size;
    }
    return MZ_TRUE;
}
mz_bool mz_zip_writer_init_heap(mz_zip_archive *pZip, size_t size_to_reserve_at_beginning, size_t initial_allocation_size) {
    return mz_zip_writer_init_heap_v2(pZip, size_to_reserve_at_beginning, initial_allocation_size, 0);
}
#ifndef MINIZ_NO_STDIO
static size_t mz_zip_file_write_func(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    mz_zip_archive *pZip = (mz_zip_archive *)pOpaque;
    mz_int64 cur_ofs = MZ_FTELL64(pZip->m_pState->m_pFile);
    file_ofs += pZip->m_pState->m_file_archive_start_ofs;
    if (((mz_int64)file_ofs < 0) || (((cur_ofs != (mz_int64)file_ofs)) && (MZ_FSEEK64(pZip->m_pState->m_pFile, (mz_int64)file_ofs, SEEK_SET)))) {
        mz_zip_set_error(pZip, MZ_ZIP_FILE_SEEK_FAILED);
        return 0;
    }
    return MZ_FWRITE(pBuf, 1, n, pZip->m_pState->m_pFile);
}
mz_bool mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning) {
    return mz_zip_writer_init_file_v2(pZip, pFilename, size_to_reserve_at_beginning, 0);
}
mz_bool mz_zip_writer_init_file_v2(mz_zip_archive *pZip, const char *pFilename, mz_uint64 size_to_reserve_at_beginning, mz_uint flags) {
    MZ_FILE *pFile;
    pZip->m_pWrite = mz_zip_file_write_func;
    pZip->m_pNeeds_keepalive = NULL;
    if (flags & MZ_ZIP_FLAG_WRITE_ALLOW_READING)
        pZip->m_pRead = mz_zip_file_read_func;
    pZip->m_pIO_opaque = pZip;
    if (!mz_zip_writer_init_v2(pZip, size_to_reserve_at_beginning, flags))
        return MZ_FALSE;
    if (NULL == (pFile = MZ_FOPEN(pFilename, (flags & MZ_ZIP_FLAG_WRITE_ALLOW_READING) ? "w+b" : "wb"))) {
        mz_zip_writer_end(pZip);
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_OPEN_FAILED);
    }
    pZip->m_pState->m_pFile = pFile;
    pZip->m_zip_type = MZ_ZIP_TYPE_FILE;
    if (size_to_reserve_at_beginning) {
        mz_uint64 cur_ofs = 0;
        char buf[4096];
        MZ_CLEAR_ARR(buf);
        do {
            size_t n = (size_t)MZ_MIN(sizeof(buf), size_to_reserve_at_beginning);
            if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_ofs, buf, n) != n) {
                mz_zip_writer_end(pZip);
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
            }
            cur_ofs += n;
            size_to_reserve_at_beginning -= n;
        } while (size_to_reserve_at_beginning);
    }
    return MZ_TRUE;
}
mz_bool mz_zip_writer_init_cfile(mz_zip_archive *pZip, MZ_FILE *pFile, mz_uint flags) {
    pZip->m_pWrite = mz_zip_file_write_func;
    pZip->m_pNeeds_keepalive = NULL;
    if (flags & MZ_ZIP_FLAG_WRITE_ALLOW_READING)
        pZip->m_pRead = mz_zip_file_read_func;
    pZip->m_pIO_opaque = pZip;
    if (!mz_zip_writer_init_v2(pZip, 0, flags))
        return MZ_FALSE;
    pZip->m_pState->m_pFile = pFile;
    pZip->m_pState->m_file_archive_start_ofs = MZ_FTELL64(pZip->m_pState->m_pFile);
    pZip->m_zip_type = MZ_ZIP_TYPE_CFILE;
    return MZ_TRUE;
}
#endif
mz_bool mz_zip_writer_init_from_reader_v2(mz_zip_archive *pZip, const char *pFilename, mz_uint flags) {
    mz_zip_internal_state *pState;
    if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MZ_ZIP_MODE_READING))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (flags & MZ_ZIP_FLAG_WRITE_ZIP64) {
        if (!pZip->m_pState->m_zip64)
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    }
    if (pZip->m_pState->m_zip64) {
        if (pZip->m_total_files == MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    } else {
        if (pZip->m_total_files == MZ_UINT16_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
        if ((pZip->m_archive_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + MZ_ZIP_LOCAL_DIR_HEADER_SIZE) > MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_TOO_LARGE);
    }
    pState = pZip->m_pState;
    if (pState->m_pFile) {
#ifdef MINIZ_NO_STDIO
        (void)pFilename;
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
#else
        if (pZip->m_pIO_opaque != pZip)
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        if (pZip->m_zip_type == MZ_ZIP_TYPE_FILE &&
            !(flags & MZ_ZIP_FLAG_READ_ALLOW_WRITING)) {
            if (!pFilename)
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
            if (NULL == (pState->m_pFile = MZ_FREOPEN(pFilename, "r+b", pState->m_pFile))) {
                mz_zip_reader_end_internal(pZip, MZ_FALSE);
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_OPEN_FAILED);
            }
        }
        pZip->m_pWrite = mz_zip_file_write_func;
        pZip->m_pNeeds_keepalive = NULL;
#endif
    } else if (pState->m_pMem) {
        if (pZip->m_pIO_opaque != pZip)
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        pState->m_mem_capacity = pState->m_mem_size;
        pZip->m_pWrite = mz_zip_heap_write_func;
        pZip->m_pNeeds_keepalive = NULL;
    } else if (!pZip->m_pWrite)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pZip->m_archive_size = pZip->m_central_directory_file_ofs;
    pZip->m_central_directory_file_ofs = 0;
    mz_zip_array_clear(pZip, &pZip->m_pState->m_sorted_central_dir_offsets);
    pZip->m_zip_mode = MZ_ZIP_MODE_WRITING;
    return MZ_TRUE;
}
mz_bool mz_zip_writer_init_from_reader(mz_zip_archive *pZip, const char *pFilename) {
    return mz_zip_writer_init_from_reader_v2(pZip, pFilename, 0);
}
mz_bool mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, mz_uint level_and_flags) {
    return mz_zip_writer_add_mem_ex(pZip, pArchive_name, pBuf, buf_size, NULL, 0, level_and_flags, 0, 0);
}
#define MZ_ZIP64_MAX_LOCAL_EXTRA_FIELD_SIZE (sizeof(mz_uint16) * 2 + sizeof(mz_uint64) * 2)
#define MZ_ZIP64_MAX_CENTRAL_EXTRA_FIELD_SIZE (sizeof(mz_uint16) * 2 + sizeof(mz_uint64) * 3)
static mz_uint32 mz_zip_writer_create_zip64_extra_data(mz_uint8 *pBuf, mz_uint64 *pUncomp_size, mz_uint64 *pComp_size, mz_uint64 *pLocal_header_ofs) {
    mz_uint8 *pDst = pBuf;
    mz_uint32 field_size = 0;
    MZ_WRITE_LE16(pDst + 0, MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID);
    MZ_WRITE_LE16(pDst + 2, 0);
    pDst += sizeof(mz_uint16) * 2;
    if (pUncomp_size) {
        MZ_WRITE_LE64(pDst, *pUncomp_size);
        pDst += sizeof(mz_uint64);
        field_size += sizeof(mz_uint64);
    }
    if (pComp_size) {
        MZ_WRITE_LE64(pDst, *pComp_size);
        pDst += sizeof(mz_uint64);
        field_size += sizeof(mz_uint64);
    }
    if (pLocal_header_ofs) {
        MZ_WRITE_LE64(pDst, *pLocal_header_ofs);
        pDst += sizeof(mz_uint64);
        field_size += sizeof(mz_uint64);
    }
    MZ_WRITE_LE16(pBuf + 2, field_size);
    return (mz_uint32)(pDst - pBuf);
}
static mz_bool mz_zip_writer_create_local_dir_header(mz_zip_archive *pZip, mz_uint8 *pDst, mz_uint16 filename_size, mz_uint16 extra_size, mz_uint64 uncomp_size, mz_uint64 comp_size, mz_uint32 uncomp_crc32, mz_uint16 method, mz_uint16 bit_flags, mz_uint16 dos_time, mz_uint16 dos_date) {
    (void)pZip;
    memset(pDst, 0, MZ_ZIP_LOCAL_DIR_HEADER_SIZE);
    MZ_WRITE_LE32(pDst + MZ_ZIP_LDH_SIG_OFS, MZ_ZIP_LOCAL_DIR_HEADER_SIG);
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_VERSION_NEEDED_OFS, method ? 20 : 0);
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_BIT_FLAG_OFS, bit_flags);
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_METHOD_OFS, method);
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_FILE_TIME_OFS, dos_time);
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_FILE_DATE_OFS, dos_date);
    MZ_WRITE_LE32(pDst + MZ_ZIP_LDH_CRC32_OFS, uncomp_crc32);
    MZ_WRITE_LE32(pDst + MZ_ZIP_LDH_COMPRESSED_SIZE_OFS, MZ_MIN(comp_size, MZ_UINT32_MAX));
    MZ_WRITE_LE32(pDst + MZ_ZIP_LDH_DECOMPRESSED_SIZE_OFS, MZ_MIN(uncomp_size, MZ_UINT32_MAX));
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_FILENAME_LEN_OFS, filename_size);
    MZ_WRITE_LE16(pDst + MZ_ZIP_LDH_EXTRA_LEN_OFS, extra_size);
    return MZ_TRUE;
}
static mz_bool mz_zip_writer_create_central_dir_header(mz_zip_archive *pZip, mz_uint8 *pDst,
                                                       mz_uint16 filename_size, mz_uint16 extra_size, mz_uint16 comment_size,
                                                       mz_uint64 uncomp_size, mz_uint64 comp_size, mz_uint32 uncomp_crc32,
                                                       mz_uint16 method, mz_uint16 bit_flags, mz_uint16 dos_time, mz_uint16 dos_date,
                                                       mz_uint64 local_header_ofs, mz_uint32 ext_attributes) {
    (void)pZip;
    memset(pDst, 0, MZ_ZIP_CENTRAL_DIR_HEADER_SIZE);
    MZ_WRITE_LE32(pDst + MZ_ZIP_CDH_SIG_OFS, MZ_ZIP_CENTRAL_DIR_HEADER_SIG);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_VERSION_NEEDED_OFS, method ? 20 : 0);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_BIT_FLAG_OFS, bit_flags);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_METHOD_OFS, method);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_FILE_TIME_OFS, dos_time);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_FILE_DATE_OFS, dos_date);
    MZ_WRITE_LE32(pDst + MZ_ZIP_CDH_CRC32_OFS, uncomp_crc32);
    MZ_WRITE_LE32(pDst + MZ_ZIP_CDH_COMPRESSED_SIZE_OFS, MZ_MIN(comp_size, MZ_UINT32_MAX));
    MZ_WRITE_LE32(pDst + MZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS, MZ_MIN(uncomp_size, MZ_UINT32_MAX));
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_FILENAME_LEN_OFS, filename_size);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_EXTRA_LEN_OFS, extra_size);
    MZ_WRITE_LE16(pDst + MZ_ZIP_CDH_COMMENT_LEN_OFS, comment_size);
    MZ_WRITE_LE32(pDst + MZ_ZIP_CDH_EXTERNAL_ATTR_OFS, ext_attributes);
    MZ_WRITE_LE32(pDst + MZ_ZIP_CDH_LOCAL_HEADER_OFS, MZ_MIN(local_header_ofs, MZ_UINT32_MAX));
    return MZ_TRUE;
}
static mz_bool mz_zip_writer_add_to_central_dir(mz_zip_archive *pZip, const char *pFilename, mz_uint16 filename_size,
                                                const void *pExtra, mz_uint16 extra_size, const void *pComment, mz_uint16 comment_size,
                                                mz_uint64 uncomp_size, mz_uint64 comp_size, mz_uint32 uncomp_crc32,
                                                mz_uint16 method, mz_uint16 bit_flags, mz_uint16 dos_time, mz_uint16 dos_date,
                                                mz_uint64 local_header_ofs, mz_uint32 ext_attributes,
                                                const char *user_extra_data, mz_uint user_extra_data_len) {
    mz_zip_internal_state *pState = pZip->m_pState;
    mz_uint32 central_dir_ofs = (mz_uint32)pState->m_central_dir.m_size;
    size_t orig_central_dir_size = pState->m_central_dir.m_size;
    mz_uint8 central_dir_header[MZ_ZIP_CENTRAL_DIR_HEADER_SIZE];
    if (!pZip->m_pState->m_zip64) {
        if (local_header_ofs > 0xFFFFFFFF)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_TOO_LARGE);
    }
    if (((mz_uint64)pState->m_central_dir.m_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + filename_size + extra_size + user_extra_data_len + comment_size) >= MZ_UINT32_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_CDIR_SIZE);
    if (!mz_zip_writer_create_central_dir_header(pZip, central_dir_header, filename_size, (mz_uint16)(extra_size + user_extra_data_len), comment_size, uncomp_size, comp_size, uncomp_crc32, method, bit_flags, dos_time, dos_date, local_header_ofs, ext_attributes))
        return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
    if ((!mz_zip_array_push_back(pZip, &pState->m_central_dir, central_dir_header, MZ_ZIP_CENTRAL_DIR_HEADER_SIZE)) ||
        (!mz_zip_array_push_back(pZip, &pState->m_central_dir, pFilename, filename_size)) ||
        (!mz_zip_array_push_back(pZip, &pState->m_central_dir, pExtra, extra_size)) ||
        (!mz_zip_array_push_back(pZip, &pState->m_central_dir, user_extra_data, user_extra_data_len)) ||
        (!mz_zip_array_push_back(pZip, &pState->m_central_dir, pComment, comment_size)) ||
        (!mz_zip_array_push_back(pZip, &pState->m_central_dir_offsets, &central_dir_ofs, 1))) {
        mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    }
    return MZ_TRUE;
}
static mz_bool mz_zip_writer_validate_archive_name(const char *pArchive_name) {
    if (*pArchive_name == '/')
        return MZ_FALSE;
    return MZ_TRUE;
}
static mz_uint mz_zip_writer_compute_padding_needed_for_file_alignment(mz_zip_archive *pZip) {
    mz_uint32 n;
    if (!pZip->m_file_offset_alignment)
        return 0;
    n = (mz_uint32)(pZip->m_archive_size & (pZip->m_file_offset_alignment - 1));
    return (mz_uint)((pZip->m_file_offset_alignment - n) & (pZip->m_file_offset_alignment - 1));
}
static mz_bool mz_zip_writer_write_zeros(mz_zip_archive *pZip, mz_uint64 cur_file_ofs, mz_uint32 n) {
    char buf[4096];
    memset(buf, 0, MZ_MIN(sizeof(buf), n));
    while (n) {
        mz_uint32 s = MZ_MIN(sizeof(buf), n);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_file_ofs, buf, s) != s)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_file_ofs += s;
        n -= s;
    }
    return MZ_TRUE;
}
mz_bool mz_zip_writer_add_mem_ex(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags,
                                 mz_uint64 uncomp_size, mz_uint32 uncomp_crc32) {
    return mz_zip_writer_add_mem_ex_v2(pZip, pArchive_name, pBuf, buf_size, pComment, comment_size, level_and_flags, uncomp_size, uncomp_crc32, NULL, NULL, 0, NULL, 0);
}
mz_bool mz_zip_writer_add_mem_ex_v2(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size,
                                    mz_uint level_and_flags, mz_uint64 uncomp_size, mz_uint32 uncomp_crc32, MZ_TIME_T *last_modified,
                                    const char *user_extra_data, mz_uint user_extra_data_len, const char *user_extra_data_central, mz_uint user_extra_data_central_len) {
    mz_uint16 method = 0, dos_time = 0, dos_date = 0;
    mz_uint ext_attributes = 0, num_alignment_padding_bytes;
    mz_uint64 local_dir_header_ofs = pZip->m_archive_size, cur_archive_file_ofs = pZip->m_archive_size, comp_size = 0;
    size_t archive_name_size;
    mz_uint8 local_dir_header[MZ_ZIP_LOCAL_DIR_HEADER_SIZE];
    mz_zip_internal_state *pState;
    mz_uint8 *pExtra_data = NULL;
    mz_uint32 extra_size = 0;
    mz_uint8 extra_data[MZ_ZIP64_MAX_CENTRAL_EXTRA_FIELD_SIZE];
    mz_uint16 bit_flags = 0;
    if ((int)level_and_flags < 0)
        level_and_flags = MZ_DEFAULT_LEVEL;
    if (uncomp_size || (buf_size && !(level_and_flags & MZ_ZIP_FLAG_COMPRESSED_DATA)))
        bit_flags |= MZ_ZIP_LDH_BIT_FLAG_HAS_LOCATOR;
    if (!(level_and_flags & MZ_ZIP_FLAG_ASCII_FILENAME))
        bit_flags |= MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_UTF8;
    if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MZ_ZIP_MODE_WRITING) || ((buf_size) && (!pBuf)) || (!pArchive_name) || ((comment_size) && (!pComment)) || ((level_and_flags & 0xF) > MZ_UBER_COMPRESSION))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pState = pZip->m_pState;
    if (pState->m_zip64) {
        if (pZip->m_total_files == MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    } else {
        if (pZip->m_total_files == MZ_UINT16_MAX) {
            pState->m_zip64 = MZ_TRUE;
        }
        if (((mz_uint64)buf_size > 0xFFFFFFFF) || (uncomp_size > 0xFFFFFFFF)) {
            pState->m_zip64 = MZ_TRUE;
        }
    }
    if ((!(level_and_flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) && (uncomp_size))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (!mz_zip_writer_validate_archive_name(pArchive_name))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_FILENAME);
#ifndef MINIZ_NO_TIME
    if (last_modified != NULL) {
        mz_zip_time_t_to_dos_time(*last_modified, &dos_time, &dos_date);
    } else {
        MZ_TIME_T cur_time;
        time(&cur_time);
        mz_zip_time_t_to_dos_time(cur_time, &dos_time, &dos_date);
    }
#else
    (void)last_modified;
#endif
    if (!(level_and_flags & MZ_ZIP_FLAG_COMPRESSED_DATA)) {
        uncomp_crc32 = (mz_uint32)mz_crc32(MZ_CRC32_INIT, (const mz_uint8 *)pBuf, buf_size);
        uncomp_size = buf_size;
    }
    archive_name_size = strlen(pArchive_name);
    if (archive_name_size > MZ_UINT16_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_FILENAME);
    num_alignment_padding_bytes = mz_zip_writer_compute_padding_needed_for_file_alignment(pZip);
    if (((mz_uint64)pState->m_central_dir.m_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + archive_name_size + MZ_ZIP64_MAX_CENTRAL_EXTRA_FIELD_SIZE + comment_size) >= MZ_UINT32_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_CDIR_SIZE);
    if (!pState->m_zip64) {
        if ((pZip->m_archive_size + num_alignment_padding_bytes + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + archive_name_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + archive_name_size + comment_size + user_extra_data_len +
             pState->m_central_dir.m_size + MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE + user_extra_data_central_len + MZ_ZIP_DATA_DESCRIPTER_SIZE32) > 0xFFFFFFFF) {
            pState->m_zip64 = MZ_TRUE;
        }
    }
    if ((archive_name_size) && (pArchive_name[archive_name_size - 1] == '/')) {
        ext_attributes |= MZ_ZIP_DOS_DIR_ATTRIBUTE_BITFLAG;
        if ((buf_size) || (uncomp_size))
            return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    }
    if ((!mz_zip_array_ensure_room(pZip, &pState->m_central_dir, MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + archive_name_size + comment_size + (pState->m_zip64 ? MZ_ZIP64_MAX_CENTRAL_EXTRA_FIELD_SIZE : 0))) || (!mz_zip_array_ensure_room(pZip, &pState->m_central_dir_offsets, 1)))
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    if (!mz_zip_writer_write_zeros(pZip, cur_archive_file_ofs, num_alignment_padding_bytes)) {
        return MZ_FALSE;
    }
    local_dir_header_ofs += num_alignment_padding_bytes;
    if (pZip->m_file_offset_alignment) {
        MZ_ASSERT((local_dir_header_ofs & (pZip->m_file_offset_alignment - 1)) == 0);
    }
    cur_archive_file_ofs += num_alignment_padding_bytes;
    MZ_CLEAR_ARR(local_dir_header);
    if (level_and_flags & MZ_ZIP_FLAG_COMPRESSED_DATA) {
        method = MZ_DEFLATED;
    }
    if (pState->m_zip64) {
        if (uncomp_size >= MZ_UINT32_MAX || local_dir_header_ofs >= MZ_UINT32_MAX) {
            pExtra_data = extra_data;
            extra_size = mz_zip_writer_create_zip64_extra_data(extra_data, (uncomp_size >= MZ_UINT32_MAX) ? &uncomp_size : NULL,
                                                               (uncomp_size >= MZ_UINT32_MAX) ? &comp_size : NULL, (local_dir_header_ofs >= MZ_UINT32_MAX) ? &local_dir_header_ofs : NULL);
        }
        if (!mz_zip_writer_create_local_dir_header(pZip, local_dir_header, (mz_uint16)archive_name_size, (mz_uint16)(extra_size + user_extra_data_len), 0, 0, 0, method, bit_flags, dos_time, dos_date))
            return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, local_dir_header_ofs, local_dir_header, sizeof(local_dir_header)) != sizeof(local_dir_header))
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += sizeof(local_dir_header);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pArchive_name, archive_name_size) != archive_name_size) {
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        }
        cur_archive_file_ofs += archive_name_size;
        if (pExtra_data != NULL) {
            if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, extra_data, extra_size) != extra_size)
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
            cur_archive_file_ofs += extra_size;
        }
    } else {
        if ((comp_size > MZ_UINT32_MAX) || (cur_archive_file_ofs > MZ_UINT32_MAX))
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
        if (!mz_zip_writer_create_local_dir_header(pZip, local_dir_header, (mz_uint16)archive_name_size, (mz_uint16)user_extra_data_len, 0, 0, 0, method, bit_flags, dos_time, dos_date))
            return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, local_dir_header_ofs, local_dir_header, sizeof(local_dir_header)) != sizeof(local_dir_header))
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += sizeof(local_dir_header);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pArchive_name, archive_name_size) != archive_name_size) {
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        }
        cur_archive_file_ofs += archive_name_size;
    }
    if (user_extra_data_len > 0) {
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, user_extra_data, user_extra_data_len) != user_extra_data_len)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += user_extra_data_len;
    }
    if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pBuf, buf_size) != buf_size) {
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
    }
    cur_archive_file_ofs += buf_size;
    comp_size = buf_size;
    if (uncomp_size) {
        mz_uint8 local_dir_footer[MZ_ZIP_DATA_DESCRIPTER_SIZE64];
        mz_uint32 local_dir_footer_size = MZ_ZIP_DATA_DESCRIPTER_SIZE32;
        MZ_ASSERT(bit_flags & MZ_ZIP_LDH_BIT_FLAG_HAS_LOCATOR);
        MZ_WRITE_LE32(local_dir_footer + 0, MZ_ZIP_DATA_DESCRIPTOR_ID);
        MZ_WRITE_LE32(local_dir_footer + 4, uncomp_crc32);
        if (pExtra_data == NULL) {
            if (comp_size > MZ_UINT32_MAX)
                return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
            MZ_WRITE_LE32(local_dir_footer + 8, comp_size);
            MZ_WRITE_LE32(local_dir_footer + 12, uncomp_size);
        } else {
            MZ_WRITE_LE64(local_dir_footer + 8, comp_size);
            MZ_WRITE_LE64(local_dir_footer + 16, uncomp_size);
            local_dir_footer_size = MZ_ZIP_DATA_DESCRIPTER_SIZE64;
        }
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, local_dir_footer, local_dir_footer_size) != local_dir_footer_size)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += local_dir_footer_size;
    }
    if (pExtra_data != NULL) {
        extra_size = mz_zip_writer_create_zip64_extra_data(extra_data, (uncomp_size >= MZ_UINT32_MAX) ? &uncomp_size : NULL,
                                                           (uncomp_size >= MZ_UINT32_MAX) ? &comp_size : NULL, (local_dir_header_ofs >= MZ_UINT32_MAX) ? &local_dir_header_ofs : NULL);
    }
    if (!mz_zip_writer_add_to_central_dir(pZip, pArchive_name, (mz_uint16)archive_name_size, pExtra_data, (mz_uint16)extra_size, pComment,
                                          comment_size, uncomp_size, comp_size, uncomp_crc32, method, bit_flags, dos_time, dos_date, local_dir_header_ofs, ext_attributes,
                                          user_extra_data_central, user_extra_data_central_len))
        return MZ_FALSE;
    pZip->m_total_files++;
    pZip->m_archive_size = cur_archive_file_ofs;
    return MZ_TRUE;
}
mz_bool mz_zip_writer_add_read_buf_callback(mz_zip_archive *pZip, const char *pArchive_name, mz_file_read_func read_callback, void *callback_opaque, mz_uint64 max_size, const MZ_TIME_T *pFile_time, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags,
                                            const char *user_extra_data, mz_uint user_extra_data_len, const char *user_extra_data_central, mz_uint user_extra_data_central_len) {
    mz_uint16 gen_flags;
    mz_uint uncomp_crc32 = MZ_CRC32_INIT, num_alignment_padding_bytes;
    mz_uint16 method = 0, dos_time = 0, dos_date = 0, ext_attributes = 0;
    mz_uint64 local_dir_header_ofs, cur_archive_file_ofs = pZip->m_archive_size, uncomp_size = 0, comp_size = 0;
    size_t archive_name_size;
    mz_uint8 local_dir_header[MZ_ZIP_LOCAL_DIR_HEADER_SIZE];
    mz_uint8 *pExtra_data = NULL;
    mz_uint32 extra_size = 0;
    mz_uint8 extra_data[MZ_ZIP64_MAX_CENTRAL_EXTRA_FIELD_SIZE];
    mz_zip_internal_state *pState;
    mz_uint64 file_ofs = 0, cur_archive_header_file_ofs;
    if ((int)level_and_flags < 0)
        level_and_flags = MZ_DEFAULT_LEVEL;
    gen_flags = (level_and_flags & MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE) ? 0 : MZ_ZIP_LDH_BIT_FLAG_HAS_LOCATOR;
    if (!(level_and_flags & MZ_ZIP_FLAG_ASCII_FILENAME))
        gen_flags |= MZ_ZIP_GENERAL_PURPOSE_BIT_FLAG_UTF8;
    if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MZ_ZIP_MODE_WRITING) || (!pArchive_name) || ((comment_size) && (!pComment)) || ((level_and_flags & 0xF) > MZ_UBER_COMPRESSION))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pState = pZip->m_pState;
    if ((!pState->m_zip64) && (max_size > MZ_UINT32_MAX)) {
        pState->m_zip64 = MZ_TRUE;
    }
    if (level_and_flags & MZ_ZIP_FLAG_COMPRESSED_DATA)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (!mz_zip_writer_validate_archive_name(pArchive_name))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_FILENAME);
    if (pState->m_zip64) {
        if (pZip->m_total_files == MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    } else {
        if (pZip->m_total_files == MZ_UINT16_MAX) {
            pState->m_zip64 = MZ_TRUE;
        }
    }
    archive_name_size = strlen(pArchive_name);
    if (archive_name_size > MZ_UINT16_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_FILENAME);
    num_alignment_padding_bytes = mz_zip_writer_compute_padding_needed_for_file_alignment(pZip);
    if (((mz_uint64)pState->m_central_dir.m_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + archive_name_size + MZ_ZIP64_MAX_CENTRAL_EXTRA_FIELD_SIZE + comment_size) >= MZ_UINT32_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_CDIR_SIZE);
    if (!pState->m_zip64) {
        if ((pZip->m_archive_size + num_alignment_padding_bytes + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + archive_name_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + archive_name_size + comment_size + user_extra_data_len + pState->m_central_dir.m_size + MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE + 1024 + MZ_ZIP_DATA_DESCRIPTER_SIZE32 + user_extra_data_central_len) > 0xFFFFFFFF) {
            pState->m_zip64 = MZ_TRUE;
        }
    }
#ifndef MINIZ_NO_TIME
    if (pFile_time) {
        mz_zip_time_t_to_dos_time(*pFile_time, &dos_time, &dos_date);
    }
#else
    (void)pFile_time;
#endif
    if (!mz_zip_writer_write_zeros(pZip, cur_archive_file_ofs, num_alignment_padding_bytes)) {
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
    }
    cur_archive_file_ofs += num_alignment_padding_bytes;
    local_dir_header_ofs = cur_archive_file_ofs;
    if (pZip->m_file_offset_alignment) {
        MZ_ASSERT((cur_archive_file_ofs & (pZip->m_file_offset_alignment - 1)) == 0);
    }
    MZ_CLEAR_ARR(local_dir_header);
    if (pState->m_zip64) {
        if (max_size >= MZ_UINT32_MAX || local_dir_header_ofs >= MZ_UINT32_MAX) {
            pExtra_data = extra_data;
            if (level_and_flags & MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE)
                extra_size = mz_zip_writer_create_zip64_extra_data(extra_data, (max_size >= MZ_UINT32_MAX) ? &uncomp_size : NULL,
                                                                   (max_size >= MZ_UINT32_MAX) ? &comp_size : NULL,
                                                                   (local_dir_header_ofs >= MZ_UINT32_MAX) ? &local_dir_header_ofs : NULL);
            else
                extra_size = mz_zip_writer_create_zip64_extra_data(extra_data, NULL,
                                                                   NULL,
                                                                   (local_dir_header_ofs >= MZ_UINT32_MAX) ? &local_dir_header_ofs : NULL);
        }
        if (!mz_zip_writer_create_local_dir_header(pZip, local_dir_header, (mz_uint16)archive_name_size, (mz_uint16)(extra_size + user_extra_data_len), 0, 0, 0, method, gen_flags, dos_time, dos_date))
            return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, local_dir_header, sizeof(local_dir_header)) != sizeof(local_dir_header))
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += sizeof(local_dir_header);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pArchive_name, archive_name_size) != archive_name_size) {
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        }
        cur_archive_file_ofs += archive_name_size;
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, extra_data, extra_size) != extra_size)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += extra_size;
    } else {
        if ((comp_size > MZ_UINT32_MAX) || (cur_archive_file_ofs > MZ_UINT32_MAX))
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
        if (!mz_zip_writer_create_local_dir_header(pZip, local_dir_header, (mz_uint16)archive_name_size, (mz_uint16)user_extra_data_len, 0, 0, 0, method, gen_flags, dos_time, dos_date))
            return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, local_dir_header, sizeof(local_dir_header)) != sizeof(local_dir_header))
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += sizeof(local_dir_header);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pArchive_name, archive_name_size) != archive_name_size) {
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        }
        cur_archive_file_ofs += archive_name_size;
    }
    if (user_extra_data_len > 0) {
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, user_extra_data, user_extra_data_len) != user_extra_data_len)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += user_extra_data_len;
    }
    if (max_size) {
        void *pRead_buf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, MZ_ZIP_MAX_IO_BUF_SIZE);
        if (!pRead_buf) {
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        while (1) {
            size_t n = read_callback(callback_opaque, file_ofs, pRead_buf, MZ_ZIP_MAX_IO_BUF_SIZE);
            if (n == 0)
                break;
            if ((n > MZ_ZIP_MAX_IO_BUF_SIZE) || (file_ofs + n > max_size)) {
                pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
            }
            if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, pRead_buf, n) != n) {
                pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
            }
            file_ofs += n;
            uncomp_crc32 = (mz_uint32)mz_crc32(uncomp_crc32, (const mz_uint8 *)pRead_buf, n);
            cur_archive_file_ofs += n;
        }
        uncomp_size = file_ofs;
        comp_size = uncomp_size;
        pZip->m_pFree(pZip->m_pAlloc_opaque, pRead_buf);
    }
    if (!(level_and_flags & MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE)) {
        mz_uint8 local_dir_footer[MZ_ZIP_DATA_DESCRIPTER_SIZE64];
        mz_uint32 local_dir_footer_size = MZ_ZIP_DATA_DESCRIPTER_SIZE32;
        MZ_WRITE_LE32(local_dir_footer + 0, MZ_ZIP_DATA_DESCRIPTOR_ID);
        MZ_WRITE_LE32(local_dir_footer + 4, uncomp_crc32);
        if (pExtra_data == NULL) {
            if (comp_size > MZ_UINT32_MAX)
                return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
            MZ_WRITE_LE32(local_dir_footer + 8, comp_size);
            MZ_WRITE_LE32(local_dir_footer + 12, uncomp_size);
        } else {
            MZ_WRITE_LE64(local_dir_footer + 8, comp_size);
            MZ_WRITE_LE64(local_dir_footer + 16, uncomp_size);
            local_dir_footer_size = MZ_ZIP_DATA_DESCRIPTER_SIZE64;
        }
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_file_ofs, local_dir_footer, local_dir_footer_size) != local_dir_footer_size)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        cur_archive_file_ofs += local_dir_footer_size;
    }
    if (level_and_flags & MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE) {
        if (pExtra_data != NULL) {
            extra_size = mz_zip_writer_create_zip64_extra_data(extra_data, (max_size >= MZ_UINT32_MAX) ? &uncomp_size : NULL,
                                                               (max_size >= MZ_UINT32_MAX) ? &comp_size : NULL, (local_dir_header_ofs >= MZ_UINT32_MAX) ? &local_dir_header_ofs : NULL);
        }
        if (!mz_zip_writer_create_local_dir_header(pZip, local_dir_header,
                                                   (mz_uint16)archive_name_size, (mz_uint16)(extra_size + user_extra_data_len),
                                                   (max_size >= MZ_UINT32_MAX) ? MZ_UINT32_MAX : uncomp_size,
                                                   (max_size >= MZ_UINT32_MAX) ? MZ_UINT32_MAX : comp_size,
                                                   uncomp_crc32, method, gen_flags, dos_time, dos_date))
            return mz_zip_set_error(pZip, MZ_ZIP_INTERNAL_ERROR);
        cur_archive_header_file_ofs = local_dir_header_ofs;
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_header_file_ofs, local_dir_header, sizeof(local_dir_header)) != sizeof(local_dir_header))
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        if (pExtra_data != NULL) {
            cur_archive_header_file_ofs += sizeof(local_dir_header);
            if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_header_file_ofs, pArchive_name, archive_name_size) != archive_name_size) {
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
            }
            cur_archive_header_file_ofs += archive_name_size;
            if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_archive_header_file_ofs, extra_data, extra_size) != extra_size)
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
            cur_archive_header_file_ofs += extra_size;
        }
    }
    if (pExtra_data != NULL) {
        extra_size = mz_zip_writer_create_zip64_extra_data(extra_data, (uncomp_size >= MZ_UINT32_MAX) ? &uncomp_size : NULL,
                                                           (uncomp_size >= MZ_UINT32_MAX) ? &comp_size : NULL, (local_dir_header_ofs >= MZ_UINT32_MAX) ? &local_dir_header_ofs : NULL);
    }
    if (!mz_zip_writer_add_to_central_dir(pZip, pArchive_name, (mz_uint16)archive_name_size, pExtra_data, (mz_uint16)extra_size, pComment, comment_size,
                                          uncomp_size, comp_size, uncomp_crc32, method, gen_flags, dos_time, dos_date, local_dir_header_ofs, ext_attributes,
                                          user_extra_data_central, user_extra_data_central_len))
        return MZ_FALSE;
    pZip->m_total_files++;
    pZip->m_archive_size = cur_archive_file_ofs;
    return MZ_TRUE;
}
#ifndef MINIZ_NO_STDIO
static size_t mz_file_read_func_stdio(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
    MZ_FILE *pSrc_file = (MZ_FILE *)pOpaque;
    mz_int64 cur_ofs = MZ_FTELL64(pSrc_file);
    if (((mz_int64)file_ofs < 0) || (((cur_ofs != (mz_int64)file_ofs)) && (MZ_FSEEK64(pSrc_file, (mz_int64)file_ofs, SEEK_SET))))
        return 0;
    return MZ_FREAD(pBuf, 1, n, pSrc_file);
}
mz_bool mz_zip_writer_add_cfile(mz_zip_archive *pZip, const char *pArchive_name, MZ_FILE *pSrc_file, mz_uint64 max_size, const MZ_TIME_T *pFile_time, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags,
                                const char *user_extra_data, mz_uint user_extra_data_len, const char *user_extra_data_central, mz_uint user_extra_data_central_len) {
    return mz_zip_writer_add_read_buf_callback(pZip, pArchive_name, mz_file_read_func_stdio, pSrc_file, max_size, pFile_time, pComment, comment_size, level_and_flags,
                                               user_extra_data, user_extra_data_len, user_extra_data_central, user_extra_data_central_len);
}
mz_bool mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags) {
    MZ_FILE *pSrc_file = NULL;
    mz_uint64 uncomp_size = 0;
    MZ_TIME_T file_modified_time;
    MZ_TIME_T *pFile_time = NULL;
    mz_bool status;
    memset(&file_modified_time, 0, sizeof(file_modified_time));
#if !defined(MINIZ_NO_TIME) && !defined(MINIZ_NO_STDIO)
    pFile_time = &file_modified_time;
    if (!mz_zip_get_file_modified_time(pSrc_filename, &file_modified_time))
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_STAT_FAILED);
#endif
    pSrc_file = MZ_FOPEN(pSrc_filename, "rb");
    if (!pSrc_file)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_OPEN_FAILED);
    MZ_FSEEK64(pSrc_file, 0, SEEK_END);
    uncomp_size = MZ_FTELL64(pSrc_file);
    MZ_FSEEK64(pSrc_file, 0, SEEK_SET);
    status = mz_zip_writer_add_cfile(pZip, pArchive_name, pSrc_file, uncomp_size, pFile_time, pComment, comment_size, level_and_flags, NULL, 0, NULL, 0);
    MZ_FCLOSE(pSrc_file);
    return status;
}
#endif
static mz_bool mz_zip_writer_update_zip64_extension_block(mz_zip_array *pNew_ext, mz_zip_archive *pZip, const mz_uint8 *pExt, mz_uint32 ext_len, mz_uint64 *pComp_size, mz_uint64 *pUncomp_size, mz_uint64 *pLocal_header_ofs, mz_uint32 *pDisk_start) {
    if (!mz_zip_array_reserve(pZip, pNew_ext, ext_len + 64, MZ_FALSE))
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    mz_zip_array_resize(pZip, pNew_ext, 0, MZ_FALSE);
    if ((pUncomp_size) || (pComp_size) || (pLocal_header_ofs) || (pDisk_start)) {
        mz_uint8 new_ext_block[64];
        mz_uint8 *pDst = new_ext_block;
        mz_write_le16(pDst, MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID);
        mz_write_le16(pDst + sizeof(mz_uint16), 0);
        pDst += sizeof(mz_uint16) * 2;
        if (pUncomp_size) {
            mz_write_le64(pDst, *pUncomp_size);
            pDst += sizeof(mz_uint64);
        }
        if (pComp_size) {
            mz_write_le64(pDst, *pComp_size);
            pDst += sizeof(mz_uint64);
        }
        if (pLocal_header_ofs) {
            mz_write_le64(pDst, *pLocal_header_ofs);
            pDst += sizeof(mz_uint64);
        }
        if (pDisk_start) {
            mz_write_le32(pDst, *pDisk_start);
            pDst += sizeof(mz_uint32);
        }
        mz_write_le16(new_ext_block + sizeof(mz_uint16), (mz_uint16)((pDst - new_ext_block) - sizeof(mz_uint16) * 2));
        if (!mz_zip_array_push_back(pZip, pNew_ext, new_ext_block, pDst - new_ext_block))
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    }
    if ((pExt) && (ext_len)) {
        mz_uint32 extra_size_remaining = ext_len;
        const mz_uint8 *pExtra_data = pExt;
        do {
            mz_uint32 field_id, field_data_size, field_total_size;
            if (extra_size_remaining < (sizeof(mz_uint16) * 2))
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            field_id = MZ_READ_LE16(pExtra_data);
            field_data_size = MZ_READ_LE16(pExtra_data + sizeof(mz_uint16));
            field_total_size = field_data_size + sizeof(mz_uint16) * 2;
            if (field_total_size > extra_size_remaining)
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            if (field_id != MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID) {
                if (!mz_zip_array_push_back(pZip, pNew_ext, pExtra_data, field_total_size))
                    return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
            }
            pExtra_data += field_total_size;
            extra_size_remaining -= field_total_size;
        } while (extra_size_remaining);
    }
    return MZ_TRUE;
}
mz_bool mz_zip_writer_add_from_zip_reader(mz_zip_archive *pZip, mz_zip_archive *pSource_zip, mz_uint src_file_index) {
    mz_uint n, bit_flags, num_alignment_padding_bytes, src_central_dir_following_data_size;
    mz_uint64 src_archive_bytes_remaining, local_dir_header_ofs;
    mz_uint64 cur_src_file_ofs, cur_dst_file_ofs;
    mz_uint32 local_header_u32[(MZ_ZIP_LOCAL_DIR_HEADER_SIZE + sizeof(mz_uint32) - 1) / sizeof(mz_uint32)];
    mz_uint8 *pLocal_header = (mz_uint8 *)local_header_u32;
    mz_uint8 new_central_header[MZ_ZIP_CENTRAL_DIR_HEADER_SIZE];
    size_t orig_central_dir_size;
    mz_zip_internal_state *pState;
    void *pBuf;
    const mz_uint8 *pSrc_central_header;
    mz_zip_archive_file_stat src_file_stat;
    mz_uint32 src_filename_len, src_comment_len, src_ext_len;
    mz_uint32 local_header_filename_size, local_header_extra_len;
    mz_uint64 local_header_comp_size, local_header_uncomp_size;
    mz_bool found_zip64_ext_data_in_ldir = MZ_FALSE;
    if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MZ_ZIP_MODE_WRITING) || (!pSource_zip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pState = pZip->m_pState;
    if ((pSource_zip->m_pState->m_zip64) && (!pZip->m_pState->m_zip64))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (NULL == (pSrc_central_header = mz_zip_get_cdh(pSource_zip, src_file_index)))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (MZ_READ_LE32(pSrc_central_header + MZ_ZIP_CDH_SIG_OFS) != MZ_ZIP_CENTRAL_DIR_HEADER_SIG)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    src_filename_len = MZ_READ_LE16(pSrc_central_header + MZ_ZIP_CDH_FILENAME_LEN_OFS);
    src_comment_len = MZ_READ_LE16(pSrc_central_header + MZ_ZIP_CDH_COMMENT_LEN_OFS);
    src_ext_len = MZ_READ_LE16(pSrc_central_header + MZ_ZIP_CDH_EXTRA_LEN_OFS);
    src_central_dir_following_data_size = src_filename_len + src_ext_len + src_comment_len;
    if ((pState->m_central_dir.m_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + src_central_dir_following_data_size + 32) >= MZ_UINT32_MAX)
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_CDIR_SIZE);
    num_alignment_padding_bytes = mz_zip_writer_compute_padding_needed_for_file_alignment(pZip);
    if (!pState->m_zip64) {
        if (pZip->m_total_files == MZ_UINT16_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    } else {
        if (pZip->m_total_files == MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    }
    if (!mz_zip_file_stat_internal(pSource_zip, src_file_index, pSrc_central_header, &src_file_stat, NULL))
        return MZ_FALSE;
    cur_src_file_ofs = src_file_stat.m_local_header_ofs;
    cur_dst_file_ofs = pZip->m_archive_size;
    if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs, pLocal_header, MZ_ZIP_LOCAL_DIR_HEADER_SIZE) != MZ_ZIP_LOCAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
    if (MZ_READ_LE32(pLocal_header) != MZ_ZIP_LOCAL_DIR_HEADER_SIG)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
    cur_src_file_ofs += MZ_ZIP_LOCAL_DIR_HEADER_SIZE;
    local_header_filename_size = MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_FILENAME_LEN_OFS);
    local_header_extra_len = MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_EXTRA_LEN_OFS);
    local_header_comp_size = MZ_READ_LE32(pLocal_header + MZ_ZIP_LDH_COMPRESSED_SIZE_OFS);
    local_header_uncomp_size = MZ_READ_LE32(pLocal_header + MZ_ZIP_LDH_DECOMPRESSED_SIZE_OFS);
    src_archive_bytes_remaining = src_file_stat.m_comp_size + local_header_filename_size + local_header_extra_len;
    if ((local_header_extra_len) && ((local_header_comp_size == MZ_UINT32_MAX) || (local_header_uncomp_size == MZ_UINT32_MAX))) {
        mz_zip_array file_data_array;
        const mz_uint8 *pExtra_data;
        mz_uint32 extra_size_remaining = local_header_extra_len;
        mz_zip_array_init(&file_data_array, 1);
        if (!mz_zip_array_resize(pZip, &file_data_array, local_header_extra_len, MZ_FALSE)) {
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, src_file_stat.m_local_header_ofs + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + local_header_filename_size, file_data_array.m_p, local_header_extra_len) != local_header_extra_len) {
            mz_zip_array_clear(pZip, &file_data_array);
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
        }
        pExtra_data = (const mz_uint8 *)file_data_array.m_p;
        do {
            mz_uint32 field_id, field_data_size, field_total_size;
            if (extra_size_remaining < (sizeof(mz_uint16) * 2)) {
                mz_zip_array_clear(pZip, &file_data_array);
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            }
            field_id = MZ_READ_LE16(pExtra_data);
            field_data_size = MZ_READ_LE16(pExtra_data + sizeof(mz_uint16));
            field_total_size = field_data_size + sizeof(mz_uint16) * 2;
            if (field_total_size > extra_size_remaining) {
                mz_zip_array_clear(pZip, &file_data_array);
                return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
            }
            if (field_id == MZ_ZIP64_EXTENDED_INFORMATION_FIELD_HEADER_ID) {
                const mz_uint8 *pSrc_field_data = pExtra_data + sizeof(mz_uint32);
                if (field_data_size < sizeof(mz_uint64) * 2) {
                    mz_zip_array_clear(pZip, &file_data_array);
                    return mz_zip_set_error(pZip, MZ_ZIP_INVALID_HEADER_OR_CORRUPTED);
                }
                local_header_uncomp_size = MZ_READ_LE64(pSrc_field_data);
                local_header_comp_size = MZ_READ_LE64(pSrc_field_data + sizeof(mz_uint64));
                found_zip64_ext_data_in_ldir = MZ_TRUE;
                break;
            }
            pExtra_data += field_total_size;
            extra_size_remaining -= field_total_size;
        } while (extra_size_remaining);
        mz_zip_array_clear(pZip, &file_data_array);
    }
    if (!pState->m_zip64) {
        mz_uint64 approx_new_archive_size = cur_dst_file_ofs + num_alignment_padding_bytes + MZ_ZIP_LOCAL_DIR_HEADER_SIZE + src_archive_bytes_remaining + (sizeof(mz_uint32) * 4) +
                                            pState->m_central_dir.m_size + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + src_central_dir_following_data_size + MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE + 64;
        if (approx_new_archive_size >= MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
    }
    if (!mz_zip_writer_write_zeros(pZip, cur_dst_file_ofs, num_alignment_padding_bytes))
        return MZ_FALSE;
    cur_dst_file_ofs += num_alignment_padding_bytes;
    local_dir_header_ofs = cur_dst_file_ofs;
    if (pZip->m_file_offset_alignment) {
        MZ_ASSERT((local_dir_header_ofs & (pZip->m_file_offset_alignment - 1)) == 0);
    }
    if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_dst_file_ofs, pLocal_header, MZ_ZIP_LOCAL_DIR_HEADER_SIZE) != MZ_ZIP_LOCAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
    cur_dst_file_ofs += MZ_ZIP_LOCAL_DIR_HEADER_SIZE;
    if (NULL == (pBuf = pZip->m_pAlloc(pZip->m_pAlloc_opaque, 1, (size_t)MZ_MAX(32U, MZ_MIN((mz_uint64)MZ_ZIP_MAX_IO_BUF_SIZE, src_archive_bytes_remaining)))))
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    while (src_archive_bytes_remaining) {
        n = (mz_uint)MZ_MIN((mz_uint64)MZ_ZIP_MAX_IO_BUF_SIZE, src_archive_bytes_remaining);
        if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs, pBuf, n) != n) {
            pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
        }
        cur_src_file_ofs += n;
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_dst_file_ofs, pBuf, n) != n) {
            pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        }
        cur_dst_file_ofs += n;
        src_archive_bytes_remaining -= n;
    }
    bit_flags = MZ_READ_LE16(pLocal_header + MZ_ZIP_LDH_BIT_FLAG_OFS);
    if (bit_flags & 8) {
        if ((pSource_zip->m_pState->m_zip64) || (found_zip64_ext_data_in_ldir)) {
            if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs, pBuf, (sizeof(mz_uint32) * 6)) != (sizeof(mz_uint32) * 6)) {
                pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
            }
            n = sizeof(mz_uint32) * ((MZ_READ_LE32(pBuf) == MZ_ZIP_DATA_DESCRIPTOR_ID) ? 6 : 5);
        } else {
            mz_bool has_id;
            if (pSource_zip->m_pRead(pSource_zip->m_pIO_opaque, cur_src_file_ofs, pBuf, sizeof(mz_uint32) * 4) != sizeof(mz_uint32) * 4) {
                pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
                return mz_zip_set_error(pZip, MZ_ZIP_FILE_READ_FAILED);
            }
            has_id = (MZ_READ_LE32(pBuf) == MZ_ZIP_DATA_DESCRIPTOR_ID);
            if (pZip->m_pState->m_zip64) {
                const mz_uint8 *pSrc_descriptor = (const mz_uint8 *)pBuf + (has_id ? sizeof(mz_uint32) : 0);
                const mz_uint32 src_crc32 = MZ_READ_LE32(pSrc_descriptor);
                const mz_uint64 src_comp_size = MZ_READ_LE32(pSrc_descriptor + sizeof(mz_uint32));
                const mz_uint64 src_uncomp_size = MZ_READ_LE32(pSrc_descriptor + 2 * sizeof(mz_uint32));
                mz_write_le32((mz_uint8 *)pBuf, MZ_ZIP_DATA_DESCRIPTOR_ID);
                mz_write_le32((mz_uint8 *)pBuf + sizeof(mz_uint32) * 1, src_crc32);
                mz_write_le64((mz_uint8 *)pBuf + sizeof(mz_uint32) * 2, src_comp_size);
                mz_write_le64((mz_uint8 *)pBuf + sizeof(mz_uint32) * 4, src_uncomp_size);
                n = sizeof(mz_uint32) * 6;
            } else {
                n = sizeof(mz_uint32) * (has_id ? 4 : 3);
            }
        }
        if (pZip->m_pWrite(pZip->m_pIO_opaque, cur_dst_file_ofs, pBuf, n) != n) {
            pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        }
        cur_src_file_ofs += n;
        cur_dst_file_ofs += n;
    }
    pZip->m_pFree(pZip->m_pAlloc_opaque, pBuf);
    orig_central_dir_size = pState->m_central_dir.m_size;
    memcpy(new_central_header, pSrc_central_header, MZ_ZIP_CENTRAL_DIR_HEADER_SIZE);
    if (pState->m_zip64) {
        const mz_uint8 *pSrc_ext = pSrc_central_header + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + src_filename_len;
        mz_zip_array new_ext_block;
        mz_zip_array_init(&new_ext_block, sizeof(mz_uint8));
        MZ_WRITE_LE32(new_central_header + MZ_ZIP_CDH_COMPRESSED_SIZE_OFS, MZ_UINT32_MAX);
        MZ_WRITE_LE32(new_central_header + MZ_ZIP_CDH_DECOMPRESSED_SIZE_OFS, MZ_UINT32_MAX);
        MZ_WRITE_LE32(new_central_header + MZ_ZIP_CDH_LOCAL_HEADER_OFS, MZ_UINT32_MAX);
        if (!mz_zip_writer_update_zip64_extension_block(&new_ext_block, pZip, pSrc_ext, src_ext_len, &src_file_stat.m_comp_size, &src_file_stat.m_uncomp_size, &local_dir_header_ofs, NULL)) {
            mz_zip_array_clear(pZip, &new_ext_block);
            return MZ_FALSE;
        }
        MZ_WRITE_LE16(new_central_header + MZ_ZIP_CDH_EXTRA_LEN_OFS, new_ext_block.m_size);
        if (!mz_zip_array_push_back(pZip, &pState->m_central_dir, new_central_header, MZ_ZIP_CENTRAL_DIR_HEADER_SIZE)) {
            mz_zip_array_clear(pZip, &new_ext_block);
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        if (!mz_zip_array_push_back(pZip, &pState->m_central_dir, pSrc_central_header + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE, src_filename_len)) {
            mz_zip_array_clear(pZip, &new_ext_block);
            mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        if (!mz_zip_array_push_back(pZip, &pState->m_central_dir, new_ext_block.m_p, new_ext_block.m_size)) {
            mz_zip_array_clear(pZip, &new_ext_block);
            mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        if (!mz_zip_array_push_back(pZip, &pState->m_central_dir, pSrc_central_header + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE + src_filename_len + src_ext_len, src_comment_len)) {
            mz_zip_array_clear(pZip, &new_ext_block);
            mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
        mz_zip_array_clear(pZip, &new_ext_block);
    } else {
        if (cur_dst_file_ofs > MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
        if (local_dir_header_ofs >= MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_ARCHIVE_TOO_LARGE);
        MZ_WRITE_LE32(new_central_header + MZ_ZIP_CDH_LOCAL_HEADER_OFS, local_dir_header_ofs);
        if (!mz_zip_array_push_back(pZip, &pState->m_central_dir, new_central_header, MZ_ZIP_CENTRAL_DIR_HEADER_SIZE))
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        if (!mz_zip_array_push_back(pZip, &pState->m_central_dir, pSrc_central_header + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE, src_central_dir_following_data_size)) {
            mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
            return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
        }
    }
    if (pState->m_central_dir.m_size >= MZ_UINT32_MAX) {
        mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
        return mz_zip_set_error(pZip, MZ_ZIP_UNSUPPORTED_CDIR_SIZE);
    }
    n = (mz_uint32)orig_central_dir_size;
    if (!mz_zip_array_push_back(pZip, &pState->m_central_dir_offsets, &n, 1)) {
        mz_zip_array_resize(pZip, &pState->m_central_dir, orig_central_dir_size, MZ_FALSE);
        return mz_zip_set_error(pZip, MZ_ZIP_ALLOC_FAILED);
    }
    pZip->m_total_files++;
    pZip->m_archive_size = cur_dst_file_ofs;
    return MZ_TRUE;
}
mz_bool mz_zip_writer_finalize_archive(mz_zip_archive *pZip) {
    mz_zip_internal_state *pState;
    mz_uint64 central_dir_ofs, central_dir_size;
    mz_uint8 hdr[256];
    if ((!pZip) || (!pZip->m_pState) || (pZip->m_zip_mode != MZ_ZIP_MODE_WRITING))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    pState = pZip->m_pState;
    if (pState->m_zip64) {
        if ((mz_uint64)pState->m_central_dir.m_size >= MZ_UINT32_MAX)
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    } else {
        if ((pZip->m_total_files > MZ_UINT16_MAX) || ((pZip->m_archive_size + pState->m_central_dir.m_size + MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE) > MZ_UINT32_MAX))
            return mz_zip_set_error(pZip, MZ_ZIP_TOO_MANY_FILES);
    }
    central_dir_ofs = 0;
    central_dir_size = 0;
    if (pZip->m_total_files) {
        central_dir_ofs = pZip->m_archive_size;
        central_dir_size = pState->m_central_dir.m_size;
        pZip->m_central_directory_file_ofs = central_dir_ofs;
        if (pZip->m_pWrite(pZip->m_pIO_opaque, central_dir_ofs, pState->m_central_dir.m_p, (size_t)central_dir_size) != central_dir_size)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        pZip->m_archive_size += central_dir_size;
    }
    if (pState->m_zip64) {
        mz_uint64 rel_ofs_to_zip64_ecdr = pZip->m_archive_size;
        MZ_CLEAR_ARR(hdr);
        MZ_WRITE_LE32(hdr + MZ_ZIP64_ECDH_SIG_OFS, MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIG);
        MZ_WRITE_LE64(hdr + MZ_ZIP64_ECDH_SIZE_OF_RECORD_OFS, MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE - sizeof(mz_uint32) - sizeof(mz_uint64));
        MZ_WRITE_LE16(hdr + MZ_ZIP64_ECDH_VERSION_MADE_BY_OFS, 0x031E);
        MZ_WRITE_LE16(hdr + MZ_ZIP64_ECDH_VERSION_NEEDED_OFS, 0x002D);
        MZ_WRITE_LE64(hdr + MZ_ZIP64_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS, pZip->m_total_files);
        MZ_WRITE_LE64(hdr + MZ_ZIP64_ECDH_CDIR_TOTAL_ENTRIES_OFS, pZip->m_total_files);
        MZ_WRITE_LE64(hdr + MZ_ZIP64_ECDH_CDIR_SIZE_OFS, central_dir_size);
        MZ_WRITE_LE64(hdr + MZ_ZIP64_ECDH_CDIR_OFS_OFS, central_dir_ofs);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, pZip->m_archive_size, hdr, MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE) != MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        pZip->m_archive_size += MZ_ZIP64_END_OF_CENTRAL_DIR_HEADER_SIZE;
        MZ_CLEAR_ARR(hdr);
        MZ_WRITE_LE32(hdr + MZ_ZIP64_ECDL_SIG_OFS, MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIG);
        MZ_WRITE_LE64(hdr + MZ_ZIP64_ECDL_REL_OFS_TO_ZIP64_ECDR_OFS, rel_ofs_to_zip64_ecdr);
        MZ_WRITE_LE32(hdr + MZ_ZIP64_ECDL_TOTAL_NUMBER_OF_DISKS_OFS, 1);
        if (pZip->m_pWrite(pZip->m_pIO_opaque, pZip->m_archive_size, hdr, MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE) != MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE)
            return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
        pZip->m_archive_size += MZ_ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIZE;
    }
    MZ_CLEAR_ARR(hdr);
    MZ_WRITE_LE32(hdr + MZ_ZIP_ECDH_SIG_OFS, MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIG);
    MZ_WRITE_LE16(hdr + MZ_ZIP_ECDH_CDIR_NUM_ENTRIES_ON_DISK_OFS, MZ_MIN(MZ_UINT16_MAX, pZip->m_total_files));
    MZ_WRITE_LE16(hdr + MZ_ZIP_ECDH_CDIR_TOTAL_ENTRIES_OFS, MZ_MIN(MZ_UINT16_MAX, pZip->m_total_files));
    MZ_WRITE_LE32(hdr + MZ_ZIP_ECDH_CDIR_SIZE_OFS, MZ_MIN(MZ_UINT32_MAX, central_dir_size));
    MZ_WRITE_LE32(hdr + MZ_ZIP_ECDH_CDIR_OFS_OFS, MZ_MIN(MZ_UINT32_MAX, central_dir_ofs));
    if (pZip->m_pWrite(pZip->m_pIO_opaque, pZip->m_archive_size, hdr, MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE) != MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE)
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_WRITE_FAILED);
#ifndef MINIZ_NO_STDIO
    if ((pState->m_pFile) && (MZ_FFLUSH(pState->m_pFile) == EOF))
        return mz_zip_set_error(pZip, MZ_ZIP_FILE_CLOSE_FAILED);
#endif
    pZip->m_archive_size += MZ_ZIP_END_OF_CENTRAL_DIR_HEADER_SIZE;
    pZip->m_zip_mode = MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED;
    return MZ_TRUE;
}
mz_bool mz_zip_writer_finalize_heap_archive(mz_zip_archive *pZip, void **ppBuf, size_t *pSize) {
    if ((!ppBuf) || (!pSize))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    *ppBuf = NULL;
    *pSize = 0;
    if ((!pZip) || (!pZip->m_pState))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (pZip->m_pWrite != mz_zip_heap_write_func)
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    if (!mz_zip_writer_finalize_archive(pZip))
        return MZ_FALSE;
    *ppBuf = pZip->m_pState->m_pMem;
    *pSize = pZip->m_pState->m_mem_size;
    pZip->m_pState->m_pMem = NULL;
    pZip->m_pState->m_mem_size = pZip->m_pState->m_mem_capacity = 0;
    return MZ_TRUE;
}
mz_bool mz_zip_writer_end(mz_zip_archive *pZip) {
    return mz_zip_writer_end_internal(pZip, MZ_TRUE);
}
#ifndef MINIZ_NO_STDIO
mz_bool mz_zip_add_mem_to_archive_file_in_place(const char *pZip_filename, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags) {
    return mz_zip_add_mem_to_archive_file_in_place_v2(pZip_filename, pArchive_name, pBuf, buf_size, pComment, comment_size, level_and_flags, NULL);
}
mz_bool mz_zip_add_mem_to_archive_file_in_place_v2(const char *pZip_filename, const char *pArchive_name, const void *pBuf, size_t buf_size, const void *pComment, mz_uint16 comment_size, mz_uint level_and_flags, mz_zip_error *pErr) {
    mz_bool status, created_new_archive = MZ_FALSE;
    mz_zip_archive zip_archive;
    struct MZ_FILE_STAT_STRUCT file_stat;
    mz_zip_error actual_err = MZ_ZIP_NO_ERROR;
    mz_zip_zero_struct(&zip_archive);
    if ((int)level_and_flags < 0)
        level_and_flags = MZ_DEFAULT_LEVEL;
    if ((!pZip_filename) || (!pArchive_name) || ((buf_size) && (!pBuf)) || ((comment_size) && (!pComment)) || ((level_and_flags & 0xF) > MZ_UBER_COMPRESSION)) {
        if (pErr)
            *pErr = MZ_ZIP_INVALID_PARAMETER;
        return MZ_FALSE;
    }
    if (!mz_zip_writer_validate_archive_name(pArchive_name)) {
        if (pErr)
            *pErr = MZ_ZIP_INVALID_FILENAME;
        return MZ_FALSE;
    }
    if (MZ_FILE_STAT(pZip_filename, &file_stat) != 0) {
        if (!mz_zip_writer_init_file_v2(&zip_archive, pZip_filename, 0, level_and_flags)) {
            if (pErr)
                *pErr = zip_archive.m_last_error;
            return MZ_FALSE;
        }
        created_new_archive = MZ_TRUE;
    } else {
        if (!mz_zip_reader_init_file_v2(&zip_archive, pZip_filename, level_and_flags | MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY | MZ_ZIP_FLAG_READ_ALLOW_WRITING, 0, 0)) {
            if (pErr)
                *pErr = zip_archive.m_last_error;
            return MZ_FALSE;
        }
        if (!mz_zip_writer_init_from_reader_v2(&zip_archive, pZip_filename, level_and_flags | MZ_ZIP_FLAG_READ_ALLOW_WRITING)) {
            if (pErr)
                *pErr = zip_archive.m_last_error;
            mz_zip_reader_end_internal(&zip_archive, MZ_FALSE);
            return MZ_FALSE;
        }
    }
    status = mz_zip_writer_add_mem_ex(&zip_archive, pArchive_name, pBuf, buf_size, pComment, comment_size, level_and_flags, 0, 0);
    actual_err = zip_archive.m_last_error;
    if (!mz_zip_writer_finalize_archive(&zip_archive)) {
        if (!actual_err)
            actual_err = zip_archive.m_last_error;
        status = MZ_FALSE;
    }
    if (!mz_zip_writer_end_internal(&zip_archive, status)) {
        if (!actual_err)
            actual_err = zip_archive.m_last_error;
        status = MZ_FALSE;
    }
    if ((!status) && (created_new_archive)) {
        int ignoredStatus = MZ_DELETE_FILE(pZip_filename);
        (void)ignoredStatus;
    }
    if (pErr)
        *pErr = actual_err;
    return status;
}
void *mz_zip_extract_archive_file_to_heap_v2(const char *pZip_filename, const char *pArchive_name, const char *pComment, size_t *pSize, mz_uint flags, mz_zip_error *pErr) {
    mz_uint32 file_index;
    mz_zip_archive zip_archive;
    void *p = NULL;
    if (pSize)
        *pSize = 0;
    if ((!pZip_filename) || (!pArchive_name)) {
        if (pErr)
            *pErr = MZ_ZIP_INVALID_PARAMETER;
        return NULL;
    }
    mz_zip_zero_struct(&zip_archive);
    if (!mz_zip_reader_init_file_v2(&zip_archive, pZip_filename, flags | MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY, 0, 0)) {
        if (pErr)
            *pErr = zip_archive.m_last_error;
        return NULL;
    }
    if (mz_zip_reader_locate_file_v2(&zip_archive, pArchive_name, pComment, flags, &file_index)) {
        p = mz_zip_reader_extract_to_heap(&zip_archive, file_index, pSize, flags);
    }
    mz_zip_reader_end_internal(&zip_archive, p != NULL);
    if (pErr)
        *pErr = zip_archive.m_last_error;
    return p;
}
void *mz_zip_extract_archive_file_to_heap(const char *pZip_filename, const char *pArchive_name, size_t *pSize, mz_uint flags) {
    return mz_zip_extract_archive_file_to_heap_v2(pZip_filename, pArchive_name, NULL, pSize, flags, NULL);
}
#endif
#endif
mz_zip_mode mz_zip_get_mode(mz_zip_archive *pZip) {
    return pZip ? pZip->m_zip_mode : MZ_ZIP_MODE_INVALID;
}
mz_zip_type mz_zip_get_type(mz_zip_archive *pZip) {
    return pZip ? pZip->m_zip_type : MZ_ZIP_TYPE_INVALID;
}
mz_zip_error mz_zip_set_last_error(mz_zip_archive *pZip, mz_zip_error err_num) {
    mz_zip_error prev_err;
    if (!pZip)
        return MZ_ZIP_INVALID_PARAMETER;
    prev_err = pZip->m_last_error;
    pZip->m_last_error = err_num;
    return prev_err;
}
mz_zip_error mz_zip_peek_last_error(mz_zip_archive *pZip) {
    if (!pZip)
        return MZ_ZIP_INVALID_PARAMETER;
    return pZip->m_last_error;
}
mz_zip_error mz_zip_clear_last_error(mz_zip_archive *pZip) {
    return mz_zip_set_last_error(pZip, MZ_ZIP_NO_ERROR);
}
mz_zip_error mz_zip_get_last_error(mz_zip_archive *pZip) {
    mz_zip_error prev_err;
    if (!pZip)
        return MZ_ZIP_INVALID_PARAMETER;
    prev_err = pZip->m_last_error;
    pZip->m_last_error = MZ_ZIP_NO_ERROR;
    return prev_err;
}
static const char *const s_zip_error_strings[MZ_ZIP_TOTAL_ERRORS + 1] = {
    "no error", "undefined error", "too many files", "file too large",
    "unsupported method", "unsupported encryption", "unsupported feature",
    "failed finding central directory", "not a ZIP archive",
    "invalid header or archive is corrupted", "unsupported multidisk archive",
    "decompression failed or archive is corrupted", "compression failed",
    "unexpected decompressed size", "CRC-32 check failed",
    "unsupported central directory size", "allocation failed",
    "file open failed", "file create failed", "file write failed",
    "file read failed", "file close failed", "file seek failed",
    "file stat failed", "invalid parameter", "invalid filename",
    "buffer too small", "internal error", "file not found",
    "archive is too large", "validation failed", "write callback failed",
    "total errors"
};
const char *mz_zip_get_error_string(mz_zip_error mz_err) {
    return ((mz_err >= 0) && (mz_err <= MZ_ZIP_TOTAL_ERRORS)) ? s_zip_error_strings[mz_err] : "unknown error";
}
mz_bool mz_zip_is_zip64(mz_zip_archive *pZip) {
    if ((!pZip) || (!pZip->m_pState))
        return MZ_FALSE;
    return pZip->m_pState->m_zip64;
}
size_t mz_zip_get_central_dir_size(mz_zip_archive *pZip) {
    if ((!pZip) || (!pZip->m_pState))
        return 0;
    return pZip->m_pState->m_central_dir.m_size;
}
mz_uint mz_zip_reader_get_num_files(mz_zip_archive *pZip) {
    return pZip ? pZip->m_total_files : 0;
}
mz_uint64 mz_zip_get_archive_size(mz_zip_archive *pZip) {
    if (!pZip)
        return 0;
    return pZip->m_archive_size;
}
mz_uint64 mz_zip_get_archive_file_start_offset(mz_zip_archive *pZip) {
    if ((!pZip) || (!pZip->m_pState))
        return 0;
    return pZip->m_pState->m_file_archive_start_ofs;
}
MZ_FILE *mz_zip_get_cfile(mz_zip_archive *pZip) {
    if ((!pZip) || (!pZip->m_pState))
        return 0;
    return pZip->m_pState->m_pFile;
}
size_t mz_zip_read_archive_data(mz_zip_archive *pZip, mz_uint64 file_ofs, void *pBuf, size_t n) {
    if ((!pZip) || (!pZip->m_pState) || (!pBuf) || (!pZip->m_pRead))
        return mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
    return pZip->m_pRead(pZip->m_pIO_opaque, file_ofs, pBuf, n);
}
mz_uint mz_zip_reader_get_filename(mz_zip_archive *pZip, mz_uint file_index, char *pFilename, mz_uint filename_buf_size) {
    mz_uint n;
    const mz_uint8 *p = mz_zip_get_cdh(pZip, file_index);
    if (!p) {
        if (filename_buf_size)
            pFilename[0] = '\0';
        mz_zip_set_error(pZip, MZ_ZIP_INVALID_PARAMETER);
        return 0;
    }
    n = MZ_READ_LE16(p + MZ_ZIP_CDH_FILENAME_LEN_OFS);
    if (filename_buf_size) {
        n = MZ_MIN(n, filename_buf_size - 1);
        memcpy(pFilename, p + MZ_ZIP_CENTRAL_DIR_HEADER_SIZE, n);
        pFilename[n] = '\0';
    }
    return n + 1;
}
mz_bool mz_zip_reader_file_stat(mz_zip_archive *pZip, mz_uint file_index, mz_zip_archive_file_stat *pStat) {
    return mz_zip_file_stat_internal(pZip, file_index, mz_zip_get_cdh(pZip, file_index), pStat, NULL);
}
mz_bool mz_zip_end(mz_zip_archive *pZip) {
    if (!pZip)
        return MZ_FALSE;
    if (pZip->m_zip_mode == MZ_ZIP_MODE_READING)
        return mz_zip_reader_end(pZip);
#ifndef MINIZ_NO_ARCHIVE_WRITING_APIS
    else if ((pZip->m_zip_mode == MZ_ZIP_MODE_WRITING) || (pZip->m_zip_mode == MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED))
        return mz_zip_writer_end(pZip);
#endif
    return MZ_FALSE;
}
#ifdef __cplusplus
}
#endif
#endif
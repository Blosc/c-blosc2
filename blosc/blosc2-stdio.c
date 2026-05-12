/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#if defined(__linux__)
  /* Must be defined before anything else is included */
  #define _GNU_SOURCE
#endif

#include "blosc2/blosc2-stdio.h"
#include "blosc2.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#if defined(_WIN32)
  #include <memoryapi.h>
  // See https://github.com/Blosc/python-blosc2/issues/359
  #define fseek _fseeki64
  #define ftell _ftelli64
#else
  #include <sys/mman.h>
#endif


static bool checked_mul_int64_nonneg(int64_t a, int64_t b, int64_t* out) {
  if (a < 0 || b < 0) {
    return false;
  }
  if (a == 0 || b == 0) {
    *out = 0;
    return true;
  }
  if (a > INT64_MAX / b) {
    return false;
  }
  *out = a * b;
  return true;
}

static bool checked_add_int64_nonneg(int64_t a, int64_t b, int64_t* out) {
  if (a < 0 || b < 0) {
    return false;
  }
  if (a > INT64_MAX - b) {
    return false;
  }
  *out = a + b;
  return true;
}

static bool checked_size_t_to_int64(size_t value, int64_t* out) {
  if (value > (size_t)INT64_MAX) {
    return false;
  }
  *out = (int64_t)value;
  return true;
}


void *blosc2_stdio_open(const char *urlpath, const char *mode, void *params) {
  BLOSC_UNUSED_PARAM(params);
  if (urlpath == NULL || mode == NULL) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio open.");
    return NULL;
  }
  FILE *file = fopen(urlpath, mode);
  if (file == NULL) {
    BLOSC_TRACE_ERROR("Cannot open the file %s with mode %s.", urlpath, mode);
    return NULL;
  }
  blosc2_stdio_file *my_fp = malloc(sizeof(blosc2_stdio_file));
  if (my_fp == NULL) {
    BLOSC_TRACE_ERROR("Cannot allocate memory for stdio file wrapper.");
    fclose(file);
    return NULL;
  }
  my_fp->file = file;
  return my_fp;
}

int blosc2_stdio_close(void *stream) {
  if (stream == NULL) {
    BLOSC_TRACE_ERROR("Invalid stream for stdio close.");
    return -1;
  }
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  if (my_fp->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid stream for stdio close.");
    free(my_fp);
    return -1;
  }
  int err = fclose(my_fp->file);
  free(my_fp);
  return err;
}

int64_t blosc2_stdio_size(void *stream) {
  if (stream == NULL) {
    BLOSC_TRACE_ERROR("Invalid stream for stdio size.");
    return -1;
  }
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  if (my_fp->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid stream for stdio size.");
    return -1;
  }

  int64_t current = ftell(my_fp->file);
  if (current < 0) {
    BLOSC_TRACE_ERROR("ftell failed while determining current position (error: %s).", strerror(errno));
    return -1;
  }

  if (fseek(my_fp->file, 0, SEEK_END) != 0) {
    BLOSC_TRACE_ERROR("fseek to file end failed while getting size (error: %s).", strerror(errno));
    return -1;
  }
  int64_t size = ftell(my_fp->file);
  if (size < 0) {
    BLOSC_TRACE_ERROR("ftell failed while getting file size (error: %s).", strerror(errno));
    return -1;
  }

  if (fseek(my_fp->file, current, SEEK_SET) != 0) {
    BLOSC_TRACE_ERROR("fseek restore failed after getting file size (error: %s).", strerror(errno));
    return -1;
  }

  return size;
}

int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  if (stream == NULL || ptr == NULL || size < 0 || nitems < 0 || position < 0) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio write.");
    return 0;
  }
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  if (my_fp->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio write.");
    return 0;
  }

  int64_t n_bytes_i64;
  if (!checked_mul_int64_nonneg(size, nitems, &n_bytes_i64)) {
    BLOSC_TRACE_ERROR("stdio write size overflow (size=%" PRId64 ", nitems=%" PRId64 ").", size, nitems);
    return 0;
  }
  if ((uint64_t)n_bytes_i64 > SIZE_MAX) {
    BLOSC_TRACE_ERROR("stdio write size does not fit in size_t (%" PRId64 ").", n_bytes_i64);
    return 0;
  }

#if !defined(_WIN32)
  /* POSIX fseek takes long; reject offsets that would silently truncate (e.g. on 32-bit). */
  if (position > (int64_t)LONG_MAX) {
    BLOSC_TRACE_ERROR("stdio write position %" PRId64 " exceeds LONG_MAX for fseek.", position);
    return 0;
  }
#endif

  int rc = fseek(my_fp->file, position, SEEK_SET);
  if (rc != 0) {
    BLOSC_TRACE_ERROR("fseek failed at position %" PRId64 " (error: %s).", position, strerror(errno));
    return 0;
  }

  size_t nitems_ = fwrite(ptr, (size_t) size, (size_t) nitems, my_fp->file);
  if ((int64_t)nitems_ != nitems) {
    BLOSC_TRACE_ERROR("Short write at position %" PRId64 ": requested %" PRId64 " items of size %" PRId64
                      ", wrote %zu (error: %s).", position, nitems, size, nitems_, strerror(errno));
  }
  return (int64_t) nitems_;
}

int64_t blosc2_stdio_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  if (stream == NULL || ptr == NULL || size < 0 || nitems < 0 || position < 0) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio read.");
    return 0;
  }
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  if (my_fp->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio read.");
    return 0;
  }

  int64_t n_bytes_i64;
  if (!checked_mul_int64_nonneg(size, nitems, &n_bytes_i64)) {
    BLOSC_TRACE_ERROR("stdio read size overflow (size=%" PRId64 ", nitems=%" PRId64 ").", size, nitems);
    return 0;
  }
  if ((uint64_t)n_bytes_i64 > SIZE_MAX) {
    BLOSC_TRACE_ERROR("stdio read size does not fit in size_t (%" PRId64 ").", n_bytes_i64);
    return 0;
  }
  /* For allocation-necessary backends, *ptr must point at a real buffer
     when the caller asked for any bytes. Leave *ptr untouched on failure. */
  if (n_bytes_i64 > 0 && *ptr == NULL) {
    BLOSC_TRACE_ERROR("stdio read called with NULL buffer for %" PRId64 " bytes.", n_bytes_i64);
    return 0;
  }

#if !defined(_WIN32)
  /* POSIX fseek takes long; reject offsets that would silently truncate (e.g. on 32-bit). */
  if (position > (int64_t)LONG_MAX) {
    BLOSC_TRACE_ERROR("stdio read position %" PRId64 " exceeds LONG_MAX for fseek.", position);
    return 0;
  }
#endif

  int rc = fseek(my_fp->file, position, SEEK_SET);
  if (rc != 0) {
    BLOSC_TRACE_ERROR("fseek failed at position %" PRId64 " (error: %s).", position, strerror(errno));
    return 0;
  }

  void* data_ptr = *ptr;
  size_t nitems_ = fread(data_ptr, (size_t) size, (size_t) nitems, my_fp->file);
  if ((int64_t)nitems_ != nitems) {
    BLOSC_TRACE_ERROR("Short read at position %" PRId64 ": requested %" PRId64 " items of size %" PRId64
                      ", read %zu (error: %s).", position, nitems, size, nitems_, strerror(errno));
  }
  return (int64_t) nitems_;
}

int blosc2_stdio_truncate(void *stream, int64_t size) {
  if (stream == NULL || size < 0) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio truncate.");
    return -1;
  }
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  if (my_fp->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid arguments for stdio truncate.");
    return -1;
  }
  int rc;
#if defined(_MSC_VER)
  rc = _chsize_s(_fileno(my_fp->file), size);
#else
  off_t size_off_t = (off_t)size;
  if ((int64_t)size_off_t != size) {
    BLOSC_TRACE_ERROR("stdio truncate size does not fit in off_t (%" PRId64 ").", size);
    return -1;
  }
  rc = ftruncate(fileno(my_fp->file), size_off_t);
#endif
  return rc;
}

int blosc2_stdio_destroy(void* params) {
  BLOSC_UNUSED_PARAM(params);
  return 0;
}

#if defined(_WIN32)
void _print_last_error() {
    DWORD last_error = GetLastError();
    if(last_error == 0) {
        return;
    }

    LPSTR msg = NULL;
    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      last_error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&msg,
      0,
      NULL
    );

    printf("Message for the error %lu:\n%s\n", last_error, msg);
    LocalFree(msg);
}
#endif

void *blosc2_stdio_mmap_open(const char *urlpath, const char *mode, void* params) {
  BLOSC_UNUSED_PARAM(mode);

  if (urlpath == NULL || params == NULL) {
    BLOSC_TRACE_ERROR("Invalid arguments for memory-mapped open.");
    return NULL;
  }

  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  if (mmap_file->mode == NULL) {
    BLOSC_TRACE_ERROR("Memory-mapped mode is NULL.");
    return NULL;
  }
  if (mmap_file->addr != NULL) {
    if (mmap_file->urlpath == NULL) {
      BLOSC_TRACE_ERROR("Memory-mapped file has invalid state: urlpath is NULL.");
      return NULL;
    }
    if (strcmp(mmap_file->urlpath, urlpath) != 0) {
      BLOSC_TRACE_ERROR(
        "The memory-mapped file is already opened with the path %s and hence cannot be reopened with the path %s. This "
        "happens if you try to open a sframe (sparse frame); please note that memory-mapped files are not supported "
        "for sframes.",
        mmap_file->urlpath,
        urlpath
      );
      return NULL;
    }

    /* A memory-mapped file is only opened once */
    return mmap_file;
  }

  // Keep the original path to ensure that all future file openings are with the same path
  size_t urlpath_len = strlen(urlpath);
  mmap_file->urlpath = malloc(urlpath_len + 1);
  if (mmap_file->urlpath == NULL) {
    BLOSC_TRACE_ERROR("Cannot allocate memory for the path of the memory-mapped file.");
    return NULL;
  }
  memcpy(mmap_file->urlpath, urlpath, urlpath_len + 1);

  /* mmap_file->mode mapping is similar to Numpy's memmap
  (https://github.com/numpy/numpy/blob/main/numpy/_core/memmap.py) and CPython
  (https://github.com/python/cpython/blob/main/Modules/mmapmodule.c) */
#if defined(_WIN32)
  char* open_mode;
  bool use_initial_mapping_size;
  if (strcmp(mmap_file->mode, "r") == 0) {
    mmap_file->access_flags = PAGE_READONLY;
    mmap_file->map_flags = FILE_MAP_READ;
    mmap_file->is_memory_only = false;
    open_mode = "rb";
    use_initial_mapping_size = false;
  } else if (strcmp(mmap_file->mode, "r+") == 0) {
    mmap_file->access_flags = PAGE_READWRITE;
    mmap_file->map_flags = FILE_MAP_WRITE;
    mmap_file->is_memory_only = false;
    open_mode = "rb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "w+") == 0) {
    mmap_file->access_flags = PAGE_READWRITE;
    mmap_file->map_flags = FILE_MAP_WRITE;
    mmap_file->is_memory_only = false;
    open_mode = "wb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "c") == 0) {
    mmap_file->access_flags = PAGE_WRITECOPY;
    mmap_file->map_flags = FILE_MAP_COPY;
    mmap_file->is_memory_only = true;
    open_mode = "rb";
    use_initial_mapping_size = false;
  } else {
    BLOSC_TRACE_ERROR("Mode %s not supported for memory-mapped files.", mmap_file->mode);
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }
#else
  char* open_mode;
  bool use_initial_mapping_size;
  if (strcmp(mmap_file->mode, "r") == 0) {
    mmap_file->access_flags = PROT_READ;
    mmap_file->map_flags = MAP_SHARED;
    mmap_file->is_memory_only = false;
    open_mode = "rb";
    use_initial_mapping_size = false;
  } else if (strcmp(mmap_file->mode, "r+") == 0) {
    mmap_file->access_flags = PROT_READ | PROT_WRITE;
    mmap_file->map_flags = MAP_SHARED;
    mmap_file->is_memory_only = false;
    open_mode = "rb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "w+") == 0) {
    mmap_file->access_flags = PROT_READ | PROT_WRITE;
    mmap_file->map_flags = MAP_SHARED;
    mmap_file->is_memory_only = false;
    open_mode = "wb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "c") == 0) {
    mmap_file->access_flags = PROT_READ | PROT_WRITE;
    mmap_file->map_flags = MAP_PRIVATE;
    mmap_file->is_memory_only = true;
    open_mode = "rb";
    use_initial_mapping_size = true;
  } else {
    BLOSC_TRACE_ERROR("Mode %s not supported for memory-mapped files.", mmap_file->mode);
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }
#endif

  mmap_file->file = fopen(urlpath, open_mode);
  if (mmap_file->file == NULL) {
    BLOSC_TRACE_ERROR("Cannot open the file %s with mode %s.", urlpath, open_mode);
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }

  /* Retrieve the size of the file */
  if (fseek(mmap_file->file, 0, SEEK_END) != 0) {
    BLOSC_TRACE_ERROR("Cannot seek to the end of %s (error: %s).", urlpath, strerror(errno));
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }
  int64_t file_size_i64 = ftell(mmap_file->file);
  if (file_size_i64 < 0) {
    BLOSC_TRACE_ERROR("Cannot retrieve file size for %s.", urlpath);
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }
  if ((uint64_t)file_size_i64 > SIZE_MAX) {
    BLOSC_TRACE_ERROR("File size for %s exceeds size_t range.", urlpath);
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }
  mmap_file->file_size = (size_t)file_size_i64;
  if (fseek(mmap_file->file, 0, SEEK_SET) != 0) {
    BLOSC_TRACE_ERROR("Cannot seek to the beginning of %s (error: %s).", urlpath, strerror(errno));
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }

  /* The size of the mapping must be > 0 so we are using a large enough buffer for writing
  (which will be increased later if needed) */
  if (use_initial_mapping_size) {
    mmap_file->mapping_size = mmap_file->initial_mapping_size;
  }
  else {
    mmap_file->mapping_size = mmap_file->file_size;
  }

  if (mmap_file->file_size > mmap_file->mapping_size) {
    mmap_file->mapping_size = mmap_file->file_size;
  }
  if (mmap_file->mapping_size == 0) {
    mmap_file->mapping_size = 1;
  }

#if defined(_WIN32)
  mmap_file->fd = _fileno(mmap_file->file);

  /* Windows automatically expands the file size to the memory mapped file
  (https://learn.microsoft.com/en-us/windows/win32/memory/creating-a-file-mapping-object).
  In general, the size of the file is directly connected to the size of the mapping and cannot change. We cut the
  file size to the target size in the end after we close the mapping */
  HANDLE file_handle = (HANDLE) _get_osfhandle(mmap_file->fd);
  uint64_t mapping_size64 = (uint64_t)mmap_file->mapping_size;
  DWORD size_hi = (DWORD)(mapping_size64 >> 32);
  DWORD size_lo = (DWORD)(mapping_size64 & 0xFFFFFFFFu);
  mmap_file->mmap_handle = CreateFileMapping(file_handle, NULL, mmap_file->access_flags, size_hi, size_lo, NULL);
  if (mmap_file->mmap_handle == NULL) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Creating the memory mapping failed for the file %s.", urlpath);
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }

  DWORD offset = 0;
  mmap_file->addr = (char*) MapViewOfFile(
    mmap_file->mmap_handle, mmap_file->map_flags, offset, offset, mmap_file->mapping_size);
  if (mmap_file->addr == NULL) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Memory mapping failed for the file %s.", urlpath);

    if (!CloseHandle(mmap_file->mmap_handle)) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot close the handle to the memory-mapped file.");
    }

    mmap_file->mmap_handle = INVALID_HANDLE_VALUE;
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;

    return NULL;
  }
#else
  mmap_file->fd = fileno(mmap_file->file);

  /* Offset where the mapping should start */
  int64_t offset = 0;
  mmap_file->addr = mmap(
    NULL, mmap_file->mapping_size, mmap_file->access_flags, mmap_file->map_flags, mmap_file->fd, offset);
  if (mmap_file->addr == MAP_FAILED) {
    BLOSC_TRACE_ERROR("Memory mapping failed for file %s (error: %s).", urlpath, strerror(errno));
    mmap_file->addr = NULL;
    fclose(mmap_file->file);
    mmap_file->file = NULL;
    free(mmap_file->urlpath);
    mmap_file->urlpath = NULL;
    return NULL;
  }
#endif

  BLOSC_INFO(
    "Opened memory-mapped file %s in mode %s with an mapping size of %zu bytes.",
    mmap_file->urlpath,
    mmap_file->mode,
    mmap_file->mapping_size
  );

  /* The mmap_file->mode parameter is only available during the opening call and cannot be used in any of the other
     I/O functions since this string is managed by the caller (e.g., from Python) and the memory of the string may not
     be available anymore at a later point. */
  mmap_file->mode = NULL;

  return mmap_file;
}

int blosc2_stdio_mmap_close(void *stream) {
  BLOSC_UNUSED_PARAM(stream);
  return 0;
}

int64_t blosc2_stdio_mmap_size(void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;
  if (mmap_file->file_size > (size_t)INT64_MAX) {
    BLOSC_TRACE_ERROR("mmap file size exceeds int64_t return range (%zu).", mmap_file->file_size);
    return INT64_MAX;
  }
  return (int64_t)mmap_file->file_size;
}

int64_t blosc2_stdio_mmap_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (ptr == NULL || size < 0 || nitems < 0 || position < 0) {
    BLOSC_TRACE_ERROR("Invalid arguments for mmap write.");
    return 0;
  }

  int64_t n_bytes_i64;
  if (!checked_mul_int64_nonneg(size, nitems, &n_bytes_i64)) {
    BLOSC_TRACE_ERROR("mmap write size overflow (size=%" PRId64 ", nitems=%" PRId64 ").", size, nitems);
    return 0;
  }
  if ((uint64_t)n_bytes_i64 > SIZE_MAX) {
    BLOSC_TRACE_ERROR("mmap write size does not fit in size_t (%" PRId64 ").", n_bytes_i64);
    return 0;
  }
  size_t n_bytes = (size_t)n_bytes_i64;
  if (n_bytes == 0) {
    return 0;
  }

  int64_t position_end_i64;
  if (!checked_add_int64_nonneg(position, n_bytes_i64, &position_end_i64)) {
    BLOSC_TRACE_ERROR("mmap write position overflow (position=%" PRId64 ", nbytes=%" PRId64 ").", position, n_bytes_i64);
    return 0;
  }
  if ((uint64_t)position_end_i64 > SIZE_MAX) {
    BLOSC_TRACE_ERROR("mmap write end position does not fit in size_t (%" PRId64 ").", position_end_i64);
    return 0;
  }
  size_t position_size = (size_t)position;
  size_t position_end = (size_t)position_end_i64;
  size_t new_size = position_end > mmap_file->file_size ? position_end : mmap_file->file_size;

#if defined(_WIN32)
  if (mmap_file->file_size < new_size) {
    mmap_file->file_size = new_size;
  }

  if (mmap_file->mapping_size < mmap_file->file_size) {
    size_t remap_size;
    if (mmap_file->file_size > SIZE_MAX / 2) {
      BLOSC_TRACE_WARNING("mmap remap growth fallback: cannot double mapping_size near SIZE_MAX; using file_size (%zu).", mmap_file->file_size);
      remap_size = mmap_file->file_size;
    }
    else {
      remap_size = mmap_file->file_size * 2;
    }
    if (remap_size > (size_t)INT64_MAX) {
      BLOSC_TRACE_ERROR("mmap mapping size exceeds supported OS range (%zu).", remap_size);
      return 0;
    }
    mmap_file->mapping_size = remap_size;

    /* We need to remap the file completely and cannot pass the previous used address on Windows */
    if (!UnmapViewOfFile(mmap_file->addr)) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file.");
      return 0;
    }
    if (!CloseHandle(mmap_file->mmap_handle)) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot close the handle to the memory-mapped file.");
      return 0;
    }

    HANDLE file_handle = (HANDLE) _get_osfhandle(mmap_file->fd);
    uint64_t mapping_size64 = (uint64_t)mmap_file->mapping_size;
    DWORD size_hi = (DWORD)(mapping_size64 >> 32);
    DWORD size_lo = (DWORD)(mapping_size64 & 0xFFFFFFFFu);
    mmap_file->mmap_handle = CreateFileMapping(file_handle, NULL, mmap_file->access_flags, size_hi, size_lo, NULL);
    if (mmap_file->mmap_handle == NULL) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot remapt the memory-mapped file.");
      return 0;
    }

    DWORD offset = 0;
    char* new_address = (char*) MapViewOfFile(
      mmap_file->mmap_handle, mmap_file->map_flags, offset, offset, mmap_file->mapping_size);
    if (new_address == NULL) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot remapt the memory-mapped file");

      if (!CloseHandle(mmap_file->mmap_handle)) {
        _print_last_error();
        BLOSC_TRACE_ERROR("Cannot close the handle to the memory-mapped file.");
      }

      return 0;
    }

    mmap_file->addr = new_address;
  }
#else
  if (mmap_file->file_size < new_size) {
    mmap_file->file_size = new_size;

    if (!mmap_file->is_memory_only) {
      int64_t ftruncate_size;
      if (!checked_size_t_to_int64(new_size, &ftruncate_size)) {
        BLOSC_TRACE_ERROR("Cannot extend the file size to %zu bytes: value exceeds int64_t.", new_size);
        return 0;
      }
      int rc = ftruncate(mmap_file->fd, ftruncate_size);
      if (rc < 0) {
        BLOSC_TRACE_ERROR("Cannot extend the file size to %zu bytes (error: %s).", new_size, strerror(errno));
        return 0;
      }
    }
  }

  if (mmap_file->mapping_size < mmap_file->file_size) {
    size_t new_mapping_size;
    if (mmap_file->file_size > SIZE_MAX / 2) {
      BLOSC_TRACE_WARNING("mmap remap growth fallback: cannot double mapping_size near SIZE_MAX; using file_size (%zu).", mmap_file->file_size);
      new_mapping_size = mmap_file->file_size;
    }
    else {
      new_mapping_size = mmap_file->file_size * 2;
    }

#if defined(__linux__)
    /* mremap is the best option as it also ensures that the old data is still available in c mode. Unfortunately, it
    is no POSIX standard and only available on Linux */
  size_t old_mapping_size = mmap_file->mapping_size;
    char* new_address = mremap(mmap_file->addr, old_mapping_size, new_mapping_size, MREMAP_MAYMOVE);
#else
    if (mmap_file->is_memory_only) {
      BLOSC_TRACE_ERROR("Remapping a memory-mapping in c mode is only possible on Linux."
      "Please specify either a different mode or set initial_mapping_size to a large enough number.");
      return 0;
    }
    /* Extend the current mapping with the help of MAP_FIXED */
    int64_t offset = 0;
    char* new_address = mmap(
      mmap_file->addr,
      new_mapping_size,
      mmap_file->access_flags,
      mmap_file->map_flags | MAP_FIXED,
      mmap_file->fd,
      offset
    );
#endif

    if (new_address == MAP_FAILED) {
      BLOSC_TRACE_ERROR("Cannot remap the memory-mapped file (error: %s).", strerror(errno));
      return 0;
    }

    mmap_file->mapping_size = new_mapping_size;
    mmap_file->addr = new_address;
  }
#endif

  memcpy(mmap_file->addr + position_size, ptr, n_bytes);
  return nitems;
}

int64_t blosc2_stdio_mmap_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (ptr == NULL) {
    BLOSC_TRACE_ERROR("Invalid pointer argument for mmap read.");
    return 0;
  }

  if (size < 0 || nitems < 0 || position < 0) {
    BLOSC_TRACE_ERROR("Invalid arguments for mmap read.");
    *ptr = NULL;
    return 0;
  }

  int64_t n_bytes_i64;
  if (!checked_mul_int64_nonneg(size, nitems, &n_bytes_i64)) {
    BLOSC_TRACE_ERROR("mmap read size overflow (size=%" PRId64 ", nitems=%" PRId64 ").", size, nitems);
    *ptr = NULL;
    return 0;
  }
  int64_t position_end_i64;
  if (!checked_add_int64_nonneg(position, n_bytes_i64, &position_end_i64)) {
    BLOSC_TRACE_ERROR("mmap read position overflow (position=%" PRId64 ", nbytes=%" PRId64 ").", position, n_bytes_i64);
    *ptr = NULL;
    return 0;
  }
  if ((uint64_t)position_end_i64 > SIZE_MAX) {
    BLOSC_TRACE_ERROR("mmap read end position does not fit in size_t (%" PRId64 ").", position_end_i64);
    *ptr = NULL;
    return 0;
  }

  size_t position_size = (size_t)position;
  size_t position_end = (size_t)position_end_i64;
  if (position_end > mmap_file->file_size) {
    BLOSC_TRACE_ERROR("Cannot read beyond the end of the memory-mapped file.");
    *ptr = NULL;
    return 0;
  }

  *ptr = mmap_file->addr + position_size;

  return nitems;
}

int blosc2_stdio_mmap_truncate(void *stream, int64_t size) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (size < 0) {
    BLOSC_TRACE_ERROR("Cannot truncate mmap file to negative size (%" PRId64 ").", size);
    return -1;
  }

  if ((uint64_t)size > SIZE_MAX) {
    BLOSC_TRACE_ERROR("Cannot truncate mmap file to size beyond size_t range (%" PRId64 ").", size);
    return -1;
  }

  size_t target_size = (size_t)size;

  if (mmap_file->file_size == target_size) {
    return 0;
  }

  mmap_file->file_size = target_size;

  /* No file operations in c mode */
  if (mmap_file->is_memory_only) {
    return 0;
  }

#if defined(_WIN32)
  /* On Windows, we can truncate the file only at the end after we released the mapping */
  return 0;
#else
  return ftruncate(mmap_file->fd, size);
#endif
}

int blosc2_stdio_mmap_destroy(void* params) {
  if (params == NULL) {
    BLOSC_TRACE_ERROR("Invalid arguments for memory-mapped destroy.");
    return -1;
  }
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  int err = 0;

#if defined(_WIN32)
  if (mmap_file->addr == NULL || mmap_file->mmap_handle == INVALID_HANDLE_VALUE || mmap_file->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid memory-mapped state during destroy.");
    err = -1;
    goto cleanup;
  }

  if (mmap_file->access_flags == PAGE_READWRITE) {
    /* Ensure modified pages are written to disk */
    if (!FlushViewOfFile(mmap_file->addr, mmap_file->file_size)) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot flush the memory-mapped view to disk.");
      err = -1;
    }
    HANDLE file_handle = (HANDLE) _get_osfhandle(mmap_file->fd);
    if (!FlushFileBuffers(file_handle)) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot flush the memory-mapped file to disk.");
      err = -1;
    }
  }

  if (!UnmapViewOfFile(mmap_file->addr)) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file.");
    err = -1;
  }
  if (!CloseHandle(mmap_file->mmap_handle)) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Cannot close the handle to the memory-mapped file.");
    err = -1;
  }
  int64_t file_size_i64;
  if (!checked_size_t_to_int64(mmap_file->file_size, &file_size_i64)) {
    BLOSC_TRACE_ERROR("Cannot extend the file size to %zu bytes: value exceeds int64_t.", mmap_file->file_size);
    err = -1;
  }
  else {
    int rc = _chsize_s(mmap_file->fd, (long long)file_size_i64);
    if (rc != 0) {
      BLOSC_TRACE_ERROR(
        "Cannot extend the file size to %zu bytes (error: %s).", mmap_file->file_size, strerror(errno));
      err = -1;
    }
  }
#else
  if (mmap_file->addr == NULL || mmap_file->file == NULL) {
    BLOSC_TRACE_ERROR("Invalid memory-mapped state during destroy.");
    err = -1;
    goto cleanup;
  }

  if ((mmap_file->access_flags & PROT_WRITE) && !mmap_file->is_memory_only) {
    /* Ensure modified pages are written to disk */
    /* This is important since not every munmap implementation flushes modified pages to disk
    (e.g.: https://nfs.sourceforge.net/#faq_d8) */
    int rc = msync(mmap_file->addr, mmap_file->file_size, MS_SYNC);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Cannot sync the memory-mapped file to disk (error: %s).", strerror(errno));
      err = -1;
    }
  }

  if (munmap(mmap_file->addr, mmap_file->mapping_size) < 0) {
    BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file (error: %s).", strerror(errno));
    err = -1;
  }
#endif
cleanup:
  /* Also closes the HANDLE on Windows */
  if (mmap_file->file != NULL && fclose(mmap_file->file) < 0) {
    BLOSC_TRACE_ERROR("Could not close the memory-mapped file.");
    err = -1;
  }
  mmap_file->file = NULL;
  mmap_file->addr = NULL;

  free(mmap_file->urlpath);
  mmap_file->urlpath = NULL;
  if (mmap_file->needs_free) {
    free(mmap_file);
  }

  return err;
}

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
#include <errno.h>
#include <inttypes.h>

#if defined(_WIN32)
  #include <memoryapi.h>
#else
  #include <sys/mman.h>
#endif


void *blosc2_stdio_open(const char *urlpath, const char *mode, void *params) {
  BLOSC_UNUSED_PARAM(params);
  FILE *file = fopen(urlpath, mode);
  if (file == NULL)
    return NULL;
  blosc2_stdio_file *my_fp = malloc(sizeof(blosc2_stdio_file));
  my_fp->file = file;
  return my_fp;
}

int blosc2_stdio_close(void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  int err = fclose(my_fp->file);
  free(my_fp);
  return err;
}

int64_t blosc2_stdio_size(void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;

  fseek(my_fp->file, 0, SEEK_END);
  int64_t size = ftell(my_fp->file);
  fseek(my_fp->file, 0, SEEK_SET);

  return size;
}

int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  fseek(my_fp->file, position, SEEK_SET);

  size_t nitems_ = fwrite(ptr, (size_t) size, (size_t) nitems, my_fp->file);
  return (int64_t) nitems_;
}

int64_t blosc2_stdio_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  fseek(my_fp->file, position, SEEK_SET);

  void* data_ptr = *ptr;
  size_t nitems_ = fread(data_ptr, (size_t) size, (size_t) nitems, my_fp->file);
  return (int64_t) nitems_;
}

int blosc2_stdio_truncate(void *stream, int64_t size) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  int rc;
#if defined(_MSC_VER)
  rc = _chsize_s(_fileno(my_fp->file), size);
#else
  rc = ftruncate(fileno(my_fp->file), size);
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

  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  if (mmap_file->addr != NULL) {
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
  mmap_file->urlpath = malloc(strlen(urlpath) + 1);
  strcpy(mmap_file->urlpath, urlpath);

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
    return NULL;
  }
#endif

  mmap_file->file = fopen(urlpath, open_mode);
  if (mmap_file->file == NULL) {
    BLOSC_TRACE_ERROR("Cannot open the file %s with mode %s.", urlpath, open_mode);
    return NULL;
  }

  /* Retrieve the size of the file */
  fseek(mmap_file->file, 0, SEEK_END);
  mmap_file->file_size = ftell(mmap_file->file);
  fseek(mmap_file->file, 0, SEEK_SET);

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

#if defined(_WIN32)
  mmap_file->fd = _fileno(mmap_file->file);

  /* Windows automatically expands the file size to the memory mapped file
  (https://learn.microsoft.com/en-us/windows/win32/memory/creating-a-file-mapping-object).
  In general, the size of the file is directly connected to the size of the mapping and cannot change. We cut the
  file size to the target size in the end after we close the mapping */
  HANDLE file_handle = (HANDLE) _get_osfhandle(mmap_file->fd);
  DWORD size_hi = (DWORD)(mmap_file->mapping_size >> 32);
  DWORD size_lo = (DWORD)(mmap_file->mapping_size & 0xFFFFFFFF);
  mmap_file->mmap_handle = CreateFileMapping(file_handle, NULL, mmap_file->access_flags, size_hi, size_lo, NULL);
  if (mmap_file->mmap_handle == NULL) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Creating the memory mapping failed for the file %s.", urlpath);
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
    return NULL;
  }
#endif

  BLOSC_INFO(
    "Opened memory-mapped file %s in mode %s with an mapping size of %" PRId64 " bytes.",
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
  return mmap_file->file_size;
}

int64_t blosc2_stdio_mmap_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (position < 0) {
    BLOSC_TRACE_ERROR("Cannot write to a negative position.");
    return 0;
  }

  int64_t n_bytes = size * nitems;
  if (n_bytes == 0) {
    return 0;
  }

  int64_t position_end = position + n_bytes;
  int64_t new_size = position_end > mmap_file->file_size ? position_end : mmap_file->file_size;

#if defined(_WIN32)
  if (mmap_file->file_size < new_size) {
    mmap_file->file_size = new_size;
  }

  if (mmap_file->mapping_size < mmap_file->file_size) {
    mmap_file->mapping_size = mmap_file->file_size * 2;

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
    DWORD size_hi = (DWORD)(mmap_file->mapping_size >> 32);
    DWORD size_lo = (DWORD)(mmap_file->mapping_size & 0xFFFFFFFF);
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
      int rc = ftruncate(mmap_file->fd, new_size);
      if (rc < 0) {
        BLOSC_TRACE_ERROR("Cannot extend the file size to %" PRId64 " bytes (error: %s).", new_size, strerror(errno));
        return 0;
      }
    }
  }

  if (mmap_file->mapping_size < mmap_file->file_size) {
    int64_t old_mapping_size = mmap_file->mapping_size;
    mmap_file->mapping_size = mmap_file->file_size * 2;

#if defined(__linux__)
    /* mremap is the best option as it also ensures that the old data is still available in c mode. Unfortunately, it
    is no POSIX standard and only available on Linux */
    char* new_address = mremap(mmap_file->addr, old_mapping_size, mmap_file->mapping_size, MREMAP_MAYMOVE);
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
      mmap_file->mapping_size,
      mmap_file->access_flags,
      mmap_file->map_flags | MAP_FIXED,
      mmap_file->fd,
      offset
    );
#endif

    if (new_address == MAP_FAILED) {
      BLOSC_TRACE_ERROR("Cannot remap the memory-mapped file (error: %s).", strerror(errno));
      if (munmap(mmap_file->addr, mmap_file->mapping_size) < 0) {
        BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file (error: %s).", strerror(errno));
      }

      return 0;
    }

    mmap_file->addr = new_address;
  }
#endif

  memcpy(mmap_file->addr + position, ptr, n_bytes);
  return nitems;
}

int64_t blosc2_stdio_mmap_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (position < 0) {
    BLOSC_TRACE_ERROR("Cannot read from a negative position.");
    *ptr = NULL;
    return 0;
  }

  if (position + size * nitems > mmap_file->file_size) {
    BLOSC_TRACE_ERROR("Cannot read beyond the end of the memory-mapped file.");
    *ptr = NULL;
    return 0;
  }

  *ptr = mmap_file->addr + position;

  return nitems;
}

int blosc2_stdio_mmap_truncate(void *stream, int64_t size) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (mmap_file->file_size == size) {
    return 0;
  }

  mmap_file->file_size = size;

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
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  int err = 0;

#if defined(_WIN32)
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
  int rc = _chsize_s(mmap_file->fd, mmap_file->file_size);
  if (rc != 0) {
    BLOSC_TRACE_ERROR(
      "Cannot extend the file size to %" PRId64 " bytes (error: %s).", mmap_file->file_size, strerror(errno));
    err = -1;
  }
#else
  /* Ensure modified pages are written to disk */
  /* This is important since not every munmap implementation flushes modified pages to disk
  (e.g.: https://nfs.sourceforge.net/#faq_d8) */
  int rc = msync(mmap_file->addr, mmap_file->file_size, MS_SYNC);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Cannot sync the memory-mapped file to disk (error: %s).", strerror(errno));
    err = -1;
  }

  if (munmap(mmap_file->addr, mmap_file->mapping_size) < 0) {
    BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file (error: %s).", strerror(errno));
    err = -1;
  }
#endif
  /* Also closes the HANDLE on Windows */
  if (fclose(mmap_file->file) < 0) {
    BLOSC_TRACE_ERROR("Could not close the memory-mapped file.");
    err = -1;
  }

  free(mmap_file->urlpath);
  if (mmap_file->needs_free) {
    free(mmap_file);
  }

  return err;
}

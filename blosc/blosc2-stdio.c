/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "blosc2/blosc2-stdio.h"
#include "blosc2.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

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

int64_t blosc2_stdio_tell(void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  int64_t pos;
#if defined(_MSC_VER)
  pos = _ftelli64(my_fp->file);
#else
  pos = (int64_t)ftell(my_fp->file);
#endif
  return pos;
}

int blosc2_stdio_seek(void *stream, int64_t offset, int whence) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  int rc;
#if defined(_MSC_VER)
  rc = _fseeki64(my_fp->file, offset, whence);
#else
  rc = fseek(my_fp->file, (long) offset, whence);
#endif
  return rc;
}

int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;

  size_t nitems_ = fwrite(ptr, (size_t) size, (size_t) nitems, my_fp->file);
  return (int64_t) nitems_;
}

int64_t blosc2_stdio_read(void **ptr, int64_t size, int64_t nitems, void *stream) {
  void* data_ptr = *ptr;
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
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
    /* A memory-mapped file is only opened once */
    mmap_file->offset = 0;
    return mmap_file;
  }

  /* mmap_file->mode mapping is similar to Numpy's memmap (https://github.com/numpy/numpy/blob/main/numpy/_core/memmap.py) and CPython (https://github.com/python/cpython/blob/main/Modules/mmapmodule.c) */
#if defined(_WIN32)
  char* open_mode;
  bool use_initial_mapping_size;
  if (strcmp(mmap_file->mode, "r") == 0) {
    mmap_file->access_flags = PAGE_READONLY;
    mmap_file->map_flags = FILE_MAP_READ;
    open_mode = "rb";
    use_initial_mapping_size = false;
  } else if (strcmp(mmap_file->mode, "r+") == 0) {
    mmap_file->access_flags = PAGE_READWRITE;
    mmap_file->map_flags = FILE_MAP_WRITE;
    open_mode = "rb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "w+") == 0) {
    mmap_file->access_flags = PAGE_READWRITE;
    mmap_file->map_flags = FILE_MAP_WRITE;
    open_mode = "wb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "c") == 0) {
    mmap_file->access_flags = PAGE_WRITECOPY;
    mmap_file->map_flags = FILE_MAP_COPY;
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
    open_mode = "rb";
    use_initial_mapping_size = false;
  } else if (strcmp(mmap_file->mode, "r+") == 0) {
    mmap_file->access_flags = PROT_READ | PROT_WRITE;
    mmap_file->map_flags = MAP_SHARED;
    open_mode = "rb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "w+") == 0) {
    mmap_file->access_flags = PROT_READ | PROT_WRITE;
    mmap_file->map_flags = MAP_SHARED;
    open_mode = "wb+";
    use_initial_mapping_size = true;
  } else if (strcmp(mmap_file->mode, "c") == 0) {
    mmap_file->access_flags = PROT_READ | PROT_WRITE;
    mmap_file->map_flags = MAP_PRIVATE;
    open_mode = "rb";
    use_initial_mapping_size = true;
  } else {
    BLOSC_TRACE_ERROR("Mode %s not supported for memory-mapped files.", mmap_file->mode);
    return NULL;
  }
#endif

  mmap_file->file = fopen(urlpath, open_mode);
  if (mmap_file->file == NULL)
    return NULL;

  /* Retrieve the size of the file */
  fseek(mmap_file->file, 0, SEEK_END);
  mmap_file->file_size = ftell(mmap_file->file);
  fseek(mmap_file->file, 0, SEEK_SET);

  /* The size of the mapping must be > 0 so we are using a large enough buffer for writing (which will be increased later if needed) */
  if (use_initial_mapping_size)
    mmap_file->mapping_size = mmap_file->initial_mapping_size;
  else
    mmap_file->mapping_size = mmap_file->file_size;

  if (mmap_file->file_size > mmap_file->mapping_size)
    mmap_file->mapping_size = mmap_file->file_size;

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
  mmap_file->addr = (char*) MapViewOfFile(mmap_file->mmap_handle, mmap_file->map_flags, offset, offset, mmap_file->mapping_size);
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

  /* Offset where the mapping should start (different to mmap_file->offset which denotes the current position and may change) */
  int64_t offset = 0;
  mmap_file->addr = mmap(NULL, mmap_file->mapping_size, mmap_file->access_flags, mmap_file->map_flags, mmap_file->fd, offset);
  if (mmap_file->addr == MAP_FAILED) {
    BLOSC_TRACE_ERROR("Memory mapping failed for file %s.", urlpath);
    return NULL;
  }
#endif

  return mmap_file;
}

int blosc2_stdio_mmap_close(void *stream) {
  BLOSC_UNUSED_PARAM(stream);
  return 0;
}

int64_t blosc2_stdio_mmap_tell(void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;
  return mmap_file->offset;
}

int blosc2_stdio_mmap_seek(void *stream, int64_t offset, int whence) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  switch (whence) {
    case SEEK_SET:
      mmap_file->offset = offset;
      break;
    case SEEK_CUR:
      mmap_file->offset += offset;
      break;
    case SEEK_END:
      mmap_file->offset = mmap_file->file_size + offset;
      break;
    default:
      BLOSC_TRACE_ERROR("Invalid whence %d argument.", whence);
      return -1;
  }

  if (mmap_file->offset < 0) {
    BLOSC_TRACE_ERROR("Cannot seek to a negative offset.");
    return -1;
  }
  
  return 0;
}

int64_t blosc2_stdio_mmap_write(const void *ptr, int64_t size, int64_t nitems, void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  int64_t n_bytes = size * nitems;
  if (n_bytes == 0) {
    return 0;
  }

  int64_t new_size = mmap_file->offset + n_bytes;

#if defined(_WIN32)
  if (strcmp(mmap_file->mode, "c") != 0 && mmap_file->file_size < new_size) {
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
    char* new_address = (char*) MapViewOfFile(mmap_file->mmap_handle, mmap_file->map_flags, offset, offset, mmap_file->mapping_size);
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

  memcpy(mmap_file->addr + mmap_file->offset, ptr, n_bytes);

  /* Ensure modified pages are written to disk */
  if (!FlushViewOfFile(mmap_file->addr + mmap_file->offset, n_bytes)) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Cannot flush the memory-mapped file to disk.");
    return 0;
  }
  mmap_file->offset += n_bytes;
#else
  if (strcmp(mmap_file->mode, "c") != 0 && mmap_file->file_size < new_size) {
    int rc = ftruncate(mmap_file->fd, new_size);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Cannot extend the file size to %ld bytes (error: %s).", new_size, strerror(errno));
      return 0;
    }

    mmap_file->file_size = new_size;
  }

  if (mmap_file->mapping_size < mmap_file->file_size) {
    mmap_file->mapping_size = mmap_file->file_size * 2;

    /* Extend the current mapping with the help of MAP_FIXED */
    int64_t offset = 0;
    char* new_address = mmap(mmap_file->addr, mmap_file->mapping_size, mmap_file->access_flags, mmap_file->map_flags | MAP_FIXED, mmap_file->fd, offset);
    if (new_address == MAP_FAILED) {
      BLOSC_TRACE_ERROR("Cannot remap the memory-mapped file (error: %s).", strerror(errno));
      if (munmap(mmap_file->addr, mmap_file->mapping_size) < 0) {
        BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file (error: %s).", strerror(errno));
      }

      return 0;
    }

    mmap_file->addr = new_address;
  }
  
  memcpy(mmap_file->addr + mmap_file->offset, ptr, n_bytes);

  /* Ensure modified pages are written to disk */
  int rc = msync(mmap_file->addr, mmap_file->file_size, MS_ASYNC);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Cannot sync the memory-mapped file to disk (error: %s).", strerror(errno));
    return 0;
  }
  mmap_file->offset += n_bytes;
#endif

  return nitems;
}

int64_t blosc2_stdio_mmap_read(void **ptr, int64_t size, int64_t nitems, void *stream) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (mmap_file->offset + size * nitems > mmap_file->file_size) {
    BLOSC_TRACE_ERROR("Cannot read beyond the end of the memory-mapped file.");
    *ptr = NULL;
    return 0;
  }

  *ptr = mmap_file->addr + mmap_file->offset;

  return nitems;
}

int blosc2_stdio_mmap_truncate(void *stream, int64_t size) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (mmap_file->file_size == size)
    return 0;

  mmap_file->file_size = size;

  /* No file operations in c mode */
  if (strcmp(mmap_file->mode, "c") == 0)
    return 0;

#if defined(_MSC_VER)
  /* On Windows, we can truncate the file only at the end after we released the mapping */
  return 0;
#else
  return ftruncate(mmap_file->fd, size);
#endif
}

int blosc2_stdio_mmap_free(void* params) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  int err = 0;

#if defined(_WIN32)
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
    BLOSC_TRACE_ERROR("Cannot extend the file size to %lld bytes (error: %s).", mmap_file->file_size, strerror(errno));
    err = -1;
  }
#else
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

  if (mmap_file->needs_free)
    free(mmap_file);

  return err;
}

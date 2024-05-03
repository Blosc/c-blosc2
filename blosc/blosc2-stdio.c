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

#if defined(_MSC_VER)
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

#if defined(_MSC_VER)
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
    return mmap_file;
  }

#if defined(_MSC_VER)
  DWORD protect_flags;
  DWORD access_flags;
  char* open_mode;
  if (strstr(mmap_file->mode, "r") != NULL) {
    protect_flags = PAGE_READONLY;
    access_flags = FILE_MAP_READ;
    open_mode = "rb";
  } else if (strstr(mmap_file->mode, "r+") != NULL) {
    protect_flags = PAGE_READWRITE;
    access_flags = FILE_MAP_WRITE;
    open_mode = "rb+";
  } else if (strstr(mmap_file->mode, "w+") != NULL) {
    protect_flags = PAGE_READWRITE;
    access_flags = FILE_MAP_WRITE;
    open_mode = "wb+";
  } else if (strstr(mmap_file->mode, "c") != NULL) {
    protect_flags = PAGE_WRITECOPY;
    access_flags = FILE_MAP_COPY;
    open_mode = "rb";
  } else {
    BLOSC_TRACE_ERROR("Mode %s not supported for memory-mapped files.", mmap_file->mode);
    return NULL;
  }

  mmap_file->file = fopen(urlpath, open_mode);
  if (mmap_file->file == NULL)
    return NULL;

  mmap_file->fd = _fileno(mmap_file->file);

  /* Retrieve the size of the file */
  fseek(mmap_file->file, 0, SEEK_END);
  mmap_file->size = ftell(mmap_file->file);
  fseek(mmap_file->file, 0, SEEK_SET);

  if (mmap_file->size == 0) {
    /* The length of the file mapping must be > 0 */
    mmap_file->size = 1;
  }

  /* Similar to https://github.com/python/cpython/blob/main/Modules/mmapmodule.c */
  DWORD size_hi = (DWORD)(mmap_file->size >> 32);
  DWORD size_lo = (DWORD)(mmap_file->size & 0xFFFFFFFF);
  HANDLE file_handle = (HANDLE) _get_osfhandle(mmap_file->fd);
  mmap_file->mmap_handle = CreateFileMapping(file_handle, NULL, protect_flags, size_hi, size_lo, NULL);
  if (mmap_file->mmap_handle == NULL) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Memory mapping failed for the file %s.", urlpath);
    return NULL;
  }

  DWORD offset = 0;
  mmap_file->addr = (char*) MapViewOfFile(mmap_file->mmap_handle, access_flags, offset, offset, mmap_file->size);
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
  /* mmap_mode mapping is similar to Numpy's memmap (https://github.com/numpy/numpy/blob/main/numpy/_core/memmap.py) and CPython (https://github.com/python/cpython/blob/main/Modules/mmapmodule.c) */
  int prot;
  int flags;
  char* open_mode;
  if (strstr(mmap_file->mode, "r") != NULL) {
    prot = PROT_READ;
    flags = MAP_SHARED;
    open_mode = "rb";
  } else if (strstr(mmap_file->mode, "r+") != NULL) {
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_SHARED;
    open_mode = "rb+";
  } else if (strstr(mmap_file->mode, "w+") != NULL) {
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_SHARED;
    open_mode = "wb+";
  } else if (strstr(mmap_file->mode, "c") != NULL) {
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_PRIVATE;
    open_mode = "rb";
  } else {
    BLOSC_TRACE_ERROR("Mode %s not supported for memory-mapped files.", mmap_file->mode);
    return NULL;
  }

  mmap_file->file = fopen(urlpath, open_mode);
  if (mmap_file->file == NULL)
    return NULL;

  mmap_file->fd = fileno(mmap_file->file);

  /* Retrieve the size of the file */
  fseek(mmap_file->file, 0, SEEK_END);
  mmap_file->size = ftell(mmap_file->file);
  fseek(mmap_file->file, 0, SEEK_SET);
  
  if (mmap_file->size == 0) {
    /* The length of the file mapping must be > 0 */
    mmap_file->size = 1;
  }

  /* Offset where the mapping should start (different to mmap_file->offset which denotes the current position and may change) */
  int64_t offset = 0;

  mmap_file->addr = mmap(NULL, mmap_file->size, prot, flags, mmap_file->fd, offset);
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
      mmap_file->offset = mmap_file->size + offset;
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

  /* The writing routine is currently not optimized, neither for POSIX nor for Windows (even though probably worse
   on Windows). In all cases, we remap everything on every write request (because the mapping is static) which may be slow or has unwanted side
   effects. An improved solution could increment the memory mapping in larger chunks independent of the actual file size:
   https://stackoverflow.com/a/6098864 */

#if defined(_MSC_VER)
  if (mmap_file->size < new_size) {
    int rc = _chsize_s(mmap_file->fd, size);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Cannot extend the file size to %lld bytes (error: %s).", new_size, strerror(errno));
      return 0;
    }

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

    DWORD protect_flags = strstr(mmap_file->mode, "c") ? PAGE_WRITECOPY : PAGE_READWRITE;
    HANDLE file_handle = (HANDLE) _get_osfhandle(mmap_file->fd);
    DWORD size_hi = (DWORD)(new_size >> 32);
    DWORD size_lo = (DWORD)(new_size & 0xFFFFFFFF);
    mmap_file->mmap_handle = CreateFileMapping(file_handle, NULL, protect_flags, size_hi, size_lo, NULL);
    if (mmap_file->mmap_handle == NULL) {
      _print_last_error();
      BLOSC_TRACE_ERROR("Cannot remapt the memory-mapped file.");
      return 0;
    }

    int access_flags = strstr(mmap_file->mode, "c") ? FILE_MAP_COPY : FILE_MAP_WRITE;
    DWORD offset = 0;
    char* new_address = (char*) MapViewOfFile(mmap_file->mmap_handle, access_flags, offset, offset, new_size);
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
    mmap_file->size = new_size;
  }
  
  memcpy(mmap_file->addr + mmap_file->offset, ptr, n_bytes);

  /* Ensure modified pages are written to disk */
  if (!FlushViewOfFile(mmap_file->addr, mmap_file->size)) {
    _print_last_error();
    BLOSC_TRACE_ERROR("Cannot flush the memory-mapped file to disk.");
    return 0;
  }
  mmap_file->offset += n_bytes;
#else
  if (mmap_file->size < new_size) {
    int rc = ftruncate(mmap_file->fd, new_size);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Cannot extend the file size to %ld bytes (error: %s).", new_size, strerror(errno));
      return 0;
    }

    /* Extend the current mapping */
    int flags = strstr(mmap_file->mode, "c") ? MAP_PRIVATE : MAP_SHARED;
    int64_t offset = 0;
    char* new_address = mmap(mmap_file->addr, new_size, PROT_READ | PROT_WRITE, flags | MAP_FIXED, mmap_file->fd, offset);
    if (new_address == MAP_FAILED) {
      BLOSC_TRACE_ERROR("Cannot remap the memory-mapped file (error: %s).", strerror(errno));
      if (munmap(mmap_file->addr, mmap_file->size) < 0) {
        BLOSC_TRACE_ERROR("Cannot unmap the memory-mapped file (error: %s).", strerror(errno));
      }

      return 0;
    }

    mmap_file->addr = new_address;
    mmap_file->size = new_size;
  }
  
  memcpy(mmap_file->addr + mmap_file->offset, ptr, n_bytes);

  /* Ensure modified pages are written to disk */
  int rc = msync(mmap_file->addr, mmap_file->size, MS_SYNC);
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

  if (mmap_file->offset + size * nitems > mmap_file->size) {
    BLOSC_TRACE_ERROR("Cannot read beyond the end of the memory-mapped file.");
    *ptr = NULL;
    return 0;
  }

  *ptr = mmap_file->addr + mmap_file->offset;

  return nitems;
}

int blosc2_stdio_mmap_truncate(void *stream, int64_t size) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (mmap_file->size == size) {
    return 0;
  }

  mmap_file->size = size;

#if defined(_MSC_VER)
  return _chsize_s(mmap_file->fd, size);
#else
  return ftruncate(mmap_file->fd, size);
#endif
}

int blosc2_stdio_mmap_free(void* params) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  int err = 0;

#if defined(_MSC_VER)
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
#else
  if (munmap(mmap_file->addr, mmap_file->size) < 0) {
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

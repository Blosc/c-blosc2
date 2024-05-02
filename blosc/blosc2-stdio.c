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
#include <sys/mman.h>


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


void *blosc2_stdio_mmap_open(const char *urlpath, const char *mode, void* params) {
  BLOSC_UNUSED_PARAM(mode);

  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;
  if (mmap_file->addr != NULL) {
    /* A memory mapped file is only opened once */
    return mmap_file;
  }

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
    BLOSC_TRACE_ERROR("Mode %s not supported for memory mapped files.", mmap_file->mode);
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
  if (mmap_file->size < new_size) {
    int rc = ftruncate(mmap_file->fd, new_size);
    if (rc < 0) {
      BLOSC_TRACE_ERROR("Cannot extend the file size to %ld bytes (error: %s).", new_size, strerror(errno));
      return 0;
    }

    /* Extend the current mapping */
    int flags = strstr(mmap_file->mode, "c") ? MAP_PRIVATE : MAP_SHARED;
    int64_t offset = 0;
    void* new_address = mmap(mmap_file->addr, new_size, PROT_READ | PROT_WRITE, flags | MAP_FIXED, mmap_file->fd, offset);
    if (new_address == MAP_FAILED) {
      BLOSC_TRACE_ERROR("Cannot remap the memory mapped file (error: %s).", strerror(errno));
      return 0;
    }

    mmap_file->addr = new_address;
    mmap_file->size = new_size;
  }
  
  memcpy(mmap_file->addr + mmap_file->offset, ptr, n_bytes);

  /* Ensure modified pages are written to disk */
  int rc = msync(mmap_file->addr, mmap_file->size, MS_SYNC);
  if (rc < 0) {
    BLOSC_TRACE_ERROR("Cannot sync the memory mapped file to disk (error: %s).", strerror(errno));
    return 0;
  }
  mmap_file->offset += n_bytes;

  return nitems;
}

int64_t blosc2_stdio_mmap_read(void **ptr, int64_t size, int64_t nitems, void *stream) {
  BLOSC_UNUSED_PARAM(size);
  BLOSC_UNUSED_PARAM(nitems);
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  *ptr = mmap_file->addr + mmap_file->offset;

  return nitems;
}

int blosc2_stdio_mmap_truncate(void *stream, int64_t size) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) stream;

  if (mmap_file->size == size) {
    return 0;
  }

  mmap_file->size = size;
  return ftruncate(mmap_file->fd, size);
}

int blosc2_stdio_mmap_free(void* params) {
  blosc2_stdio_mmap *mmap_file = (blosc2_stdio_mmap *) params;

  if (munmap(mmap_file->addr, mmap_file->size) < 0) {
    BLOSC_TRACE_ERROR("Cannot unmap the memory mapped file (error: %s).", strerror(errno));
    if (mmap_file->needs_free)
      free(mmap_file);
    return -1;
  }

  int err = fclose(mmap_file->file);
  if (mmap_file->needs_free)
    free(mmap_file);

  return err;
}

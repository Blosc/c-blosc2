/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_BLOSC2_BLOSC2_STDIO_H
#define BLOSC_BLOSC2_BLOSC2_STDIO_H

#include "blosc2-export.h"

#if defined(_MSC_VER)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#if defined(_MSC_VER)
#include <Windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  FILE *file;
} blosc2_stdio_file;

BLOSC_EXPORT void *blosc2_stdio_open(const char *urlpath, const char *mode, void* params);
BLOSC_EXPORT int blosc2_stdio_close(void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_tell(void *stream);
BLOSC_EXPORT int blosc2_stdio_seek(void *stream, int64_t offset, int whence);
BLOSC_EXPORT int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_read(void **ptr, int64_t size, int64_t nitems, void *stream);
BLOSC_EXPORT int blosc2_stdio_truncate(void *stream, int64_t size);


typedef struct {
  char* addr;
  //!< The starting address of the mapping.
  int64_t size;
  //!< The size of the mapping.
  int64_t offset;
  //!< The current position inside the mapping.
  FILE* file;
  //!< The underlying filehandle.
  int fd;
  //!< The underlying file descriptor.
  const char* mode;
  //!< The opening mode of the memory-mapped file (r, r+, w+ or c).
  bool needs_free;
  //!< Indicates whether this object should be freed in the blosc2_free_cb callback.
#if defined(_MSC_VER)
  HANDLE mmap_handle;
  //!< The Windows handle to the memory mapping.
#endif
} blosc2_stdio_mmap;

BLOSC_EXPORT void *blosc2_stdio_mmap_open(const char *urlpath, const char *mode, void* params);
BLOSC_EXPORT int blosc2_stdio_mmap_close(void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_mmap_tell(void *stream);
BLOSC_EXPORT int blosc2_stdio_mmap_seek(void *stream, int64_t offset, int whence);
BLOSC_EXPORT int64_t blosc2_stdio_mmap_write(const void *ptr, int64_t size, int64_t nitems, void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_mmap_read(void **ptr, int64_t size, int64_t nitems, void *stream);
BLOSC_EXPORT int blosc2_stdio_mmap_truncate(void *stream, int64_t size);
BLOSC_EXPORT int blosc2_stdio_mmap_free(void* params);

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_BLOSC2_BLOSC2_STDIO_H */

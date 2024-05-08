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

#if defined(_WIN32)
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
BLOSC_EXPORT int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream);
BLOSC_EXPORT int blosc2_stdio_truncate(void *stream, int64_t size);


/**
 * @brief Parameters for memory-mapped I/O.
 */
typedef struct {
  /* Arguments of the mapping */
  const char* mode;
  //!< The opening mode of the memory-mapped file (r, r+, w+ or c) similar to Numpy's np.memmap
  //!< (https://numpy.org/doc/stable/reference/generated/numpy.memmap.html). Set to r if the file should only be read,
  //!< r+ if you want to extend data to an existing file, w+ to create a new file and c to use an existing file as basis
  //!<  but keep all modifications in-memory. On Windows, the file size cannot change in the c mode.
  int64_t initial_mapping_size;
  //!< The initial size of the memory mapping used as a large enough write buffer for the r+, w+ and c modes (for
  //!< Windows, only the r+ and w+ modes).
  bool needs_free;
  //!< Indicates whether this object should be freed in the blosc2_destroy_cb callback (set to true if the
  //!< blosc2_stdio_mmap struct was created on the heap).

  /* Internal attributes of the mapping */
  char* addr;
  //!< The starting address of the mapping.
  int64_t file_size;
  //!< The size of the file.
  int64_t mapping_size;
  //!< The size of the mapping (mapping_size >= file_size).
  int64_t offset;
  //!< The current position inside the mapping.
  FILE* file;
  //!< The underlying file handle.
  int fd;
  //!< The underlying file descriptor.
  int64_t access_flags;
  //!< The access attributes for the memory pages.
  int64_t map_flags;
  //!< The attributes of the mapping.
#if defined(_WIN32)
  HANDLE mmap_handle;
  //!< The Windows handle to the memory mapping.
#endif
} blosc2_stdio_mmap;

/**
 * @brief Default struct for memory-mapped I/O for user initialization.
 */
static const blosc2_stdio_mmap BLOSC2_STDIO_MMAP_DEFAULTS = {
  "r", (1 << 30), false, NULL, -1, -1, 0, NULL, -1, -1, -1
#if defined(_WIN32)
  , INVALID_HANDLE_VALUE
#endif
};

BLOSC_EXPORT void *blosc2_stdio_mmap_open(const char *urlpath, const char *mode, void* params);
BLOSC_EXPORT int blosc2_stdio_mmap_close(void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_mmap_tell(void *stream);
BLOSC_EXPORT int blosc2_stdio_mmap_seek(void *stream, int64_t offset, int whence);
BLOSC_EXPORT int64_t blosc2_stdio_mmap_write(
  const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_mmap_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream);
BLOSC_EXPORT int blosc2_stdio_mmap_truncate(void *stream, int64_t size);
BLOSC_EXPORT int blosc2_stdio_mmap_destroy(void* params);

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_BLOSC2_BLOSC2_STDIO_H */

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef BLOSC_BLOSC2_STDIO_H
#define BLOSC_BLOSC2_STDIO_H


#include <stdio.h>
#include <stdlib.h>
#include "blosc2-export.h"


#if defined(_WIN32) && !defined(__MINGW32__)

/* stdint.h only available in VS2010 (VC++ 16.0) and newer */
   #if defined(_MSC_VER) && _MSC_VER < 1600
     #include "win32/stdint-windows.h"
   #else
     #include <stdint.h>
   #endif

#else
#include <stdint.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#include <io.h>
#else
#include <unistd.h>
#endif

typedef struct {
  FILE *file;
} blosc2_stdio_file;

BLOSC_EXPORT void *blosc2_stdio_open(const char *urlpath, const char *mode, void* params);
BLOSC_EXPORT int blosc2_stdio_close(void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_tell(void *stream);
BLOSC_EXPORT int blosc2_stdio_seek(void *stream, int64_t offset, int whence);
BLOSC_EXPORT int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, void *stream);
BLOSC_EXPORT int64_t blosc2_stdio_read(void *ptr, int64_t size, int64_t nitems, void *stream);
BLOSC_EXPORT int blosc2_stdio_truncate(void *stream, int64_t size);

#endif //BLOSC_BLOSC2_STDIO_H

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_BLOSC2_STDIO_H
#define BLOSC_BLOSC2_STDIO_H

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>

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

void *blosc2_stdio_open(const char *urlpath, const char *mode, void *params);
int blosc2_stdio_close(void *stream, void* params);
int blosc2_stdio_seek(void *stream, int64_t offset, int whence, void* params);
int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, void *stream, void *params);
int64_t blosc2_stdio_read(void *ptr, int64_t size, int64_t nitems, void *stream, void *params);

#endif //BLOSC_BLOSC2_STDIO_H

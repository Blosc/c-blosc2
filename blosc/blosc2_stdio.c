/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/
#include "blosc2_stdio.h"
#include <stdio.h>

void *blosc2_stdio_open(const char *urlpath, const char *mode, void *params) {
  return fopen(urlpath, mode);
}

int blosc2_stdio_close(void *stream, void* params) {
  return fclose(stream);
}

int blosc2_stdio_seek(void *stream, int64_t offset, int whence, void* params) {
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
  return _fseeki64(stream, offset, whence);
#else
  return fseek(stream, (long) offset, whence);
#endif
}

size_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, void *stream, void *params) {
  size_t nitems_ = fwrite(ptr, (size_t) size, (size_t) nitems, stream);
  return nitems_;
}

size_t blosc2_stdio_read(void *ptr, int64_t size, int64_t nitems, void *stream, void *params) {
  size_t nitems_ = fread(ptr, (size_t) size, (size_t) nitems, stream);
  return nitems_;
}

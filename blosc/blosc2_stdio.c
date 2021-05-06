/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/
#include "blosc2_stdio.h"

void *blosc2_stdio_open(const char *urlpath, const char *mode, void *params) {
  blosc2_stdio_file *my_fp = malloc(sizeof(blosc2_stdio_file));
  my_fp->file = fopen(urlpath, mode);
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
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
  return _ftelli64(my_fp->file);
#else
  long pos = ftell(my_fp->file);
  return (int64_t) pos;
#endif
}

int blosc2_stdio_seek(void *stream, int64_t offset, int whence) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
  return _fseeki64(my_fp->file, offset, whence);
#else
  return fseek(my_fp->file, (long) offset, whence);
#endif
}

int64_t blosc2_stdio_write(const void *ptr, int64_t size, int64_t nitems, void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;

  size_t nitems_ = fwrite(ptr, (size_t) size, (size_t) nitems, my_fp->file);
  return (int64_t) nitems_;
}

int64_t blosc2_stdio_read(void *ptr, int64_t size, int64_t nitems, void *stream) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;
  size_t nitems_ = fread(ptr, (size_t) size, (size_t) nitems, my_fp->file);
  return (int64_t) nitems_;
}

int blosc2_stdio_truncate(void *stream, int64_t size) {
  blosc2_stdio_file *my_fp = (blosc2_stdio_file *) stream;

  #if defined(_MSC_VER) && (_MSC_VER >= 1400)
    return _chsize_s( _fileno(my_fp->file), size);
  #else
    return ftruncate(fileno(my_fp->file), size);
  #endif
}

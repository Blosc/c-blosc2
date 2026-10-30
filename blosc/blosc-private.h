/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef BLOSC_BLOSC_PRIVATE_H
#define BLOSC_BLOSC_PRIVATE_H

#include "blosc2/blosc2-common.h"
#include "blosc2.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

/*********************************************************************

  Utility functions meant to be used internally.

*********************************************************************/

int blosc2_decompress_block_ctx(blosc2_context* context, const void* src,
                                int32_t srcsize, int32_t nblock, void* dest,
                                int32_t destsize);
int blosc2_run_parallel(int16_t nthreads, void (*dojob)(void *),
                        size_t jobdata_elsize, void *jobdata);

#define to_little(dest, src, itemsize)    endian_handler(true, dest, src, itemsize)
#define from_little(dest, src, itemsize)  endian_handler(true, dest, src, itemsize)
#define to_big(dest, src, itemsize)       endian_handler(false, dest, src, itemsize)
#define from_big(dest, src, itemsize)     endian_handler(false, dest, src, itemsize)


// Return true if platform is little endian; else false
static bool is_little_endian(void) {
  static const int i = 1;
  char* p = (char*)&i;

  if (p[0] == 1) {
    return true;
  }
  else {
    return false;
  }
}


static inline void endian_handler(bool little, void *dest, const void *pa, int size)
{
  bool little_endian = is_little_endian();
  if (little_endian == little) {
    memcpy(dest, pa, size);
  }
  else {
    uint8_t* pa_ = (uint8_t*)pa;
    uint8_t pa2_[8];
    switch (size) {
      case 8:
        pa2_[0] = pa_[7];
        pa2_[1] = pa_[6];
        pa2_[2] = pa_[5];
        pa2_[3] = pa_[4];
        pa2_[4] = pa_[3];
        pa2_[5] = pa_[2];
        pa2_[6] = pa_[1];
        pa2_[7] = pa_[0];
        break;
      case 4:
        pa2_[0] = pa_[3];
        pa2_[1] = pa_[2];
        pa2_[2] = pa_[1];
        pa2_[3] = pa_[0];
        break;
      case 2:
        pa2_[0] = pa_[1];
        pa2_[1] = pa_[0];
        break;
      case 1:
        pa2_[0] = pa_[0];
        break;
      default:
        BLOSC_TRACE_ERROR("Unhandled size: %d.", size);
    }
    memcpy(dest, pa2_, size);
  }
}

/*
 * Convert a chunk count to the serialized offsets payload size.
 *
 * Security rationale: frame metadata can be attacker-controlled, and the offsets payload
 * eventually flows into APIs that accept int32 byte lengths. This helper enforces a
 * single overflow-checked conversion so callers cannot accidentally truncate
 * nchunks * sizeof(int64_t) into a smaller signed length.
 */
static inline bool blosc2_nchunks_to_offsets_nbytes(int64_t nchunks, int32_t *off_nbytes) {
  const int64_t max_nchunks = INT32_MAX / (int64_t)sizeof(int64_t);
  if (nchunks < 0 || nchunks > max_nchunks) {
    return false;
  }
  if (off_nbytes != NULL) {
    *off_nbytes = (int32_t)(nchunks * (int64_t)sizeof(int64_t));
  }
  return true;
}

/* The flags2 byte lives in the extended header, at offset 0x1e, so it only
   exists when the chunk actually carries one.  Whether it does is recorded in
   the flags byte, the same way read_chunk_header() decides it -- not in the
   chunk size: a Blosc1-style chunk with 16 or more bytes of payload is 32 bytes
   or longer, yet offset 0x1e is compressed data there, and reading it as flags2
   can mark a schunk as using variable-length blocks out of thin air.  The size
   check stays as the bound that keeps a short chunk from being over-read. */
static inline uint8_t get_chunk_flags2(const uint8_t *chunk, int32_t chunk_cbytes) {
  uint8_t flags = chunk[BLOSC2_CHUNK_FLAGS];
  bool extended_header = (flags & BLOSC_DOSHUFFLE) && (flags & BLOSC_DOBITSHUFFLE);
  if (!extended_header || chunk_cbytes < BLOSC_EXTENDED_HEADER_LENGTH) {
    return 0;
  }
  return chunk[BLOSC2_CHUNK_BLOSC2_FLAGS2];
}

/* Whether the filter pipeline `filters` (BLOSC2_MAX_FILTERS codes) has filter `id` */
static inline bool filters_contain(const uint8_t *filters, uint8_t id) {
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    if (filters[i] == id) {
      return true;
    }
  }
  return false;
}

/* The special value (BLOSC2_SPECIAL_ZERO, _NAN or _UNINIT) that a frame codes
   `chunk` (`cbytes` long) with in its offset instead of storing it, or 0 if it
   stores the chunk.  A chunk shorter than an extended header has no special value. */
static inline int chunk_special_in_offset(const uint8_t *chunk, int32_t cbytes) {
  if (cbytes < BLOSC_EXTENDED_HEADER_LENGTH) {
    return 0;
  }
  int special_value = (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] >> 4) & BLOSC2_SPECIAL_MASK;
  switch (special_value) {
    case BLOSC2_SPECIAL_ZERO:
    case BLOSC2_SPECIAL_NAN:
    case BLOSC2_SPECIAL_UNINIT:
      return special_value;
    default:
      return 0;
  }
}

/* The chunksize of a super-chunk (-1 before its first chunk, 0 once its chunks
   vary in size) after adding a chunk of `nbytes` bytes at the end of (or, if not
   `at_end`, among) its `nchunks` chunks, the last of which has `last_nbytes`: the
   chunks stop having one size when the last one is shorter, or the new one is
   longer, or is not of that size and goes in between.  `last_nbytes` is only read
   when `*chunksize` is not 0 and `nchunks` > 0.  Returns false, and leaves
   *chunksize, for a first chunk longer than a chunksize set in advance. */
static inline bool chunksize_after_adding(int32_t *chunksize, int64_t nchunks, int32_t last_nbytes,
                                         int32_t nbytes, bool at_end) {
  if (*chunksize == 0) {
    return true;
  }
  int32_t size = (*chunksize == -1) ? nbytes : *chunksize;
  if (nchunks > 0 && (last_nbytes < size || nbytes > size || (!at_end && nbytes != size))) {
    *chunksize = 0;
    return true;
  }
  if (size > 0 && nbytes > size) {
    return false;
  }
  *chunksize = size;
  return true;
}

/* blosc2_schunk_update_chunk(schunk, nchunk, chunk, true) for a chunk in private memory of the
   caller, which keeps it: unlike a chunk that may point anywhere, a frame writes it out without
   copying it first.  It must not point into the storage of any frame (in memory or mapped). */
int64_t schunk_update_chunk_lent(blosc2_schunk *schunk, int64_t nchunk, uint8_t *chunk);

/* Decompress chunk `nchunk` of `schunk`, or `chunk` (`cbytes` long) if it is not NULL,
   with the decompression context of `schunk`, decoding only the blocks that `maskout`
   (`nblocks` items) leaves in and, if `postfilter` is not NULL, running it (with
   `user_data`) instead of the context's own postfilter.  The context gets its own
   postfilter back and loses the mask in any case.  Returns what the decompression
   returns. */
int schunk_decompress_masked(blosc2_schunk *schunk, int64_t nchunk, const uint8_t *chunk, int32_t cbytes,
                             const bool *maskout, int32_t nblocks, blosc2_postfilter_fn postfilter,
                             void *user_data, void *dest, int32_t destsize);

/* Copy 4 bytes from @p *pa to int32_t, changing endianness if necessary. */
static inline int32_t sw32_(const void* pa) {
  int32_t idest;

  bool little_endian = is_little_endian();
  if (little_endian) {
    memcpy(&idest, pa, sizeof(idest));
  }
  else {
#if defined (__GNUC__)
    return __builtin_bswap32(*(unsigned int *)pa);
#elif defined (_MSC_VER) /* Visual Studio */
    return _byteswap_ulong(*(unsigned int *)pa);
#else
    const uint8_t *pa_ = (const uint8_t *)pa;
    uint8_t *dest = (uint8_t *)&idest;
    dest[0] = pa_[3];
    dest[1] = pa_[2];
    dest[2] = pa_[1];
    dest[3] = pa_[0];
#endif
  }
  return idest;
}

/* Copy 4 bytes from int32_t to @p *dest, changing endianness if necessary. */
static inline void _sw32(void* dest, int32_t a) {
  uint8_t* dest_ = (uint8_t*)dest;
  uint8_t* pa = (uint8_t*)&a;

  bool little_endian = is_little_endian();
  if (little_endian) {
    memcpy(dest_, &a, sizeof(a));;
  }
  else {
#if defined (__GNUC__)
    *(int32_t *)dest_ = __builtin_bswap32(*(unsigned int *)pa);
#elif defined (_MSC_VER) /* Visual Studio */
    *(int32_t *)dest_ = _byteswap_ulong(*(unsigned int *)pa);
#else
    dest_[0] = pa[3];
    dest_[1] = pa[2];
    dest_[2] = pa[1];
    dest_[3] = pa[0];
#endif
  }
}

/* Reverse swap bits in a 32-bit integer */
static inline int32_t bswap32_(int32_t a) {
#if defined (__GNUC__)
  return __builtin_bswap32(a);

#elif defined (_MSC_VER) /* Visual Studio */
  return _byteswap_ulong(a);
#else
  a = ((a & 0x000000FF) << 24) |
      ((a & 0x0000FF00) <<  8) |
      ((a & 0x00FF0000) >>  8) |
      ((a & 0xFF000000) >> 24);
  return a;
#endif
}

/**
 * @brief Register a filter in Blosc.
 *
 * @param filter The filter to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_filter_private(blosc2_filter *filter);

/**
 * @brief Register a codec in Blosc.
 *
 * @param codec The codec to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_codec_private(blosc2_codec *codec);


/**
 * @brief Register a tune in Blosc.
 *
 * @param tune The tune to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_tuner_private(blosc2_tuner *tuner);

int fill_tuner(blosc2_tuner *tuner);

extern blosc2_tuner g_tuners[256];
extern int g_ntuners;

/* Large short-lived buffers (chunk scratch, compressed chunks) cycle through a
   small per-thread cache instead of malloc/free on every call: glibc trims
   freed memory at the top of the heap back to the kernel, so a buffer of
   several MB freed per call is page-faulted in again by the next one.
   blosc2_tmp_malloc() returns memory for at least `nbytes` (malloc'ed, so it
   may be released with plain free()) and its real size in *capacity;
   blosc2_tmp_free() keeps it for reuse or frees it.  At most
   BLOSC2_TMP_CACHE_MAX bytes stay cached per thread, released at thread exit
   or by blosc2_destroy() for the calling thread. */
void *blosc2_tmp_malloc(size_t nbytes, size_t *capacity);
void blosc2_tmp_free(void *ptr, size_t capacity);
void blosc2_tmp_cache_clear(void);
/* Bytes currently held by the calling thread's cache (for tests). */
size_t blosc2_tmp_cache_nbytes(void);


#if defined(_WIN32)
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#define RTLD_LAZY   0x000
#define popen _popen
#define pclose _pclose

static struct {
    long lasterror;
    const char *err_rutin;
}
var = {
    0,
    NULL
};

static inline void *dlopen (const char *filename, int flags) {
  BLOSC_UNUSED_PARAM(flags);
  HINSTANCE hInst;
  hInst = LoadLibrary(filename);
  if (hInst==NULL) {
    var.lasterror = GetLastError();
    var.err_rutin = "dlopen";
  }

  return hInst;
}

static inline void *dlsym(void *handle, const char *name) {
  FARPROC fp;
  fp = GetProcAddress((HINSTANCE)handle, name);
  if (!fp) {
    var.lasterror = GetLastError ();
    var.err_rutin = "dlsym";
  }
  return (void *)(intptr_t)fp;
}

static inline int dlclose(void *handle) {
  bool ok = FreeLibrary((HINSTANCE)handle);
  if (!ok) {
    var.lasterror = GetLastError();
    var.err_rutin = "dlclose";
    return BLOSC2_ERROR_FAILURE;
  }
  return BLOSC2_ERROR_SUCCESS;
}

static inline const char *dlerror (void) {
  static char errstr [88];
  if (var.lasterror) {
      snprintf(errstr, sizeof(errstr), "%s error #%ld", var.err_rutin, var.lasterror);
      return errstr;
  } else {
      return NULL;
  }
}
#else
#include <dlfcn.h>
#endif

static inline bool blosc2_valid_plugin_name(const char *plugin_name) {
  if (plugin_name == NULL || plugin_name[0] == '\0') {
    return false;
  }
  for (const unsigned char *p = (const unsigned char *)plugin_name; *p != '\0'; ++p) {
    if (!isalnum(*p) && *p != '_') {
      return false;
    }
  }
  return true;
}


static inline int get_libpath(char *plugin_name, char *libpath, char *python_version) {
  BLOSC_TRACE_INFO("Trying to get plugin path with python%s\n", python_version);
  char python_cmd[PATH_MAX] = {0};
  if (!blosc2_valid_plugin_name(plugin_name)) {
    BLOSC_TRACE_ERROR("Invalid plugin name");
    return BLOSC2_ERROR_INVALID_PARAM;
  }
  int written = snprintf(python_cmd, sizeof(python_cmd),
                         "python%s -c \"import blosc2_%s; blosc2_%s.print_libpath()\"",
                         python_version, plugin_name, plugin_name);
  if (written < 0 || (size_t)written >= sizeof(python_cmd)) {
    BLOSC_TRACE_ERROR("Python command is too long");
    return BLOSC2_ERROR_FAILURE;
  }
  FILE *fp = popen(python_cmd, "r");
  if (fp == NULL) {
    BLOSC_TRACE_ERROR("Could not run python");
    return BLOSC2_ERROR_FAILURE;
  }
  if (fgets(libpath, PATH_MAX, fp) == NULL) {
    BLOSC_TRACE_ERROR("Could not read python output");
    pclose(fp);
    return BLOSC2_ERROR_FAILURE;
  }
  pclose(fp);

  return BLOSC2_ERROR_SUCCESS;
}

static inline void* load_lib(char *plugin_name, char *libpath) {
  if (!blosc2_valid_plugin_name(plugin_name)) {
    BLOSC_TRACE_ERROR("Invalid plugin name");
    return NULL;
  }
    // Attempt to directly load the library by name
#if defined(_WIN32)
    // Windows dynamic library (DLL) format
    snprintf(libpath, PATH_MAX, "blosc2_%s.dll", plugin_name);
#else
    // Unix/Linux/Mac OS dynamic library (.so) format
    snprintf(libpath, PATH_MAX, "libblosc2_%s.so", plugin_name);
#endif
    void* loaded_lib = dlopen(libpath, RTLD_LAZY);
    if (loaded_lib != NULL) {
        BLOSC_TRACE_INFO("Successfully loaded %s directly\n", libpath);
        return loaded_lib;
    } else {
#if defined(_WIN32)
        BLOSC_TRACE_INFO("Failed to load %s directly, error: %lu\n", libpath, GetLastError());
#else
        BLOSC_TRACE_INFO("Failed to load %s directly, error: %s\n", libpath, dlerror());
#endif
    }
    // If direct loading fails, fallback to using Python to find the library path
    if (get_libpath(plugin_name, libpath, "") < 0 && get_libpath(plugin_name, libpath, "3") < 0) {
        BLOSC_TRACE_ERROR("Problems when running python or python3 for getting plugin path");
        return NULL;
    }

    if (strlen(libpath) == 0) {
        BLOSC_TRACE_ERROR("Could not find plugin libpath");
        return NULL;
    }

    // Try to load the library again with the path from Python
    loaded_lib = dlopen(libpath, RTLD_LAZY);
    if (loaded_lib == NULL) {
        BLOSC_TRACE_ERROR("Attempt to load plugin in path '%s' failed with error: %s", libpath, dlerror());
    } else {
        BLOSC_TRACE_INFO("Successfully loaded library with Python path: %s\n", libpath);
    }

    return loaded_lib;
}


#endif /* BLOSC_BLOSC_PRIVATE_H */

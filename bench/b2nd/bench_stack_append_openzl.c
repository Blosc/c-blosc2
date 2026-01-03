/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// Benchmark for appending and reading stacks of images in b2nd arrays.
// This is intended for comparing OpenZL profiles with standard Blosc2
// codecs + filters.

#include <inttypes.h>
#include <limits.h>
#ifndef PATH_MAX
  #ifdef _WIN32
    #define PATH_MAX MAX_PATH
  #else
    #define PATH_MAX 4096
  #endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "b2nd.h"

#define KB  1024u
#define MB  (1024 * KB)

static void print_usage(const char *name) {
  printf("Usage: %s [openzl <profile> | <codec> [filter]] [--nthreads N|-t N] [--images N|-n N]\n", name);
  printf("           [--write-only|-w | --read-only|-r] [--urlpath PATH|-u PATH]\n");
  printf("           [--clevel N|-l N] [--image-size WxH|-s WxH]\n");
  printf("  openzl <profile>: use OpenZL with profile (e.g. SH_LZ4, SH_ZSTD)\n");
  printf("  <codec> [filter]: e.g. zstd shuffle, lz4hc bitshuffle, zlib noshuffle\n");
  printf("  --nthreads N, -t N: number of threads (default 8)\n");
  printf("  --images N, -n N: number of images in stack (default 1000)\n");
  printf("  --write-only, -w: only create/append the array\n");
  printf("  --read-only, -r: only read an existing array from urlpath\n");
  printf("  --urlpath PATH, -u PATH: b2nd file path (default bench_stack_append_openzl.b2nd)\n");
  printf("  --clevel N, -l N: run a single clevel (0-9)\n");
  printf("  --image-size WxH, -s WxH: image size (default 2000x2000)\n");
  printf("  Note: read-only/write-only runs use per-clevel files.\n");
  printf("Defaults: codec=zstd, filter=shuffle\n");
}

static int parse_openzl_profile(const char *profile, int *compcode_meta) {
  if (strcmp(profile, "BD_SH_LZ4") == 0) {
    *compcode_meta = 12;
  } else if (strcmp(profile, "BD_SH_ZSTD") == 0) {
    *compcode_meta = 13;
  } else if (strcmp(profile, "SH_BD_LZ4") == 0) {
    *compcode_meta = 6;
  } else if (strcmp(profile, "SH_BD_ZSTD") == 0) {
    *compcode_meta = 7;
  } else if (strcmp(profile, "SH_LZ4") == 0) {
    *compcode_meta = 8;
  } else if (strcmp(profile, "SH_ZSTD") == 0) {
    *compcode_meta = 9;
  } else if (strcmp(profile, "LZ4") == 0) {
    *compcode_meta = 10;
  } else if (strcmp(profile, "ZSTD") == 0) {
    *compcode_meta = 11;
  } else {
    return -1;
  }
  return 0;
}

static int parse_filter(const char *filter_name, uint8_t *filter) {
  if (filter_name == NULL || strcmp(filter_name, "shuffle") == 0) {
    *filter = BLOSC_SHUFFLE;
  } else if (strcmp(filter_name, "bitshuffle") == 0) {
    *filter = BLOSC_BITSHUFFLE;
  } else if (strcmp(filter_name, "noshuffle") == 0) {
    *filter = BLOSC_NOFILTER;
  } else {
    return -1;
  }
  return 0;
}

static int build_clevel_urlpath(char *dst, size_t dst_size, const char *base,
                                int clevel, int per_clevel) {
  if (!per_clevel) {
    return snprintf(dst, dst_size, "%s", base);
  }
  const char *ext = strrchr(base, '.');
  if (ext != NULL && strcmp(ext, ".b2nd") == 0) {
    size_t prefix_len = (size_t)(ext - base);
    return snprintf(dst, dst_size, "%.*s_clevel%d%s",
                    (int)prefix_len, base, clevel, ext);
  }
  return snprintf(dst, dst_size, "%s_clevel%d.b2nd", base, clevel);
}

static int build_default_urlpath(char *dst, size_t dst_size,
                                 int use_openzl, const char *codec) {
  if (use_openzl) {
    return snprintf(dst, dst_size, "bench_stack_append_openzl.b2nd");
  }
  return snprintf(dst, dst_size, "bench_stack_append_%s.b2nd", codec);
}

static int build_tmp_urlpath(char *dst, size_t dst_size,
                             int use_openzl, const char *codec) {
  const char *tmp = getenv("TMPDIR");
  if (tmp == NULL || tmp[0] == '\0') {
    tmp = "/tmp";
  }
  if (use_openzl) {
    return snprintf(dst, dst_size, "%s/%s", tmp, "bench_stack_append_openzl.b2nd");
  }
  return snprintf(dst, dst_size, "%s/bench_stack_append_%s.b2nd", tmp, codec);
}

int main(int argc, char *argv[]) {
  const char *codec = "zstd";
  const char *filter_name = "shuffle";
  const char *openzl_profile = "SH_ZSTD";
  int use_openzl = 0;
  int nthreads = 8;
  int64_t nimages_total = 1000;
  int width = 2000;
  int height = 2000;
  char urlpath_default[PATH_MAX];
  const char *urlpath = NULL;
  int urlpath_set = 0;
  int mode_write = 1;
  int mode_read = 1;
  int clevel_single = -1;
  const char *posargs[3];
  int poscount = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--nthreads") == 0 || strcmp(argv[i], "-t") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      nthreads = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--images") == 0 || strcmp(argv[i], "-n") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      nimages_total = (int64_t)atoll(argv[++i]);
    } else if (strcmp(argv[i], "--urlpath") == 0 || strcmp(argv[i], "-u") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      urlpath = argv[++i];
      urlpath_set = 1;
    } else if (strcmp(argv[i], "--image-size") == 0 || strcmp(argv[i], "-s") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      const char *size_arg = argv[++i];
      int w = 0, h = 0;
      if (sscanf(size_arg, "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) {
        printf("Error: invalid --image-size (expected WxH)\n");
        return 1;
      }
      width = w;
      height = h;
    } else if (strcmp(argv[i], "--clevel") == 0 || strcmp(argv[i], "-l") == 0) {
      if (i + 1 >= argc) {
        print_usage(argv[0]);
        return 1;
      }
      clevel_single = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--write-only") == 0 || strcmp(argv[i], "-w") == 0) {
      mode_read = 0;
    } else if (strcmp(argv[i], "--read-only") == 0 || strcmp(argv[i], "-r") == 0) {
      mode_write = 0;
    } else {
      if (poscount < 3) {
        posargs[poscount++] = argv[i];
      } else {
        print_usage(argv[0]);
        return 1;
      }
    }
  }

  if (nthreads <= 0 || nimages_total <= 0 || (!mode_write && !mode_read)) {
    print_usage(argv[0]);
    return 1;
  }
  if (clevel_single >= 0 && clevel_single > 9) {
    print_usage(argv[0]);
    return 1;
  }

  if (poscount > 0) {
    if (strcmp(posargs[0], "openzl") == 0) {
      if (poscount < 2) {
        print_usage(argv[0]);
        return 1;
      }
      use_openzl = 1;
      openzl_profile = posargs[1];
    } else {
      codec = posargs[0];
      if (poscount >= 2) {
        filter_name = posargs[1];
      }
    }
  }

  if (!urlpath_set) {
    int urlpath_len = build_default_urlpath(urlpath_default, sizeof(urlpath_default),
                                            use_openzl, codec);
    if (urlpath_len < 0 || (size_t)urlpath_len >= sizeof(urlpath_default)) {
      printf("Error: default urlpath too long\n");
      return 1;
    }
    urlpath = urlpath_default;
  }

  blosc2_init();
  const int nimages_inbuf = 1;
  const int64_t buffershape[] = {1, height, width};
  const int64_t buffersize = (int64_t)height * width * (int64_t)sizeof(uint16_t);
  int64_t data_bytes = nimages_total * height * width * (int64_t)sizeof(uint16_t);

  // Shapes of the b2nd array
  int64_t shape[] = {0, height, width};
  int32_t chunkshape[] = {nimages_inbuf, height, width};
  int32_t blockshape[] = {1, height, width};

  uint16_t *image = NULL;
  if (mode_write) {
    image = malloc(buffersize);
    if (image == NULL) {
      printf("Error: could not allocate input buffer.\n");
      return 1;
    }
  }

  int compcode_meta = 0;
  int compcode = 0;
  uint8_t filter = BLOSC_NOFILTER;

  if (use_openzl) {
    if (blosc2_compname_to_compcode(BLOSC_OPENZL_COMPNAME) < 0) {
      printf("Compiled w/o support for compressor: '%s', so sorry.\n", BLOSC_OPENZL_COMPNAME);
      free(image);
      blosc2_destroy();
      return 1;
    }
    compcode = BLOSC_OPENZL;
    if (parse_openzl_profile(openzl_profile, &compcode_meta) < 0) {
      printf("Unknown OpenZL profile: %s\n", openzl_profile);
      print_usage(argv[0]);
      free(image);
      blosc2_destroy();
      return 1;
    }
  } else {
    compcode = blosc2_compname_to_compcode(codec);
    if (compcode < 0) {
      printf("Unknown compressor: %s\n", codec);
      print_usage(argv[0]);
      free(image);
      blosc2_destroy();
      return 1;
    }
    if (parse_filter(filter_name, &filter) < 0) {
      printf("Unknown filter: %s\n", filter_name);
      print_usage(argv[0]);
      free(image);
      blosc2_destroy();
      return 1;
    }
  }

  printf("Benchmarking stack append/read for b2nd arrays\n");
  printf("Images: %" PRId64 ", image shape: %dx%d, chunk images: %d, nthreads: %d\n",
         nimages_total, width, height, nimages_inbuf, nthreads);
  if (clevel_single >= 0 && (mode_read != mode_write)) {
    char urlpath_display[PATH_MAX];
    int path_len = build_clevel_urlpath(urlpath_display, sizeof(urlpath_display),
                                        urlpath, clevel_single, 1);
    if (path_len < 0 || (size_t)path_len >= sizeof(urlpath_display)) {
      printf("Error: urlpath too long\n");
      free(image);
      blosc2_destroy();
      return 1;
    }
    printf("urlpath: %s\n", urlpath_display);
  } else {
    printf("urlpath: %s\n", urlpath);
  }
  if (use_openzl) {
    printf("Codec: openzl, profile: %s\n", openzl_profile);
  } else {
    printf("Codec: %s, filter: %s\n", codec, filter_name);
  }

  int per_clevel = (mode_read != mode_write);
  int use_tmp_urlpath = 0;
  char urlpath_tmp[PATH_MAX];
  int clevel_start = (clevel_single >= 0) ? clevel_single : 0;
  int clevel_end = (clevel_single >= 0) ? clevel_single : 9;
  for (int clevel = clevel_start; clevel <= clevel_end; clevel++) {
    char urlpath_buf[PATH_MAX];
    const char *urlpath_base = use_tmp_urlpath ? urlpath_tmp : urlpath;
    int path_len = build_clevel_urlpath(urlpath_buf, sizeof(urlpath_buf),
                                        urlpath_base, clevel, per_clevel);
    if (path_len < 0 || (size_t)path_len >= sizeof(urlpath_buf)) {
      printf("Error: urlpath too long\n");
      free(image);
      blosc2_destroy();
      return 1;
    }
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = sizeof(uint16_t);
    cparams.clevel = clevel;
    cparams.compcode = compcode;
    cparams.compcode_meta = compcode_meta;
    cparams.nthreads = nthreads;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = use_openzl ? BLOSC_NOFILTER : filter;
    cparams.filters_meta[BLOSC2_MAX_FILTERS - 1] = 0;

    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.cparams = &cparams;
    storage.contiguous = true;
    storage.urlpath = urlpath_buf;

    b2nd_context_t *ctx = NULL;
    b2nd_array_t *arr;
    if (mode_read && !mode_write) {
      if (b2nd_open(urlpath_buf, &arr) < 0) {
        printf("Error in b2nd_open\n");
        free(image);
        blosc2_destroy();
        return 1;
      }
      if (arr->shape[0] < nimages_total) {
        nimages_total = arr->shape[0];
        if (nimages_total <= 0) {
          printf("Error: no images available for reading\n");
          b2nd_free(arr);
          free(image);
          blosc2_destroy();
          return 1;
        }
        data_bytes = nimages_total * height * width * (int64_t)sizeof(uint16_t);
      }
    } else {
      if (mode_write) {
        int rc = blosc2_remove_urlpath(urlpath_buf);
        if (rc < 0 && !urlpath_set && !use_tmp_urlpath) {
          int tmp_len = build_tmp_urlpath(urlpath_tmp, sizeof(urlpath_tmp),
                                          use_openzl, codec);
          if (tmp_len < 0 || (size_t)tmp_len >= sizeof(urlpath_tmp)) {
            printf("Error: tmp urlpath too long\n");
            b2nd_free_ctx(ctx);
            free(image);
            blosc2_destroy();
            return 1;
          }
          use_tmp_urlpath = 1;
          urlpath_base = urlpath_tmp;
          path_len = build_clevel_urlpath(urlpath_buf, sizeof(urlpath_buf),
                                          urlpath_base, clevel, per_clevel);
          if (path_len < 0 || (size_t)path_len >= sizeof(urlpath_buf)) {
            printf("Error: urlpath too long\n");
            b2nd_free_ctx(ctx);
            free(image);
            blosc2_destroy();
            return 1;
          }
          rc = blosc2_remove_urlpath(urlpath_buf);
          if (rc < 0) {
            printf("Error removing urlpath: %s\n", urlpath_buf);
            b2nd_free_ctx(ctx);
            free(image);
            blosc2_destroy();
            return 1;
          }
          printf("Note: urlpath not writable; using %s\n", urlpath_tmp);
        } else if (rc < 0) {
          printf("Error removing urlpath: %s\n", urlpath_buf);
          b2nd_free_ctx(ctx);
          free(image);
          blosc2_destroy();
          return 1;
        }
      }
      ctx = b2nd_create_ctx(&storage, 3,
                            shape, chunkshape, blockshape,
                            "|u2", DTYPE_NUMPY_FORMAT,
                            NULL, 0);
      if (ctx == NULL) {
        printf("Error in b2nd_create_ctx\n");
        free(image);
        blosc2_destroy();
        return 1;
      }
      if (b2nd_empty(ctx, &arr) < 0) {
        printf("Error in b2nd_empty\n");
        b2nd_free_ctx(ctx);
        free(image);
        blosc2_destroy();
        return 1;
      }
    }

    blosc_timestamp_t t0, t1, t2;
    if (mode_write) {
      blosc_set_timestamp(&t0);
      for (int64_t i = 0; i < nimages_total; i++) {
        for (int64_t j = 0; j < (int64_t)height * width; j++) {
          image[j] = (uint16_t)(j + i);
        }
        if (b2nd_append(arr, image, buffersize, 0) < 0) {
          printf("Error in b2nd_append\n");
          b2nd_free(arr);
          if (ctx != NULL) {
            b2nd_free_ctx(ctx);
          }
          free(image);
          blosc2_destroy();
          return 1;
        }
      }
      blosc_set_timestamp(&t1);
    } else {
      blosc_set_timestamp(&t0);
      t1 = t0;
    }

    uint16_t *readbuf = malloc(buffersize);
    if (readbuf == NULL) {
      printf("Error: could not allocate read buffer.\n");
      b2nd_free(arr);
      b2nd_free_ctx(ctx);
      free(image);
      blosc2_destroy();
      return 1;
    }

    if (mode_read) {
      for (int64_t i = 0; i < nimages_total; i++) {
        int64_t start[] = {i, 0, 0};
        int64_t stop[] = {i + 1, height, width};
        if (b2nd_get_slice_cbuffer(arr, start, stop, readbuf, buffershape, buffersize) < 0) {
          printf("Error in b2nd_get_slice_cbuffer\n");
          free(readbuf);
          b2nd_free(arr);
          if (ctx != NULL) {
            b2nd_free_ctx(ctx);
          }
          free(image);
          blosc2_destroy();
          return 1;
        }
      }
      blosc_set_timestamp(&t2);
    } else {
      blosc_set_timestamp(&t2);
    }

    double write_s = mode_write ? blosc_elapsed_secs(t0, t1) : 0.0;
    double read_s = mode_read ? blosc_elapsed_secs(t1, t2) : 0.0;
    double write_mb_s = mode_write && write_s > 0.0 ? (double)data_bytes / (write_s * (double)MB) : 0.0;
    double read_mb_s = mode_read && read_s > 0.0 ? (double)data_bytes / (read_s * (double)MB) : 0.0;
    double ratio = (arr->sc->cbytes > 0) ?
                   ((double)arr->sc->nbytes / (double)arr->sc->cbytes) : 0.0;

    if (mode_write && mode_read) {
      printf("clevel %d: append %.4f s (%.1f MB/s), read %.4f s (%.1f MB/s), ratio %.2f\n",
             clevel, write_s, write_mb_s, read_s, read_mb_s, ratio);
    } else if (mode_write) {
      printf("clevel %d: append %.4f s (%.1f MB/s), ratio %.2f\n",
             clevel, write_s, write_mb_s, ratio);
    } else {
      printf("clevel %d: read %.4f s (%.1f MB/s), ratio %.2f\n",
             clevel, read_s, read_mb_s, ratio);
    }

    free(readbuf);
    b2nd_free(arr);
    if (ctx != NULL) {
      b2nd_free_ctx(ctx);
    }
  }

  free(image);
  blosc2_destroy();
  return 0;
}

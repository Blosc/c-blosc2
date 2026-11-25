/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*

  Benchmark for the Blosc2 EmbedStore (B2E) operations.

  Each operation lives in its own function (`s_bench_set`, `s_bench_get`,
  `s_bench_remove`, `s_bench_serialize` and `s_bench_open`) so that it can be
  profiled (e.g. with `perf` or `valgrind`) in isolation by selecting it on the
  command line.

  Benchmarked operations:

    - set         : insertion of a value (a whole SChunk) under a key
    - get         : retrieval of the SChunk associated to a key
    - remove      : deletion of a key from the index
    - serialize   : serialization of the whole index (msgpack) to a buffer
    - open        : opening of a persisted EmbedStore from disk

  Usage:

    $ ./embed_store_bench [nkeys] [elem_size_bytes] [reps] [operation]

  Defaults: nkeys=1000, elem_size=4096, reps=5, operation=all
  operation is one of: all, set, get, remove, serialize, open

  Examples:

    $ ./embed_store_bench                       # run every operation
    $ ./embed_store_bench 1000 4096 5 get       # profile only the get path

  Example output:

    EmbedStore benchmark: 1000 keys, 4096 bytes/value, 5 reps
    ---------------------------------------------------------------------
    operation     calls     total (ms)        ns/op        ops/s      MB/s
    ---------------------------------------------------------------------
    set            1000         ...
    get            1000         ...
    serialize         5         ...
    remove         1000         ...
    open              5         ...
    ---------------------------------------------------------------------

  When the throughput is not meaningful for an operation (serialize, open),
  the MB/s column is shown as "-".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include <blosc2.h>
#include <stores.h>
#include <utils/msgpack.h>

#define DEFAULT_NKEYS 1000
#define DEFAULT_ELEM_SIZE 4096
#define DEFAULT_REPS 5

#define URLPATH "embed_store_bench.b2e"

#define KEY_SIZE 32


static const char* s_usage =
    "Usage: embed_store_bench [nkeys] [elem_size_bytes] [reps] [operation]\n"
    "  nkeys           number of keys/elements (default %d)\n"
    "  elem_size_bytes size in bytes of each stored value (default %d)\n"
    "  reps            repetitions for the serialize/open benchmarks (default %d)\n"
    "  operation       all | set | get | remove | serialize | open (default all)\n";


static void s_report(const char* name, blosc_timestamp_t t0, blosc_timestamp_t t1,
                     double calls, size_t bytes_per_op, bool show_throughput) {
  double elapsed_ns = (double)blosc_elapsed_nsecs(t0, t1);
  double ns_per_op = elapsed_ns / calls;
  double ops_per_s = 1e9 / ns_per_op;

  printf("%-12s %6.0f %14.3f %12.1f %12.0f", name, calls, elapsed_ns / 1e6, ns_per_op, ops_per_s);
  if (show_throughput) {
    double mb_per_s = (calls * (double)bytes_per_op) / (elapsed_ns / 1e9) / 1e6;
    printf(" %9.1f\n", mb_per_s);
  } else {
    printf(" %9s\n", "-");
  }
}


static void s_make_key(char* key, size_t index) {
  snprintf(key, KEY_SIZE, "/key_%08" PRIu64, (uint64_t)index);
}


static blosc2_schunk* s_make_value_schunk(size_t elem_size) {
  blosc2_cparams cparams = blosc2_get_blosc2_cparams_defaults();
  cparams.typesize = 4;
  blosc2_dparams dparams = blosc2_get_blosc2_dparams_defaults();
  blosc2_storage storage = {.contiguous = true, .cparams = &cparams, .dparams = &dparams};

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }

  uint8_t* buffer = malloc(elem_size);
  if (buffer == NULL) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  // A cheap but not fully trivial pattern, so that compression has some work to do
  for (size_t i = 0; i < elem_size; i++) {
    buffer[i] = (uint8_t)(i % 251);
  }
  if (blosc2_schunk_append_buffer(schunk, buffer, (int32_t)elem_size) < 0) {
    free(buffer);
    blosc2_schunk_free(schunk);
    return NULL;
  }
  free(buffer);
  return schunk;
}


/* Inserts `nkeys` values in the store. Used as untimed setup by several benchmarks. */
static int s_populate(blosc2_embed_store* es, size_t nkeys, char** keys, blosc2_schunk* value) {
  for (size_t i = 0; i < nkeys; i++) {
    if (blosc2_embed_store_set(es, (const uint8_t*)keys[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = value}) != 0) {
      printf("Error setting key '%s'\n", keys[i]);
      return -1;
    }
  }
  return 0;
}


/* Creates a persisted EmbedStore at `path` and populates it with `nkeys` values. */
static int s_create_persisted_store(const char* path, size_t nkeys, char** keys, blosc2_schunk* value) {
  blosc2_remove_urlpath(path);

  blosc2_cparams cparams = blosc2_get_blosc2_cparams_defaults();
  cparams.typesize = 1;
  blosc2_dparams dparams = blosc2_get_blosc2_dparams_defaults();
  blosc2_io io = blosc2_get_blosc2_io_defaults();
  blosc2_storage storage = {
      .contiguous = true,
      .cparams = &cparams,
      .dparams = &dparams,
      .io = &io,
      .urlpath = (char*)path,
  };

  blosc2_embed_store* es = NULL;
  if (blosc2_embed_store_new(&es, &storage, BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE, BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE) != 0 || es == NULL) {
    return -1;
  }
  if (s_populate(es, nkeys, keys, value) != 0) {
    blosc2_embed_store_free(es);
    return -1;
  }
  int rc = blosc2_embed_store_close(es);
  // Freeing the writer flushes the backing frame to disk
  blosc2_embed_store_free(es);
  return rc;
}


/* == Individual benchmarks == */

/* set: insertion of `nkeys` values. */
static int s_bench_set(size_t nkeys, size_t elem_size, char** keys, blosc2_schunk* value) {
  blosc2_embed_store* es = NULL;
  if (blosc2_embed_store_new(&es, NULL, BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE, BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE) != 0 || es == NULL) {
    printf("Error creating the embed store\n");
    return -1;
  }

  blosc_timestamp_t t0, t1;
  blosc_set_timestamp(&t0);
  if (s_populate(es, nkeys, keys, value) != 0) {
    blosc2_embed_store_free(es);
    return -1;
  }
  blosc_set_timestamp(&t1);

  s_report("set", t0, t1, (double)nkeys, elem_size, true);
  blosc2_embed_store_free(es);
  return 0;
}


/* get: retrieval of `nkeys` values. */
static int s_bench_get(size_t nkeys, size_t elem_size, char** keys, blosc2_schunk* value) {
  blosc2_embed_store* es = NULL;
  if (blosc2_embed_store_new(&es, NULL, BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE, BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE) != 0 || es == NULL) {
    printf("Error creating the embed store\n");
    return -1;
  }

  blosc_timestamp_t t0, t1;
  // Untimed setup
  if (s_populate(es, nkeys, keys, value) != 0) {
    blosc2_embed_store_free(es);
    return -1;
  }

  blosc_set_timestamp(&t0);
  for (size_t i = 0; i < nkeys; i++) {
    blosc2_store_value got = {0};
    if (blosc2_embed_store_get(es, (const uint8_t*)keys[i], &got) != 0) {
      printf("Error getting key '%s'\n", keys[i]);
      blosc2_embed_store_free(es);
      return -1;
    }
    blosc2_schunk_free(got.schunk);
  }
  blosc_set_timestamp(&t1);

  s_report("get", t0, t1, (double)nkeys, elem_size, true);
  blosc2_embed_store_free(es);
  return 0;
}


/* remove: deletion of `nkeys` keys. */
static int s_bench_remove(size_t nkeys, char** keys, blosc2_schunk* value) {
  blosc2_embed_store* es = NULL;
  if (blosc2_embed_store_new(&es, NULL, BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE, BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE) != 0 || es == NULL) {
    printf("Error creating the embed store\n");
    return -1;
  }

  blosc_timestamp_t t0, t1;
  // Untimed setup
  if (s_populate(es, nkeys, keys, value) != 0) {
    blosc2_embed_store_free(es);
    return -1;
  }

  blosc_set_timestamp(&t0);
  for (size_t i = 0; i < nkeys; i++) {
    if (blosc2_embed_store_remove(es, (const uint8_t*)keys[i]) != 0) {
      printf("Error removing key '%s'\n", keys[i]);
      blosc2_embed_store_free(es);
      return -1;
    }
  }
  blosc_set_timestamp(&t1);

  s_report("remove", t0, t1, (double)nkeys, 0, false);
  blosc2_embed_store_free(es);
  return 0;
}


/* serialize: serialization of the whole index to a msgpack buffer. */
static int s_bench_serialize(size_t nkeys, int reps, char** keys, blosc2_schunk* value) {
  blosc2_embed_store* es = NULL;
  if (blosc2_embed_store_new(&es, NULL, BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE, BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE) != 0 || es == NULL) {
    printf("Error creating the embed store\n");
    return -1;
  }

  blosc_timestamp_t t0, t1;
  // Untimed setup
  if (s_populate(es, nkeys, keys, value) != 0) {
    blosc2_embed_store_free(es);
    return -1;
  }

  blosc_set_timestamp(&t0);
  for (int r = 0; r < reps; r++) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    if (msgpack_embed_store_serialize(es, &packed, &packed_size) != 0) {
      printf("Error serializing the embed store\n");
      blosc2_embed_store_free(es);
      return -1;
    }
    free(packed);
  }
  blosc_set_timestamp(&t1);

  s_report("serialize", t0, t1, (double)reps, 0, false);
  blosc2_embed_store_free(es);
  return 0;
}


/* open: opening of a persisted EmbedStore from disk. */
static int s_bench_open(size_t nkeys, int reps, char** keys, blosc2_schunk* value) {
  // Untimed setup: persist a store with `nkeys` values
  if (s_create_persisted_store(URLPATH, nkeys, keys, value) != 0) {
    printf("Error creating the persisted embed store\n");
    return -1;
  }

  blosc_timestamp_t t0, t1;
  blosc_set_timestamp(&t0);
  for (int r = 0; r < reps; r++) {
    blosc2_embed_store* opened = NULL;
    if (blosc2_embed_store_open(&opened, (const uint8_t*)URLPATH, BLOSC2_STORE_OPEN_MODE_RDONLY) != 0 || opened == NULL) {
      printf("Error opening the embed store\n");
      blosc2_remove_urlpath(URLPATH);
      return -1;
    }
    if (blosc2_embed_store_get_length(opened) != nkeys) {
      printf("Error: opened store has an unexpected number of keys\n");
      blosc2_embed_store_free(opened);
      blosc2_remove_urlpath(URLPATH);
      return -1;
    }
    blosc2_embed_store_free(opened);
  }
  blosc_set_timestamp(&t1);

  s_report("open", t0, t1, (double)reps, 0, false);
  blosc2_remove_urlpath(URLPATH);
  return 0;
}


typedef enum {
  OP_ALL = 0,
  OP_SET,
  OP_GET,
  OP_REMOVE,
  OP_SERIALIZE,
  OP_OPEN,
} operation;


static int s_parse_operation(const char* name, operation* op) {
  if (strcmp(name, "all") == 0) {
    *op = OP_ALL;
  } else if (strcmp(name, "set") == 0) {
    *op = OP_SET;
  } else if (strcmp(name, "get") == 0) {
    *op = OP_GET;
  } else if (strcmp(name, "remove") == 0) {
    *op = OP_REMOVE;
  } else if (strcmp(name, "serialize") == 0) {
    *op = OP_SERIALIZE;
  } else if (strcmp(name, "open") == 0) {
    *op = OP_OPEN;
  } else {
    return -1;
  }
  return 0;
}


int main(int argc, char* argv[]) {
  size_t nkeys = DEFAULT_NKEYS;
  size_t elem_size = DEFAULT_ELEM_SIZE;
  int reps = DEFAULT_REPS;
  operation op = OP_ALL;

  if (argc > 1) {
    nkeys = (size_t)strtoull(argv[1], NULL, 10);
  }
  if (argc > 2) {
    elem_size = (size_t)strtoull(argv[2], NULL, 10);
  }
  if (argc > 3) {
    reps = (int)strtol(argv[3], NULL, 10);
  }
  if (argc > 4 && s_parse_operation(argv[4], &op) != 0) {
    printf("Unknown operation: '%s'\n", argv[4]);
    printf(s_usage, DEFAULT_NKEYS, DEFAULT_ELEM_SIZE, DEFAULT_REPS);
    return 1;
  }
  if (nkeys == 0 || elem_size == 0 || reps <= 0) {
    printf(s_usage, DEFAULT_NKEYS, DEFAULT_ELEM_SIZE, DEFAULT_REPS);
    return 1;
  }

  blosc2_init();

  char** keys = NULL;
  blosc2_schunk* value = NULL;
  int ret = 1;

  printf("EmbedStore benchmark: %zu keys, %zu bytes/value, %d reps\n",
         nkeys, elem_size, reps);

  keys = calloc(nkeys, sizeof(char*));
  if (keys == NULL) {
    printf("Error allocating keys\n");
    goto cleanup;
  }
  for (size_t i = 0; i < nkeys; i++) {
    keys[i] = malloc(KEY_SIZE);
    if (keys[i] == NULL) {
      printf("Error allocating keys\n");
      goto cleanup;
    }
    s_make_key(keys[i], i);
  }

  value = s_make_value_schunk(elem_size);
  if (value == NULL) {
    printf("Error creating the value schunk\n");
    goto cleanup;
  }

  printf("---------------------------------------------------------------------\n");
  printf("%-12s %6s %14s %12s %12s %9s\n",
         "operation", "calls", "total (ms)", "ns/op", "ops/s", "MB/s");
  printf("---------------------------------------------------------------------\n");

  int rc = 0;
  switch (op) {
    case OP_SET:
      rc = s_bench_set(nkeys, elem_size, keys, value);
      break;
    case OP_GET:
      rc = s_bench_get(nkeys, elem_size, keys, value);
      break;
    case OP_REMOVE:
      rc = s_bench_remove(nkeys, keys, value);
      break;
    case OP_SERIALIZE:
      rc = s_bench_serialize(nkeys, reps, keys, value);
      break;
    case OP_OPEN:
      rc = s_bench_open(nkeys, reps, keys, value);
      break;
    case OP_ALL:
    default:
      rc |= s_bench_set(nkeys, elem_size, keys, value);
      rc |= s_bench_get(nkeys, elem_size, keys, value);
      rc |= s_bench_serialize(nkeys, reps, keys, value);
      rc |= s_bench_remove(nkeys, keys, value);
      rc |= s_bench_open(nkeys, reps, keys, value);
      break;
  }
  ret = (rc == 0) ? 0 : 1;

cleanup:
  blosc2_remove_urlpath(URLPATH);
  if (value != NULL) {
    blosc2_schunk_free(value);
  }
  if (keys != NULL) {
    for (size_t i = 0; i < nkeys; i++) {
      free(keys[i]);
    }
    free(keys);
  }
  blosc2_destroy();
  return ret;
}

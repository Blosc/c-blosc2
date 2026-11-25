#include <blosc2.h>
#include <stores.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/filesystem.h>


#define SCHUNKS_SIZE 750
#define PRINT_RESULT(label, totaltime) printf("[%s]: %6.6f s\n", (const char*)label, totaltime)

#define PRINT_LABEL(label) printf("[%s]\n", label)

static blosc2_cparams cparams;
static blosc2_dparams dparams;
static blosc2_io io;
static blosc2_storage b2_storage = {0};

static blosc2_schunk* b2_schunks[SCHUNKS_SIZE];
static const uint8_t **paths = NULL;
static uint8_t buffer[SCHUNKS_SIZE][SCHUNKS_SIZE * SCHUNKS_SIZE];

static const uint8_t* archive_path = (const uint8_t*) "archive.b2z";
//static const uint8_t* other_archive_path = (const uint8_t*) "other_archive.b2z";
static const uint8_t* dir_path = (const uint8_t*) "dir.b2d";
//static const uint8_t* other_dir_path = (const uint8_t*) "other_dir.b2d";
static blosc2_dict_store *ds = NULL;

static uint8_t* data;

static void s_init_bench(void) {

    data = malloc(SCHUNKS_SIZE);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        for (size_t j = 0; j < SCHUNKS_SIZE * SCHUNKS_SIZE; j+=1) {
            buffer[i][j] = j;
        }
    }

    cparams = blosc2_get_blosc2_cparams_defaults();
    dparams = blosc2_get_blosc2_dparams_defaults();
    io = blosc2_get_blosc2_io_defaults();

    b2_storage.contiguous = true;
    b2_storage.cparams = &cparams;
    b2_storage.dparams = &dparams;
    b2_storage.io = &io;

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_schunk* pschunk = blosc2_schunk_new(&b2_storage);
        blosc2_schunk_append_buffer(pschunk, buffer[i], SCHUNKS_SIZE * SCHUNKS_SIZE);
        b2_schunks[i] = pschunk;
    }

    paths = malloc(sizeof(uint8_t*) * SCHUNKS_SIZE);
    const uint8_t* str_builder = (const uint8_t*) "/key";
    const size_t str_builder_size = strlen((const char*)str_builder);

    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        size_t times_repeated = i + 1;
        size_t some_path_length = str_builder_size * times_repeated;
        uint8_t* some_path = malloc(some_path_length + 1);

        for (size_t j = 0; j < some_path_length; j+=1) {
            some_path[j] = str_builder[j % str_builder_size];
        }
        some_path[some_path_length] = '\0';

        paths[i] = some_path;
    }

    //for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
    //    printf("Path %lu: %s\n", i, (const char*)paths[i]);
    //}
}

static void s_free_bench(void) {
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        free((void*)paths[i]);
        blosc2_schunk_free(b2_schunks[i]);
    }
    free(paths);

}

static void s_set_pairs(blosc2_dict_store *ds) {
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
    }
}

BLOSC_ATTRIBUTE_UNUSED
static void s_generate_b2z(const uint8_t* path)  {
    blosc2_dict_store *pds;
    blosc2_dict_store_new(&pds, path, NULL, -1);
    s_set_pairs(pds);
    blosc2_dict_store_close(pds);
    blosc2_dict_store_free(pds);
}

static void s_bench_set_memory(void) {
  blosc_timestamp_t last, current;
  double totaltime = 0;

  blosc2_dict_store_new(&ds, NULL, NULL, -1);
  blosc_set_timestamp(&last);
  for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
    blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("Memory set", totaltime / (double)SCHUNKS_SIZE);
  blosc2_dict_store_close(ds);
  blosc2_dict_store_free(ds);
}

static void s_bench_set_b2z(void) {
  blosc_timestamp_t last, current;
  double totaltime = 0;

  // B2Z benchmarks
  blosc2_dict_store_new(&ds, archive_path, NULL, -1);
  blosc_set_timestamp(&last);
  for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
    blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("B2Z set", totaltime / (double)SCHUNKS_SIZE);

  blosc_set_timestamp(&last);
  blosc2_dict_store_close(ds);
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("B2Z close - Store from temporal directory into archive", totaltime);

  blosc2_dict_store_free(ds);
  remove((const char*)archive_path);
}

static void s_bench_set_b2d(void) {
  blosc_timestamp_t last, current;
  double totaltime = 0;

  blosc2_dict_store_new(&ds, dir_path, NULL, -1);
  blosc_set_timestamp(&last);
  for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
    blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("B2D set", totaltime / (double)SCHUNKS_SIZE);

  blosc_set_timestamp(&last);
  blosc2_dict_store_close(ds);
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("B2D close", totaltime);

  blosc2_dict_store_free(ds);
  blosc2_remove_dir_recursive((const char*)dir_path);
}

static void s_bench_open_b2z(void) {
  blosc_timestamp_t last, current;
  double totaltime = 0;

  blosc2_dict_store_new(&ds, archive_path, NULL, -1);
  for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
    blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
  }
  blosc2_dict_store_close(ds);
  blosc2_dict_store_free(ds);
  ds = NULL;

  blosc2_dict_store* arr_ds[SCHUNKS_SIZE];
  blosc_set_timestamp(&last);
  for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
      blosc2_dict_store_open(&arr_ds[i], archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("Open B2Z RDONLY", totaltime / (double)SCHUNKS_SIZE);
  for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
      blosc2_dict_store_free(arr_ds[i]);
  }

  blosc_set_timestamp(&last);
  for (size_t i = 0; i < 5; i+=1) {
      blosc2_dict_store_open(&arr_ds[i], archive_path, BLOSC2_STORE_OPEN_MODE_RDWR);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  PRINT_RESULT("Open B2Z RDWR", totaltime / 5.0);
  for (size_t i = 0; i < 5; i+=1) {
      blosc2_dict_store_close(arr_ds[i]);
      blosc2_dict_store_free(arr_ds[i]);
  }

  remove((const char*)archive_path);
}

static void s_bench_open_b2d(void) {
    blosc_timestamp_t last, current;
    double totaltime = 0;

    blosc2_dict_store_new(&ds, dir_path, NULL, -1);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
    }
    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    ds = NULL;

    blosc2_dict_store* arr_ds[SCHUNKS_SIZE];
    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_open(&arr_ds[i], dir_path, BLOSC2_STORE_OPEN_MODE_RDONLY);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);
    PRINT_RESULT("Open B2D RDONLY", totaltime / (double)SCHUNKS_SIZE);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_free(arr_ds[i]);
    }

    blosc_set_timestamp(&last);
    for (size_t i = 0; i < 5; i+=1) {
        blosc2_dict_store_open(&arr_ds[i], dir_path, BLOSC2_STORE_OPEN_MODE_RDWR);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);
    PRINT_RESULT("Open B2D RDWR", totaltime / 5.0);
    for (size_t i = 0; i < 5; i+=1) {
        blosc2_dict_store_close(arr_ds[i]);
        blosc2_dict_store_free(arr_ds[i]);
    }

    blosc2_remove_dir_recursive((const char*)dir_path);
}

static void s_bench_get_memory(void) {
    blosc_timestamp_t last, current;
    double totaltime = 0;

    blosc2_dict_store_new(&ds, NULL, NULL, -1);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
    }

    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        blosc2_dict_store_get(ds, paths[i], &item);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);

    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    PRINT_RESULT("MEMORY get", totaltime / (double)SCHUNKS_SIZE);
}

static void s_bench_get_b2z(void) {
    blosc_timestamp_t last, current;
    double totaltime = 0;
    int rc = BLOSC2_ERROR_SUCCESS;
    (void)(rc);

    blosc2_dict_store_new(&ds, archive_path, NULL, -1);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_set(ds, paths[i], &(blosc2_store_value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .schunk = b2_schunks[i]});
    }

    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        rc = blosc2_dict_store_get(ds, paths[i], &item);
        rc = blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        rc = blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);
    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    ds = NULL;
    PRINT_RESULT("B2Z get - new", totaltime / (double)SCHUNKS_SIZE);

    rc = blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY);
    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        rc = blosc2_dict_store_get(ds, paths[i], &item);
        rc = blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);

    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    PRINT_RESULT("B2Z get - RDONLY", totaltime / (double)SCHUNKS_SIZE);

    blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDWR);
    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        blosc2_dict_store_get(ds, paths[i], &item);
        rc = blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);

    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    PRINT_RESULT("B2Z get - RDWR", totaltime / (double)SCHUNKS_SIZE);
    remove((const char*)archive_path);
}

static void s_bench_get_b2d(void) {
    blosc_timestamp_t last, current;
    double totaltime = 0;
    int rc = BLOSC2_ERROR_SUCCESS;
    (void)(rc);

    blosc2_dict_store_new(&ds, dir_path, NULL, -1);
    s_set_pairs(ds);

    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        blosc2_dict_store_get(ds, paths[i], &item);
        rc = blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);

    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    ds = NULL;
    PRINT_RESULT("B2D get - new", totaltime / (double)SCHUNKS_SIZE);

    blosc2_dict_store_open(&ds, dir_path, BLOSC2_STORE_OPEN_MODE_RDONLY);
    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        rc = blosc2_dict_store_get(ds, paths[i], &item);
        rc = blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);

    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    PRINT_RESULT("B2D get - RDONLY", totaltime / (double)SCHUNKS_SIZE);

    blosc2_dict_store_open(&ds, dir_path, BLOSC2_STORE_OPEN_MODE_RDWR);
    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        blosc2_dict_store_get(ds, paths[i], &item);
        rc = blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);

    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
    PRINT_RESULT("B2D get - RDWR", totaltime / (double)SCHUNKS_SIZE);
    blosc2_remove_dir_recursive((const char*)dir_path);
}

static void s_bench_to_buffer(void) {
    blosc_timestamp_t last, current;
    double totaltime = 0;

    blosc2_dict_store_new(&ds, NULL, NULL, -1);
    s_set_pairs(ds);

    void* buffer = NULL;
    size_t buffer_size = 0;

    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_to_buffer(ds, &buffer, &buffer_size);
        free(buffer);
    }

    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);
    PRINT_RESULT("To Buffer", totaltime / (double)SCHUNKS_SIZE);
}

static void s_bench_from_buffer(void) {
    blosc_timestamp_t last, current;
    double totaltime = 0;

    blosc2_dict_store_new(&ds, NULL, NULL, -1);
    s_set_pairs(ds);

    void* buffer = NULL;
    blosc2_dict_store* arr_ds[SCHUNKS_SIZE];
    size_t buffer_size = 0;
    blosc2_dict_store_to_buffer(ds, &buffer, &buffer_size);

    blosc_set_timestamp(&last);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1 ) {
        blosc2_dict_store_from_buffer(&arr_ds[i], buffer, buffer_size);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(last, current);
    PRINT_RESULT("From Buffer", totaltime / (double)SCHUNKS_SIZE);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_dict_store_free(arr_ds[i]);
    }

    free(buffer);
}

BLOSC_ATTRIBUTE_UNUSED
static void s_perf_get_b2z_rdonly(void) {
    blosc_timestamp_t least, current;
    double totaltime = 0;

    blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDONLY);
    blosc_set_timestamp(&least);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        blosc2_dict_store_get(ds, paths[i], &item);
        blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(least, current);
    PRINT_RESULT("B2Z GET RDONLY", totaltime / (double)SCHUNKS_SIZE);
    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
}

BLOSC_ATTRIBUTE_UNUSED
static void s_perf_get_b2z_rdwr(void) {
    blosc_timestamp_t least, current;
    double totaltime = 0;

    blosc2_dict_store_open(&ds, archive_path, BLOSC2_STORE_OPEN_MODE_RDWR);
    blosc_set_timestamp(&least);
    for (size_t i = 0; i < SCHUNKS_SIZE; i+=1) {
        blosc2_store_value item = {0};
        blosc2_dict_store_get(ds, paths[i], &item);
        blosc2_schunk_get_slice_buffer(item.schunk, 0, 1, data);
        blosc2_schunk_free(item.schunk);
    }
    blosc_set_timestamp(&current);
    totaltime = blosc_elapsed_secs(least, current);
    PRINT_RESULT("B2Z GET RDWR", totaltime / (double)SCHUNKS_SIZE);
    blosc2_dict_store_close(ds);
    blosc2_dict_store_free(ds);
}

int main(void) {
    blosc2_init();
    s_init_bench();

    PRINT_LABEL("Start bench");

    //s_generate_b2z(archive_path);
    //s_perf_get_b2z_rdonly();
    ////s_perf_get_b2z_rdwr();
    //remove((const char*)archive_path);

    s_bench_set_memory();
    s_bench_set_b2z();
    s_bench_set_b2d();
    s_bench_get_memory();
    s_bench_get_b2z();
    s_bench_get_b2d();
    s_bench_open_b2z();
    s_bench_open_b2d();
    s_bench_to_buffer();
    s_bench_from_buffer();

    PRINT_LABEL("End bench");
    s_free_bench();
    blosc2_destroy();
    return 0;
}

/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

# include <b2nd.h>
# include <stdlib.h>
# include "time.h"


int frame_generator(int8_t *data, int8_t ndim, int64_t *shape, int32_t *chunkshape,
                    int32_t *blockshape, int32_t typesize, int64_t size, char *urlpath) {
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.urlpath = urlpath;
  b2_storage.contiguous = true;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, size));
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  b2nd_print_meta(arr);
  BLOSC_ERROR(b2nd_free(arr));

  return 0;
}

int rand_() {
  int ndim = 3;
  int typesize = 4;
  int64_t shape[] = {32, 18, 32};
  int32_t chunkshape[] = {17, 16, 24};
  int32_t blockshape[] = {8, 9, 8};
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= (int) (shape[i]);
  }
  int64_t size = typesize * nelem;
  float *data = malloc(size);
  for (int64_t i = 0; i < nelem; i++) {
    data[i] = (float) (rand() % 220);
  }
  char *urlpath = "rand.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int all_eq() {
  int8_t ndim = 3;
  int64_t shape[] = {100, 50, 100};
  int32_t chunkshape[] = {40, 20, 60};
  int32_t blockshape[] = {20, 10, 30};
  int32_t typesize = 8;
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  int8_t *data = malloc(size);
  for (int i = 0; i < nelem; i++) {
    data[i] = (int8_t) 22;
  }
  char *urlpath = "all_eq.b2nd";
  BLOSC_ERROR(frame_generator(data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int cyclic() {
  int8_t ndim = 3;
  int64_t shape[] = {100, 50, 100};
  int32_t chunkshape[] = {40, 20, 60};
  int32_t blockshape[] = {20, 10, 30};
  int32_t typesize = 8;
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  int8_t *data = malloc(size);
  for (int i = 0; i < nelem; i++) {
    data[i] = (int8_t) i;
  }
  char *urlpath = "cyclic.b2nd";
  BLOSC_ERROR(frame_generator(data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int same_cells() {
  int ndim = 2;
  int typesize = 8;
  int64_t shape[] = {128, 111};
  int32_t chunkshape[] = {32, 11};
  int32_t blockshape[] = {16, 7};
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= (int) (shape[i]);
  }
  int64_t size = typesize * nelem;
  double *data = malloc(size);
  for (int64_t i = 0; i < (nelem / 4); i++) {
    data[i * 4] = (double) 11111111;
    data[i * 4 + 1] = (double) 99999999;
  }
  char *urlpath = "same_cells.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int some_matches() {
  int ndim = 2;
  int typesize = 8;
  int64_t shape[] = {128, 111};
  int32_t chunkshape[] = {48, 32};
  int32_t blockshape[] = {14, 18};
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= (int) (shape[i]);
  }
  int64_t size = typesize * nelem;
  double *data = malloc(size);
  for (int64_t i = 0; i < (nelem / 2); i++) {
    data[i] = (double) i;
  }
  for (int64_t i = (nelem / 2); i < nelem; i++) {
    data[i] = (double) 1;
  }
  char *urlpath = "some_matches.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int many_matches() {
  int8_t ndim = 3;
  int64_t shape[] = {80, 120, 111};
  int32_t chunkshape[] = {40, 30, 50};
  int32_t blockshape[] = {11, 14, 24};
  int32_t typesize = 8;
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  int8_t *data = malloc(size);
  for (int i = 0; i < nelem; i += 2) {
    data[i] = (int8_t) i;
    data[i + 1] = (int8_t) 2;
  }
  char *urlpath = "many_matches.b2nd";
  BLOSC_ERROR(frame_generator(data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int float_cyclic() {
  int8_t ndim = 3;
  int64_t shape[] = {40, 60, 20};
  int32_t chunkshape[] = {20, 30, 16};
  int32_t blockshape[] = {11, 14, 7};
  int32_t typesize = sizeof(float);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  float *data = malloc(size);
  for (int i = 0; i < nelem; i += 2) {
    float j = (float) i;
    data[i] = (j + j / 10 + j / 100);
    data[i + 1] = (2 + j / 10 + j / 1000);
  }
  char *urlpath = "example_float_cyclic.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int double_same_cells() {
  int8_t ndim = 2;
  int64_t shape[] = {40, 60};
  int32_t chunkshape[] = {20, 30};
  int32_t blockshape[] = {16, 16};
  int32_t typesize = sizeof(double);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  double *data = malloc(size);
  for (int i = 0; i < nelem; i += 4) {
    data[i] = 1.5;
    data[i + 1] = 14.7;
    data[i + 2] = 23.6;
    data[i + 3] = 3.2;
  }
  char *urlpath = "example_double_same_cells.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int big_float_frame() {
  int ndim = 3;
  int64_t shape[] = {200, 310, 214};
  int32_t chunkshape[] = {110, 120, 76};
  int32_t blockshape[] = {57, 52, 35};
  int32_t typesize = sizeof(float);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  float *data = malloc(size);
  for (int i = 0; i < nelem; i += 4) {
    float j = (float) i;
    data[i] = (float) 2.73;
    data[i + 1] = (2 + j / 10 + j / 1000);
    data[i + 2] = (7 + j / 10 - j / 100);
    data[i + 3] = (11 + j / 100 - j / 1000);
  }
  char *urlpath = "example_big_float_frame.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int day_month_temp() {
  int ndim = 2;
  int64_t shape[] = {400, 3};
  int32_t chunkshape[] = {110, 3};
  int32_t blockshape[] = {57, 3};
  int32_t typesize = sizeof(float);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  float temp_min = -20;
  float temp_max = 40;
  srand(time(NULL));
  float *data = malloc(size);
  for (int i = 0; i < nelem / 3; i++) {
    data[i] = (float) (rand() % 31);
    data[i + 1] = (float) (rand() % 12);
    data[i + 2] = ((float) (rand() % 10000) / 10000 * (temp_max - temp_min) + temp_min);
    i += 3;
  }
  char *urlpath = "example_day_month_temp.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}

int item_prices() {
  int ndim = 3;
  int64_t shape[] = {12, 25, 250};
  int32_t chunkshape[] = {8, 10, 50};
  int32_t blockshape[] = {4, 5, 10};
  int32_t typesize = sizeof(float);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  float price_min = (float) 1;        // if I choose 0.99 results are less aproppiate
  float price_max = (float) 251;
  float *data = malloc(size);
  int index = 0;
  for (int month = 1; month <= shape[0]; month++) {               // month (1 to 12)
    for (int store = 1; store <= shape[1]; store++) {           // store ID (less to more expensive)
      for (int item = 1; item <= shape[2]; item++) {           // item ID
        srand(item);
        data[index] = ((float) store + (float) (3 - (month % 3)) *
                                       ((float) (rand() % 1000) / 1000 * (price_max - price_min) + price_min));
        index++;
      }
    }
  }
  char *urlpath = "example_item_prices.b2nd";
  BLOSC_ERROR(frame_generator((int8_t *) data, ndim, shape, chunkshape, blockshape, typesize, size, urlpath));

  return 0;
}


int main() {
  blosc2_init();

  int err;
  err = rand_();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Rand_ error: %d", err);
  }
  err = all_eq();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n All_eq error: %d", err);
  }
  err = cyclic();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Cyclic error: %d", err);
  }
  err = same_cells();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Same_cells error: %d", err);
  }
  err = some_matches();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Some_matches error: %d", err);
  }
  err = many_matches();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Many_matches error: %d", err);
  }
  err = float_cyclic();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Float_cyclic error: %d", err);
  }
  err = double_same_cells();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Double_same_cells error: %d", err);
  }
  err = big_float_frame();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Double_same_cells error: %d", err);
  }
  err = day_month_temp();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Day_month_temp error: %d", err);
  }
  err = item_prices();
  if (err != BLOSC2_ERROR_SUCCESS) {
    printf("\n Item_prices error: %d", err);
  }

  blosc2_destroy();

  return err;
}

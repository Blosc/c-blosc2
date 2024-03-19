/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef CUTEST_CUTEST_H
#define CUTEST_CUTEST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define RESET   "\033[0m"


#define CUNIT_OK 0
#define CUNIT_FAIL 1


#define CUTEST_DATA(...) __VA_ARGS__

#define CUTEST_PARAMETRIZE(name, type, ...)                                       \
    do {                                                                          \
        type cutest_##name[] = {__VA_ARGS__};                                     \
        _cutest_parametrize(#name, cutest_##name,                                 \
                            sizeof(cutest_##name) / sizeof(type), sizeof(type));  \
    } while(0)

#define CUTEST_PARAMETRIZE2(name, type, params_len, params)                       \
    do {                                                                          \
        (type) *cutest_##name = params;                                           \
        _cutest_parametrize(#name, cutest_##name, params_len, sizeof(type));      \
    } while(0)

#define CUTEST_GET_PARAMETER(name, type) \
    type name = * (type *) _cutest_get_parameter(#name)

#define CUTEST_TEST_SETUP(sname) \
    void sname##_setup()

#define CUTEST_TEST_TEARDOWN(sname) \
    void sname##_teardown()


#define CUTEST_TEST_TEST(sname)   \
    CUTEST_TEST_SETUP(sname);     \
    CUTEST_TEST_TEARDOWN(sname);  \
    int sname##_test();           \
    int sname##_test()


#define CUTEST_TEST_RUN(sname)                                \
        do {                                                  \
          _cutest_setup();                                    \
          sname##_setup();                                    \
          int rc = _cutest_run((int (*)(void)) sname##_test,  \
                               #sname);                       \
          sname##_teardown();                                 \
          _cutest_teardown();                                 \
          return rc;                                          \
    } while(0)

#define CUTEST_ASSERT(msg, cond)                                                     \
    do {                                                                             \
        if (!(cond)) {                                                               \
            sprintf(_cutest_error_msg, "Error: %s %s:%d", msg, __FILE__, __LINE__);  \
            return CUNIT_FAIL;                                                       \
        }                                                                            \
    } while(0)


#define CUTEST_PARAMS_MAX 16
#define MAXLEN_TESTNAME 1024


typedef struct {
    char *name;
    uint8_t *params;
    int32_t params_len;
    int32_t param_size;
} cutest_param_t;

static cutest_param_t cutest_params[CUTEST_PARAMS_MAX] = {0};
static int8_t cutest_params_ind[CUTEST_PARAMS_MAX] = {0};


void _cutest_parametrize(char *name, void *params, int32_t params_len, int32_t param_size) {
  int i = 0;
  while (cutest_params[i].name != NULL) {
    i++;
  }
  uint8_t *new_params = malloc(param_size * params_len);
  char *new_name = strdup(name);
  memcpy(new_params, params, param_size * params_len);
  cutest_params[i].name = new_name;
  cutest_params[i].params = new_params;
  cutest_params[i].param_size = param_size;
  cutest_params[i].params_len = params_len;
}

uint8_t *_cutest_get_parameter(char *name) {
  int i = 0;
  while (strcmp(cutest_params[i].name, name) != 0) {
    i++;
  }
  return cutest_params[i].params + cutest_params_ind[i] * cutest_params[i].param_size;
}


void _cutest_setup() {
  for (int i = 0; i < CUTEST_PARAMS_MAX; ++i) {
    cutest_params[i].name = NULL;
  }
}


void _cutest_teardown() {
  int i = 0;
  while (cutest_params[i].name != NULL) {
    free(cutest_params[i].params);
    free(cutest_params[i].name);
    i++;
  }
}


char _cutest_error_msg[1024];


int _cutest_run(int (*test)(void), char *name) {
  int cutest_ok = 0;
  int cutest_failed = 0;
  int cutest_total = 0;

  int nparams = 0;
  while (cutest_params[nparams].name != NULL) {
    nparams++;
  }

  int niters = 1;
  for (int i = 0; i < nparams; ++i) {
    niters *= cutest_params[i].params_len;
  }

  int32_t params_strides[CUTEST_PARAMS_MAX] = {0};
  params_strides[0] = 1;
  for (int i = 1; i < nparams; ++i) {
    params_strides[i] = params_strides[i - 1] * cutest_params[i - 1].params_len;
  }

  char test_name[MAXLEN_TESTNAME + 1];
  char aux[MAXLEN_TESTNAME + 1];
  uint8_t count = 0;
  int num = niters;
  do {
    count++;
    num /= 10;
  } while (num != 0);
  for (int niter = 0; niter < niters; ++niter) {
    sprintf(test_name, "[%0*d/%d] %s(", count, niter + 1, niters, name);
    for (int i = 0; i < nparams; ++i) {
      cutest_params_ind[i] = (int8_t) (niter / params_strides[i] %
                                       cutest_params[i].params_len);
      strcpy(aux, test_name);
      snprintf(test_name, MAXLEN_TESTNAME, "%s%s[%d], ", aux, cutest_params[i].name,
               cutest_params_ind[i]);
    }
    if (nparams > 0)
      test_name[strlen(test_name) - 2] = ')';
    test_name[strlen(test_name) - 1] = '\0';
    printf("%s ", test_name);

    cutest_total++;

    int rc = test();
    if (rc == CUNIT_OK) {
      cutest_ok++;
      fprintf(stdout, GREEN "[  OK  ]\n" RESET);
    } else {
      cutest_failed++;
      fprintf(stdout, RED   "[FAILED]\n" RESET);
    }
    if (_cutest_error_msg[0] != 0) {
      fprintf(stdout, RED "    %s\n" RESET, _cutest_error_msg);
      _cutest_error_msg[0] = 0;
    }
  }

  printf("\nTEST RESULTS: %d tests (%d ok, %d failed)\n",
         cutest_total, cutest_ok, cutest_failed);

  return cutest_failed;
}


#endif //CUTEST_CUTEST_H

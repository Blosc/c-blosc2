#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
#include "threading.h"

static int is_expected_compressor(const char* comp) {
    return comp != NULL &&
           (strcmp(comp, "blosclz") == 0 || strcmp(comp, "lz4") == 0);
}

struct set_compressor_arg {
    const char* comp;
    int rc;
};

struct get_compressor_arg {
    volatile int stop;
    int errors;
};

void* set_compressor_thread(void* arg) {
    struct set_compressor_arg* state = (struct set_compressor_arg*)arg;
    state->rc = blosc1_set_compressor(state->comp);
    return NULL;
}

void* get_compressor_thread(void* arg) {
    struct get_compressor_arg* state = (struct get_compressor_arg*)arg;
    while (!state->stop) {
        const char* comp = blosc1_get_compressor();
        if (!is_expected_compressor(comp)) {
            state->errors++;
        }
    }
    return NULL;
}

int main() {
    int i;
    const int iterations = 1000;

    blosc2_init();

    for (i = 0; i < iterations; ++i) {
        blosc2_pthread_t t1, t2, treader;
        int rc;
        const char* comp;

        struct set_compressor_arg arg1 = {"blosclz", 0};
        struct set_compressor_arg arg2 = {"lz4", 0};
        struct get_compressor_arg reader_arg = {0, 0};

        rc = blosc2_pthread_create(&treader, NULL, get_compressor_thread, &reader_arg);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_create for reader failed on iteration %d: %d\n", i, rc);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_create(&t1, NULL, set_compressor_thread, &arg1);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_create for t1 failed on iteration %d: %d\n", i, rc);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_create(&t2, NULL, set_compressor_thread, &arg2);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_create for t2 failed on iteration %d: %d\n", i, rc);
            blosc2_pthread_join(t1, NULL);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_join(t1, NULL);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_join for t1 failed on iteration %d: %d\n", i, rc);
            blosc2_pthread_join(t2, NULL);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_join(t2, NULL);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_join for t2 failed on iteration %d: %d\n", i, rc);
            reader_arg.stop = 1;
            blosc2_pthread_join(treader, NULL);
            blosc2_destroy();
            return 1;
        }

        reader_arg.stop = 1;
        rc = blosc2_pthread_join(treader, NULL);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_join for reader failed on iteration %d: %d\n", i, rc);
            blosc2_destroy();
            return 1;
        }
        if (reader_arg.errors != 0) {
            fprintf(stderr, "blosc1_get_compressor() returned invalid value %d times on iteration %d\n",
                    reader_arg.errors, i);
            blosc2_destroy();
            return 1;
        }

        if (arg1.rc < 0) {
            fprintf(stderr, "blosc1_set_compressor(\"blosclz\") failed on iteration %d: %d\n",
                    i, arg1.rc);
            blosc2_destroy();
            return 1;
        }

        if (arg2.rc < 0) {
            fprintf(stderr, "blosc1_set_compressor(\"lz4\") failed on iteration %d: %d\n",
                    i, arg2.rc);
            blosc2_destroy();
            return 1;
        }

        comp = blosc1_get_compressor();
        if (!is_expected_compressor(comp)) {
            fprintf(stderr, "Unexpected compressor on iteration %d: %s\n",
                    i, comp != NULL ? comp : "(null)");
            blosc2_destroy();
            return 1;
        }
    }

    blosc2_destroy();
    return 0;
}
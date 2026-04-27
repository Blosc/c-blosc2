#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
#include "threading.h"

static int is_expected_compressor(const char* comp) {
    return comp != NULL &&
           (strcmp(comp, "blosclz") == 0 || strcmp(comp, "lz4") == 0);
}

void* set_compressor_thread(void* arg) {
    const char* comp = (const char*)arg;
    int rc = blosc1_set_compressor(comp);
    return (void*)(intptr_t)rc;
}

int main() {
    int i;
    const int iterations = 1000;

    blosc2_init();

    for (i = 0; i < iterations; ++i) {
        blosc2_pthread_t t1, t2;
        void* t1_result;
        void* t2_result;
        int rc;
        const char* comp;

        rc = blosc2_pthread_create(&t1, NULL, set_compressor_thread, "blosclz");
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_create for t1 failed on iteration %d: %d\n", i, rc);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_create(&t2, NULL, set_compressor_thread, "lz4");
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_create for t2 failed on iteration %d: %d\n", i, rc);
            blosc2_pthread_join(t1, NULL);
            blosc2_destroy();
            return 1;
        }

        comp = blosc1_get_compressor();
        if (comp != NULL && !is_expected_compressor(comp)) {
            fprintf(stderr, "Unexpected compressor before join on iteration %d: %s\n",
                    i, comp != NULL ? comp : "(null)");
            blosc2_pthread_join(t1, NULL);
            blosc2_pthread_join(t2, NULL);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_join(t1, &t1_result);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_join for t1 failed on iteration %d: %d\n", i, rc);
            blosc2_pthread_join(t2, NULL);
            blosc2_destroy();
            return 1;
        }

        rc = blosc2_pthread_join(t2, &t2_result);
        if (rc != 0) {
            fprintf(stderr, "blosc2_pthread_join for t2 failed on iteration %d: %d\n", i, rc);
            blosc2_destroy();
            return 1;
        }

        if ((int)(intptr_t)t1_result < 0) {
            fprintf(stderr, "blosc1_set_compressor(\"blosclz\") failed on iteration %d: %d\n",
                    i, (int)(intptr_t)t1_result);
            blosc2_destroy();
            return 1;
        }

        if ((int)(intptr_t)t2_result < 0) {
            fprintf(stderr, "blosc1_set_compressor(\"lz4\") failed on iteration %d: %d\n",
                    i, (int)(intptr_t)t2_result);
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
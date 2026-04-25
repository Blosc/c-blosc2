#include <stdio.h>
#include <stdlib.h>
#include "blosc2.h"
#include "threading.h"

void* set_compressor_thread(void* arg) {
    const char* comp = (const char*)arg;
    blosc1_set_compressor(comp);
    return NULL;
}

int main() {
    blosc2_init();

    blosc2_pthread_t t1, t2;
    blosc2_pthread_create(&t1, NULL, set_compressor_thread, "blosclz");
    blosc2_pthread_create(&t2, NULL, set_compressor_thread, "lz4");

    blosc2_pthread_join(t1, NULL);
    blosc2_pthread_join(t2, NULL);

    const char* comp = blosc1_get_compressor();
    printf("Final compressor: %s\n", comp);

    blosc2_destroy();
    return 0;
}
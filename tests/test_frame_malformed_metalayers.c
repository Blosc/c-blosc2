#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
#include "test_common.h"

int tests_run = 0;

static char* test_malformed_metalayers(void) {
    blosc2_init();

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.cparams = &cparams;
    storage.dparams = &dparams;

    blosc2_schunk* schunk = blosc2_schunk_new(&storage);
    mu_assert("Failed to create schunk", schunk != NULL);

    // Add a metalayer
    uint8_t content[] = "This is a test content";
    blosc2_meta_add(schunk, "bad_meta", content, sizeof(content));

    uint8_t* cframe;
    bool needs_free;
    int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
    mu_assert("Failed to serialize schunk", len >= 0);

    // Find the metalayer name to tamper with it
    const char* target = "bad_meta";
    int target_len = strlen(target);
    int found = 0;
    
    for (int64_t i = 0; i < len - target_len; i++) {
        if (memcmp(cframe + i, target, target_len) == 0) {
            uint8_t* idxp = cframe + i + target_len;
            if (*idxp == 0xd2) {
                idxp++;
                int32_t offset = (idxp[0] << 24) | (idxp[1] << 16) | (idxp[2] << 8) | idxp[3];
                
                uint8_t* content_marker = cframe + offset;
                if (*content_marker == 0xc6) {
                    // Tamper with the content length (big endian)
                    uint8_t* clen_ptr = content_marker + 1;
                    int32_t bad_len = 0x7FFFFFFF;
                    clen_ptr[0] = (bad_len >> 24) & 0xFF;
                    clen_ptr[1] = (bad_len >> 16) & 0xFF;
                    clen_ptr[2] = (bad_len >> 8) & 0xFF;
                    clen_ptr[3] = bad_len & 0xFF;
                    found = 1;
                }
            }
            break;
        }
    }

    mu_assert("Could not find the metalayer in the buffer", found == 1);

    // Now try to open the tampered buffer
    blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, len, true);
    
    mu_assert("Malformed frame should be rejected", schunk2 == NULL);

    if (needs_free) {
        free(cframe);
    }
    blosc2_schunk_free(schunk);
    blosc2_destroy();

    return EXIT_SUCCESS;
}

static char *all_tests(void) {
    mu_run_test(test_malformed_metalayers);
    return EXIT_SUCCESS;
}

int main(void) {
    char *result;
    result = all_tests();
    if (result != EXIT_SUCCESS) {
        printf(" (%s)\n", result);
    } else {
        printf(" ALL TESTS PASSED\n");
    }
    printf("\tTests run: %d\n", tests_run);

    return result != EXIT_SUCCESS;
}

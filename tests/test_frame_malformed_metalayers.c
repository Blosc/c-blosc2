/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include "blosc2.h"
#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

/* 
 * This PoC demonstrates a maliciously crafted metalayer header that aims to 
 * bypass bounds checks via integer overflow. 
 */

int tests_run = 0;

static char* test_malformed_metalayer_bounds(void) {
    blosc2_schunk* schunk = NULL;
    blosc2_schunk* malicious_schunk = NULL;
    uint8_t *buffer = NULL;
    bool needs_free = false;
    char* result_msg = NULL;

    /* 1. Create a minimal valid schunk and frame */
    int32_t data[100] = {0};
    blosc2_storage storage = {.contiguous = true};
    schunk = blosc2_schunk_new(&storage);
    if (schunk == NULL) return "Failed to create schunk";
    
    blosc2_schunk_append_buffer(schunk, data, sizeof(data));

    /* 2. Add a standard metalayer to serve as a template */
    uint8_t content[] = "normal content";
    blosc2_meta_add(schunk, "testmeta", content, sizeof(content));

    /* 3. Export to buffer */
    int64_t buffer_len = blosc2_schunk_to_buffer(schunk, &buffer, &needs_free);
    if (buffer_len <= 0) {
        result_msg = "Failed to create frame buffer";
        goto cleanup;
    }

    /* 4. Malform the buffer: Inject malicious metalayer metadata */
    int32_t header_len;
    from_big(&header_len, buffer + FRAME_HEADER_LEN, sizeof(header_len));
    
    if (header_len < FRAME_IDX_SIZE + 2) {
        result_msg = "Header too small for index";
        goto cleanup;
    }

    /* Parse the metalayer index (map) to find "testmeta" */
    uint8_t *idx_ptr = buffer + FRAME_IDX_SIZE + 2; 
    if (*idx_ptr != 0xde) {
        result_msg = "Could not find metalayer index marker";
        goto cleanup;
    }
    idx_ptr++;
    
    uint16_t nmetalayers;
    from_big(&nmetalayers, idx_ptr, sizeof(nmetalayers));
    idx_ptr += 2;
    
    int found = 0;
    for (int i = 0; i < nmetalayers; i++) {
        if ((*idx_ptr & 0xe0u) != 0xa0u) break;
        uint8_t nslen = *idx_ptr & 0x1fu;
        idx_ptr++;
        
        if (nslen == strlen("testmeta") && memcmp(idx_ptr, "testmeta", nslen) == 0) {
            idx_ptr += nslen;
            if (*idx_ptr != 0xd2) {
                result_msg = "Unexpected offset marker";
                goto cleanup;
            }
            idx_ptr++;
            
            /* Get the original offset to the content marker */
            int32_t original_offset;
            from_big(&original_offset, idx_ptr, sizeof(original_offset));
            
            if (original_offset < 0 || original_offset >= buffer_len) {
                result_msg = "Invalid original offset";
                goto cleanup;
            }

            uint8_t *content_marker = buffer + original_offset;
            if (*content_marker != 0xc6) {
                result_msg = "Expected bin32 MessagePack marker (0xc6)";
                goto cleanup;
            }
            
            int32_t malicious_len = INT32_MAX;
            to_big(content_marker + 1, &malicious_len, sizeof(malicious_len));

            /* 
             * Overwrite index offset with header_len - FRAME_META_HDR_SIZE to 
             * pass the first check but trigger the second one.
             */
            if (header_len < FRAME_META_HDR_SIZE) {
                result_msg = "header_len too small for malicious injection";
                goto cleanup;
            }
            int32_t malicious_offset = header_len - FRAME_META_HDR_SIZE; 
            to_big(idx_ptr, &malicious_offset, sizeof(malicious_offset));

            found = 1;
            break;
        }
        idx_ptr += nslen + 1 + 4; 
    }

    if (!found) {
        result_msg = "Could not locate 'testmeta' metalayer in buffer";
        goto cleanup;
    }

    /* 5. Attempt to open the malformed frame */
    malicious_schunk = blosc2_schunk_from_buffer(buffer, buffer_len, false);

    if (malicious_schunk != NULL) {
        result_msg = "FAILURE: Malformed frame was ACCEPTED! (Potential Vulnerability)";
    }

cleanup:
    if (malicious_schunk != NULL) blosc2_schunk_free(malicious_schunk);
    if (schunk != NULL) blosc2_schunk_free(schunk);
    if (needs_free && buffer != NULL) free(buffer);

    return result_msg;
}

static char *all_tests(void) {
    mu_run_test(test_malformed_metalayer_bounds);
    return EXIT_SUCCESS;
}

int main(void) {
    char *result;

    blosc2_init();
    result = all_tests();
    if (result != EXIT_SUCCESS) {
        printf(" (%s)\n", result);
    }
    else {
        printf(" ALL TESTS PASSED");
    }
    printf("\tTests run: %d\n", tests_run);

    blosc2_destroy();

    return result != EXIT_SUCCESS;
}

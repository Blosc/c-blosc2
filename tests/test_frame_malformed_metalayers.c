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
#include "blosc2.h"
#include "blosc-private.h"
#include "frame.h"

/* 
 * This PoC demonstrates a maliciously crafted metalayer header that aims to 
 * bypass bounds checks via integer overflow. 
 */

int main() {
    blosc2_schunk* schunk = NULL;
    blosc2_schunk* malicious_schunk = NULL;
    uint8_t *buffer = NULL;
    bool needs_free = false;
    int result = 1;

    blosc2_init();

    /* 1. Create a minimal valid schunk and frame */
    int32_t data[100] = {0};
    blosc2_storage storage = {.contiguous = true};
    schunk = blosc2_schunk_new(&storage);
    if (schunk == NULL) {
        printf("Failed to create schunk\n");
        goto cleanup;
    }
    blosc2_schunk_append_buffer(schunk, data, sizeof(data));

    /* 2. Add a standard metalayer to serve as a template */
    uint8_t content[] = "normal content";
    blosc2_meta_add(schunk, "testmeta", content, sizeof(content));

    /* 3. Export to buffer */
    int64_t buffer_len = blosc2_schunk_to_buffer(schunk, &buffer, &needs_free);
    
    if (buffer_len <= 0) {
        printf("Failed to create frame buffer\n");
        goto cleanup;
    }

    /* 4. Malform the buffer: Inject malicious metalayer metadata */
    int32_t header_len;
    from_big(&header_len, buffer + FRAME_HEADER_LEN, sizeof(header_len));
    
    printf("Original Header Len: %d\n", header_len);

    /* Parse the metalayer index (map) to find "testmeta" */
    uint8_t *idx_ptr = buffer + FRAME_IDX_SIZE + 2; // Skip idx_size
    if (*idx_ptr != 0xde) {
        printf("Error: Could not find metalayer index marker\n");
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
            printf("Found 'testmeta' in index at offset %td\n", idx_ptr - buffer);
            idx_ptr += nslen;
            
            if (*idx_ptr != 0xd2) {
                printf("Error: Unexpected offset marker 0x%02x\n", *idx_ptr);
                goto cleanup;
            }
            idx_ptr++;
            
            /* Get the original offset to the content marker */
            int32_t original_offset;
            from_big(&original_offset, idx_ptr, sizeof(original_offset));
            
            /* 
             * Keep the overall frame/header invariants intact so metalayer parsing is reached.
             * Only corrupt the embedded metalayer content length or offsets to exercise 
             * the overflow-resistant bounds checks in the metalayer parser itself.
             */
            uint8_t *content_marker = buffer + original_offset;
            if (*content_marker == 0xc6) {
                int32_t malicious_len = INT32_MAX;
                to_big(content_marker + 1, &malicious_len, sizeof(malicious_len));
                printf("Overwrote content_len at offset %d with INT32_MAX while preserving frame invariants\n", 
                       original_offset + 1);
            } else {
                printf("Error: Unexpected content marker 0x%02x at offset %d\n", 
                       *content_marker, original_offset);
                goto cleanup;
            }

            /* 
             * We can also test the offset overflow if we have enough buffer,
             * but since this frame is small, setting a huge offset would be caught 
             * by (offset >= header_len). 
             * Instead, set it to header_len - 1 to trigger (header_len - offset < FRAME_META_HDR_SIZE)
             */
            int32_t malicious_offset = header_len - 1; 
            to_big(idx_ptr, &malicious_offset, sizeof(malicious_offset));
            printf("Overwrote index offset with %d to trigger direct bounds check failure\n", malicious_offset);

            found = 1;
            break;
        }
        idx_ptr += nslen + 1 + 4; // Skip name + 0xd2 + offset
    }

    if (!found) {
        printf("Error: Could not locate 'testmeta' metalayer in buffer\n");
        goto cleanup;
    }

    /* 5. Attempt to open the malformed frame */
    printf("\nAttempting to open malformed frame...\n");
    malicious_schunk = blosc2_schunk_from_buffer(buffer, buffer_len, false);

    if (malicious_schunk == NULL) {
        printf("SUCCESS: Malformed frame was REJECTED by safety checks.\n");
        result = 0;
    } else {
        printf("FAILURE: Malformed frame was ACCEPTED! (Potential Vulnerability)\n");
        result = 1; 
    }

cleanup:
    if (malicious_schunk != NULL) blosc2_schunk_free(malicious_schunk);
    if (schunk != NULL) blosc2_schunk_free(schunk);
    if (needs_free && buffer != NULL) free(buffer);
    blosc2_destroy();

    return result; 
}

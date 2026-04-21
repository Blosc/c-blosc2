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
    blosc2_init();

    /* 1. Create a minimal valid schunk and frame */
    int32_t data[100] = {0};
    blosc2_storage storage = {.contiguous = true};
    blosc2_schunk* schunk = blosc2_schunk_new(&storage);
    blosc2_schunk_append_buffer(schunk, data, sizeof(data));

    /* 2. Add a standard metalayer to serve as a template */
    uint8_t content[] = "normal content";
    blosc2_meta_add(schunk, "testmeta", content, sizeof(content));

    /* 3. Export to buffer */
    uint8_t *buffer = NULL;
    bool needs_free = false;
    int64_t buffer_len = blosc2_schunk_to_buffer(schunk, &buffer, &needs_free);
    
    if (buffer_len <= 0) {
        printf("Failed to create frame buffer\n");
        return 1;
    }

    /* 4. Malfunction the buffer: Inject malicious metalayer metadata */
    int32_t header_len;
    from_big(&header_len, buffer + FRAME_HEADER_LEN, sizeof(header_len));
    
    printf("Original Header Len: %d\n", header_len);

    /* Parse the metalayer index (map) to find "testmeta" */
    uint8_t *idx_ptr = buffer + FRAME_IDX_SIZE + 2; // Skip idx_size
    if (*idx_ptr != 0xde) {
        printf("Error: Could not find metalayer index marker\n");
        return 1;
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
                return 1;
            }
            idx_ptr++;
            
            /* Get the original offset to the content marker */
            int32_t original_offset;
            from_big(&original_offset, idx_ptr, sizeof(original_offset));
            
            /* Overwrite the content_len at the destination with a huge value */
            uint8_t *content_marker = buffer + original_offset;
            if (*content_marker == 0xc6) {
                int32_t malicious_len = INT32_MAX;
                to_big(content_marker + 1, &malicious_len, sizeof(malicious_len));
                printf("Overwrote content_len at offset %d with INT32_MAX\n", original_offset + 1);
            }

            /* Overwrite the offset in the index to be very large to trigger bypass */
            /* We use a value that + 5 will overflow in 32-bit signed arithmetic */
            int32_t malicious_offset = INT32_MAX - 2; 
            to_big(idx_ptr, &malicious_offset, sizeof(malicious_offset));
            printf("Overwrote index offset with %d to trigger overflow\n", malicious_offset);
            
            /* We also need header_len to be large so (offset < header_len) passes */
            int32_t malicious_header_len = INT32_MAX;
            to_big(buffer + FRAME_HEADER_LEN, &malicious_header_len, sizeof(malicious_header_len));
            printf("Overwrote header_len with INT32_MAX\n");

            found = 1;
            break;
        }
        idx_ptr += nslen + 1 + 4; // Skip name + 0xd2 + offset
    }

    if (!found) {
        printf("Error: Could not locate 'testmeta' metalayer in buffer\n");
        return 1;
    }

    /* 5. Attempt to open the malformed frame */
    printf("\nAttempting to open malformed frame...\n");
    blosc2_schunk* malicious_schunk = blosc2_schunk_from_buffer(buffer, buffer_len, false);

    if (malicious_schunk == NULL) {
        printf("SUCCESS: Malformed frame was REJECTED by safety checks.\n");
    } else {
        printf("FAILURE: Malformed frame was ACCEPTED! (Potential Vulnerability)\n");
        blosc2_schunk_free(malicious_schunk);
        return 1; 
    }

    blosc2_schunk_free(schunk);
    if (needs_free) free(buffer);
    blosc2_destroy();

    return 0; 
}

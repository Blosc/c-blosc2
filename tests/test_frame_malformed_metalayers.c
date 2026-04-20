/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "blosc2.h"
#include "frame.h"

/* 
 * This PoC demonstrates a maliciously crafted metalayer header that aims to 
 * bypass bounds checks via integer overflow. 
 */

int main() {
    blosc2_init();

    /* 1. Create a minimal valid schunk and frame */
    int32_t data[100];
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
    /* We need to find the metalayer index and overwrite it.
       In a real attack, the entire frame would be crafted. 
       Here we just demonstrate the logic bypass. */
       
    int32_t header_len;
    blosc2_from_big(&header_len, buffer + FRAME_HEADER_LEN, sizeof(header_len));
    
    printf("Original Header Len: %d\n", header_len);

    /* Search for the metalayer content marker (0xc6) and its size */
    /* This is a simplification for the PoC demonstration. */
    for (int i = FRAME_IDX_SIZE; i < header_len - 10; i++) {
        if (buffer[i] == 0xc6) {
            printf("Found metalayer content marker at offset %d\n", i);
            
            /* Overwrite the content_len with a huge value (INT32_MAX) */
            int32_t malicious_len = 2147483647; 
            blosc2_to_big(buffer + i + 1, &malicious_len, sizeof(malicious_len));
            
            /* Overwrite the offset in the index to be very large */
            /* This requires more complex parsing to find the exact index entry, 
               but the principle is that 'offset + content_len' would overflow. */
               
            break;
        }
    }

    /* 5. Attempt to open the malformed frame */
    printf("\nAttempting to open malformed frame...\n");
    blosc2_schunk* malicious_schunk = blosc2_schunk_from_buffer(buffer, buffer_len, false);

    if (malicious_schunk == NULL) {
        printf("SUCCESS: Malformed frame was REJECTED by safety checks.\n");
    } else {
        printf("FAILURE: Malformed frame was ACCEPTED! (Potential Vulnerability)\n");
        blosc2_schunk_free(malicious_schunk);
        return 1; /* Return failure code */
    }

    blosc2_schunk_free(schunk);
    if (needs_free) free(buffer);
    blosc2_destroy();

    return 0; /* Return success code */
}

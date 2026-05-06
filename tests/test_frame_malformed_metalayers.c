#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"

int main() {
    blosc2_init();

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.cparams = &cparams;
    storage.dparams = &dparams;

    blosc2_schunk* schunk = blosc2_schunk_new(&storage);
    if (!schunk) {
        printf("Failed to create schunk\n");
        return 1;
    }

    // Add a metalayer
    uint8_t content[] = "This is a test content";
    blosc2_meta_add(schunk, "bad_meta", content, sizeof(content));

    uint8_t* cframe;
    bool needs_free;
    int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
    if (len < 0) {
        printf("Failed to serialize schunk\n");
        return 1;
    }

    printf("Serialized to buffer of length %lld\n", len);

    // Find the metalayer name to tamper with it
    const char* target = "bad_meta";
    int target_len = strlen(target);
    int found = 0;
    
    for (int64_t i = 0; i < len - target_len; i++) {
        if (memcmp(cframe + i, target, target_len) == 0) {
            printf("Found 'bad_meta' at offset %lld\n", i);
            uint8_t* idxp = cframe + i + target_len;
            if (*idxp == 0xd2) {
                idxp++;
                int32_t offset;
                // Read 4 bytes big endian
                offset = (idxp[0] << 24) | (idxp[1] << 16) | (idxp[2] << 8) | idxp[3];
                printf("Offset is %d\n", offset);
                
                uint8_t* content_marker = cframe + offset;
                if (*content_marker == 0xc6) {
                    printf("Found content marker 0xc6 at offset %d\n", offset);
                    // Tamper with the content length (big endian)
                    uint8_t* clen_ptr = content_marker + 1;
                    int32_t bad_len = 0x7FFFFFFF;
                    clen_ptr[0] = (bad_len >> 24) & 0xFF;
                    clen_ptr[1] = (bad_len >> 16) & 0xFF;
                    clen_ptr[2] = (bad_len >> 8) & 0xFF;
                    clen_ptr[3] = bad_len & 0xFF;
                    printf("Tampered content length to %d\n", bad_len);
                    found = 1;
                }
            }
            break;
        }
    }

    if (!found) {
        printf("Could not find the metalayer in the buffer.\n");
        return 1;
    }

    // Now try to open the tampered buffer
    printf("Attempting to parse the malformed frame...\n");
    blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, len, true);
    
    if (schunk2) {
        printf("Parsing succeeded! (Unexpected)\n");
        blosc2_schunk_free(schunk2);
    } else {
        printf("Parsing failed cleanly.\n");
    }

    if (needs_free) {
        free(cframe);
    }
    blosc2_schunk_free(schunk);
    blosc2_destroy();

    return 0;
}

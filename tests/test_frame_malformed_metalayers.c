#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blosc2.h"
#include "test_common.h"

static void* my_memmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char* h = (const char*)haystack;
    const char* n = (const char*)needle;
    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (memcmp(h + i, n, needlelen) == 0) return (void*)(h + i);
    }
    return NULL;
}

int tests_run = 0;

static char* test_malformed_metalayer_header(void) {
    blosc2_init();

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.cparams = &cparams;
    storage.dparams = &dparams;

    blosc2_schunk* schunk = blosc2_schunk_new(&storage);
    mu_assert("Failed to create schunk", schunk != NULL);

    // Add a metalayer
    char* name = "test_metalayer";
    uint8_t content[] = {1, 2, 3, 4};
    int rc = blosc2_meta_add(schunk, name, content, sizeof(content));
    mu_assert("Failed to add metalayer", rc >= 0);

    uint8_t* cframe;
    bool needs_free;
    int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
    mu_assert("Failed to serialize schunk", len >= 0);

    // Find the metalayers section in the frame header
    // It is at an offset defined in the frame index.
    // For a simple frame, let's just search for the metalayer name.
    uint8_t* name_pos = (uint8_t*)my_memmem(cframe, (size_t)len, name, strlen(name));
    mu_assert("Could not find metalayer name in frame", name_pos != NULL);

    // The metalayer entry is [name_len (1B) | name (N B) | content_offset (4B)]
    // The content_offset points to a bin32 [0xc6 | content_len (4B) | content (M B)]
    
    // Let's tamper with the content_len of the metalayer to be huge.
    // The content_offset is 4 bytes after the name.
    uint32_t content_offset;
    memcpy(&content_offset, name_pos + strlen(name), 4);
    // On little-endian, we need to swap this because it's stored in big-endian in frame
    uint32_t content_offset_be = content_offset;
    if (is_little_endian()) {
        content_offset = swap4(content_offset_be);
    }
    
    // Tamper with the bin32 header at content_offset
    // header_len is at cframe[11..14]
    uint32_t header_len = (cframe[11] << 24) | (cframe[12] << 16) | (cframe[13] << 8) | cframe[14];
    
    uint8_t* content_header = cframe + content_offset;
    // content_header[0] is 0xc6. content_header[1..4] is content_len.
    // Set content_len to something that would overflow header_len if added to content_offset
    uint32_t huge_len = 0x7FFFFFFF;
    content_header[1] = (huge_len >> 24) & 0xFF;
    content_header[2] = (huge_len >> 16) & 0xFF;
    content_header[3] = (huge_len >> 8) & 0xFF;
    content_header[4] = huge_len & 0xFF;

    printf("Attempting to parse frame with malformed metalayer content_len...\n");
    blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, len, true);
    
    // It SHOULD fail because of the overflow checks.
    mu_assert("Parsing should have failed for malformed metalayer", schunk2 == NULL);

    if (needs_free) {
        free(cframe);
    }
    blosc2_schunk_free(schunk);
    blosc2_destroy();

    return EXIT_SUCCESS;
}

static char *all_tests(void) {
    mu_run_test(test_malformed_metalayer_header);
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

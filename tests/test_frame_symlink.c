/*
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/
#include "blosc2.h"
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define TARGET "test_frame_symlink_target.b2frame"
#define LINK "test_frame_symlink_link.b2frame"

int main(void) {
  int result = 1;
  blosc2_schunk *source = NULL, *reader = NULL;
  remove(LINK);
  remove(TARGET);
  blosc2_init();
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  int32_t input[32], output[32];
  for (int i = 0; i < 32; i++) input[i] = i * 17 - 10;
  source = blosc2_schunk_new(&storage);
  if (source == NULL || blosc2_schunk_append_buffer(source, input, sizeof(input)) != 1) goto cleanup;
  if (blosc2_schunk_to_file(source, TARGET) <= 0) goto cleanup;
  int64_t offsets[2] = {0, blosc2_schunk_append_file(source, TARGET)};
  if (offsets[1] <= 0) goto cleanup;
#if defined(_WIN32)
  /* Developer Mode allows unprivileged symlinks; older systems may require
     the privilege. Skip only when creation is denied, never when open fails. */
  BOOL created = CreateSymbolicLinkA(LINK, TARGET, 0x2);
  if (!created && GetLastError() == ERROR_INVALID_PARAMETER) {
    created = CreateSymbolicLinkA(LINK, TARGET, 0);
  }
  if (!created) {
    if (GetLastError() == ERROR_PRIVILEGE_NOT_HELD) {
      puts("Skipping symlink test: creation privilege unavailable");
      result = 0;
    }
    goto cleanup;
  }
#else
  if (symlink(TARGET, LINK) != 0) goto cleanup;
#endif
  for (int i = 0; i < 2; i++) {
    reader = blosc2_schunk_open_offset(LINK, offsets[i]);
    if (reader == NULL) {
      fprintf(stderr, "Cannot open symlink frame at offset %lld\n", (long long) offsets[i]);
      goto cleanup;
    }
    if (blosc2_schunk_decompress_chunk(reader, 0, output, sizeof(output)) != sizeof(output) ||
        memcmp(input, output, sizeof(input)) != 0) goto cleanup;
    blosc2_schunk_free(reader);
    reader = NULL;
  }
  result = 0;
cleanup:
  if (reader != NULL) blosc2_schunk_free(reader);
  if (source != NULL) blosc2_schunk_free(source);
  blosc2_destroy();
  remove(LINK);
  remove(TARGET);
  return result;
}

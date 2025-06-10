/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc vlmetalayer from C code.

  To compile this program:

  $ gcc frame_vlmetalayer.c -o frame_vlmetalyer -lblosc2

  To run:

  $ ./frame_vlmetalyer

 */

#include <stdio.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main(void) {
  blosc2_init();

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  char* urlpath = "vlmetalayers.b2frame";
  remove(urlpath);
  blosc2_storage storage = {.contiguous=true, .urlpath=urlpath, .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);


  // Add a metalayer
  int metalater_len = blosc2_meta_add(schunk, "metalayer", (uint8_t *) "This is a metalayers content...", 10);
  if (metalater_len < 0) {
    printf("Cannot write metalayers chunk");
    return metalater_len;
  }

  // Add some vlmetalayers data
  metalater_len = blosc2_vlmeta_add(schunk, "vlmetalayer", (uint8_t *) "This is a vlmetalayers content...", 32, NULL);
  if (metalater_len < 0) {
    printf("Cannot write vlmetalayers chunk");
    return metalater_len;
  }

  // Add some vlmetalayers data
  metalater_len = blosc2_vlmeta_add(schunk, "vlmetalayer2", (uint8_t *) "This is a content...", 10, NULL);
  if (metalater_len < 0) {
    printf("Cannot write vlmetalayers chunk");
    return metalater_len;
  }

  metalater_len = blosc2_vlmeta_update(schunk, "vlmetalayer", (uint8_t *) "This is a another vlmetalayer content...",
                                       20,
                                       NULL);
  if (metalater_len < 0) {
    printf("Cannot write vlmetalayers chunk");
    return metalater_len;
  }

  // Delete a vlmetalayer
  int nvlmetalayers =  blosc2_vlmeta_delete(schunk, "vlmetalayer");
  if (nvlmetalayers < 0) {
    printf("Cannot delete the  vlmetalayer");
  }

  metalater_len = blosc2_vlmeta_exists(schunk, "vlmetalayer");
  if (metalater_len >= 0) {
    printf("The vlmetalayer was not deleted successfully");
    return metalater_len;
  }

  metalater_len = blosc2_vlmeta_exists(schunk, "vlmetalayer2");
  if (metalater_len < 0) {
    printf("The vlmetalayer was not deleted successfully");
    return metalater_len;
  }
  blosc2_schunk *sc = blosc2_schunk_open(urlpath);

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(sc);
  blosc2_destroy();

  return 0;
}

/* Warm-cache stage profiling; no changes to the codec or on-disk filter IDs. */
#include "bytedelta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ROUNDS = 9, ITERATIONS = 200, STAGES = 9 };
static const char *names[STAGES] = {
  "shuffle", "bytedelta", "forward_staged", "forward_fused", "lz4",
  "unbytedelta", "unshuffle", "backward_staged", "backward_fused"
};

static int profile(int typesize, int length, int random_data) {
  uint8_t *input = malloc((size_t)length);
  uint8_t *shuffled = malloc((size_t)length);
  uint8_t *delta = malloc((size_t)length);
  uint8_t *scratch = malloc((size_t)length);
  uint8_t *output = malloc((size_t)length + BLOSC2_MAX_OVERHEAD);
  if (!input || !shuffled || !delta || !scratch || !output) return 1;
  uint32_t random = 20261113;
  for (int i = 0; i < length; i++) {
    random = random * 1664525U + 1013904223U;
    input[i] = (uint8_t)(random >> 24);
  }
  if (!random_data) {
    for (int i = 0; i < length / typesize; i++) {
      double value = (double)i / (length / typesize - 1);
      if (typesize == 2) {
        uint16_t integer = (uint16_t)i;
        memcpy(input + i * typesize, &integer, 2);
      }
      else if (typesize == 8) memcpy(input + i * typesize, &value, 8);
      else {
        float single = (float)value;
        memcpy(input + i * typesize, &single, 4);
      }
    }
  }
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
  cp.compcode = BLOSC_LZ4;
  cp.clevel = 5;
  cp.typesize = typesize;
  cp.blocksize = length;
  cp.nthreads = 1;
  memset(cp.filters, 0, sizeof(cp.filters));
  blosc2_context *encoder = blosc2_create_cctx(cp);
  if (!encoder) return 1;
  blosc2_shuffle(typesize, length, input, shuffled);
  bytedelta_forward(shuffled, delta, length, (uint8_t)typesize, &cp, 0);
  if (!shuffle_bytedelta_forward(input, output, length, typesize)) return 77;
  if (memcmp(output, delta, (size_t)length)) return 1;
  if (!shuffle_bytedelta_backward(delta, output, length, typesize)) return 77;
  if (memcmp(output, input, (size_t)length)) return 1;
  double elapsed[STAGES][ROUNDS];
  for (int round = -1; round < ROUNDS; round++) {
    for (int position = 0; position < STAGES; position++) {
      int stage = (position + round + 1) % STAGES;
      if (round % 2 == 0) stage = STAGES - 1 - stage;
      blosc_timestamp_t start, end;
      blosc_set_timestamp(&start);
      for (int repeat = 0; repeat < ITERATIONS; repeat++) {
        switch (stage) {
          case 0: blosc2_shuffle(typesize, length, input, output); break;
          case 1: bytedelta_forward(shuffled, output, length, (uint8_t)typesize, &cp, 0); break;
          case 2:
            blosc2_shuffle(typesize, length, input, scratch);
            bytedelta_forward(scratch, output, length, (uint8_t)typesize, &cp, 0);
            break;
          case 3: shuffle_bytedelta_forward(input, output, length, typesize); break;
          case 4:
            if (blosc2_compress_ctx(encoder, delta, length, output, length + BLOSC2_MAX_OVERHEAD) <= 0) return 1;
            break;
          case 5: bytedelta_backward(delta, output, length, (uint8_t)typesize, &dp, 0); break;
          case 6: blosc2_unshuffle(typesize, length, shuffled, output); break;
          case 7:
            bytedelta_backward(delta, scratch, length, (uint8_t)typesize, &dp, 0);
            blosc2_unshuffle(typesize, length, scratch, output);
            break;
          case 8: shuffle_bytedelta_backward(delta, output, length, typesize); break;
        }
      }
      blosc_set_timestamp(&end);
      if (round >= 0) elapsed[stage][round] = blosc_elapsed_secs(start, end) / ITERATIONS;
    }
  }
  printf("{\"typesize\":%d,\"block_bytes\":%d,\"pattern\":\"%s\",\"stages\":{",
         typesize, length, random_data ? "random-bytes" : typesize == 2 ? "u16-ramp" : "ramp");
  for (int stage = 0; stage < STAGES; stage++) {
    printf("%s\"%s\":[", stage ? "," : "", names[stage]);
    for (int round = 0; round < ROUNDS; round++) printf("%s%.12g", round ? "," : "", elapsed[stage][round]);
    printf("]");
  }
  puts("}}");
  blosc2_free_ctx(encoder);
  free(input);
  free(shuffled);
  free(delta);
  free(scratch);
  free(output);
  return 0;
}

int main(void) {
  blosc2_init();
  for (int typesize = 2; typesize <= 8; typesize *= 2) {
    for (int length = 80000; length <= 400000; length += 320000) {
      for (int random = 0; random < 2; random++) {
        int status = profile(typesize, length, random);
        if (status) return status;
      }
    }
  }
  blosc2_destroy();
  return 0;
}

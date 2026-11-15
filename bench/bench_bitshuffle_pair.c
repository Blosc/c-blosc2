/* Paired warm-cache NEON kernel benchmark.
 * Build: cc -O3 bench/bench_bitshuffle_pair.c -o bench_pair [-ldl]
 * Usage: bench_pair before.dylib after.dylib [block_bytes]
 * Libraries export bshuf_trans_bit_elem_NEON (compile with BLOSC_TESTING).
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int64_t (*kernel)(const void*, void*, size_t, size_t, void*);
static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}
static int compare_double(const void* a, const void* b) {
  double x = *(const double*)a, y = *(const double*)b;
  return (x > y) - (x < y);
}
int main(int argc, char** argv) {
  if (argc != 3 && argc != 4) return 1;
  void* libraries[2];
  kernel functions[2];
  for (int k = 0; k < 2; ++k) {
    libraries[k] = dlopen(argv[k + 1], RTLD_LOCAL | RTLD_NOW);
    if (!libraries[k]) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    functions[k] = (kernel)dlsym(libraries[k], "bshuf_trans_bit_elem_NEON");
    if (!functions[k]) return 1;
  }
  const char* patterns[] = {"random", "arange", "linspace", "constant", "interior-difference"};
  size_t selected = argc == 4 ? strtoul(argv[3], NULL, 10) : 0;
  if (argc == 4 && (selected == 0 || selected % 128)) return 1;
  puts("bytes,typesize,pattern,before_us,after_us,speedup");
  for (size_t bytes = selected ? selected : 80000; bytes <= (selected ? selected : 400000);
       bytes = selected ? selected + 1 : bytes == 80000 ? 128000 : bytes + 272000) {
    int iterations = bytes < 4096 ? 10000 : 500;
    uint8_t *input = malloc(bytes), *out[2] = {malloc(bytes), malloc(bytes)}, *scratch = malloc(bytes);
    if (!input || !out[0] || !out[1] || !scratch) return 1;
    for (size_t type = 1; type <= 16; type *= 2) {
      for (int pattern = 0; pattern < 5; ++pattern) {
        uint32_t random = 12345;
        for (size_t i = 0; i < bytes; ++i) {
          random ^= random << 13; random ^= random >> 17; random ^= random << 5;
          input[i] = (uint8_t)random;
        }
        if (pattern == 1 || pattern == 2) {
          if (type != 4 && type != 8) continue;
          for (size_t i = 0; i < bytes / type; ++i) {
            /* A block in the middle of a 1B-element constructor. */
            double value = 500000000.0 + i;
            if (pattern == 2) value /= 999999999.0;
            if (type == 8) memcpy(input + i * type, &value, 8);
            else { float v = (float)value; memcpy(input + i * type, &v, 4); }
          }
        }
        if (pattern >= 3) {
          for (size_t i = 0; i < bytes; ++i) input[i] = (uint8_t)((i % type) * 31);
          if (pattern == 4) input[bytes / 2] ^= 0x80;
        }
        for (int k = 0; k < 2; ++k) functions[k](input, out[k], bytes / type, type, scratch);
        if (memcmp(out[0], out[1], bytes)) { fputs("Mismatch\n", stderr); return 1; }
        double times[2][9];
        for (int round = -1; round < 9; ++round) {
          for (int position = 0; position < 2; ++position) {
            int k = (position + round + 1) % 2;
            double start = now();
            for (int iteration = 0; iteration < iterations; ++iteration) {
              functions[k](input, out[k], bytes / type, type, scratch);
            }
            if (round >= 0) times[k][round] = (now() - start) / iterations;
          }
        }
        for (int k = 0; k < 2; ++k) qsort(times[k], 9, sizeof(double), compare_double);
        printf("%zu,%zu,%s,%.3f,%.3f,%.3f\n", bytes, type, patterns[pattern],
               times[0][4] * 1e6, times[1][4] * 1e6, times[0][4] / times[1][4]);
      }
    }
    free(input); free(out[0]); free(out[1]); free(scratch);
  }
  for (int k = 0; k < 2; ++k) dlclose(libraries[k]);
  return 0;
}

/* Direct kernel benchmark and scalar-format compatibility check.
 * Build with bitshuffle-{generic,neon,sse2,avx2}.c and shuffle-{neon,sse2,avx2}.c.
 * Run: bench_bitshuffle_simd [iterations] [block bytes]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bitshuffle-generic.h"
#include "bitshuffle-neon.h"
#include "bitshuffle-sse2.h"
#include "bitshuffle-avx2.h"

typedef int64_t (*kernel)(const void*, void*, size_t, size_t, void*);
static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}
static int check(kernel f, kernel g, int pattern) {
  unsigned char a[32768], b[32768], c[32768], d[32768], tmp[32768];
  unsigned state = 42;
  for (size_t i = 0; i < sizeof(a); ++i) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    a[i] = (unsigned char)state;
  }
  const size_t types[] = {1, 2, 3, 4, 8, 16, 24};
  for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); ++t) {
    size_t e = types[t];
    if (pattern) {
      for (size_t i = 0; i < sizeof(a); ++i) a[i] = (unsigned char)((i % e) * 31 + e);
    }
    for (size_t n = 8; n <= 1032; n += 8) {
      for (size_t off = 0; off < 4; ++off) {
        if (pattern == 2) a[off + (n / 2) * e] ^= 0x80;
        bshuf_trans_bit_elem_scal(a + off, b + off, n, e, tmp + off);
        if (f(a + off, c + off, n, e, tmp + off) != (int64_t)(n * e) ||
            memcmp(b + off, c + off, n * e) ||
            g(b + off, d + off, n, e, tmp + off) != (int64_t)(n * e) ||
            memcmp(a + off, d + off, n * e)) {
          fprintf(stderr, "Mismatch: type=%zu size=%zu offset=%zu pattern=%d\n", e, n, off, pattern);
          return 1;
        }
        if (pattern == 2) a[off + (n / 2) * e] ^= 0x80;
      }
    }
  }
  return 0;
}
int main(int argc, char** argv) {
  size_t iterations = argc > 1 ? strtoul(argv[1], NULL, 10) : 4000;
  size_t bytes = argc > 2 ? strtoul(argv[2], NULL, 10) : 128000;
  const char* names[] = {"NEON", "SSE2", "AVX2", "scalar"};
  const int enabled[] = {is_bshuf_NEON, is_bshuf_SSE, is_bshuf_AVX, 1};
  kernel forward[] = {bshuf_trans_bit_elem_NEON, bshuf_trans_bit_elem_SSE,
                       bshuf_trans_bit_elem_AVX, bshuf_trans_bit_elem_scal};
  kernel inverse[] = {bshuf_untrans_bit_elem_NEON, bshuf_untrans_bit_elem_SSE,
                       bshuf_untrans_bit_elem_AVX, bshuf_untrans_bit_elem_scal};
  unsigned char *a = malloc(bytes), *b = malloc(bytes), *c = malloc(bytes), *tmp = malloc(bytes);
  if (!a || !b || !c || !tmp) return 1;
  for (size_t i = 0; i < bytes; ++i) a[i] = (unsigned char)(i * 131 + i / 17);
  puts("simd,typesize,bytes,shuffle_us,unshuffle_us");
  for (size_t k = 0; k < 4; ++k) {
    if (!enabled[k]) continue;
    for (int pattern = 0; pattern < 3; ++pattern) {
      if (check(forward[k], inverse[k], pattern)) return 1;
    }
    for (size_t e = 1; e <= 16; e *= 2) {
      size_t n = bytes / (8 * e) * 8;
      double best[2] = {1e30, 1e30};
      for (int repeat = 0; repeat < 5; ++repeat) {
        double start = now();
        for (size_t i = 0; i < iterations; ++i) forward[k](a, b, n, e, tmp);
        double elapsed = (now() - start) / iterations;
        if (elapsed < best[0]) best[0] = elapsed;
        start = now();
        for (size_t i = 0; i < iterations; ++i) inverse[k](b, c, n, e, tmp);
        elapsed = (now() - start) / iterations;
        if (elapsed < best[1]) best[1] = elapsed;
      }
      printf("%s,%zu,%zu,%.3f,%.3f\n", names[k], e, n * e, best[0] * 1e6, best[1] * 1e6);
      if (memcmp(a, c, n * e)) return 1;
    }
  }
  free(a); free(b); free(c); free(tmp);
  return 0;
}

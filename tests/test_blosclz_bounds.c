/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Regression test for blosclz bounds checking and integer overflow.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "test_common.h"
#include "blosclz.h"

static int test_blosclz_oversized_match(void) {
  int maxout = 65536;
  int input_len = 100000;
  uint8_t* input = malloc((size_t)input_len);
  uint8_t* output = malloc((size_t)maxout);
  if (!input || !output) {
    free(input);
    free(output);
    return EXIT_FAILURE;
  }

  /* Craft a blosclz stream:
     1 literal byte first, then match token 0xE0 (len_code=7), followed by many 0xFF bytes */
  input[0] = 0;      /* literal token (1 byte) */
  input[1] = 'A';    /* 1 literal byte */
  input[2] = 0xE0;   /* match token with len = 7 - 1 */
  memset(input + 3, 0xFF, (size_t)(input_len - 5));
  input[input_len - 2] = 0x01; /* offset low byte */
  input[input_len - 1] = 0x00; /* offset high byte */

  int dsize = blosclz_decompress(input, input_len, output, maxout);
  if (dsize != 0) {
    printf("Expected blosclz_decompress to return 0 for oversized match, got %d\n", dsize);
    free(input);
    free(output);
    return EXIT_FAILURE;
  }

  free(input);
  free(output);
  return EXIT_SUCCESS;
}

static int test_blosclz_oversized_literal(void) {
  int maxout = 10;
  uint8_t input[33];
  uint8_t* output = malloc((size_t)maxout);
  if (!output) {
    return EXIT_FAILURE;
  }

  /* Literal ctrl = 31 -> 32 bytes literal, which exceeds maxout = 10 */
  input[0] = 31;
  memset(input + 1, 'X', 32);

  int dsize = blosclz_decompress(input, 33, output, maxout);
  if (dsize != 0) {
    printf("Expected blosclz_decompress to return 0 for literal > maxout, got %d\n", dsize);
    free(output);
    return EXIT_FAILURE;
  }

  free(output);
  return EXIT_SUCCESS;
}

static int test_blosclz_chunk_poc(void) {
  int32_t blocksize = 65536;
  int32_t cbytes = 100000;
  uint8_t* chunk = calloc(1, (size_t)cbytes);
  uint8_t* output = malloc((size_t)blocksize);
  if (!chunk || !output) {
    free(chunk);
    free(output);
    return EXIT_FAILURE;
  }

  /* Blosc2 chunk header */
  chunk[0] = 2; /* Blosc format version */
  chunk[1] = 4; /* Blosc2 format version */
  chunk[2] = 0; /* Flags (no filter) */
  chunk[3] = 1; /* Typesize */

  /* nbytes */
  chunk[4] = (uint8_t)(blocksize & 0xff);
  chunk[5] = (uint8_t)((blocksize >> 8) & 0xff);
  chunk[6] = (uint8_t)((blocksize >> 16) & 0xff);
  chunk[7] = (uint8_t)((blocksize >> 24) & 0xff);

  /* blocksize */
  chunk[8] = (uint8_t)(blocksize & 0xff);
  chunk[9] = (uint8_t)((blocksize >> 8) & 0xff);
  chunk[10] = (uint8_t)((blocksize >> 16) & 0xff);
  chunk[11] = (uint8_t)((blocksize >> 24) & 0xff);

  /* cbytes */
  chunk[12] = (uint8_t)(cbytes & 0xff);
  chunk[13] = (uint8_t)((cbytes >> 8) & 0xff);
  chunk[14] = (uint8_t)((cbytes >> 16) & 0xff);
  chunk[15] = (uint8_t)((cbytes >> 24) & 0xff);

  /* Codec is BLOSC_BLOSCLZ (0) at byte 20 */
  chunk[20] = BLOSC_BLOSCLZ;

  int header_len = BLOSC_EXTENDED_HEADER_LENGTH;
  chunk[header_len] = 0;        /* 1-byte literal ctrl */
  chunk[header_len + 1] = 'A';  /* literal data */
  chunk[header_len + 2] = 0xE0; /* match token */
  memset(chunk + header_len + 3, 0xFF, (size_t)(cbytes - header_len - 5));
  chunk[cbytes - 2] = 0x01;
  chunk[cbytes - 1] = 0x00;

  int dsize = blosc2_decompress(chunk, cbytes, output, blocksize);
  if (dsize >= 0) {
    printf("Expected blosc2_decompress to reject crafted chunk, got %d\n", dsize);
    free(chunk);
    free(output);
    return EXIT_FAILURE;
  }

  free(chunk);
  free(output);
  return EXIT_SUCCESS;
}

static int test_blosclz_roundtrip_valid(void) {
  const int size = 100000;
  uint8_t* src = malloc((size_t)size);
  uint8_t* compressed = malloc((size_t)size + 1000);
  uint8_t* decompressed = malloc((size_t)size);

  if (!src || !compressed || !decompressed) {
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  for (int i = 0; i < size; i++) {
    src[i] = (uint8_t)(i % 127);
  }

  int csize = blosclz_compress(5, src, size, compressed, size + 1000, NULL);
  if (csize <= 0) {
    printf("blosclz_compress failed: %d\n", csize);
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  int dsize = blosclz_decompress(compressed, csize, decompressed, size);
  if (dsize != size) {
    printf("blosclz_decompress returned %d, expected %d\n", dsize, size);
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  if (memcmp(src, decompressed, (size_t)size) != 0) {
    printf("Decompressed data mismatch!\n");
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  free(src);
  free(compressed);
  free(decompressed);
  return EXIT_SUCCESS;
}

/* Exercise vector scan tails, short matches, runs and unaligned inputs.
 * Allocations end at the last valid byte so ASan also detects overreads. */
static int test_blosclz_synthetic_roundtrips(void) {
  const int sizes[] = {16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129,
                       255, 256, 257, 352, 1023, 1024, 1025, 16000, 131073};
  for (size_t s = 0; s < ARRAY_SIZE(sizes); s++) {
    int size = sizes[s];
    for (int offset = 0; offset <= 1; offset++) {
      uint8_t *allocation = malloc((size_t)size + offset);
      uint8_t *compressed = malloc((size_t)size + size / 20 + 66);
      uint8_t *decompressed = malloc((size_t)size);
      if (!allocation || !compressed || !decompressed) {
        free(allocation);
        free(compressed);
        free(decompressed);
        return EXIT_FAILURE;
      }
      uint8_t *src = allocation + offset;
      for (int pattern = 0; pattern < 7; pattern++) {
        uint32_t state = 42;
        int remaining = 0;
        uint8_t run_value = 0;
        for (int i = 0; i < size; i++) {
          state ^= state << 13;
          state ^= state >> 17;
          state ^= state << 5;
          switch (pattern) {
            case 0: src[i] = 0xa5; break;
            case 1: src[i] = (uint8_t)(i % 7); break;
            case 2: src[i] = (uint8_t)((i % 9 == 0) ? i / 9 : i % 9); break;
            case 3:
              /* Move the first mismatch through every lane of a 64-byte group. */
              src[i] = (uint8_t)((i % 256 < 128 + (i / 256) % 64) ? 7 : i / 256);
              break;
            case 4: src[i] = (uint8_t)state; break;
            case 5:
              if (remaining == 0) {
                remaining = 1 + (int)(state & 255);
                run_value++;
              }
              src[i] = run_value;
              remaining--;
              break;
            default:
              /* A vector-aligned run followed immediately by a different run.
               * The tail must retain the first run's byte, not reload ip[-1]. */
              src[i] = (uint8_t)(i < 27 ? i : (i < 335 ? 0 : 129));
              break;
          }
        }
        for (int level = 1; level <= 9; level++) {
          for (int limited = 0; limited <= 1; limited++) {
            int capacity = limited ? 66 : size + size / 20 + 66;
            int csize = blosclz_compress(level, src, size, compressed, capacity, NULL);
            if (csize == 0) {
              if (!limited && size >= 1024 && pattern <= 1) {
                printf("Failed to compress synthetic pattern %d at level %d\n", pattern, level);
                free(allocation);
                free(compressed);
                free(decompressed);
                return EXIT_FAILURE;
              }
              continue;
            }
            if (csize < 0 || csize > capacity ||
                blosclz_decompress(compressed, csize, decompressed, size) != size ||
                memcmp(src, decompressed, (size_t)size) != 0) {
              printf("Synthetic roundtrip failed: size=%d offset=%d pattern=%d level=%d cap=%d\n",
                     size, offset, pattern, level, capacity);
              free(allocation);
              free(compressed);
              free(decompressed);
              return EXIT_FAILURE;
            }
          }
        }
      }
      free(allocation);
      free(compressed);
      free(decompressed);
    }
  }
  return EXIT_SUCCESS;
}

/* Construct streams independently of the encoder, covering both overlap-copy
 * widths, literal overcopy boundaries and long extension chains. */
static int test_blosclz_decode_copies(void) {
  const int lengths[] = {3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33,
                         63, 64, 65, 255, 256, 257, 2049, 4090, 65536};
  for (int distance = 1; distance <= 64; distance++) {
    for (size_t l = 0; l < ARRAY_SIZE(lengths); l++) {
      int length = lengths[l];
      for (int tail = 1; tail <= 32; tail += 15) {
        int nbytes = 64 + length + tail;
        int capacity = 2 + 64 + 4 + length / 255 + 1 + tail;
        uint8_t *stream = malloc((size_t)capacity);
        uint8_t *expected = malloc((size_t)nbytes);
        if (!stream || !expected) {
          free(stream);
          free(expected);
          return EXIT_FAILURE;
        }
        int pos = 0;
        for (int i = 0; i < 64; i++) {
          if (i % 32 == 0) {
            stream[pos++] = 31;  /* 32 literal bytes */
          }
          stream[pos++] = expected[i] = (uint8_t)(i * 37 + 11);
        }
        stream[0] |= 32;  /* BloscLZ marker */
        if (length < 9) {
          stream[pos++] = (uint8_t)((length - 2) << 5);
        }
        else {
          stream[pos++] = 7 << 5;
          int remaining = length - 9;
          while (remaining >= 255) {
            stream[pos++] = 255;
            remaining -= 255;
          }
          stream[pos++] = (uint8_t)remaining;
        }
        stream[pos++] = (uint8_t)(distance - 1);
        for (int i = 64; i < 64 + length; i++) {
          expected[i] = expected[i - distance];
        }
        stream[pos++] = (uint8_t)(tail - 1);
        for (int i = 0; i < tail; i++) {
          stream[pos++] = expected[64 + length + i] = (uint8_t)i;
        }
        for (int offset = 0; offset <= 1; offset++) {
          /* No readable/writable suffix padding. */
          uint8_t *input = malloc((size_t)pos + offset);
          uint8_t *output = malloc((size_t)nbytes + offset);
          if (!input || !output) {
            free(input);
            free(output);
            free(stream);
            free(expected);
            return EXIT_FAILURE;
          }
          memcpy(input + offset, stream, (size_t)pos);
          int dsize = blosclz_decompress(input + offset, pos, output + offset, nbytes);
          if (dsize != nbytes || memcmp(output + offset, expected, (size_t)nbytes) != 0 ||
              blosclz_decompress(input + offset, pos, output + offset, nbytes - 1) != 0 ||
              blosclz_decompress(input + offset, pos - 1, output + offset, nbytes) != 0) {
            printf("Decode copy failed: distance=%d length=%d tail=%d offset=%d\n",
                   distance, length, tail, offset);
            free(input);
            free(output);
            free(stream);
            free(expected);
            return EXIT_FAILURE;
          }
          free(input);
          free(output);
        }
        free(stream);
        free(expected);
      }
    }
  }
  return EXIT_SUCCESS;
}

int main(void) {
  blosc2_init();

  if (test_blosclz_oversized_match() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_oversized_literal() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_chunk_poc() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_roundtrip_valid() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_synthetic_roundtrips() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_decode_copies() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  blosc2_destroy();
  printf("All blosclz bounds tests passed successfully.\n");
  return EXIT_SUCCESS;
}

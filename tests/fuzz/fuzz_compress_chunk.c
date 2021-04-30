#include <stdint.h>
#include <stdlib.h>

#include <blosc2.h>

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *compressors[] = { "blosclz", "lz4", "lz4hc", "zlib", "zstd" };
  int num_comp = sizeof(compressors) / sizeof(char*);
  int level = 9, filter = BLOSC_BITSHUFFLE, cindex = 0, i = 0;
  size_t nbytes, cbytes, blocksize;
  void *output, *input;

  blosc_set_nthreads(1);

  if (size > 0)
    level = data[0] % (9 + 1);
  if (size > 1)
    filter = data[1] % (BLOSC_BITSHUFFLE + 1);
  if (size > 2)
    cindex = data[2];

  /* Find next available compressor */
  while (blosc_set_compressor(compressors[cindex % num_comp]) == -1 && i < num_comp) {
    cindex++, i++;
  }
  if (i == num_comp) {
    /* No compressors available */
    return 0;
  }

  if (size > 3 && data[3] % 7 == 0)
    blosc_set_blocksize(4096);

  output = malloc(size + 1);
  if (output == NULL)
    return 0;

  if (blosc2_compress(level, filter, 1, data, size, output, size) == 0) {
    /* Cannot compress src buffer into dest */
    free(output);
    return 0;
  }

  blosc_cbuffer_sizes(output, &nbytes, &cbytes, &blocksize);

  /* Don't allow address sanitizer to allocate more than INT32_MAX */
  if (cbytes >= INT32_MAX) {
    free(output);
    return 0;
  }

  input = malloc(cbytes);
  if (input != NULL) {
    blosc_decompress(output, input, cbytes);
    free(input);
  }

  free(output);

  return 0;
}

#ifdef __cplusplus
}
#endif

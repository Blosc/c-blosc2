/*
  Copyright (C) 2020  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Benchmark showing Blosc zero detection capabilities via run-length.

*/

#include <stdio.h>
#include <stdint.h>

#include "blosc2.h"
#include "cutest.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS (10)
#define CHUNKSIZE (5 * 1000)  // > NCHUNKS for the bench purposes
#define NTHREADS 4


typedef struct {
  uint8_t itemsize;
} codec_params;


int codec_encoder(const uint8_t* input, int32_t input_len,
                  uint8_t* output, int32_t output_len,
                  void* params) {
  codec_params *fparams = (codec_params *) params;
  if (fparams->itemsize != 4) {
    BLOSC_TRACE_ERROR("Itemsize %d != 4", fparams->itemsize);
    return BLOSC2_ERROR_FAILURE;
  }

  int32_t nelem = input_len / fparams->itemsize;
  int32_t *in_ = ((int32_t *) input);
  int32_t *out_ = ((int32_t *) output);

  // Check that is an arange
  int32_t start = in_[0];
  int32_t step = in_[1] - start;
  for (int i = 1; i < nelem - 1; ++i) {
    if (in_[i + 1] - in_[i] != step) {
      BLOSC_TRACE_ERROR("Buffer is not an arange");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  if (8 > output_len) {
    return BLOSC2_ERROR_WRITE_BUFFER;
  }
  out_[0] = start;
  out_[1] = step;

  return 8;
}

int codec_decoder(const uint8_t* input, int32_t input_len,
                  uint8_t* output, int32_t output_len,
                  void* params) {

  codec_params *fparams = (codec_params *) params;
  if (fparams->itemsize != 4) {
    BLOSC_TRACE_ERROR("Itemsize %d != 4", fparams->itemsize);
    return BLOSC2_ERROR_FAILURE;
  }

  int32_t nelem = output_len / fparams->itemsize;
  int32_t *in_ = ((int32_t *) input);
  int32_t *out_ = ((int32_t *) output);

  if (8 > input_len) {
    return BLOSC2_ERROR_WRITE_BUFFER;
  }
  int32_t start = in_[0];
  int32_t step = in_[1];
  for (int i = 0; i < nelem; ++i) {
    out_[i] = start + i * step;
  }

  return output_len;
}

int codec_decoder_error(const uint8_t* input, int32_t input_len,
                        uint8_t* output, int32_t output_len,
                        void* params) {

  codec_params *fparams = (codec_params *) params;
  if (fparams->itemsize != 4) {
    BLOSC_TRACE_ERROR("Itemsize %d != 4", fparams->itemsize);
    return BLOSC2_ERROR_FAILURE;
  }

  int32_t nelem = output_len / fparams->itemsize;
  int32_t *in_ = ((int32_t *) input);
  int32_t *out_ = ((int32_t *) output);

  if (8 > input_len) {
    return BLOSC2_ERROR_WRITE_BUFFER;
  }
  int32_t start = in_[0];
  int32_t step = in_[1];
  for (int i = 0; i < nelem; ++i) {
    out_[i] = start + i * step + 10;
  }

  return output_len;
}

CUTEST_TEST_DATA(udcodecs) {
  blosc2_cparams cparams;
};

CUTEST_TEST_SETUP(udcodecs) {
  blosc_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.clevel = 9;
  data->cparams.nthreads = NTHREADS;


  CUTEST_PARAMETRIZE(correct_backward, bool, CUTEST_DATA(
      true, false,
  ));

}


CUTEST_TEST_TEST(udcodecs) {
  CUTEST_GET_PARAMETER(correct_backward, bool);

  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  uint8_t *bdata = malloc(isize);
  uint8_t *bdata_dest = malloc(isize);

  int dsize;

  codec_params params = {.itemsize=sizeof(int32_t)};
  blosc2_udcodec udcodec;
  udcodec.id = 128;
  udcodec.encoder = codec_encoder;
  if (correct_backward) {
    udcodec.decoder = codec_decoder;
  } else {
    udcodec.decoder = codec_decoder_error;
  }
  udcodec.params = &params;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; ++i) {
    cparams.filters[i] = 0;
  }
  cparams.udcodecs[0] = udcodec;
  cparams.compcode = BLOSC_UDCODEC;
  cparams.compcode_meta = 128;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.udcodecs[0] = udcodec;


  blosc2_schunk* schunk;
  int i, nchunk;

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      ((int32_t *) bdata)[i] = i * nchunk;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, bdata, isize);
    if (nchunks_ != nchunk + 1) {
      BLOSC_TRACE_ERROR("Unexpected nchunks!");
      return -1;
    }
  }

  /* Retrieve and decompress the chunks (0-based count) */
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, bdata_dest, isize);
    if (dsize < 0) {
      BLOSC_TRACE_ERROR("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
  }

  /* Check integrity of the second chunk (made of non-zeros) */
  blosc2_schunk_decompress_chunk(schunk, 1, bdata_dest, isize);
  for (i = 0; i < CHUNKSIZE; i++) {
    bool equals = false;
    if (((int32_t *) bdata_dest)[i] == i) {
      equals = true;
    }

    if (!equals && correct_backward) {
      BLOSC_TRACE_ERROR("Decompressed bdata differs from original!\n");
      return -1;
    }
    if (equals && !correct_backward) {
      BLOSC_TRACE_ERROR("Decompressed bdata is equal than original!\n");
      return -1;
    }
  }

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  free(bdata);
  free(bdata_dest);

  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_TEARDOWN(udcodecs) {
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(udcodecs)
}

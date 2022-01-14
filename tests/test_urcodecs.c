/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
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



int codec_encoder(const uint8_t* input, int32_t input_len,
                  uint8_t* output, int32_t output_len,
                  uint8_t meta,
                  blosc2_cparams* cparams) {
  if (cparams->schunk == NULL) {
    return -1;
  }
  if (cparams->typesize != 4) {
    BLOSC_TRACE_ERROR("Itemsize %d != 4", cparams->typesize);
    return BLOSC2_ERROR_FAILURE;
  }
  uint8_t *content;
  uint32_t content_len;
  blosc2_vlmeta_get(cparams->schunk, "codec_arange", &content, &content_len);
  if (content[0] != 222) {
    return -1;
  }
  free(content);

  if (meta != 111) {
    return -1;
  }

  int32_t nelem = input_len / 4;
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
                  uint8_t meta,
                  blosc2_dparams *dparams) {
  if (dparams->schunk == NULL) {
    return -1;
  }

  uint8_t *content;
  uint32_t content_len;
  blosc2_vlmeta_get(dparams->schunk, "codec_arange", &content, &content_len);
  if (content[0] != 222) {
    return -1;
  }
  free(content);

  if (meta != 111) {
    return -1;
  }

  int32_t nelem = output_len / 4;
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
                        uint8_t meta,
                        blosc2_dparams* dparams) {
  if (meta != 111) {
    return -1;
  }

  int32_t nelem = output_len / 4;
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

CUTEST_TEST_DATA(urcodecs) {
  blosc2_cparams cparams;
  char* urlpath;
};

CUTEST_TEST_SETUP(urcodecs) {
  blosc_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.clevel = 9;
  data->cparams.nthreads = NTHREADS;
  data->urlpath = "test_udcodecs.b2frame";

  CUTEST_PARAMETRIZE(correct_backward, bool, CUTEST_DATA(
      true, false,
  ));

}


CUTEST_TEST_TEST(urcodecs) {
  CUTEST_GET_PARAMETER(correct_backward, bool);

  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  uint8_t *bdata = malloc(isize);
  uint8_t *bdata_dest = malloc(isize);

  int dsize;

  blosc2_codec udcodec;
  udcodec.compname = "arange";
  udcodec.compver = 1;
  udcodec.encoder = codec_encoder;
  if (correct_backward) {
    udcodec.compcode = 250;
    udcodec.complib = 250;
    udcodec.decoder = codec_decoder;
  } else {
    udcodec.compcode = 251;
    udcodec.complib = 251;
    udcodec.decoder = codec_decoder_error;
  }
  int rc = blosc2_register_codec(&udcodec);
  if (rc != 0) {
    BLOSC_TRACE_ERROR("Error registering the code.");
    return BLOSC2_ERROR_FAILURE;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; ++i) {
    cparams.filters[i] = 0;
  }
  cparams.compcode = udcodec.compcode;
  cparams.compcode_meta = 111;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;


  blosc2_schunk* schunk;
  int i, nchunk;

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  blosc2_storage storage = {.cparams=&cparams,
                            .dparams=&dparams,
                            .urlpath=data->urlpath,
                            .contiguous=true};
  remove(data->urlpath);
  schunk = blosc2_schunk_new(&storage);
  uint8_t codec_params = 222;
  blosc2_cparams cparams2 = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_vlmeta_add(schunk, "codec_arange", &codec_params, 1, &cparams2);

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
  blosc2_schunk_free(schunk);

  schunk = blosc2_schunk_open(data->urlpath);

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


CUTEST_TEST_TEARDOWN(urcodecs) {
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(urcodecs)
}

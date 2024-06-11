/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "zfp.h"
#include "blosc2-zfp.h"
#include "../plugins/codecs/zfp/zfp-private.h"
#include "context.h"
#include "blosc2.h"
#include "b2nd.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


int zfp_acc_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                     int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void *chunk) {
  BLOSC_UNUSED_PARAM(chunk);
  ZFP_ERROR_NULL(input);
  ZFP_ERROR_NULL(output);
  ZFP_ERROR_NULL(cparams);
  ZFP_ERROR_NULL(cparams->schunk);

  double tol = (int8_t) meta;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  for(int i = 0; i < ndim; i++) {
    if (blockshape[i] < 4) {
      BLOSC_TRACE_ERROR("ZFP does not support blocks smaller than cells (4x...x4");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  zfp_type type;     /* array scalar type */
  zfp_field *field;  /* array meta data */
  zfp_stream *zfp;   /* stream containing the real output buffer */
  zfp_stream *zfp_aux;   /* auxiliary compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  bitstream *stream_aux; /* auxiliary bit stream to write to or read from */
  size_t zfpsize;    /* byte size of compressed stream */
  double tolerance = pow(10, tol);

  int32_t typesize = cparams->typesize;

  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }

  zfp = zfp_stream_open(NULL);
  zfp_stream_set_accuracy(zfp, tolerance);
  stream = stream_open(output, output_len);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  switch (ndim) {
    case 1:
      field = zfp_field_1d((void *) input, type, blockshape[0]);
      break;
    case 2:
      field = zfp_field_2d((void *) input, type, blockshape[1], blockshape[0]);
      break;
    case 3:
      field = zfp_field_3d((void *) input, type, blockshape[2], blockshape[1], blockshape[0]);
      break;
    case 4:
      field = zfp_field_4d((void *) input, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  int zfp_maxout = (int) zfp_stream_maximum_size(zfp, field);
  zfp_stream_close(zfp);
  stream_close(stream);
  uint8_t *aux_out = malloc(zfp_maxout);
  zfp_aux = zfp_stream_open(NULL);
  zfp_stream_set_accuracy(zfp_aux, tolerance);
  stream_aux = stream_open(aux_out, zfp_maxout);
  zfp_stream_set_bit_stream(zfp_aux, stream_aux);
  zfp_stream_rewind(zfp_aux);

  zfpsize = zfp_compress(zfp_aux, field);

  /* clean up */
  zfp_field_free(field);
  zfp_stream_close(zfp_aux);
  stream_close(stream_aux);
  free(shape);
  free(chunkshape);
  free(blockshape);

  if (zfpsize == 0) {
    BLOSC_TRACE_ERROR("\n ZFP: Compression failed\n");
    free(aux_out);
    return (int) zfpsize;
  }
  if ((int32_t) zfpsize >= input_len) {
    BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
    free(aux_out);
    return 0;
  }

  memcpy(output, aux_out, zfpsize);
  free(aux_out);

  return (int) zfpsize;
}

int zfp_acc_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                       int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void *chunk) {
  ZFP_ERROR_NULL(input);
  ZFP_ERROR_NULL(output);
  ZFP_ERROR_NULL(dparams);
  ZFP_ERROR_NULL(dparams->schunk);
  BLOSC_UNUSED_PARAM(chunk);

  blosc2_schunk *sc = dparams->schunk;
  int32_t typesize = sc->typesize;

  double tol = (int8_t) meta;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(sc, "b2nd", &smeta, &smeta_len) < 0) {
    BLOSC_TRACE_ERROR("Cannot access b2nd meta info");
    free(shape);
    free(chunkshape);
    free(blockshape);
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  zfp_type type;     /* array scalar type */
  zfp_field *field;  /* array meta data */
  zfp_stream *zfp;   /* compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  size_t zfpsize;    /* byte size of compressed stream */
  double tolerance = pow(10, tol);

  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }

  zfp = zfp_stream_open(NULL);
  zfp_stream_set_accuracy(zfp, tolerance);
  stream = stream_open((void *) input, input_len);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  switch (ndim) {
    case 1:
      field = zfp_field_1d((void *) output, type, blockshape[0]);
      break;
    case 2:
      field = zfp_field_2d((void *) output, type, blockshape[1], blockshape[0]);
      break;
    case 3:
      field = zfp_field_3d((void *) output, type, blockshape[2], blockshape[1], blockshape[0]);
      break;
    case 4:
      field = zfp_field_4d((void *) output, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  zfpsize = zfp_decompress(zfp, field);

  /* clean up */
  zfp_field_free(field);
  zfp_stream_close(zfp);
  stream_close(stream);
  free(shape);
  free(chunkshape);
  free(blockshape);

  if (zfpsize == 0) {
    BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
    return (int) zfpsize;
  }

  return (int) output_len;
}

int zfp_prec_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                      int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void *chunk) {
  BLOSC_UNUSED_PARAM(chunk);
  ZFP_ERROR_NULL(input);
  ZFP_ERROR_NULL(output);
  ZFP_ERROR_NULL(cparams);
  ZFP_ERROR_NULL(cparams->schunk);

  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  for(int i = 0; i < ndim; i++) {
    if (blockshape[i] < 4) {
      BLOSC_TRACE_ERROR("ZFP does not support blocks smaller than cells (4x...x4");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  zfp_type type;     /* array scalar type */
  zfp_field *field;  /* array meta data */
  zfp_stream *zfp;   /* stream containing the real output buffer */
  zfp_stream *zfp_aux;   /* auxiliary compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  bitstream *stream_aux; /* auxiliary bit stream to write to or read from */
  size_t zfpsize;    /* byte size of compressed stream */

  uint prec;
  switch (ndim) {
    case 1:
      prec = meta + 5;
      break;
    case 2:
      prec = meta + 7;
      break;
    case 3:
      prec = meta + 9;
      break;
    case 4:
      prec = meta + 11;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  if (prec > ZFP_MAX_PREC) {
    BLOSC_TRACE_ERROR("Max precision for this codecs is %d", ZFP_MAX_PREC);
    prec = ZFP_MAX_PREC;
  }

  int32_t typesize = cparams->typesize;
  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }

  zfp = zfp_stream_open(NULL);
  zfp_stream_set_precision(zfp, prec);
  stream = stream_open(output, output_len);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  switch (ndim) {
    case 1:
      field = zfp_field_1d((void *) input, type, blockshape[0]);
      break;
    case 2:
      field = zfp_field_2d((void *) input, type, blockshape[1], blockshape[0]);
      break;
    case 3:
      field = zfp_field_3d((void *) input, type, blockshape[2], blockshape[1], blockshape[0]);
      break;
    case 4:
      field = zfp_field_4d((void *) input, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  int zfp_maxout = (int) zfp_stream_maximum_size(zfp, field);
  zfp_stream_close(zfp);
  stream_close(stream);
  uint8_t *aux_out = malloc(zfp_maxout);
  zfp_aux = zfp_stream_open(NULL);
  zfp_stream_set_precision(zfp_aux, prec);
  stream_aux = stream_open(aux_out, zfp_maxout);
  zfp_stream_set_bit_stream(zfp_aux, stream_aux);
  zfp_stream_rewind(zfp_aux);

  zfpsize = zfp_compress(zfp_aux, field);

  /* clean up */
  zfp_field_free(field);
  zfp_stream_close(zfp_aux);
  stream_close(stream_aux);
  free(shape);
  free(chunkshape);
  free(blockshape);

  if (zfpsize == 0) {
    BLOSC_TRACE_ERROR("\n ZFP: Compression failed\n");
    free(aux_out);
    return (int) zfpsize;
  }
  if ((int32_t) zfpsize >= input_len) {
    BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
    free(aux_out);
    return 0;
  }

  memcpy(output, aux_out, zfpsize);
  free(aux_out);

  return (int) zfpsize;
}

int zfp_prec_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                        int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void *chunk) {
  ZFP_ERROR_NULL(input);
  ZFP_ERROR_NULL(output);
  ZFP_ERROR_NULL(dparams);
  ZFP_ERROR_NULL(dparams->schunk);
  BLOSC_UNUSED_PARAM(chunk);

  blosc2_schunk *sc = dparams->schunk;
  int32_t typesize = sc->typesize;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(sc, "b2nd", &smeta, &smeta_len) < 0) {
    BLOSC_TRACE_ERROR("Cannot access b2nd meta info");
    free(shape);
    free(chunkshape);
    free(blockshape);
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  zfp_type type;     /* array scalar type */
  zfp_field *field;  /* array meta data */
  zfp_stream *zfp;   /* compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  size_t zfpsize;    /* byte size of compressed stream */

  uint prec;
  switch (ndim) {
    case 1:
      prec = meta + 5;
      break;
    case 2:
      prec = meta + 7;
      break;
    case 3:
      prec = meta + 9;
      break;
    case 4:
      prec = meta + 11;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  if (prec > ZFP_MAX_PREC) {
    BLOSC_TRACE_ERROR("Max precision for this codecs is %d", ZFP_MAX_PREC);
    prec = ZFP_MAX_PREC;
  }

  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }

  zfp = zfp_stream_open(NULL);
  zfp_stream_set_precision(zfp, prec);
  stream = stream_open((void *) input, input_len);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  switch (ndim) {
    case 1:
      field = zfp_field_1d((void *) output, type, blockshape[0]);
      break;
    case 2:
      field = zfp_field_2d((void *) output, type, blockshape[1], blockshape[0]);
      break;
    case 3:
      field = zfp_field_3d((void *) output, type, blockshape[2], blockshape[1], blockshape[0]);
      break;
    case 4:
      field = zfp_field_4d((void *) output, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  zfpsize = zfp_decompress(zfp, field);

  /* clean up */
  zfp_field_free(field);
  zfp_stream_close(zfp);
  stream_close(stream);
  free(shape);
  free(chunkshape);
  free(blockshape);

  if (zfpsize == 0) {
    BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
    return (int) zfpsize;
  }

  return (int) output_len;
}

int zfp_rate_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                      int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void *chunk) {
  BLOSC_UNUSED_PARAM(chunk);
  ZFP_ERROR_NULL(input);
  ZFP_ERROR_NULL(output);
  ZFP_ERROR_NULL(cparams);
  ZFP_ERROR_NULL(cparams->schunk);

  double ratio = (double) meta / 100.0;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(cparams->schunk, "b2nd", &smeta, &smeta_len) < 0) {
    free(shape);
    free(chunkshape);
    free(blockshape);
    BLOSC_TRACE_ERROR("b2nd layer not found!");
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  for(int i = 0; i < ndim; i++) {
    if (blockshape[i] < 4) {
      BLOSC_TRACE_ERROR("ZFP does not support blocks smaller than cells (4x...x4");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  zfp_type type;     /* array scalar type */
  zfp_field *field;  /* array meta data */
  zfp_stream *zfp;   /* stream containing the real output buffer */
  zfp_stream *zfp_aux;   /* auxiliary compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  bitstream *stream_aux; /* auxiliary bit stream to write to or read from */
  size_t zfpsize;    /* byte size of compressed stream */

  int32_t typesize = cparams->typesize;

  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }
  double rate = ratio * typesize * 8;     // convert from output size / input size to output bits per input value
  uint cellsize = 1u << (2 * ndim);
  double min_rate;
  if (type == zfp_type_float) {
    min_rate = (double) (1 + 8u) / cellsize;
    if (rate < min_rate) {
      BLOSC_TRACE_ERROR("ZFP minimum rate for this item type is %f. Compression will be done using this one.\n",
                        min_rate);
    }
  }
  else {
    min_rate = (double) (1 + 11u) / cellsize;
    if (rate < min_rate) {
      BLOSC_TRACE_ERROR("ZFP minimum rate for this item type is %f. Compression will be done using this one.\n",
                        min_rate);
    }
  }
  zfp = zfp_stream_open(NULL);
  stream = stream_open(output, output_len);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  switch (ndim) {
    case 1:
      field = zfp_field_1d((void *) input, type, blockshape[0]);
      break;
    case 2:
      field = zfp_field_2d((void *) input, type, blockshape[1], blockshape[0]);
      break;
    case 3:
      field = zfp_field_3d((void *) input, type, blockshape[2], blockshape[1], blockshape[0]);
      break;
    case 4:
      field = zfp_field_4d((void *) input, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  int zfp_maxout = (int) zfp_stream_maximum_size(zfp, field);
  zfp_stream_close(zfp);
  stream_close(stream);
  uint8_t *aux_out = malloc(zfp_maxout);
  zfp_aux = zfp_stream_open(NULL);
  stream_aux = stream_open(aux_out, zfp_maxout);
  zfp_stream_set_bit_stream(zfp_aux, stream_aux);
  zfp_stream_rewind(zfp_aux);
  zfp_stream_set_rate(zfp_aux, rate, type, ndim, zfp_false);

  zfpsize = zfp_compress(zfp_aux, field);

  /* clean up */
  zfp_field_free(field);
  zfp_stream_close(zfp_aux);
  stream_close(stream_aux);
  free(shape);
  free(chunkshape);
  free(blockshape);

  if (zfpsize == 0) {
    BLOSC_TRACE_ERROR("\n ZFP: Compression failed\n");
    free(aux_out);
    return (int) zfpsize;
  }
  if ((int32_t) zfpsize >= input_len) {
    BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
    free(aux_out);
    return 0;
  }

  memcpy(output, aux_out, zfpsize);
  free(aux_out);

  return (int) zfpsize;
}

int zfp_rate_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                        int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void *chunk) {
  ZFP_ERROR_NULL(input);
  ZFP_ERROR_NULL(output);
  ZFP_ERROR_NULL(dparams);
  ZFP_ERROR_NULL(dparams->schunk);
  BLOSC_UNUSED_PARAM(chunk);

  blosc2_schunk *sc = dparams->schunk;
  int32_t typesize = sc->typesize;

  double ratio = (double) meta / 100.0;
  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(sc, "b2nd", &smeta, &smeta_len) < 0) {
    BLOSC_TRACE_ERROR("Cannot access b2nd meta info");
    free(shape);
    free(chunkshape);
    free(blockshape);
    return BLOSC2_ERROR_FAILURE;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  zfp_type type;     /* array scalar type */
  zfp_field *field;  /* array meta data */
  zfp_stream *zfp;   /* compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  size_t zfpsize;    /* byte size of compressed stream */

  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }
  double rate =
      ratio * (double) typesize * 8;     // convert from output size / input size to output bits per input value
  zfp = zfp_stream_open(NULL);
  zfp_stream_set_rate(zfp, rate, type, ndim, zfp_false);

  stream = stream_open((void *) input, input_len);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  switch (ndim) {
    case 1:
      field = zfp_field_1d((void *) output, type, blockshape[0]);
      break;
    case 2:
      field = zfp_field_2d((void *) output, type, blockshape[1], blockshape[0]);
      break;
    case 3:
      field = zfp_field_3d((void *) output, type, blockshape[2], blockshape[1], blockshape[0]);
      break;
    case 4:
      field = zfp_field_4d((void *) output, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
      break;
    default:
      free(shape);
      free(chunkshape);
      free(blockshape);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }

  zfpsize = zfp_decompress(zfp, field);

  /* clean up */
  zfp_field_free(field);
  zfp_stream_close(zfp);
  stream_close(stream);
  free(shape);
  free(chunkshape);
  free(blockshape);

  if (zfpsize == 0) {
    BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
    return (int) zfpsize;
  }

  return (int) output_len;
}

int zfp_getcell(void *thread_context, const uint8_t *block, int32_t cbytes, uint8_t *dest, int32_t destsize) {
  struct thread_context *thread_ctx = thread_context;
  blosc2_context *context = thread_ctx->parent_context;
  bool meta = false;
  int8_t ndim = ZFP_MAX_DIM + 1;
  int32_t blockmeta[ZFP_MAX_DIM];
  if (context->schunk->blockshape == NULL) {
    // blockshape is not filled yet.  Use the Blosc2 NDim layer to populate it.
    for (int nmetalayer = 0; nmetalayer < context->schunk->nmetalayers; nmetalayer++) {
      if (strcmp("b2nd", context->schunk->metalayers[nmetalayer]->name) == 0) {
        meta = true;
        uint8_t *pmeta = context->schunk->metalayers[nmetalayer]->content;
        ndim = (int8_t) pmeta[2];
        assert(ndim <= ZFP_MAX_DIM);
        pmeta += (6 + ndim * 9 + ndim * 5);
        for (int8_t i = 0; (uint8_t) i < ndim; i++) {
          pmeta += 1;
          swap_store(blockmeta + i, pmeta, sizeof(int32_t));
          pmeta += sizeof(int32_t);
        }
      }
    }
    if (!meta) {
      return -1;
    }
    context->schunk->ndim = ndim;
    context->schunk->blockshape = malloc(sizeof(int64_t) * ndim);
    for (int i = 0; i < ndim; ++i) {
      context->schunk->blockshape[i] = (int64_t) blockmeta[i];
    }
  }
  ndim = context->schunk->ndim;
  int64_t *blockshape = context->schunk->blockshape;

  // Compute the coordinates of the cell
  int64_t cell_start_ndim[ZFP_MAX_DIM];
  int64_t cell_ind_ndim[ZFP_MAX_DIM];
  int64_t ncell_ndim[ZFP_MAX_DIM];
  int64_t ind_strides[ZFP_MAX_DIM];
  int64_t cell_strides[ZFP_MAX_DIM];
  int64_t cell_ind, ncell;
  blosc2_unidim_to_multidim(ndim, blockshape, thread_ctx->zfp_cell_start, cell_start_ndim);
  for (int i = 0; i < ndim; ++i) {
    cell_ind_ndim[i] = cell_start_ndim[i] % ZFP_MAX_DIM;
    ncell_ndim[i] = cell_start_ndim[i] / ZFP_MAX_DIM;
  }
  ind_strides[ndim - 1] = cell_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    ind_strides[i] = ZFP_MAX_DIM * ind_strides[i + 1];
    cell_strides[i] = ((blockshape[i + 1] - 1) / ZFP_MAX_DIM + 1) * cell_strides[i + 1];
  }
  blosc2_multidim_to_unidim(cell_ind_ndim, (int8_t) ndim, ind_strides, &cell_ind);
  blosc2_multidim_to_unidim(ncell_ndim, (int8_t) ndim, cell_strides, &ncell);
  int cell_nitems = (int) (1u << (2 * ndim));
  if ((thread_ctx->zfp_cell_nitems > cell_nitems) ||
      ((cell_ind + thread_ctx->zfp_cell_nitems) > cell_nitems)) {
    return 0;
  }

  // Get the ZFP stream
  zfp_type type;     /* array scalar type */
  zfp_stream *zfp;   /* compressed stream */
  bitstream *stream; /* bit stream to write to or read from */
  int32_t typesize = context->typesize;
  zfp = zfp_stream_open(NULL);

  switch (typesize) {
    case sizeof(float):
      type = zfp_type_float;
      break;
    case sizeof(double):
      type = zfp_type_double;
      break;
    default:
      BLOSC_TRACE_ERROR("ZFP is not available for typesize: %d", typesize);
      return BLOSC2_ERROR_FAILURE;
  }
  uint8_t compmeta = context->compcode_meta;   // access to compressed chunk header
  double rate = (double) (compmeta * typesize * 8) /
                100.0;     // convert from output size / input size to output bits per input value
  zfp_stream_set_rate(zfp, rate, type, ndim, zfp_false);

  stream = stream_open((void *) block, cbytes);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  // Check that ncell is a valid index
  int ncells = (int) ((cbytes * 8) / zfp->maxbits);
  if (ncell >= ncells) {
    BLOSC_TRACE_ERROR("Invalid cell index");
    return -1;
  }

  // Position the stream at the ncell bit offset for reading
  stream_rseek(zfp->stream, (size_t) (ncell * zfp->maxbits));

  // Get the cell
  size_t zfpsize;
  uint8_t *cell = malloc(cell_nitems * typesize);
  switch (ndim) {
    case 1:
      if (type == zfp_type_float) {
        zfpsize = zfp_decode_block_float_1(zfp, (float *) cell);
      }
      else {
        zfpsize = zfp_decode_block_double_1(zfp, (double *) cell);
      }
      break;
    case 2:
      if (type == zfp_type_float) {
        zfpsize = zfp_decode_block_float_2(zfp, (float *) cell);
      }
      else {
        zfpsize = zfp_decode_block_double_2(zfp, (double *) cell);
      }
      break;
    case 3:
      if (type == zfp_type_float) {
        zfpsize = zfp_decode_block_float_3(zfp, (float *) cell);
      }
      else {
        zfpsize = zfp_decode_block_double_3(zfp, (double *) cell);
      }
      break;
    case 4:
      if (type == zfp_type_float) {
        zfpsize = zfp_decode_block_float_4(zfp, (float *) cell);
      }
      else {
        zfpsize = zfp_decode_block_double_4(zfp, (double *) cell);
      }
      break;
    default:
      free(cell);
      BLOSC_TRACE_ERROR("ZFP is not available for ndims: %d", ndim);
      return BLOSC2_ERROR_FAILURE;
  }
  memcpy(dest, &cell[cell_ind * typesize], thread_ctx->zfp_cell_nitems * typesize);
  free(cell);
  zfp_stream_close(zfp);
  stream_close(stream);

  if ((zfpsize == 0) || ((int32_t) zfpsize > (destsize * 8)) ||
      ((int32_t) zfpsize > (cell_nitems * typesize * 8)) ||
      ((thread_ctx->zfp_cell_nitems * typesize * 8) > (int32_t) zfpsize)) {
    BLOSC_TRACE_ERROR("ZFP error or small destsize");
    return -1;
  }

  return (int) (thread_ctx->zfp_cell_nitems * typesize);
}

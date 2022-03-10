#include "blosc2.h"
#include "blosc2.c"
#include "blosc-private.h"
#include "frame.h"
#include "blosc2/codecs-registry.h"
#include "zfp.h"
#include "blosc2-zfp.h"
#include <math.h>

int blosc2_zfp_acc_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                            int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

    double tol = (int8_t) meta;
/*
    printf("\n input \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* stream containing the real output buffer */
    zfp_stream *zfp_aux;   /* auxiliar compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    bitstream *stream_aux; /* auxiliar bit stream to write to or read from */
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
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
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
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
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

    if (zfpsize < 0) {
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

int blosc2_zfp_acc_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                              int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    size_t typesize;
    int flags;
    blosc_cbuffer_metainfo(chunk, &typesize, &flags);

    double tol = (int8_t) meta;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
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
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_accuracy(zfp, tolerance);
    stream = stream_open((void*) input, input_len);
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
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfpsize = zfp_decompress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
        return (int) zfpsize;
    }

    return (int) output_len;
}

int blosc2_zfp_prec_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                            int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

/*
    printf("\n input \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* stream containing the real output buffer */
    zfp_stream *zfp_aux;   /* auxiliar compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    bitstream *stream_aux; /* auxiliar bit stream to write to or read from */
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
            printf("\n ZFP is not available for this ndim \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    if(prec > ZFP_MAX_PREC) {
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
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
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
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
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

    if (zfpsize < 0) {
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

int blosc2_zfp_prec_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                              int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    size_t typesize;
    int flags;
    blosc_cbuffer_metainfo(chunk, &typesize, &flags);
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
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
            printf("\n ZFP is not available for this ndim \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    if(prec > ZFP_MAX_PREC) {
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
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_precision(zfp, prec);
    stream = stream_open((void*) input, input_len);
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
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfpsize = zfp_decompress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
        return (int) zfpsize;
    }

    return (int) output_len;
}

int blosc2_zfp_rate_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                             int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

    double ratio = (double) meta / 100.0;
/*
    printf("\n input \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* stream containing the real output buffer */
    zfp_stream *zfp_aux;   /* auxiliar compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    bitstream *stream_aux; /* auxiliar bit stream to write to or read from */
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
            printf("\n ZFP is not available for this typesize \n");
    }
    double rate = ratio * typesize * 8;     // convert from output size / input size to output bits per input value
    uint cellsize = 1u << (2 * ndim);
    double min_rate;
    switch (type) {
        case zfp_type_float:
            min_rate = (double) (1 + 8u) / cellsize;
            if (rate < min_rate) {
                BLOSC_TRACE_ERROR("\n ZFP minimum rate for this item type is %f. Compression will be done using this rate \n", min_rate);
            }
            break;
        case zfp_type_double:
            min_rate = (double) (1 + 11u) / cellsize;
            if (rate < min_rate) {
                BLOSC_TRACE_ERROR("\n ZFP minimum rate for this item type is %f. Compression will be done using this rate \n", min_rate);
            }
            break;
        default:
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
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
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
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

    if (zfpsize < 0) {
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

int blosc2_zfp_rate_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                              int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    size_t typesize;
    int flags;
    blosc_cbuffer_metainfo(chunk, &typesize, &flags);

    double ratio = (double) meta / 100.0;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
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
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }
    double rate = ratio * (double) typesize * 8;     // convert from output size / input size to output bits per input value
    zfp = zfp_stream_open(NULL);
    zfp_stream_set_rate(zfp, rate, type, ndim, zfp_false);

    stream = stream_open((void*) input, input_len);
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
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfpsize = zfp_decompress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
        return (int) zfpsize;
    }

    return (int) output_len;
}


int blosc2_zfp_getcell(blosc2_schunk* schunk, int nchunk, int nblock, int ncell, void *dest, size_t destsize) {
    ZFP_ERROR_NULL(dest);
    int32_t typesize = schunk->typesize;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    int32_t smeta_len;
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    // Get the chunk
    blosc2_context *context = schunk->dctx;
    bool is_lazy = false;
    if (schunk->storage->urlpath != NULL) {
        is_lazy = true;
    }

    bool needs_free;
    uint8_t *chunk;
    if (is_lazy) {
        blosc2_schunk_get_lazychunk(schunk, nchunk, &chunk, &needs_free);
    } else {
        blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
    }
    uint8_t compcode = chunk[22];                                   // access to compressed chunk header
    if (compcode != BLOSC_CODEC_ZFP_FIXED_RATE) {
        BLOSC_TRACE_ERROR("\n Cell decompression only supported for ZFP FIXED_RATE mode\n");
        return BLOSC2_ERROR_CODEC_SUPPORT;
    }

    // Initialize the decompression context
    blosc_header header;
    int32_t ntbytes;
    int rc;
    int32_t nbytes, cbytes;
    blosc2_cbuffer_sizes(chunk, &nbytes, &cbytes, NULL);

    rc = read_chunk_header(chunk, cbytes, true, &header);
    if (rc < 0) {
        return rc;
    }

    if (header.nbytes > destsize) {
        // Not enough space for writing into the destination
        return BLOSC2_ERROR_WRITE_BUFFER;
    }

    context->src = chunk;
    context->srcsize = cbytes;
    context->dest = dest;
    context->destsize = nbytes;

    rc = blosc2_initialize_context_from_header(schunk->dctx, &header);
    if (rc < 0) {
        return rc;
    }


    // Get the offset of the nblock
    bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;
    int32_t block_offset = memcpyed ?
                         context->header_overhead + nblock * context->blocksize : sw32_(context->bstarts + nblock);

    // Get the csize of the nblock
    int32_t block_csize;
    size_t trailer_offset;
    if (is_lazy) {
        trailer_offset = BLOSC_EXTENDED_HEADER_LENGTH + context->nblocks * sizeof(int32_t);
        int32_t *block_csizes = (int32_t *)(chunk + trailer_offset + sizeof(int32_t) + sizeof(int64_t));
        block_csize = block_csizes[nblock];
    } else if (memcpyed) {
        block_csize = context->blocksize;
    } else {
        block_csize = chunk[block_offset];
    }

    // Get the block
    uint8_t *block;
    int64_t chunk_offset;
    if (is_lazy) {
        chunk_offset = *(int64_t*)(chunk + trailer_offset + sizeof(int32_t));
        void* fp = NULL;
        blosc2_io_cb *io_cb = blosc2_get_io_cb(schunk->storage->io->id);
        if (io_cb == NULL) {
            BLOSC_TRACE_ERROR("Error getting the input/output API");
            return BLOSC2_ERROR_PLUGIN_IO;
        }
        blosc2_frame_s* frame = (blosc2_frame_s*)schunk->frame;
        if (frame->sframe) {
            // The chunk is not in the frame
            char* chunkpath = malloc(strlen(frame->urlpath) + 1 + 8 + strlen(".chunk") + 1);
            BLOSC_ERROR_NULL(chunkpath, BLOSC2_ERROR_MEMORY_ALLOC);
            sprintf(chunkpath, "%s/%08X.chunk", frame->urlpath, nchunk);
            fp = io_cb->open(chunkpath, "rb", schunk->storage->io->params);
            free(chunkpath);
            io_cb->seek(fp, block_offset + sizeof(int32_t), SEEK_SET);
        }
        else {
            fp = io_cb->open(frame->urlpath, "rb", schunk->storage->io->params);
            io_cb->seek(fp, chunk_offset + block_offset + sizeof(int32_t), SEEK_SET);
        }
        block_csize -= sizeof(int32_t);
        block = malloc(block_csize);
        int64_t rbytes = io_cb->read(block, 1, block_csize, fp);
        io_cb->close(fp);
        if ((int32_t)rbytes != block_csize) {
            BLOSC_TRACE_ERROR("Cannot read the (lazy) block out of the fileframe.");
            return BLOSC2_ERROR_READ_BUFFER;
        }
    } else {
        block = chunk + block_offset + sizeof(int32_t);
    }

    // Get the ZFP stream
    zfp_type type;     /* array scalar type */
    zfp_stream *zfp;   /* compressed stream */
    bitstream *stream; /* bit stream to write to or read from */

    zfp = zfp_stream_open(NULL);

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            if (needs_free) {
                free(chunk);
            }
            if (is_lazy) {
                free(block);
            }
            return 0;
    }
    uint8_t compmeta = chunk[23];                                 // access to compressed chunk header
    double rate = (double) (compmeta * typesize * 8) / 100.0;     // convert from output size / input size to output bits per input value
    zfp_stream_set_rate(zfp, rate, type, ndim, zfp_false);

    stream = stream_open((void*) block, block_csize);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    // Check that ncell is a valid index
    int ncells = (int) ((block_csize * 8) / zfp->maxbits);
    if (ncell >= ncells) {
        BLOSC_TRACE_ERROR("Invalid cell index");
        return -1;
    }

    // Position the stream at the ncell bit offset for reading
    stream_rseek(zfp->stream, ncell * zfp->maxbits);

    // Get the cell
    size_t zfpsize;
    switch (ndim) {
        case 1:
            switch (type) {
                case zfp_type_float:
                    zfpsize = zfp_decode_block_float_1(zfp, dest);
                    break;
                case zfp_type_double:
                    zfpsize = zfp_decode_block_double_1(zfp, dest);
                    break;
                default:
                    printf("\n ZFP is not available for this typesize \n");
                    free(shape);
                    free(chunkshape);
                    free(blockshape);
                    zfp_stream_close(zfp);
                    stream_close(stream);
                    if (needs_free) {
                        free(chunk);
                    }
                    if (is_lazy) {
                        free(block);
                    }
                    return 0;
            }
            break;
        case 2:
            switch (type) {
                case zfp_type_float:
                    zfpsize = zfp_decode_block_float_2(zfp, dest);
                    break;
                case zfp_type_double:
                    zfpsize = zfp_decode_block_double_2(zfp, dest);
                    break;
                default:
                    printf("\n ZFP is not available for this typesize \n");
                    free(shape);
                    free(chunkshape);
                    free(blockshape);
                    zfp_stream_close(zfp);
                    stream_close(stream);
                    if (needs_free) {
                        free(chunk);
                    }
                    if (is_lazy) {
                        free(block);
                    }
                    return 0;
            }
            break;
        case 3:
            switch (type) {
                case zfp_type_float:
                    zfpsize = zfp_decode_block_float_3(zfp, dest);
                    break;
                case zfp_type_double:
                    zfpsize = zfp_decode_block_double_3(zfp, dest);
                    break;
                default:
                    printf("\n ZFP is not available for this typesize \n");
                    free(shape);
                    free(chunkshape);
                    free(blockshape);
                    zfp_stream_close(zfp);
                    stream_close(stream);
                    if (needs_free) {
                        free(chunk);
                    }
                    if (is_lazy) {
                        free(block);
                    }
                    return 0;
            }
            break;
        case 4:
            switch (type) {
                case zfp_type_float:
                    zfpsize = zfp_decode_block_float_4(zfp, dest);
                    break;
                case zfp_type_double:
                    zfpsize = zfp_decode_block_double_4(zfp, dest);
                    break;
                default:
                    printf("\n ZFP is not available for this typesize \n");
                    free(shape);
                    free(chunkshape);
                    free(blockshape);
                    zfp_stream_close(zfp);
                    stream_close(stream);
                    if (needs_free) {
                        free(chunk);
                    }
                    if (is_lazy) {
                        free(block);
                    }
                    return 0;
            }
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            return 0;
    }

    free(shape);
    free(chunkshape);
    free(blockshape);
    zfp_stream_close(zfp);
    stream_close(stream);
    if (needs_free) {
        free(chunk);
    }
    if (is_lazy) {
        free(block);
    }

    if ((zfpsize < 0) || (zfpsize > (destsize * typesize * 8))) {
        BLOSC_TRACE_ERROR("ZFP error or small destsize");
        return -1;
    }

    return (int) zfpsize;
}


int blosc2_zfp_getitem(blosc2_schunk* schunk, int64_t index, void* item) {
    int32_t typesize = schunk->typesize;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int64_t));
    int32_t *blockshape = malloc(8 * sizeof(int64_t));
    int64_t cellshape[ZFP_MAX_DIM] = {ZFP_CELL_SHAPE, ZFP_CELL_SHAPE, ZFP_CELL_SHAPE, ZFP_CELL_SHAPE};
    uint8_t *smeta;
    int32_t smeta_len;
    if (blosc2_meta_get(schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    int64_t index_ndim[ZFP_MAX_DIM];
    int64_t index_chunk_ndim[ZFP_MAX_DIM];
    int64_t index_block_ndim[ZFP_MAX_DIM];
    int64_t index_cell_ndim[ZFP_MAX_DIM];
    int64_t ind_ndim[ZFP_MAX_DIM];
    index_unidim_to_multidim(ndim, shape, index, index_ndim);
    int64_t cellsize = typesize;
    for (int i = 0; i < ndim; ++i) {
        index_chunk_ndim[i] = index_ndim[i] / chunkshape[i];
        index_block_ndim[i] = (index_ndim[i] % chunkshape[i]) / blockshape[i];
        index_cell_ndim[i] = ((index_ndim[i] % chunkshape[i]) % blockshape[i]) / ZFP_CELL_SHAPE;
        ind_ndim[i] = ((index_ndim[i] % chunkshape[i]) % blockshape[i]) % ZFP_CELL_SHAPE;
        cellsize *= cellshape[i];
    }
    int64_t chunk_strides[ZFP_MAX_DIM];
    int64_t block_strides[ZFP_MAX_DIM];
    int64_t cell_strides[ZFP_MAX_DIM];
    int64_t item_strides[ZFP_MAX_DIM];
    chunk_strides[ndim - 1] = block_strides[ndim - 1] = cell_strides[ndim - 1] = item_strides[ndim - 1] = 1;
    for (int i = ndim - 2; i >= 0; --i) {
        int j = i + 1;
        chunk_strides[i] = (shape[j] - 1) / chunkshape[j] + 1;
        block_strides[i] = (chunkshape[j] - 1) / blockshape[j] + 1;
        cell_strides[i] = (blockshape[j] - 1) / cellshape[j] + 1;
        item_strides[i] = cellshape[j];
    }
    int64_t nchunk, nblock, ncell, ind;
    index_multidim_to_unidim(index_chunk_ndim, ndim, (int64_t*) chunk_strides, &nchunk);
    index_multidim_to_unidim(index_block_ndim, ndim, (int64_t*) block_strides, &nblock);
    index_multidim_to_unidim(index_cell_ndim, ndim, (int64_t*) cell_strides, &ncell);
    index_multidim_to_unidim(ind_ndim, ndim, item_strides, &ind);

    int8_t *cell = malloc(cellsize);
    blosc2_zfp_getcell(schunk, (int) nchunk, (int) nblock, (int) ncell, cell, cellsize);
    memcpy(item, cell + ind * typesize, typesize);

    return typesize;
}

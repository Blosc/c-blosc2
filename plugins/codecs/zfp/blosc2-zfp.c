#include "blosc2.h"
#include "zfp.h"
#include "zfp-private.h"
#include "blosc2-zfp.h"
#include <math.h>

int blosc2_zfp_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                        int32_t output_len, uint8_t meta, blosc2_cparams *cparams) {
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
    }
    if (zfpsize > input_len) {
        BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
        return 0;
    }

    memcpy(output, aux_out, zfpsize);
    free(aux_out);

    return (int) zfpsize;
}

int blosc2_zfp_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                          int32_t output_len, uint8_t meta, blosc2_dparams *dparams) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    double tol = (int8_t) meta;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
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

    int32_t typesize = dparams->typesize;

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_accuracy(zfp, tolerance);
    stream = stream_open(input, input_len);
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


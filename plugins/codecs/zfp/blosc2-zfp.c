#include "blosc2.h"
#include "src/zfp.h"
#include "zfp-private.h"
#include "blosc2-zfp.h"


int blosc2_zfp_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                        int32_t output_len, uint8_t meta, blosc2_cparams *cparams) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

    printf("input_len: %d", input_len);
    printf("blocksize: %d", cparams->blocksize);
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
    int64_t *fieldshape = malloc(8 * sizeof(int32_t));
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
    zfp_stream *zfp;   /* compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */
    double tolerance = 1e-3;

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

    if (blockshape[0] != 0) {
        for (int i = 0; i < ndim; i++) {
            fieldshape[i] = blockshape[i];
        }
    } else if (chunkshape[0] != 0) {
        for (int i = 0; i < ndim; i++) {
            fieldshape[i] = chunkshape[i];
        }
    } else {
        for (int i = 0; i < ndim; i++) {
            fieldshape[i] = shape[i];
        }
    }

        switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) input, type, fieldshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) input, type, fieldshape[0], fieldshape[1]);
            break;
        case 3:
            field = zfp_field_3d((void *) input, type, fieldshape[0], fieldshape[1], fieldshape[2]);
            break;
        case 4:
            field = zfp_field_4d((void *) input, type, fieldshape[0], fieldshape[1], fieldshape[2], fieldshape[3]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            return 0;
    }
/*
    printf("\n field \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", ((uint8_t *) field->data)[i]);
    }
*/
    zfpsize = zfp_compress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);
    free(fieldshape);

    if (!zfpsize) {
        printf("\n ZFP: Compression failed\n");
        return 0;
    }
    if (zfpsize > input_len) {
        printf("\n ZFP: Compressed data is bigger than input! \n");
        return 0;
    }

    return (int) zfpsize;
}

int blosc2_zfp_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                          int32_t output_len, uint8_t meta, blosc2_dparams *dparams) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    int64_t *fieldshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    size_t typesize;
    int flags;
    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */
    double tolerance = 1e-3;

    blosc_cbuffer_metainfo(input, &typesize, &flags);

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

    if (blockshape[0] != 0) {
        for (int i = 0; i < ndim; i++) {
            fieldshape[i] = blockshape[i];
        }
    } else if (chunkshape[0] != 0) {
        for (int i = 0; i < ndim; i++) {
            fieldshape[i] = chunkshape[i];
        }
    } else {
        for (int i = 0; i < ndim; i++) {
            fieldshape[i] = shape[i];
        }
    }

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) input, type, fieldshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) input, type, fieldshape[0], fieldshape[1]);
            break;
        case 3:
            field = zfp_field_3d((void *) input, type, fieldshape[0], fieldshape[1], fieldshape[2]);
            break;
        case 4:
            field = zfp_field_4d((void *) input, type, fieldshape[0], fieldshape[1], fieldshape[2], fieldshape[3]);
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
    free(fieldshape);

    if (!zfpsize) {
        printf("\n Decompression failed\n");
        return 0;
    }

    return (int) zfpsize;
}


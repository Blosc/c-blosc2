/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Author: Oscar Griñón <oscar@blosc.org>
  Author: Aleix Alcacer <aleix@blosc.org>
  Creation date: 2020-06-12

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  This codec is meant to leverage multidimensionality for getting
  better compression ratios.  The idea is to look for similarities
  in places that are closer in a euclidean metric, not the typical
  linear one.
**********************************************************************/


#include <stdio.h>
#include <ndlz4x4.h>
#include <ndlz.h>
#include <ndlz8x8.h>



void swap_store(void *dest, const void *pa, int size) {
    uint8_t *pa_ = (uint8_t *) pa;
    uint8_t *pa2_ = malloc((size_t) size);
    int i = 1; /* for big/little endian detection */
    char *p = (char *) &i;

    if (p[0] == 1) {
        /* little endian */
        switch (size) {
            case 8:
                pa2_[0] = pa_[7];
                pa2_[1] = pa_[6];
                pa2_[2] = pa_[5];
                pa2_[3] = pa_[4];
                pa2_[4] = pa_[3];
                pa2_[5] = pa_[2];
                pa2_[6] = pa_[1];
                pa2_[7] = pa_[0];
                break;
            case 4:
                pa2_[0] = pa_[3];
                pa2_[1] = pa_[2];
                pa2_[2] = pa_[1];
                pa2_[3] = pa_[0];
                break;
            case 2:
                pa2_[0] = pa_[1];
                pa2_[1] = pa_[0];
                break;
            case 1:
                pa2_[0] = pa_[0];
                break;
            default:
                fprintf(stderr, "Unhandled nitems: %d\n", size);
        }
    }
    memcpy(dest, pa2_, size);
    free(pa2_);
}

int32_t deserialize_meta(uint8_t *smeta, uint32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape) {
    uint8_t *pmeta = smeta;

    // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
    pmeta += 1;

    // version entry
    int8_t version = pmeta[0];  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // ndim entry
    *ndim = pmeta[0];
    int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // shape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) shape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(shape + i, pmeta, sizeof(int64_t));
        pmeta += sizeof(int64_t);
    }

    // chunkshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) chunkshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(chunkshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }

    // blockshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) blockshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(blockshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }
    uint32_t slen = (uint32_t)(pmeta - smeta);
    return 0;
}

int ndlz_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                  uint8_t meta, blosc2_cparams *cparams) {
    NDLZ_ERROR_NULL(input);
    NDLZ_ERROR_NULL(output);
    NDLZ_ERROR_NULL(cparams);

    switch (meta) {
        case 4:
            return ndlz4_compress(input, input_len, output, output_len, meta, cparams);
            break;
        case 8:
            return ndlz8_compress(input, input_len, output, output_len, meta, cparams);
            break;
        default:
            printf("\n NDLZ is not avaiable for this cellsize \n");
            return 0;
    }
}

int ndlz_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                    uint8_t meta, blosc2_dparams *dparams) {
    NDLZ_ERROR_NULL(input);
    NDLZ_ERROR_NULL(output);
    NDLZ_ERROR_NULL(dparams);

    switch (meta) {
        case 4:
            return ndlz4_decompress(input, input_len, output, output_len, meta, dparams);
            break;
        case 8:
            return ndlz8_decompress(input, input_len, output, output_len, meta, dparams);
            break;
        default:
            printf("\n NDLZ is not avaiable for this cellsize \n");
            return 0;
    }
}






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
#include "ndlz.h"
#include "ndlz-private.h"
#include "ndlz4x4.h"
#include "ndlz8x8.h"

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






/*
 * Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 *
 * Example program demonstrating how to print metainfo from a caterva frame.
 * You can build frames with example_frame_generator.c
 *
 * Usage:
 * $ ./example_print_meta <urlpath>
 *
 * Example of output:
 * $ ./caterva_example_print_meta example_big_float_frame.caterva
 * Caterva metalayer parameters:
 * Ndim:       3
 * Shape:      200, 310, 214
 * Chunkshape: 110, 120, 76
 * Blockshape: 57, 52, 35
 *
*/

# include <caterva.h>

int print_meta(char *urlpath) {

    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    caterva_ctx_t *ctx;
    caterva_ctx_new(&cfg, &ctx);
    caterva_array_t *arr;
    CATERVA_ERROR(caterva_open(ctx, urlpath, &arr));
    caterva_print_meta(arr);
    caterva_free(ctx, &arr);
    caterva_ctx_free(&ctx);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s urlpath", argv[0]);
        exit(-1);
    }

    char* urlpath = argv[1];
    print_meta(urlpath);

    return 0;
}

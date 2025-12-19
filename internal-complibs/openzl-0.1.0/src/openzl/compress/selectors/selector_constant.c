// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/compress/selectors/selector_constant.h"
#include "openzl/common/assertion.h"
#include "openzl/compress/private_nodes.h"

/* SI_selector_constant():
 *
 * The goal of this selector is to select between serialized and
 * fixed-size constant encoding given an input that can be either type
 */

ZL_GraphID SI_selector_constant(
        const ZL_Selector* selCtx,
        const ZL_Input* inputStream,
        const ZL_GraphID* customGraphs,
        size_t nbCustomGraphs)
{
    (void)selCtx;
    (void)customGraphs;
    (void)nbCustomGraphs;

    ZL_ASSERT(
            ZL_Input_type(inputStream) == ZL_Type_serial
            || ZL_Input_type(inputStream) == ZL_Type_struct);
    ZL_ASSERT_GE(ZL_Input_eltWidth(inputStream), 1);

    return ZL_Input_type(inputStream) == ZL_Type_serial
            ? ZL_GRAPH_CONSTANT_SERIAL
            : ZL_GRAPH_CONSTANT_FIXED;
}

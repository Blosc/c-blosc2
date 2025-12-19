// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/compress/selectors/selector_numeric.h"
#include "openzl/compress/private_nodes.h"
#include "openzl/compress/selectors/ml/gbt.h"
#include "openzl/compress/selectors/ml/selector_numeric_model.h"

ZL_GraphID SI_selector_numeric(
        const ZL_Selector* selCtx,
        const ZL_Input* inputStream,
        const ZL_GraphID* customGraphs,
        size_t nbCustomGraphs)
{
    (void)selCtx;
    (void)customGraphs;
    (void)nbCustomGraphs;

    GBTModel gbtModel = getGenericNumericGbtModel(FeatureGen_integer);
    ZL_RESULT_OF(Label)

    result = GBTModel_predict(&gbtModel, inputStream);
    if (ZL_RES_isError(result)) {
        return ZL_GRAPH_ILLEGAL;
    }

    const char* decodedLabel = ZL_RES_value(result);
    if (!strcmp(decodedLabel, "fieldlz")) {
        return ZL_GRAPH_FIELD_LZ;
    } else if (!strcmp(decodedLabel, "range_pack")) {
        return ZL_GRAPH_RANGE_PACK;
    } else if (!strcmp(decodedLabel, "range_pack_zstd")) {
        return ZL_GRAPH_RANGE_PACK_ZSTD;
    } else if (!strcmp(decodedLabel, "delta_fieldlz")) {
        return ZL_GRAPH_DELTA_FIELD_LZ;
    } else if (!strcmp(decodedLabel, "tokenize_delta_fieldlz")) {
        return ZL_GRAPH_TOKENIZE_DELTA_FIELD_LZ;
    } else if (!strcmp(decodedLabel, "zstd")) {
        return ZL_GRAPH_ZSTD;
    }

    return ZL_GRAPH_ILLEGAL;
}

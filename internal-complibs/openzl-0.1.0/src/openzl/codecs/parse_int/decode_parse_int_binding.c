// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/parse_int/decode_parse_int_binding.h"
#include "openzl/codecs/parse_int/common_parse_int.h"
#include "openzl/codecs/parse_int/decode_parse_int_kernel.h"
#include "openzl/shared/overflow.h"

#include "openzl/common/assertion.h"
#include "openzl/zl_data.h"
#include "openzl/zl_dtransform.h"

ZL_Report DI_parseInt(ZL_Decoder* decoder, const ZL_Input* ins[])
{
    ZL_Input const* numbers = ins[0];
    const size_t eltWidth   = ZL_Input_eltWidth(numbers);
    ZL_RET_R_IF(node_invalid_input, eltWidth != 8);
    size_t const nbElts = ZL_Input_numElts(numbers);
    size_t outBound;
    ZL_RET_R_IF(
            allocation,
            ZL_overflowMulST(
                    nbElts, ZL_PARSE_INT_MAX_STRING_LENGTH, &outBound));
    ZL_Output* outStream =
            ZL_Decoder_create1StringStream(decoder, nbElts, outBound);
    ZL_RET_R_IF_NULL(allocation, outStream);
    uint32_t* fieldSizes = ZL_Output_stringLens(outStream);
    ZL_RET_R_IF_NULL(allocation, fieldSizes);
    int64_t const* nums = ZL_Input_ptr(numbers);
    size_t outSize = ZL_DecodeParseInt_fillFieldSizes(fieldSizes, nbElts, nums);
    ZL_ASSERT_LE(outSize, outBound);
    ZL_DecodeParseInt_fillContent(
            ZL_Output_ptr(outStream), outSize, nbElts, nums, fieldSizes);
    ZL_RET_R_IF_ERR(ZL_Output_commit(outStream, nbElts));
    return ZL_returnSuccess();
}

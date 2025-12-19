// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/constant/encode_constant_binding.h"
#include "openzl/codecs/constant/encode_constant_kernel.h"

#include "openzl/common/assertion.h"
#include "openzl/common/errors_internal.h"
#include "openzl/shared/varint.h"
#include "openzl/zl_data.h"
#include "openzl/zl_errors.h"

ZL_Report
EI_constant_typed(ZL_Encoder* eictx, const ZL_Input* ins[], size_t nbIns)
{
    ZL_ASSERT_EQ(nbIns, 1);
    ZL_ASSERT_NN(ins);
    const ZL_Input* in = ins[0];
    ZL_ASSERT_NN(eictx);
    ZL_ASSERT_NN(in);
    ZL_ASSERT(
            ZL_Input_type(in) == ZL_Type_serial
            || ZL_Input_type(in) == ZL_Type_struct);

    const uint8_t* const src = ZL_Input_ptr(in);
    size_t const nbElts      = ZL_Input_numElts(in);
    size_t const eltWidth    = ZL_Input_eltWidth(in);
    ZL_RET_R_IF_LT(srcSize_tooSmall, nbElts, 1);
    ZL_ASSERT_GE(eltWidth, 1);
    ZL_RET_R_IF_EQ(
            node_invalid_input, ZS_isConstantStream(src, nbElts, eltWidth), 0);

    ZL_Output* const out = ZL_Encoder_createTypedStream(eictx, 0, 1, eltWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    uint8_t* const outPtr = (uint8_t*)ZL_Output_ptr(out);
    ZL_ASSERT_NN(outPtr);

    uint8_t header[ZL_VARINT_LENGTH_64];
    size_t const varintSize = ZL_varintEncode((uint64_t)nbElts, header);
    ZL_Encoder_sendCodecHeader(eictx, header, varintSize);

    ZS_encodeConstant(outPtr, src, eltWidth);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, 1));
    return ZL_returnSuccess();
}

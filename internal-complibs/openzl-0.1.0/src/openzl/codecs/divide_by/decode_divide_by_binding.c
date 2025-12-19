// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "decode_divide_by_binding.h"
#include "decode_divide_by_kernel.h"
#include "openzl/common/assertion.h"
#include "openzl/shared/varint.h" // ZL_varintDecode64Strict

#include <limits.h> // UCHAR_MAX, USHRT_MAX, UINT_MAX, ULLONG_MAX

ZL_Report DI_divide_by_int(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_NN(ins);
    const ZL_Input* const in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_numeric);
    size_t const intWidth = ZL_Input_eltWidth(in);
    ZL_ASSERT(intWidth == 1 || intWidth == 2 || intWidth == 4 || intWidth == 8);
    size_t const nbElts     = ZL_Input_numElts(in);
    ZL_RBuffer const header = ZL_Decoder_getCodecHeader(dictx);

    void const* quotients = ZL_Input_ptr(in);
    if (header.size == 0) {
        ZL_RET_R_ERR(corruption);
    }

    ZL_Output* const out = ZL_Decoder_create1OutStream(dictx, nbElts, intWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    const uint8_t* headerPtr = (const uint8_t*)header.start;
    ZL_RESULT_OF(uint64_t)
    const varint = ZL_varintDecode(
            &headerPtr, (const uint8_t*)header.start + header.size);
    ZL_TRY_LET_T(uint64_t, divisor, varint);
    ZL_RET_R_IF_EQ(node_invalid_input, divisor, 0, "Attempt to divide by 0");
    switch (intWidth) {
        case 1:
            ZL_RET_R_IF_GT(node_invalid_input, divisor, UCHAR_MAX);
            for (size_t i = 0; i < nbElts; ++i) {
                ZL_RET_R_IF_GT(
                        node_invalid_input,
                        ((const uint8_t*)quotients)[i],
                        UCHAR_MAX / divisor);
            }
            break;
        case 2:
            ZL_RET_R_IF_GT(node_invalid_input, divisor, USHRT_MAX);
            for (size_t i = 0; i < nbElts; ++i) {
                ZL_RET_R_IF_GT(
                        node_invalid_input,
                        ((const uint16_t*)quotients)[i],
                        USHRT_MAX / divisor);
            }
            break;
        case 4:
            ZL_RET_R_IF_GT(node_invalid_input, divisor, UINT_MAX);
            for (size_t i = 0; i < nbElts; ++i) {
                ZL_RET_R_IF_GT(
                        node_invalid_input,
                        ((const uint32_t*)quotients)[i],
                        UINT_MAX / divisor);
            }
            break;
        case 8:
            for (size_t i = 0; i < nbElts; ++i) {
                ZL_RET_R_IF_GT(
                        node_invalid_input,
                        ((const uint64_t*)quotients)[i],
                        ULLONG_MAX / divisor);
            }
            break;
        default:
            ZL_ASSERT_FAIL("Unsupported int width");
    }
    ZS_divideByDecode(ZL_Output_ptr(out), quotients, nbElts, divisor, intWidth);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbElts));
    return ZL_returnSuccess();
}

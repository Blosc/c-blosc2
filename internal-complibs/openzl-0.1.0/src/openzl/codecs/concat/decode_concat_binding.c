// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/concat/decode_concat_binding.h"

#include "openzl/common/assertion.h"
#include "openzl/decompress/dictx.h" // DI_outStream_asReference
#include "openzl/shared/mem.h"       // ZL_memcpy
#include "openzl/zl_data.h"
#include "openzl/zl_errors.h"

ZL_Report DI_concat(
        ZL_Decoder* dictx,
        const ZL_Input* compulsorySrcs[],
        size_t nbCompulsorySrcs,
        const ZL_Input* variableSrcs[],
        size_t nbVariableSrcs)
{
    ZL_ASSERT_EQ(nbVariableSrcs, 0);
    (void)variableSrcs;
    ZL_ASSERT_EQ(nbCompulsorySrcs, 2);
    ZL_ASSERT_NN(compulsorySrcs);
    const ZL_Input* const sizes = compulsorySrcs[0];
    ZL_ASSERT_NN(sizes);
    ZL_RET_R_IF_NE(corruption, ZL_Input_type(sizes), ZL_Type_numeric);
    ZL_RET_R_IF_NE(corruption, ZL_Input_eltWidth(sizes), sizeof(uint32_t));
    const ZL_Input* const concatenated = compulsorySrcs[1];
    ZL_ASSERT_NN(concatenated);

    ZL_Type const type    = ZL_Input_type(concatenated);
    size_t const eltWidth = ZL_Input_eltWidth(concatenated);
    size_t const nbElts   = ZL_Input_numElts(concatenated);
    const size_t nbRegens = ZL_Input_numElts(sizes);
    ZL_RET_R_IF_EQ(corruption, nbRegens, 0);
    const uint32_t* const regenSizes = ZL_Input_ptr(sizes);

    size_t rPos = 0;
    ZL_RET_R_IF_LT(corruption, nbRegens, dictx->nbRegens);

    if (type == ZL_Type_string) {
        const uint32_t* strLens = ZL_Input_stringLens(concatenated);
        size_t bytePos          = 0;
        for (size_t n = 0; n < nbRegens; n++) {
            size_t const rSize = regenSizes[n];
            ZL_RET_R_IF_GT(corruption, rPos + rSize, nbElts);
            size_t byteSize = 0;
            for (size_t i = rPos; i < rPos + rSize; i++) {
                byteSize += strLens[i];
            }
            ZL_Output* out = DI_outStream_asReference(
                    dictx, (int)n, concatenated, bytePos, 1, byteSize);
            ZL_RET_R_IF_NULL(allocation, out);
            uint32_t* regenStrLens = ZL_Output_reserveStringLens(out, rSize);
            /* TODO(T220688634): This can be avoided if we have API to reference
             * string lengths*/
            ZL_memcpy(regenStrLens, strLens + rPos, rSize * sizeof(uint32_t));
            ZL_RET_R_IF_ERR(ZL_Output_commit(out, rSize));
            rPos += rSize;
            bytePos += byteSize;
        }
        ZL_RET_R_IF_NE(corruption, rPos, nbElts);
    } else {
        for (size_t n = 0; n < nbRegens; n++) {
            size_t const rSize = regenSizes[n];
            ZL_RET_R_IF_GT(
                    corruption, rPos + rSize * eltWidth, nbElts * eltWidth);
            ZL_Output* out = DI_outStream_asReference(
                    dictx, (int)n, concatenated, rPos, eltWidth, rSize);
            ZL_RET_R_IF_NULL(allocation, out);
            rPos += rSize * eltWidth;
        }
        ZL_RET_R_IF_NE(corruption, rPos, nbElts * eltWidth);
    }

    return ZL_returnSuccess();
}

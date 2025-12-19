// Copyright (c) Meta Platforms, Inc. and affiliates.
#include "openzl/codecs/merge_sorted/decode_merge_sorted_binding.h"

#include "openzl/codecs/merge_sorted/decode_merge_sorted_kernel.h"
#include "openzl/shared/overflow.h"
#include "openzl/shared/varint.h"
#include "openzl/zl_errors.h"

static ZL_Report fillDstPtrsFromHeader(
        ZL_Decoder* dictx,
        uint32_t* dsts[64],
        uint32_t* dstEnds[64],
        size_t bitsetWidth,
        size_t nbElts)
{
    uint64_t maxDstSize = 0;
    ZL_RET_R_IF(
            corruption,
            ZL_overflowMulU64(bitsetWidth * 8, nbElts, &maxDstSize),
            "Multiplication overflowed");

    ZL_RBuffer const header = ZL_Decoder_getCodecHeader(dictx);
    uint8_t const* const hs = (uint8_t const*)header.start;
    uint8_t const* const he = hs + header.size;
    uint8_t const* hp       = hs;
    uint64_t dstSize        = 0;
    while (hp != he) {
        ZL_TRY_LET_T(uint64_t, size, ZL_varintDecode(&hp, he));
        ZL_RET_R_IF(
                corruption,
                ZL_overflowAddU64(dstSize, size, &dstSize),
                "Addition overflowed");
    }
    ZL_RET_R_IF_GT(
            corruption, dstSize, maxDstSize, "dstSize bigger than possible!");

    ZL_Output* dst = ZL_Decoder_create1OutStream(dictx, dstSize, 4);
    ZL_RET_R_IF_NULL(allocation, dst);
    uint32_t* dstPtr = ZL_Output_ptr(dst);
    ZL_RET_R_IF_ERR(ZL_Output_commit(dst, dstSize));
    hp            = hs;
    size_t nbDsts = 0;
    while (hp != he) {
        ZL_RET_R_IF_EQ(corruption, nbDsts, 64);
        ZL_RESULT_OF(uint64_t) size = ZL_varintDecode(&hp, he);
        ZL_ASSERT(!ZL_RES_isError(size));
        dsts[nbDsts] = dstPtr;
        dstPtr += ZL_RES_value(size);
        dstEnds[nbDsts] = dstPtr;
        ++nbDsts;
    }

    ZL_RET_R_IF_GT(
            corruption,
            nbDsts,
            bitsetWidth * 8,
            "Too many dsts for the width of the bitset");

    return ZL_returnValue(nbDsts);
}

ZL_Report DI_mergeSorted(ZL_Decoder* dictx, ZL_Input const* ins[])
{
    ZL_Input const* bitset = ins[0];
    ZL_Input const* merged = ins[1];

    ZL_RET_R_IF_NE(
            corruption, ZL_Input_numElts(merged), ZL_Input_numElts(bitset));
    ZL_RET_R_IF_NE(corruption, ZL_Input_eltWidth(merged), 4);
    size_t const bitsetWidth = ZL_Input_eltWidth(bitset);

    uint32_t* dsts[64];
    uint32_t* dstEnds[64];
    ZL_Report const nbDsts = fillDstPtrsFromHeader(
            dictx, dsts, dstEnds, bitsetWidth, ZL_Input_numElts(bitset));
    ZL_RET_R_IF_ERR(nbDsts);

    bool success;
    switch (bitsetWidth) {
        case 1:
            success = ZL_MergeSorted_split8x32(
                    dsts,
                    dstEnds,
                    ZL_RES_value(nbDsts),
                    (uint8_t const*)ZL_Input_ptr(bitset),
                    (uint32_t const*)ZL_Input_ptr(merged),
                    ZL_Input_numElts(merged));
            break;
        case 2:
            success = ZL_MergeSorted_split16x32(
                    dsts,
                    dstEnds,
                    ZL_RES_value(nbDsts),
                    (uint16_t const*)ZL_Input_ptr(bitset),
                    (uint32_t const*)ZL_Input_ptr(merged),
                    ZL_Input_numElts(merged));
            break;
        case 4:
            success = ZL_MergeSorted_split32x32(
                    dsts,
                    dstEnds,
                    ZL_RES_value(nbDsts),
                    (uint32_t const*)ZL_Input_ptr(bitset),
                    (uint32_t const*)ZL_Input_ptr(merged),
                    ZL_Input_numElts(merged));
            break;
        case 8:
            success = ZL_MergeSorted_split64x32(
                    dsts,
                    dstEnds,
                    ZL_RES_value(nbDsts),
                    (uint64_t const*)ZL_Input_ptr(bitset),
                    (uint32_t const*)ZL_Input_ptr(merged),
                    ZL_Input_numElts(merged));
            break;
        default:
            ZL_RET_R_ERR(corruption, "Bad bitset width!");
    }
    ZL_RET_R_IF_NOT(corruption, success);

    return ZL_returnSuccess();
}

// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "openzl/codecs/transpose/decode_transpose_binding.h"
#include "openzl/codecs/transpose/decode_transpose_kernel.h"
#include "openzl/common/assertion.h"
#include "openzl/decompress/dictx.h" // ZL_Decoder_getScratchSpace
#include "openzl/zl_data.h"

// DI_transpose design notes:
// - Mirror of EI_transpose
// - Accepts and generates a single stream of type ZL_Type_struct
// - An N x W input stream becomes a W x N output stream
//
ZL_Report DI_transpose(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_NN(ins);
    const ZL_Input* const in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_struct);
    size_t const nbFields   = ZL_Input_numElts(in);
    size_t const fieldWidth = ZL_Input_eltWidth(in);
    ZL_ASSERT_GT(fieldWidth, 0);
    size_t const newNbFields   = nbFields ? fieldWidth : 0;
    size_t const newFieldWidth = nbFields ? nbFields : fieldWidth;
    ZL_Output* const out =
            ZL_Decoder_create1OutStream(dictx, newNbFields, newFieldWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    // TODO(@Cyan) : optimize with a reference when newFieldWidth==1, or
    // nbFields<=1
    ZS_transposeDecode(
            ZL_Output_ptr(out), ZL_Input_ptr(in), newNbFields, newFieldWidth);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, newNbFields));
    return ZL_returnValue(1);
}

ZL_Report DI_transpose_split(
        ZL_Decoder* dictx,
        const ZL_Input* inFixed[],
        size_t nbInFixed,
        const ZL_Input* inVOs[],
        size_t nbInVOs)
{
    (void)inFixed;
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_EQ(nbInFixed, 0);
    ZL_RET_R_IF_EQ(corruption, nbInVOs, 0);
    ZL_ASSERT_NN(inVOs);

    size_t const nbElts      = ZL_Input_numElts(inVOs[0]);
    size_t const dstNbElts   = nbElts;
    size_t const dstEltWidth = nbInVOs;

    for (size_t i = 0; i < nbInVOs; ++i) {
        ZL_RET_R_IF_NE(corruption, ZL_Type_serial, ZL_Input_type(inVOs[i]));
        ZL_RET_R_IF_NE(corruption, nbElts, ZL_Input_numElts(inVOs[i]));
    }

    ZL_Output* const out =
            ZL_Decoder_create1OutStream(dictx, dstNbElts, dstEltWidth);
    ZL_RET_R_IF_NULL(allocation, out);

    const uint8_t** const inPtrs =
            ZL_Decoder_getScratchSpace(dictx, nbInVOs * sizeof(uint8_t*));
    ZL_RET_R_IF_NULL(allocation, inPtrs);

    for (size_t i = 0; i < nbInVOs; i++) {
        inPtrs[i] = (const uint8_t*)ZL_Input_ptr(inVOs[i]);
    }

    ZS_splitTransposeDecode(ZL_Output_ptr(out), inPtrs, dstNbElts, dstEltWidth);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, dstNbElts));
    return ZL_returnSuccess();
}

/* =========================================================
 * Legacy transpose transforms operating on serial streams
 * using the typedTransform model
 * =========================================================
 */

static ZL_Report
DI_transposeN_typed(ZL_Decoder* dictx, const ZL_Input* ins[], size_t fieldSize)
{
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_NN(ins);
    const ZL_Input* const in = ins[0];
    ZL_ASSERT_NN(in);
    ZL_ASSERT_EQ(ZL_Input_type(in), ZL_Type_serial);
    ZL_ASSERT_EQ(ZL_Input_eltWidth(in), 1);
    size_t const srcSize = ZL_Input_numElts(in);
    ZL_RET_R_IF_NE(GENERIC, srcSize % fieldSize, 0);
    size_t const dstCapacity = srcSize;
    ZL_Output* const out = ZL_Decoder_create1OutStream(dictx, dstCapacity, 1);
    ZL_RET_R_IF_NULL(allocation, out);
    ZS_transposeDecode(
            ZL_Output_ptr(out),
            ZL_Input_ptr(in),
            srcSize / fieldSize,
            fieldSize);
    ZL_RET_R_IF_ERR(ZL_Output_commit(out, srcSize));
    return ZL_returnValue(1);
}

// ZL_TypedEncoderFn
ZL_Report DI_transpose2_typed(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    return DI_transposeN_typed(dictx, ins, 2);
}

ZL_Report DI_transpose4_typed(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    return DI_transposeN_typed(dictx, ins, 4);
}

ZL_Report DI_transpose8_typed(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    return DI_transposeN_typed(dictx, ins, 8);
}

// Split transposes, supports up to width 8 (TODO: consider using generics
// instead?)
static ZL_Report DI_transpose_split_bytes(
        ZL_Decoder* dictx,
        const ZL_Input* ins[],
        size_t eltWidth)
{
    ZL_ASSERT_NN(dictx);
    ZL_ASSERT_NN(ins);
    ZL_ASSERT_NN(ins[0]);
    ZL_ASSERT_GT(eltWidth, 1);
    ZL_ASSERT_LE(eltWidth, 8);
    size_t const nbElts = ZL_Input_numElts(ins[0]);
    uint8_t const* src[8];
    for (size_t i = 0; i < eltWidth; ++i) {
        ZL_ASSERT_NN(ins[i]);
        ZL_RET_R_IF_NE(
                corruption,
                nbElts,
                ZL_Input_numElts(ins[i]),
                "Not all streams the same size");
        src[i] = ZL_Input_ptr(ins[i]);
    }
    ZL_Output* const out = ZL_Decoder_create1OutStream(dictx, nbElts, eltWidth);
    ZL_RET_R_IF_NULL(allocation, out);
    uint8_t* dst = (uint8_t*)ZL_Output_ptr(out);
    ZS_splitTransposeDecode(dst, src, nbElts, eltWidth);

    ZL_RET_R_IF_ERR(ZL_Output_commit(out, nbElts));
    return ZL_returnValue(1);
}

ZL_Report DI_transposesplit2_bytes(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    return DI_transpose_split_bytes(dictx, ins, 2);
}

ZL_Report DI_transposesplit4_bytes(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    return DI_transpose_split_bytes(dictx, ins, 4);
}

ZL_Report DI_transposesplit8_bytes(ZL_Decoder* dictx, const ZL_Input* ins[])
{
    return DI_transpose_split_bytes(dictx, ins, 8);
}

/* ===============================================
 * Legacy Encoder Interfaces for Delta transforms
 * using the pipeTransform model
 * (no longer used)
 * ===============================================
 */

// ZL_PipeEncoderFn
size_t
DI_transpose_2(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    ZL_ASSERT_EQ(srcSize % 2, 0);       // clean multiple of uint32_t
    ZL_ASSERT_GE(dstCapacity, srcSize); // large enough capacity
    // Note : most of above conditions could become compile-time checks with
    // ZS_ENABLE_ENSURE
    ZS_transposeDecode(dst, src, srcSize / 2, 2);
    return srcSize;
}

// ZL_PipeEncoderFn
size_t
DI_transpose_4(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    ZL_ASSERT_EQ(srcSize % 4, 0);       // clean multiple of uint32_t
    ZL_ASSERT_GE(dstCapacity, srcSize); // large enough capacity
    // Note : most of above conditions could become compile-time checks with
    // ZS_ENABLE_ENSURE
    ZS_transposeDecode(dst, src, srcSize / 4, 4);
    return srcSize;
}

// ZL_PipeEncoderFn
size_t
DI_transpose_8(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    ZL_ASSERT_EQ(srcSize % 8, 0);       // clean multiple of uint32_t
    ZL_ASSERT_GE(dstCapacity, srcSize); // large enough capacity
    ZS_transposeDecode(dst, src, srcSize / 8, 8);
    return srcSize;
}

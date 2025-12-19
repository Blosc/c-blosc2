// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef ZSTRONG_TRANSFORMS_DECODER_REGISTRY_H
#define ZSTRONG_TRANSFORMS_DECODER_REGISTRY_H

#include "openzl/common/wire_format.h"     // ZL_StandardTransformID_end
#include "openzl/decompress/dtransforms.h" // DTransform, DTrDesc
#include "openzl/shared/portability.h"

ZL_BEGIN_C_DECLS

typedef struct {
    DTransform dtr;
    unsigned minFormatVersion;
    unsigned maxFormatVersion;
} StandardDTransform;

extern const StandardDTransform SDecoders_array[ZL_StandardTransformID_end];

ZL_END_C_DECLS

#endif

// Copyright (c) Meta Platforms, Inc. and affiliates.

// Design Note :
// We have multiple public headers, specialized by objects or structures,
// such as buffer, or stream.
// This design is preferred for clarity.
// If it's considered better to feature less header files,
// it would be possible to regroup multiple of them into
// some kind of generic zs2_public_types.h header.

// Note: this header is only kept for compatibility with existing user code
// which do not yet employ the new terminology (stream => data)

#include "openzl/zl_data.h"

// Note: in the future, this redirection will be removed,
// so use the zs2_data.h only from now on.

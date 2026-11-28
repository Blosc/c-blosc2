/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/** @file b2nd_exec_kernel.h
 * @brief Optional Blosc2 companion header for wrapping prepared MiniExpr kernels.
 *
 * Installed only when C-Blosc2 is built with MiniExpr support.
 */

#ifndef BLOSC_B2ND_EXEC_KERNEL_H
#define BLOSC_B2ND_EXEC_KERNEL_H

#include "b2nd_exec.h"
#include "miniexpr_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wrap an already-prepared MiniExpr me_kernel into an immutable b2nd_plan_t.
 *
 * When take_ownership is true, success transfers ownership of *kernel to the
 * plan and clears *kernel to NULL; failure leaves *kernel intact. When
 * take_ownership is false, the caller must keep *kernel alive for as long as
 * the returned plan (and any derived plans or schedules) remain alive.
 */
BLOSC_EXPORT b2nd_exec_status_t b2nd_plan_from_kernel(
    me_kernel **kernel,
    bool take_ownership,
    const b2nd_exec_reduce_opts_t *reduce_opts,
    b2nd_plan_t **out,
    b2nd_exec_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_B2ND_EXEC_KERNEL_H */

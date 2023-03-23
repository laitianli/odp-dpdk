/* Copyright (c) 2023, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_RING_MPMC_U64_INTERNAL_H_
#define ODP_RING_MPMC_U64_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_ring_common.h>

#undef _ODP_RING_TYPE
#define _ODP_RING_TYPE _ODP_RING_TYPE_U64

#include <odp_ring_mpmc_internal.h>

#ifdef __cplusplus
}
#endif

#endif
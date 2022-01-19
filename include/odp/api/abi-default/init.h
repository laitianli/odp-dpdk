/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP initialization.
 */

#ifndef ODP_ABI_INIT_H_
#define ODP_ABI_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/api/std_types.h>

typedef uint64_t odp_instance_t;

/**
 * @internal platform specific data
 */
typedef struct odp_platform_init_t {
    char dummy; /**< @internal Dummy */
} odp_platform_init_t;

#ifdef __cplusplus
}
#endif

#endif

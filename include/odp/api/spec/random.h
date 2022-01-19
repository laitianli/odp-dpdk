/* Copyright (c) 2015-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP random number API
 */

#ifndef ODP_API_SPEC_RANDOM_H_
#define ODP_API_SPEC_RANDOM_H_
#include <odp/visibility_begin.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup odp_random ODP RANDOM
 *  @{
 */

/**
 * Random kind selector
 *
 * The kind of random denotes the statistical quality of the random data
 * returned. Basic random simply appears uniformly distributed, Cryptographic
 * random is statistically random and suitable for use by cryptographic
 * functions. True random is generated from a hardware entropy source rather
 * than an algorithm and is thus completely unpredictable. These form a
 * hierarchy where higher quality data is presumably more costly to generate
 * than lower quality data.
 */
typedef enum {
    /** Basic random, presumably pseudo-random generated by SW. This
     *  is the lowest kind of random */
    ODP_RANDOM_BASIC,
    /** Cryptographic quality random */
    ODP_RANDOM_CRYPTO,
    /** True random, generated from a HW entropy source. This is the
     *  highest kind of random */
    ODP_RANDOM_TRUE,
} odp_random_kind_t;

/**
 * Query random max kind
 *
 * Implementations support the returned max kind and all kinds weaker than it.
 *
 * @return kind The maximum odp_random_kind_t supported by this implementation
 */
odp_random_kind_t odp_random_max_kind(void);

/**
 * Generate random byte data
 *
 * The intent in supporting different kinds of random data is to allow
 * tradeoffs between performance and the quality of random data needed. The
 * assumption is that basic random is cheap while true random is relatively
 * expensive in terms of time to generate, with cryptographic random being
 * something in between. Implementations that support highly efficient true
 * random are free to use this for all requested kinds. So it is always
 * permissible to "upgrade" a random data request, but never to "downgrade"
 * such requests.
 *
 * @param[out]    buf   Output buffer
 * @param         len   Length of output buffer in bytes
 * @param         kind  Specifies the type of random data required. Request
 *                      is expected to fail if the implementation is unable to
 *                      provide the requested type.
 *
 * @return Number of bytes written
 * @retval <0 on failure
 */
int32_t odp_random_data(uint8_t *buf, uint32_t len, odp_random_kind_t kind);

/**
 * Generate repeatable random data for testing purposes
 *
 * For testing purposes it is often useful to generate "random" sequences that
 * are repeatable. This is accomplished by supplying a seed value that is used
 * for pseudo-random data generation. The caller-provided seed value is
 * updated for each call to continue the sequence. Restarting a series of
 * calls with the same initial seed value will generate the same sequence of
 * random test data.
 *
 * This function returns data of ODP_RANDOM_BASIC quality and should be used
 * only for testing purposes. Use odp_random_data() for production.
 *
 * @param[out]    buf  Output buffer
 * @param         len  Length of output buffer in bytes
 * @param[in,out] seed Seed value to use. This must be a thread-local
 *                     variable. Results are undefined if multiple threads
 *                     call this routine with the same seed variable.
 *
 * @return Number of bytes written
 * @retval <0 on failure
 */
int32_t odp_random_test_data(uint8_t *buf, uint32_t len, uint64_t *seed);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <odp/visibility_end.h>
#endif

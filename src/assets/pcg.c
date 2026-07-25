/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/*
 * The PCG-XSH-RR transition, output permutation, seeding sequence, and bounded
 * rejection method are derived from PCG C (https://github.com/imneme/pcg-c).
 *
 * Copyright (c) 2014-2017 Melissa O'Neill and PCG Project contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Owns canonical PCG-XSH-RR state transitions. State is caller-owned; seed,
 * stream, and draw order are cross-platform determinism contracts.
 */

#include "../../include/aforc/assets.h"

static uint32_t aforc_rng_step(AFORC_Rng *rng)
{
    uint64_t old_state = rng->state;
    uint32_t shifted;
    uint32_t rotation;

    rng->state = old_state * UINT64_C(6364136223846793005) + rng->increment;
    shifted = (uint32_t)(((old_state >> 18u) ^ old_state) >> 27u);
    rotation = (uint32_t)(old_state >> 59u);
    return (shifted >> rotation) |
           (shifted << ((UINT32_C(0) - rotation) & UINT32_C(31)));
}

AFORC_Status aforc_rng_seed(AFORC_Rng *rng, uint64_t seed, uint64_t stream)
{
    if (rng == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    /* PCG requires two warm-up steps; changing this order changes every sequence. */
    rng->state = UINT64_C(0);
    rng->increment = (stream << 1u) | UINT64_C(1);
    (void)aforc_rng_step(rng);
    rng->state += seed;
    (void)aforc_rng_step(rng);
    return AFORC_OK;
}

AFORC_Status aforc_rng_next_u32(AFORC_Rng *rng, uint32_t *output)
{
    if (rng == NULL || output == NULL || (rng->increment & UINT64_C(1)) == 0u) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *output = aforc_rng_step(rng);
    return AFORC_OK;
}

AFORC_Status aforc_rng_bounded_u32(
    AFORC_Rng *rng,
    uint32_t exclusive_bound,
    uint32_t *output)
{
    uint32_t threshold;

    if (rng == NULL || output == NULL || exclusive_bound == 0u ||
        (rng->increment & UINT64_C(1)) == 0u) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    /* Rejection avoids modulo bias while consuming the canonical PCG sequence. */
    threshold = (UINT32_C(0) - exclusive_bound) % exclusive_bound;
    for (;;) {
        uint32_t value = aforc_rng_step(rng);

        if (value >= threshold) {
            *output = value % exclusive_bound;
            return AFORC_OK;
        }
    }
}

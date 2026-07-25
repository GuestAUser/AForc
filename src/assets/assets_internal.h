/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ASSETS_INTERNAL_H
#define AFORC_ASSETS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

static inline bool aforc_assets_growth_capacity(
    size_t current,
    size_t required,
    size_t maximum,
    size_t initial,
    size_t *output)
{
    size_t next;

    if (output == NULL || current > required || required > maximum) {
        return false;
    }
    next = current == 0u ? initial : current;
    if (next > maximum) {
        next = maximum;
    }
    while (next < required) {
        size_t increment = (next / 2u) + 1u;
        size_t remaining = maximum - next;

        if (increment > remaining) {
            increment = remaining;
        }
        next += increment;
    }
    *output = next;
    return true;
}

#endif

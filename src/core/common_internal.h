/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_CORE_COMMON_INTERNAL_H
#define AFORC_CORE_COMMON_INTERNAL_H

#include "aforc/common.h"

#if defined(__GNUC__) || defined(__clang__)
#define AFORC_INTERNAL __attribute__((visibility("hidden")))
#else
#define AFORC_INTERNAL
#endif

static inline bool aforc_allocator_is_valid(const AFORC_Allocator *allocator)
{
    return allocator != NULL && allocator->allocate != NULL &&
           allocator->reallocate != NULL && allocator->deallocate != NULL;
}

#endif

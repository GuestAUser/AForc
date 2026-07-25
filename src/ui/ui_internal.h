/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_UI_INTERNAL_H
#define AFORC_UI_INTERNAL_H

#include "aforc/common.h"

#include <limits.h>

static inline bool aforc_ui_rect_valid(AFORC_Rect rect) {
    const int64_t right = (int64_t)rect.x + rect.width;
    const int64_t bottom = (int64_t)rect.y + rect.height;

    return rect.width >= 0 && rect.height >= 0 &&
           right <= (int64_t)INT32_MAX + 1 &&
           bottom <= (int64_t)INT32_MAX + 1;
}

#endif

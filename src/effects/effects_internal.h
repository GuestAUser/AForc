/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EFFECTS_INTERNAL_H
#define AFORC_EFFECTS_INTERNAL_H

#include "aforc/common.h"

static inline bool aforc_effect_clip_valid(AFORC_Rect clip) {
    return clip.width >= 0 && clip.height >= 0;
}

#endif

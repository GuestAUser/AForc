/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_UI_INTERNAL_H
#define AFORC_UI_INTERNAL_H

#include "aforc/ui.h"

#include <limits.h>

static inline bool aforc_ui_rect_valid(AFORC_Rect rect) {
    const int64_t right = (int64_t)rect.x + rect.width;
    const int64_t bottom = (int64_t)rect.y + rect.height;

    return rect.width >= 0 && rect.height >= 0 &&
           right <= (int64_t)INT32_MAX + 1 &&
           bottom <= (int64_t)INT32_MAX + 1;
}

static inline bool aforc_ui_event_valid(AFORC_UIEvent event) {
    return event == AFORC_UI_EVENT_NONE ||
           event == AFORC_UI_EVENT_FOCUS_NEXT ||
           event == AFORC_UI_EVENT_FOCUS_PREVIOUS ||
           event == AFORC_UI_EVENT_NAV_UP || event == AFORC_UI_EVENT_NAV_DOWN ||
           event == AFORC_UI_EVENT_NAV_HOME || event == AFORC_UI_EVENT_NAV_END ||
           event == AFORC_UI_EVENT_ACTIVATE;
}

static inline void aforc_ui_action_clear(AFORC_UIAction *action) {
    action->type = AFORC_UI_ACTION_NONE;
    action->index = AFORC_UI_NO_INDEX;
    action->id = 0U;
}

static inline bool aforc_ui_text_ascii(const char *text, size_t length) {
    if (text == NULL) {
        return length == 0U;
    }
    for (size_t index = 0U; index < length; ++index) {
        const unsigned char byte = (unsigned char)text[index];

        if (byte < 0x20U || byte > 0x7eU) {
            return false;
        }
    }
    return true;
}

#endif

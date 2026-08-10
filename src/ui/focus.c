/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ui_internal.h"

AFORC_Status aforc_ui_focus_init(AFORC_UIFocusState *state,
                                 size_t count,
                                 size_t initial_index,
                                 bool wrap)
{
    if (state == NULL || (count != 0U && initial_index >= count))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    state->index = count == 0U ? AFORC_UI_NO_INDEX : initial_index;
    state->count = count;
    state->wrap = wrap;
    return AFORC_OK;
}

AFORC_Status aforc_ui_focus_handle(AFORC_UIFocusState *state,
                                   AFORC_UIEvent event,
                                   AFORC_UIAction *out_action)
{
    size_t next;

    if (state == NULL || out_action == NULL || !aforc_ui_event_valid(event))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if ((state->count == 0U && state->index != AFORC_UI_NO_INDEX) ||
        (state->count != 0U && state->index >= state->count))
    {
        return AFORC_ERROR_STATE;
    }
    aforc_ui_action_clear(out_action);
    if (state->count == 0U)
    {
        return AFORC_OK;
    }
    next = state->index;
    if (event == AFORC_UI_EVENT_FOCUS_NEXT)
    {
        if (next + 1U < state->count)
        {
            ++next;
        }
        else if (state->wrap)
        {
            next = 0U;
        }
    }
    else if (event == AFORC_UI_EVENT_FOCUS_PREVIOUS)
    {
        if (next > 0U)
        {
            --next;
        }
        else if (state->wrap)
        {
            next = state->count - 1U;
        }
    }
    if (next != state->index)
    {
        state->index = next;
        out_action->type = AFORC_UI_ACTION_FOCUS_CHANGED;
        out_action->index = next;
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_button_handle(uint32_t id,
                                    bool focused,
                                    bool enabled,
                                    AFORC_UIEvent event,
                                    AFORC_UIAction *out_action)
{
    if (out_action == NULL || !aforc_ui_event_valid(event))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    aforc_ui_action_clear(out_action);
    if (focused && enabled && event == AFORC_UI_EVENT_ACTIVATE)
    {
        out_action->type = AFORC_UI_ACTION_ACTIVATED;
        out_action->id = id;
    }
    return AFORC_OK;
}

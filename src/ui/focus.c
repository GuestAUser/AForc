/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/ui.h"

/*
 * Deterministic focus and menu navigation state machines.
 *
 * Disabled menu entries are never stable selections. Handlers emit at most
 * one edge-triggered action per event and keep scroll state synchronized with
 * selection without owning item storage or invoking rendering callbacks.
 */

static bool event_valid(AFORC_UIEvent event) {
    return event == AFORC_UI_EVENT_NONE ||
           event == AFORC_UI_EVENT_FOCUS_NEXT ||
           event == AFORC_UI_EVENT_FOCUS_PREVIOUS ||
           event == AFORC_UI_EVENT_NAV_UP || event == AFORC_UI_EVENT_NAV_DOWN ||
           event == AFORC_UI_EVENT_NAV_HOME || event == AFORC_UI_EVENT_NAV_END ||
           event == AFORC_UI_EVENT_ACTIVATE;
}

static void action_clear(AFORC_UIAction *action) {
    /* Actions are edge-triggered: every accepted event begins as no
       transition, including focus moves blocked at a boundary. */
    action->type = AFORC_UI_ACTION_NONE;
    action->index = AFORC_UI_NO_INDEX;
    action->id = 0U;
}

static size_t first_enabled(const AFORC_UIMenuItem *items,
                            size_t item_count) {
    for (size_t index = 0U; index < item_count; ++index) {
        if (items[index].enabled) {
            return index;
        }
    }
    return AFORC_UI_NO_INDEX;
}

static size_t last_enabled(const AFORC_UIMenuItem *items,
                           size_t item_count) {
    for (size_t index = item_count; index > 0U; --index) {
        if (items[index - 1U].enabled) {
            return index - 1U;
        }
    }
    return AFORC_UI_NO_INDEX;
}

static size_t enabled_from(const AFORC_UIMenuItem *items,
                           size_t item_count,
                           size_t start) {
    size_t index = start;

    for (size_t visited = 0U; visited < item_count; ++visited) {
        if (items[index].enabled) {
            return index;
        }
        ++index;
        if (index == item_count) {
            index = 0U;
        }
    }
    return AFORC_UI_NO_INDEX;
}

static size_t next_enabled(const AFORC_UIMenuItem *items,
                           size_t item_count,
                           size_t current,
                           bool wrap,
                           bool forward) {
    size_t index = current;

    /* Visit each alternative once; the current item is never re-selected. */
    for (size_t visited = 1U; visited < item_count; ++visited) {
        if (forward) {
            if (index + 1U == item_count) {
                if (!wrap) {
                    return AFORC_UI_NO_INDEX;
                }
                index = 0U;
            } else {
                ++index;
            }
        } else if (index == 0U) {
            if (!wrap) {
                return AFORC_UI_NO_INDEX;
            }
            index = item_count - 1U;
        } else {
            --index;
        }
        if (items[index].enabled) {
            return index;
        }
    }
    return AFORC_UI_NO_INDEX;
}

static void scroll_to_selection(AFORC_UIMenuState *state,
                                size_t item_count,
                                size_t visible_rows) {
    if (state->selected == AFORC_UI_NO_INDEX) {
        state->scroll = 0U;
        return;
    }
    if (visible_rows == 0U) {
        state->scroll = state->selected;
    } else if (state->selected < state->scroll) {
        state->scroll = state->selected;
    } else if (state->selected - state->scroll >= visible_rows) {
        state->scroll = state->selected - visible_rows + 1U;
    }
    if (state->scroll > item_count) {
        state->scroll = item_count;
    }
}

AFORC_Status aforc_ui_focus_init(AFORC_UIFocusState *state,
                             size_t count,
                             size_t initial_index,
                             bool wrap) {
    if (state == NULL || (count != 0U && initial_index >= count)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    state->index = count == 0U ? AFORC_UI_NO_INDEX : initial_index;
    state->count = count;
    state->wrap = wrap;
    return AFORC_OK;
}

AFORC_Status aforc_ui_focus_handle(AFORC_UIFocusState *state,
                               AFORC_UIEvent event,
                               AFORC_UIAction *out_action) {
    size_t next;

    if (state == NULL || out_action == NULL || !event_valid(event)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if ((state->count == 0U && state->index != AFORC_UI_NO_INDEX) ||
        (state->count != 0U && state->index >= state->count)) {
        return AFORC_ERROR_STATE;
    }
    action_clear(out_action);
    if (state->count == 0U) {
        return AFORC_OK;
    }
    next = state->index;
    if (event == AFORC_UI_EVENT_FOCUS_NEXT) {
        if (next + 1U < state->count) {
            ++next;
        } else if (state->wrap) {
            next = 0U;
        }
    } else if (event == AFORC_UI_EVENT_FOCUS_PREVIOUS) {
        if (next > 0U) {
            --next;
        } else if (state->wrap) {
            next = state->count - 1U;
        }
    }
    if (next != state->index) {
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
                                AFORC_UIAction *out_action) {
    if (out_action == NULL || !event_valid(event)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    action_clear(out_action);
    if (focused && enabled && event == AFORC_UI_EVENT_ACTIVATE) {
        out_action->type = AFORC_UI_ACTION_ACTIVATED;
        out_action->id = id;
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_menu_init(AFORC_UIMenuState *state,
                            const AFORC_UIMenuItem *items,
                            size_t item_count,
                            size_t initial_index,
                            bool wrap) {
    size_t start;

    if (state == NULL || (items == NULL && item_count != 0U)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (item_count == 0U) {
        state->selected = AFORC_UI_NO_INDEX;
        state->scroll = 0U;
        state->wrap = wrap;
        return AFORC_OK;
    }
    if (initial_index != AFORC_UI_NO_INDEX && initial_index >= item_count) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    start = initial_index == AFORC_UI_NO_INDEX ? 0U : initial_index;
    state->selected = enabled_from(items, item_count, start);
    state->scroll = 0U;
    state->wrap = wrap;
    return AFORC_OK;
}

AFORC_Status aforc_ui_menu_handle(AFORC_UIMenuState *state,
                              const AFORC_UIMenuItem *items,
                              size_t item_count,
                              size_t visible_rows,
                              bool focused,
                              AFORC_UIEvent event,
                              AFORC_UIAction *out_action) {
    size_t target;

    if (state == NULL || out_action == NULL || !event_valid(event) ||
        (items == NULL && item_count != 0U)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if ((state->selected != AFORC_UI_NO_INDEX &&
         state->selected >= item_count) ||
        state->scroll > item_count) {
        return AFORC_ERROR_STATE;
    }
    action_clear(out_action);
    if (item_count == 0U) {
        state->selected = AFORC_UI_NO_INDEX;
        state->scroll = 0U;
        return AFORC_OK;
    }
    /* Selection is always NO_INDEX or enabled; navigation never exposes a
       disabled item as a stable state. */
    if (state->selected != AFORC_UI_NO_INDEX &&
        !items[state->selected].enabled) {
        state->selected = first_enabled(items, item_count);
    }
    scroll_to_selection(state, item_count, visible_rows);
    if (!focused) {
        return AFORC_OK;
    }

    target = state->selected;
    if (event == AFORC_UI_EVENT_NAV_HOME) {
        target = first_enabled(items, item_count);
    } else if (event == AFORC_UI_EVENT_NAV_END) {
        target = last_enabled(items, item_count);
    } else if (event == AFORC_UI_EVENT_NAV_DOWN) {
        target = state->selected == AFORC_UI_NO_INDEX
                     ? first_enabled(items, item_count)
                     : next_enabled(items, item_count, state->selected,
                                    state->wrap, true);
        if (target == AFORC_UI_NO_INDEX) {
            target = state->selected;
        }
    } else if (event == AFORC_UI_EVENT_NAV_UP) {
        target = state->selected == AFORC_UI_NO_INDEX
                     ? last_enabled(items, item_count)
                     : next_enabled(items, item_count, state->selected,
                                    state->wrap, false);
        if (target == AFORC_UI_NO_INDEX) {
            target = state->selected;
        }
    } else if (event == AFORC_UI_EVENT_ACTIVATE &&
               state->selected != AFORC_UI_NO_INDEX) {
        out_action->type = AFORC_UI_ACTION_ACTIVATED;
        out_action->index = state->selected;
        out_action->id = items[state->selected].id;
        return AFORC_OK;
    }
    if (target != state->selected) {
        state->selected = target;
        scroll_to_selection(state, item_count, visible_rows);
        out_action->type = AFORC_UI_ACTION_SELECTION_CHANGED;
        out_action->index = target;
        if (target != AFORC_UI_NO_INDEX) {
            out_action->id = items[target].id;
        }
    }
    return AFORC_OK;
}

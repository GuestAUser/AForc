/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "input_internal.h"

#include <limits.h>

/*
 * Keyboard/mouse state transitions and synthetic release leases.
 *
 * Modern protocols can report explicit release events; legacy terminals only
 * repeat key-down bytes. One state component reconciles both models and emits
 * identical held/pressed/released snapshots to the public facade.
 */

static uint64_t aforc_input_internal_add_timeout(
    uint64_t now_ms,
    uint32_t timeout_ms
)
{
    if (now_ms > UINT64_MAX - (uint64_t)timeout_ms) {
        return UINT64_MAX;
    }
    return now_ms + (uint64_t)timeout_ms;
}

static int aforc_input_internal_timeout_until(
    uint64_t deadline,
    uint64_t now_ms
)
{
    uint64_t difference = 0u;

    if (deadline <= now_ms) {
        return 0;
    }
    difference = deadline - now_ms;
    return difference > (uint64_t)INT_MAX ? INT_MAX : (int)difference;
}

bool aforc_input_internal_key_valid(AFORC_Key key)
{
    return key > AFORC_KEY_NONE && (unsigned int)key < AFORC_KEY_COUNT;
}

bool aforc_input_internal_mouse_button_valid(AFORC_MouseButton button)
{
    return button > AFORC_MOUSE_NONE && button < AFORC_MOUSE_BUTTON_COUNT;
}

void aforc_input_internal_emit_text(
    AFORC_Input *input,
    uint32_t codepoint,
    uint64_t timestamp_ms
)
{
    AFORC_InputEvent event = aforc_input_internal_event(
        AFORC_INPUT_EVENT_TEXT,
        timestamp_ms
    );

    event.data.text.codepoint = codepoint;
    (void)aforc_input_internal_queue_event(input, &event);
}

void aforc_input_internal_emit_key_down(
    AFORC_Input *input,
    AFORC_Key key,
    uint32_t codepoint,
    AFORC_Modifiers modifiers,
    bool protocol_repeat,
    bool explicit_release,
    bool emit_text,
    uint64_t timestamp_ms
)
{
    bool repeat = protocol_repeat;

    if (aforc_input_internal_key_valid(key)) {
        AFORC_KeyState *state = &input->keys[(size_t)key];
        AFORC_InputEvent event = aforc_input_internal_event(
            AFORC_INPUT_EVENT_KEY_DOWN,
            timestamp_ms
        );

        repeat = repeat || state->held;
        if (!state->held) {
            state->pressed = true;
        }
        state->held = true;
        state->explicit_release = explicit_release;
        state->expires_at_ms = explicit_release
                                   ? 0U
                                   : aforc_input_internal_add_timeout(
                                         timestamp_ms,
                                         input->key_release_timeout_ms
                                     );
        event.data.key.key = key;
        event.data.key.codepoint = codepoint;
        event.data.key.modifiers = modifiers;
        event.data.key.repeat = repeat;
        (void)aforc_input_internal_queue_event(input, &event);
    }
    if (emit_text && codepoint != 0u &&
        (modifiers & (AFORC_MOD_ALT | AFORC_MOD_CTRL | AFORC_MOD_SUPER)) ==
            0u) {
        aforc_input_internal_emit_text(input, codepoint, timestamp_ms);
    }
}

void aforc_input_internal_emit_key_up(
    AFORC_Input *input,
    AFORC_Key key,
    uint32_t codepoint,
    AFORC_Modifiers modifiers,
    uint64_t timestamp_ms
)
{
    AFORC_KeyState *state = NULL;
    AFORC_InputEvent event;

    if (!aforc_input_internal_key_valid(key)) {
        return;
    }
    state = &input->keys[(size_t)key];
    state->held = false;
    state->released = true;
    state->explicit_release = false;
    state->expires_at_ms = 0u;
    event = aforc_input_internal_event(AFORC_INPUT_EVENT_KEY_UP, timestamp_ms);
    event.data.key.key = key;
    event.data.key.codepoint = codepoint;
    event.data.key.modifiers = modifiers;
    event.data.key.repeat = false;
    (void)aforc_input_internal_queue_event(input, &event);
}

void aforc_input_internal_release_expired(
    AFORC_Input *input,
    uint64_t timestamp_ms
)
{
    size_t index = 1u;

    /* Synthetic releases keep held state finite on terminals without key-up. */
    for (index = 1u; index < AFORC_KEY_COUNT; ++index) {
        if (input->keys[index].held && !input->keys[index].explicit_release &&
            input->keys[index].expires_at_ms <= timestamp_ms) {
            aforc_input_internal_emit_key_up(
                input,
                (AFORC_Key)index,
                0u,
                AFORC_MOD_NONE,
                timestamp_ms
            );
        }
    }
}

void aforc_input_internal_emit_mouse_button(
    AFORC_Input *input,
    AFORC_MouseButton button,
    bool down,
    AFORC_Modifiers modifiers,
    uint64_t timestamp_ms
)
{
    AFORC_MouseState *state = NULL;
    AFORC_InputEvent event;

    if (!aforc_input_internal_mouse_button_valid(button)) {
        return;
    }
    state = &input->mouse[(size_t)button];
    if (down) {
        if (!state->held) {
            state->pressed = true;
        }
        state->held = true;
        event = aforc_input_internal_event(
            AFORC_INPUT_EVENT_MOUSE_DOWN,
            timestamp_ms
        );
    } else {
        state->held = false;
        state->released = true;
        event = aforc_input_internal_event(
            AFORC_INPUT_EVENT_MOUSE_UP,
            timestamp_ms
        );
    }
    event.data.mouse.position.x = input->mouse_x;
    event.data.mouse.position.y = input->mouse_y;
    event.data.mouse.button = button;
    event.data.mouse.modifiers = modifiers;
    (void)aforc_input_internal_queue_event(input, &event);
}

void aforc_input_internal_emit_mouse_move(
    AFORC_Input *input,
    AFORC_MouseButton button,
    AFORC_Modifiers modifiers,
    uint64_t timestamp_ms
)
{
    AFORC_InputEvent event = aforc_input_internal_event(
        AFORC_INPUT_EVENT_MOUSE_MOVE,
        timestamp_ms
    );

    event.data.mouse.position.x = input->mouse_x;
    event.data.mouse.position.y = input->mouse_y;
    event.data.mouse.button = button;
    event.data.mouse.modifiers = modifiers;
    (void)aforc_input_internal_queue_event(input, &event);
}

void aforc_input_internal_release_mouse_buttons(
    AFORC_Input *input,
    AFORC_Modifiers modifiers,
    uint64_t timestamp_ms
)
{
    size_t index = 1u;

    for (index = 1u; index < AFORC_MOUSE_BUTTON_COUNT; ++index) {
        if (input->mouse[index].held) {
            aforc_input_internal_emit_mouse_button(
                input,
                (AFORC_MouseButton)index,
                false,
                modifiers,
                timestamp_ms
            );
        }
    }
}

void aforc_input_internal_release_all(
    AFORC_Input *input,
    uint64_t timestamp_ms
)
{
    size_t index = 1u;

    for (index = 1u; index < AFORC_KEY_COUNT; ++index) {
        if (input->keys[index].held && !input->keys[index].explicit_release) {
            aforc_input_internal_emit_key_up(
                input,
                (AFORC_Key)index,
                0u,
                AFORC_MOD_NONE,
                timestamp_ms
            );
        }
    }
    aforc_input_internal_release_mouse_buttons(
        input,
        AFORC_MOD_NONE,
        timestamp_ms
    );
}

int aforc_input_internal_effective_timeout(
    const AFORC_Input *input,
    int timeout_ms,
    uint64_t now_ms
)
{
    int effective = timeout_ms;
    size_t index = 0u;

    if (input->escape_pending) {
        const uint64_t deadline = aforc_input_internal_add_timeout(
            input->escape_started_ms,
            input->escape_timeout_ms
        );
        const int remaining = aforc_input_internal_timeout_until(
            deadline,
            now_ms
        );

        if (effective < 0 || remaining < effective) {
            effective = remaining;
        }
    }
    for (index = 1u; index < AFORC_KEY_COUNT; ++index) {
        if (input->keys[index].held && !input->keys[index].explicit_release) {
            const int remaining = aforc_input_internal_timeout_until(
                input->keys[index].expires_at_ms,
                now_ms
            );

            if (effective < 0 || remaining < effective) {
                effective = remaining;
            }
        }
    }
    return effective;
}

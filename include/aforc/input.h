/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_INPUT_H
#define AFORC_INPUT_H

#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AFORC_Input AFORC_Input;

typedef enum AFORC_Key {
    AFORC_KEY_NONE = 0,
    AFORC_KEY_BACKSPACE = 8,
    AFORC_KEY_TAB = 9,
    AFORC_KEY_ENTER = 13,
    AFORC_KEY_ESCAPE = 27,
    AFORC_KEY_SPACE = 32,
    AFORC_KEY_0 = '0',
    AFORC_KEY_1 = '1',
    AFORC_KEY_2 = '2',
    AFORC_KEY_3 = '3',
    AFORC_KEY_4 = '4',
    AFORC_KEY_5 = '5',
    AFORC_KEY_6 = '6',
    AFORC_KEY_7 = '7',
    AFORC_KEY_8 = '8',
    AFORC_KEY_9 = '9',
    AFORC_KEY_A = 'A',
    AFORC_KEY_B = 'B',
    AFORC_KEY_C = 'C',
    AFORC_KEY_D = 'D',
    AFORC_KEY_E = 'E',
    AFORC_KEY_F = 'F',
    AFORC_KEY_G = 'G',
    AFORC_KEY_H = 'H',
    AFORC_KEY_I = 'I',
    AFORC_KEY_J = 'J',
    AFORC_KEY_K = 'K',
    AFORC_KEY_L = 'L',
    AFORC_KEY_M = 'M',
    AFORC_KEY_N = 'N',
    AFORC_KEY_O = 'O',
    AFORC_KEY_P = 'P',
    AFORC_KEY_Q = 'Q',
    AFORC_KEY_R = 'R',
    AFORC_KEY_S = 'S',
    AFORC_KEY_T = 'T',
    AFORC_KEY_U = 'U',
    AFORC_KEY_V = 'V',
    AFORC_KEY_W = 'W',
    AFORC_KEY_X = 'X',
    AFORC_KEY_Y = 'Y',
    AFORC_KEY_Z = 'Z',
    AFORC_KEY_INSERT = 256,
    AFORC_KEY_DELETE,
    AFORC_KEY_HOME,
    AFORC_KEY_END,
    AFORC_KEY_PAGE_UP,
    AFORC_KEY_PAGE_DOWN,
    AFORC_KEY_UP,
    AFORC_KEY_DOWN,
    AFORC_KEY_LEFT,
    AFORC_KEY_RIGHT,
    AFORC_KEY_F1,
    AFORC_KEY_F2,
    AFORC_KEY_F3,
    AFORC_KEY_F4,
    AFORC_KEY_F5,
    AFORC_KEY_F6,
    AFORC_KEY_F7,
    AFORC_KEY_F8,
    AFORC_KEY_F9,
    AFORC_KEY_F10,
    AFORC_KEY_F11,
    AFORC_KEY_F12,
    AFORC_KEY_COUNT = 320
} AFORC_Key;

/* Printable ASCII keys use their uppercase codepoint value even when no named
 * enumerator is listed. Unicode text outside ASCII is reported as TEXT events
 * with AFORC_KEY_NONE rather than extending this key-state table. */

typedef uint16_t AFORC_Modifiers;

enum {
    AFORC_MOD_NONE = 0,
    AFORC_MOD_SHIFT = 1u << 0,
    AFORC_MOD_ALT = 1u << 1,
    AFORC_MOD_CTRL = 1u << 2,
    AFORC_MOD_SUPER = 1u << 3
};

typedef enum AFORC_MouseButton {
    AFORC_MOUSE_NONE = 0,
    AFORC_MOUSE_LEFT,
    AFORC_MOUSE_MIDDLE,
    AFORC_MOUSE_RIGHT,
    AFORC_MOUSE_BUTTON_4,
    AFORC_MOUSE_BUTTON_5,
    AFORC_MOUSE_BUTTON_COUNT
} AFORC_MouseButton;

typedef enum AFORC_InputEventType {
    AFORC_INPUT_EVENT_NONE = 0,
    AFORC_INPUT_EVENT_KEY_DOWN,
    AFORC_INPUT_EVENT_KEY_UP,
    AFORC_INPUT_EVENT_TEXT,
    AFORC_INPUT_EVENT_MOUSE_MOVE,
    AFORC_INPUT_EVENT_MOUSE_DOWN,
    AFORC_INPUT_EVENT_MOUSE_UP,
    AFORC_INPUT_EVENT_MOUSE_WHEEL,
    AFORC_INPUT_EVENT_RESIZE,
    AFORC_INPUT_EVENT_FOCUS_IN,
    AFORC_INPUT_EVENT_FOCUS_OUT,
    AFORC_INPUT_EVENT_PASTE_BEGIN,
    AFORC_INPUT_EVENT_PASTE_END
} AFORC_InputEventType;

typedef struct AFORC_InputEvent {
    AFORC_InputEventType type;
    uint64_t timestamp_ms;
    union {
        struct {
            AFORC_Key key;
            uint32_t codepoint;
            AFORC_Modifiers modifiers;
            bool repeat;
        } key;
        struct {
            uint32_t codepoint;
        } text;
        struct {
            AFORC_Point position;
            AFORC_MouseButton button;
            AFORC_Modifiers modifiers;
        } mouse;
        struct {
            AFORC_Point delta;
        } wheel;
        struct {
            AFORC_Size size;
        } resize;
    } data;
} AFORC_InputEvent;

typedef struct AFORC_InputConfig {
    size_t event_capacity;
    size_t byte_capacity;
    uint32_t key_release_timeout_ms;
    uint32_t escape_timeout_ms;
    AFORC_Allocator allocator;
} AFORC_InputConfig;

/* Input owns fixed-capacity byte and event buffers allocated at create time.
 * The copied allocator and its context must outlive the input object. Accepted
 * events are FIFO. When full, the newest event is dropped, dropped_events is
 * incremented (saturating), state still advances, and the producing operation
 * returns AFORC_ERROR_LIMIT. No allocations occur while parsing. */

AFORC_API AFORC_InputConfig aforc_input_config_default(void);
AFORC_API AFORC_Status aforc_input_create(
    AFORC_Input **out_input,
    const AFORC_InputConfig *config
);
AFORC_API void aforc_input_destroy(AFORC_Input *input);
/* Clears pressed/released edges, resolves elapsed parser/key deadlines against
 * CLOCK_MONOTONIC, and preserves unread events plus held state. */
AFORC_API AFORC_Status aforc_input_begin_frame(AFORC_Input *input);
/* Poll borrows an active terminal for this call and bounds its wait by pending
 * escape and synthetic key-release deadlines. */
AFORC_API AFORC_Status aforc_input_poll(
    AFORC_Input *input,
    AFORC_Terminal *terminal,
    int timeout_ms
);
/* Feed is the deterministic decoder entry point. Timestamps across feed,
 * flush, and release_all calls on one input must be nondecreasing milliseconds
 * from one clock domain. Partial UTF-8/control sequences remain buffered. */
AFORC_API AFORC_Status aforc_input_feed(
    AFORC_Input *input,
    const unsigned char *bytes,
    size_t size,
    uint64_t timestamp_ms
);
/* Resolves only deadlines elapsed at timestamp_ms; it does not force an
 * unexpired partial sequence. Explicit protocol releases are never synthesized. */
AFORC_API AFORC_Status aforc_input_flush(
    AFORC_Input *input,
    uint64_t timestamp_ms
);
AFORC_API bool aforc_input_next_event(
    AFORC_Input *input,
    AFORC_InputEvent *out_event
);
AFORC_API size_t aforc_input_event_count(const AFORC_Input *input);
AFORC_API uint64_t aforc_input_dropped_events(const AFORC_Input *input);
AFORC_API bool aforc_input_key_held(const AFORC_Input *input, AFORC_Key key);
AFORC_API bool aforc_input_key_pressed(const AFORC_Input *input, AFORC_Key key);
AFORC_API bool aforc_input_key_released(const AFORC_Input *input, AFORC_Key key);
AFORC_API bool aforc_input_mouse_held(
    const AFORC_Input *input,
    AFORC_MouseButton button
);
AFORC_API bool aforc_input_mouse_pressed(
    const AFORC_Input *input,
    AFORC_MouseButton button
);
AFORC_API bool aforc_input_mouse_released(
    const AFORC_Input *input,
    AFORC_MouseButton button
);
AFORC_API AFORC_Point aforc_input_mouse_position(const AFORC_Input *input);
AFORC_API void aforc_input_release_all(
    AFORC_Input *input,
    uint64_t timestamp_ms
);

#ifdef __cplusplus
}
#endif

#endif

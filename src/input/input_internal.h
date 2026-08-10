/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_INPUT_INTERNAL_H
#define AFORC_INPUT_INTERNAL_H

#include "../core/common_internal.h"
#include "aforc/input.h"

typedef struct AFORC_KeyState
{
    bool held;
    bool pressed;
    bool released;
    bool explicit_release;
    uint64_t expires_at_ms;
} AFORC_KeyState;

typedef struct AFORC_MouseState
{
    bool held;
    bool pressed;
    bool released;
} AFORC_MouseState;

struct AFORC_Input
{
    AFORC_Allocator allocator;

    /* Live events occupy event_count slots from event_head, modulo capacity. */
    AFORC_InputEvent *events;
    size_t event_capacity;
    size_t event_head;
    size_t event_count;
    uint64_t dropped_events;

    /* Unconsumed bytes are always packed into bytes[0..byte_count). */
    unsigned char *bytes;
    size_t byte_capacity;
    size_t byte_count;

    uint32_t key_release_timeout_ms;
    uint32_t escape_timeout_ms;
    uint64_t escape_started_ms;
    bool escape_pending;
    bool paste_mode;
    bool queue_overflowed;
    AFORC_KeyState keys[AFORC_KEY_COUNT];
    AFORC_MouseState mouse[AFORC_MOUSE_BUTTON_COUNT];
    int32_t mouse_x;
    int32_t mouse_y;
};

typedef enum AFORC_ParseResult
{
    AFORC_PARSE_COMPLETE = 0,
    AFORC_PARSE_INCOMPLETE
} AFORC_ParseResult;

typedef enum AFORC_Utf8Result
{
    AFORC_UTF8_COMPLETE = 0,
    AFORC_UTF8_INCOMPLETE,
    AFORC_UTF8_INVALID
} AFORC_Utf8Result;

AFORC_INTERNAL bool
aforc_input_internal_queue_event(AFORC_Input *input,
                                 const AFORC_InputEvent *event);
AFORC_INTERNAL AFORC_InputEvent
aforc_input_internal_event(AFORC_InputEventType type, uint64_t timestamp_ms);

AFORC_INTERNAL bool aforc_input_internal_key_valid(AFORC_Key key);
AFORC_INTERNAL bool
aforc_input_internal_mouse_button_valid(AFORC_MouseButton button);
AFORC_INTERNAL void aforc_input_internal_emit_text(AFORC_Input *input,
                                                   uint32_t codepoint,
                                                   uint64_t timestamp_ms);
AFORC_INTERNAL void
aforc_input_internal_emit_key_down(AFORC_Input *input,
                                   AFORC_Key key,
                                   uint32_t codepoint,
                                   AFORC_Modifiers modifiers,
                                   bool protocol_repeat,
                                   bool explicit_release,
                                   bool emit_text,
                                   uint64_t timestamp_ms);
AFORC_INTERNAL void aforc_input_internal_emit_key_up(AFORC_Input *input,
                                                     AFORC_Key key,
                                                     uint32_t codepoint,
                                                     AFORC_Modifiers modifiers,
                                                     uint64_t timestamp_ms);
AFORC_INTERNAL void aforc_input_internal_release_expired(AFORC_Input *input,
                                                         uint64_t timestamp_ms);
AFORC_INTERNAL void
aforc_input_internal_emit_mouse_button(AFORC_Input *input,
                                       AFORC_MouseButton button,
                                       bool down,
                                       AFORC_Modifiers modifiers,
                                       uint64_t timestamp_ms);
AFORC_INTERNAL void
aforc_input_internal_emit_mouse_move(AFORC_Input *input,
                                     AFORC_MouseButton button,
                                     AFORC_Modifiers modifiers,
                                     uint64_t timestamp_ms);
AFORC_INTERNAL void aforc_input_internal_release_mouse_buttons(
    AFORC_Input *input, AFORC_Modifiers modifiers, uint64_t timestamp_ms);
AFORC_INTERNAL void aforc_input_internal_release_all(AFORC_Input *input,
                                                     uint64_t timestamp_ms);
AFORC_INTERNAL int aforc_input_internal_effective_timeout(
    const AFORC_Input *input, int timeout_ms, uint64_t now_ms);

AFORC_INTERNAL AFORC_Utf8Result
aforc_input_internal_decode_utf8(const unsigned char *bytes,
                                 size_t size,
                                 uint32_t *out_codepoint,
                                 size_t *out_consumed);
AFORC_INTERNAL AFORC_ParseResult
aforc_input_internal_parse_plain(AFORC_Input *input,
                                 const unsigned char *bytes,
                                 size_t size,
                                 AFORC_Modifiers modifiers,
                                 bool force_incomplete,
                                 uint64_t timestamp_ms,
                                 size_t *out_consumed);
AFORC_INTERNAL void aforc_input_internal_handle_mouse(AFORC_Input *input,
                                                      uint32_t code,
                                                      uint32_t column,
                                                      uint32_t row,
                                                      bool release,
                                                      bool legacy,
                                                      uint64_t timestamp_ms);
AFORC_INTERNAL void
aforc_input_internal_handle_csi(AFORC_Input *input,
                                const unsigned char *payload,
                                size_t payload_size,
                                unsigned char final_byte,
                                uint64_t timestamp_ms);
AFORC_INTERNAL void aforc_input_internal_handle_ss3(AFORC_Input *input,
                                                    unsigned char final_byte,
                                                    uint64_t timestamp_ms);

AFORC_INTERNAL void aforc_input_internal_parse_available(AFORC_Input *input,
                                                         uint64_t timestamp_ms,
                                                         bool force_all);
AFORC_INTERNAL AFORC_Status
aforc_input_internal_feed(AFORC_Input *input,
                          const unsigned char *bytes,
                          size_t size,
                          uint64_t timestamp_ms);

#endif

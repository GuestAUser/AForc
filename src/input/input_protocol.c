/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "input_internal.h"

#include <limits.h>

/*
 * Stateless terminal-protocol decoding and normalized event emission.
 *
 * This component validates UTF-8, CSI/SS3, Kitty keyboard, focus, and mouse
 * payloads before mutating shared input state. Malformed sequences are either
 * ignored as protocol data or recover one byte at a time as replacement text.
 */

AFORC_Utf8Result aforc_input_internal_decode_utf8(const unsigned char *bytes,
                                                  size_t size,
                                                  uint32_t *out_codepoint,
                                                  size_t *out_consumed)
{
    uint32_t codepoint = 0u;
    size_t needed = 0u;
    uint32_t minimum = 0u;
    size_t index = 0u;

    if (size == 0u)
    {
        return AFORC_UTF8_INCOMPLETE;
    }
    if (bytes[0] <= 0x7fu)
    {
        *out_codepoint = (uint32_t)bytes[0];
        *out_consumed = 1u;
        return AFORC_UTF8_COMPLETE;
    }
    if (bytes[0] >= 0xc2u && bytes[0] <= 0xdfu)
    {
        needed = 2u;
        codepoint = (uint32_t)(bytes[0] & 0x1fu);
        minimum = 0x80u;
    }
    else if (bytes[0] >= 0xe0u && bytes[0] <= 0xefu)
    {
        needed = 3u;
        codepoint = (uint32_t)(bytes[0] & 0x0fu);
        minimum = 0x800u;
    }
    else if (bytes[0] >= 0xf0u && bytes[0] <= 0xf4u)
    {
        needed = 4u;
        codepoint = (uint32_t)(bytes[0] & 0x07u);
        minimum = 0x10000u;
    }
    else
    {
        *out_consumed = 1u;
        return AFORC_UTF8_INVALID;
    }
    if (size < needed)
    {
        return AFORC_UTF8_INCOMPLETE;
    }
    for (index = 1u; index < needed; ++index)
    {
        if ((bytes[index] & 0xc0u) != 0x80u)
        {
            *out_consumed = 1u;
            return AFORC_UTF8_INVALID;
        }
        codepoint = (codepoint << 6u) | (uint32_t)(bytes[index] & 0x3fu);
    }
    if (codepoint < minimum || codepoint > 0x10ffffu ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu))
    {
        *out_consumed = 1u;
        return AFORC_UTF8_INVALID;
    }
    *out_codepoint = codepoint;
    *out_consumed = needed;
    return AFORC_UTF8_COMPLETE;
}

static AFORC_Key aforc_input_internal_key_from_codepoint(uint32_t codepoint)
{
    if (codepoint >= (uint32_t)'a' && codepoint <= (uint32_t)'z')
    {
        codepoint -= (uint32_t)'a' - (uint32_t)'A';
    }
    if (codepoint >= 0x20u && codepoint <= 0x7eu)
    {
        return (AFORC_Key)codepoint;
    }
    return AFORC_KEY_NONE;
}

static bool aforc_input_internal_codepoint_valid(uint32_t codepoint)
{
    return codepoint <= UINT32_C(0x10ffff) &&
           !(codepoint >= UINT32_C(0xd800) && codepoint <= UINT32_C(0xdfff));
}

static bool aforc_input_internal_text_codepoint_valid(uint32_t codepoint)
{
    return aforc_input_internal_codepoint_valid(codepoint) &&
           codepoint >= UINT32_C(0x20) &&
           !(codepoint >= UINT32_C(0x7f) && codepoint <= UINT32_C(0x9f));
}

static void aforc_input_internal_emit_plain_byte(AFORC_Input *input,
                                                 unsigned char byte,
                                                 AFORC_Modifiers modifiers,
                                                 uint64_t timestamp_ms)
{
    AFORC_Key key = AFORC_KEY_NONE;
    uint32_t codepoint = 0u;
    bool emit_text = false;

    if (byte == (unsigned char)'\r' || byte == (unsigned char)'\n')
    {
        key = AFORC_KEY_ENTER;
    }
    else if (byte == (unsigned char)'\t')
    {
        key = AFORC_KEY_TAB;
    }
    else if (byte == 0x7fu || byte == 0x08u)
    {
        key = AFORC_KEY_BACKSPACE;
    }
    else if (byte == 0x00u)
    {
        key = AFORC_KEY_SPACE;
        modifiers |= AFORC_MOD_CTRL;
    }
    else if (byte >= 0x01u && byte <= 0x1au)
    {
        key = (AFORC_Key)((unsigned int)AFORC_KEY_A + (unsigned int)byte - 1u);
        modifiers |= AFORC_MOD_CTRL;
    }
    else if (byte >= 0x1cu && byte <= 0x1fu)
    {
        static const unsigned char control_keys[4] = {'\\', ']', '^', '_'};
        key = (AFORC_Key)control_keys[byte - 0x1cu];
        modifiers |= AFORC_MOD_CTRL;
    }
    else if (byte >= 0x20u && byte <= 0x7eu)
    {
        codepoint = (uint32_t)byte;
        key = aforc_input_internal_key_from_codepoint(codepoint);
        emit_text = true;
    }
    if (key != AFORC_KEY_NONE)
    {
        aforc_input_internal_emit_key_down(input,
                                           key,
                                           codepoint,
                                           modifiers,
                                           false,
                                           false,
                                           emit_text,
                                           timestamp_ms);
    }
}

AFORC_ParseResult aforc_input_internal_parse_plain(AFORC_Input *input,
                                                   const unsigned char *bytes,
                                                   size_t size,
                                                   AFORC_Modifiers modifiers,
                                                   bool force_incomplete,
                                                   uint64_t timestamp_ms,
                                                   size_t *out_consumed)
{
    uint32_t codepoint = 0u;
    size_t consumed = 0u;
    AFORC_Utf8Result result;

    if (size == 0u)
    {
        return AFORC_PARSE_INCOMPLETE;
    }
    if (bytes[0] <= 0x7fu)
    {
        aforc_input_internal_emit_plain_byte(
            input, bytes[0], modifiers, timestamp_ms);
        *out_consumed = 1u;
        return AFORC_PARSE_COMPLETE;
    }
    result =
        aforc_input_internal_decode_utf8(bytes, size, &codepoint, &consumed);
    if (result == AFORC_UTF8_INCOMPLETE && !force_incomplete)
    {
        return AFORC_PARSE_INCOMPLETE;
    }
    if (result != AFORC_UTF8_COMPLETE)
    {
        /* Consume one bad byte so recovery cannot hide a later valid starter.
         */
        codepoint = 0xfffdu;
        consumed = 1u;
    }
    aforc_input_internal_emit_key_down(input,
                                       AFORC_KEY_NONE,
                                       codepoint,
                                       modifiers,
                                       false,
                                       false,
                                       true,
                                       timestamp_ms);
    *out_consumed = consumed;
    return AFORC_PARSE_COMPLETE;
}

static AFORC_Modifiers aforc_input_internal_decode_modifiers(uint32_t parameter)
{
    uint32_t bits = parameter > 0u ? parameter - 1u : 0u;
    AFORC_Modifiers modifiers = AFORC_MOD_NONE;

    if ((bits & 1u) != 0u)
    {
        modifiers |= AFORC_MOD_SHIFT;
    }
    if ((bits & 2u) != 0u)
    {
        modifiers |= AFORC_MOD_ALT;
    }
    if ((bits & 4u) != 0u)
    {
        modifiers |= AFORC_MOD_CTRL;
    }
    if ((bits & 8u) != 0u)
    {
        modifiers |= AFORC_MOD_SUPER;
    }
    return modifiers;
}

static bool aforc_input_internal_parse_parameters(const unsigned char *bytes,
                                                  size_t size,
                                                  bool skip_mouse_prefix,
                                                  uint32_t *parameters,
                                                  size_t parameter_capacity,
                                                  size_t *out_count)
{
    size_t index = 0u;
    size_t count = 0u;
    uint32_t value = 0u;
    bool have_value = false;
    bool saw_field = false;

    if (skip_mouse_prefix)
    {
        if (size == 0u || bytes[0] != (unsigned char)'<')
        {
            return false;
        }
        index = 1u;
    }
    for (; index < size; ++index)
    {
        const unsigned char byte = bytes[index];

        if (byte >= (unsigned char)'0' && byte <= (unsigned char)'9')
        {
            const uint32_t digit = (uint32_t)(byte - (unsigned char)'0');

            if (value > (UINT32_MAX - digit) / 10u)
            {
                return false;
            }
            value = value * 10u + digit;
            have_value = true;
            saw_field = true;
            continue;
        }
        if (byte == (unsigned char)';' || byte == (unsigned char)':')
        {
            if (count == parameter_capacity)
            {
                return false;
            }
            parameters[count++] = have_value ? value : 0u;
            value = 0u;
            have_value = false;
            saw_field = true;
            continue;
        }
        return false;
    }
    if (have_value || saw_field)
    {
        if (count == parameter_capacity)
        {
            return false;
        }
        parameters[count++] = have_value ? value : 0u;
    }
    *out_count = count;
    return true;
}

static AFORC_Key aforc_input_internal_tilde_key(uint32_t parameter)
{
    switch (parameter)
    {
        case 1u:
        case 7u:
            return AFORC_KEY_HOME;
        case 2u:
            return AFORC_KEY_INSERT;
        case 3u:
            return AFORC_KEY_DELETE;
        case 4u:
        case 8u:
            return AFORC_KEY_END;
        case 5u:
            return AFORC_KEY_PAGE_UP;
        case 6u:
            return AFORC_KEY_PAGE_DOWN;
        case 11u:
            return AFORC_KEY_F1;
        case 12u:
            return AFORC_KEY_F2;
        case 13u:
            return AFORC_KEY_F3;
        case 14u:
            return AFORC_KEY_F4;
        case 15u:
            return AFORC_KEY_F5;
        case 17u:
            return AFORC_KEY_F6;
        case 18u:
            return AFORC_KEY_F7;
        case 19u:
            return AFORC_KEY_F8;
        case 20u:
            return AFORC_KEY_F9;
        case 21u:
            return AFORC_KEY_F10;
        case 23u:
            return AFORC_KEY_F11;
        case 24u:
            return AFORC_KEY_F12;
        default:
            return AFORC_KEY_NONE;
    }
}

static AFORC_Key aforc_input_internal_final_key(unsigned char final_byte)
{
    switch (final_byte)
    {
        case (unsigned char)'A':
            return AFORC_KEY_UP;
        case (unsigned char)'B':
            return AFORC_KEY_DOWN;
        case (unsigned char)'C':
            return AFORC_KEY_RIGHT;
        case (unsigned char)'D':
            return AFORC_KEY_LEFT;
        case (unsigned char)'H':
            return AFORC_KEY_HOME;
        case (unsigned char)'F':
            return AFORC_KEY_END;
        case (unsigned char)'P':
            return AFORC_KEY_F1;
        case (unsigned char)'Q':
            return AFORC_KEY_F2;
        case (unsigned char)'R':
            return AFORC_KEY_F3;
        case (unsigned char)'S':
            return AFORC_KEY_F4;
        default:
            return AFORC_KEY_NONE;
    }
}

void aforc_input_internal_handle_ss3(AFORC_Input *input,
                                     unsigned char final_byte,
                                     uint64_t timestamp_ms)
{
    const AFORC_Key key = aforc_input_internal_final_key(final_byte);

    if (key != AFORC_KEY_NONE)
    {
        aforc_input_internal_emit_key_down(
            input, key, 0u, AFORC_MOD_NONE, false, false, false, timestamp_ms);
    }
}

static void aforc_input_internal_emit_protocol_key(AFORC_Input *input,
                                                   AFORC_Key key,
                                                   uint32_t codepoint,
                                                   AFORC_Modifiers modifiers,
                                                   uint32_t event_type,
                                                   bool explicit_release,
                                                   uint64_t timestamp_ms)
{
    if (event_type == 3u)
    {
        aforc_input_internal_emit_key_up(
            input, key, codepoint, modifiers, timestamp_ms);
        return;
    }
    aforc_input_internal_emit_key_down(input,
                                       key,
                                       codepoint,
                                       modifiers,
                                       event_type == 2u,
                                       explicit_release,
                                       false,
                                       timestamp_ms);
}

enum
{
    AFORC_KITTY_EVENT_TYPES = 1u << 1,
    AFORC_KITTY_ALL_KEYS = 1u << 3,
    AFORC_KITTY_TEXT_CAPACITY = 32
};

typedef struct AFORC_KittyKeyReport
{
    uint32_t codepoint;
    uint32_t modifiers;
    uint32_t event_type;
    uint32_t text[AFORC_KITTY_TEXT_CAPACITY];
    size_t text_count;
    bool event_type_present;
} AFORC_KittyKeyReport;

static bool aforc_input_internal_parse_decimal(const unsigned char *bytes,
                                               size_t size,
                                               size_t *index,
                                               uint32_t *out_value,
                                               bool *out_present)
{
    uint32_t value = 0u;
    bool present = false;

    while (*index < size && bytes[*index] >= (unsigned char)'0' &&
           bytes[*index] <= (unsigned char)'9')
    {
        const uint32_t digit = (uint32_t)(bytes[*index] - (unsigned char)'0');

        if (value > (UINT32_MAX - digit) / 10u)
        {
            return false;
        }
        value = value * 10u + digit;
        present = true;
        ++*index;
    }
    *out_value = value;
    *out_present = present;
    return true;
}

static bool
aforc_input_internal_parse_kitty_report(const unsigned char *payload,
                                        size_t payload_size,
                                        AFORC_KittyKeyReport *report)
{
    size_t index = 0u;
    size_t alternate_count = 0u;
    uint32_t value = 0u;
    bool present = false;

    report->codepoint = 0u;
    report->modifiers = 1u;
    report->event_type = 1u;
    report->text_count = 0u;
    report->event_type_present = false;
    if (!aforc_input_internal_parse_decimal(
            payload, payload_size, &index, &report->codepoint, &present) ||
        !present || !aforc_input_internal_codepoint_valid(report->codepoint))
    {
        return false;
    }
    while (index < payload_size && payload[index] == (unsigned char)':')
    {
        if (alternate_count == 2u)
        {
            return false;
        }
        ++alternate_count;
        ++index;
        if (!aforc_input_internal_parse_decimal(
                payload, payload_size, &index, &value, &present) ||
            (present && !aforc_input_internal_codepoint_valid(value)))
        {
            return false;
        }
    }
    if (index == payload_size)
    {
        return true;
    }
    if (payload[index++] != (unsigned char)';' ||
        !aforc_input_internal_parse_decimal(
            payload, payload_size, &index, &value, &present))
    {
        return false;
    }
    if (present)
    {
        report->modifiers = value;
    }
    if (index < payload_size && payload[index] == (unsigned char)':')
    {
        ++index;
        if (!aforc_input_internal_parse_decimal(
                payload, payload_size, &index, &value, &present))
        {
            return false;
        }
        if (present)
        {
            report->event_type = value;
            report->event_type_present = true;
        }
    }
    if (report->event_type == 0u || report->event_type > 3u)
    {
        return false;
    }
    if (index == payload_size)
    {
        return true;
    }
    if (payload[index++] != (unsigned char)';')
    {
        return false;
    }
    while (index < payload_size)
    {
        if (report->text_count == AFORC_KITTY_TEXT_CAPACITY ||
            !aforc_input_internal_parse_decimal(
                payload, payload_size, &index, &value, &present) ||
            !present || !aforc_input_internal_text_codepoint_valid(value))
        {
            return false;
        }
        report->text[report->text_count++] = value;
        if (index == payload_size)
        {
            break;
        }
        if (payload[index++] != (unsigned char)':')
        {
            return false;
        }
        if (index == payload_size)
        {
            return false;
        }
    }
    return true;
}

static bool aforc_input_internal_parse_keyboard_flags(
    const unsigned char *payload, size_t payload_size, uint32_t *out_flags)
{
    size_t index = 1u;
    bool present = false;

    return payload_size > 1u && payload[0] == (unsigned char)'?' &&
           aforc_input_internal_parse_decimal(
               payload, payload_size, &index, out_flags, &present) &&
           present && index == payload_size;
}

static void
aforc_input_internal_handle_kitty_report(AFORC_Input *input,
                                         const unsigned char *payload,
                                         size_t payload_size,
                                         uint64_t timestamp_ms)
{
    AFORC_KittyKeyReport report;
    AFORC_Key key;
    uint32_t event_codepoint;
    size_t index;

    if (!aforc_input_internal_parse_kitty_report(
            payload, payload_size, &report))
    {
        return;
    }
    key = report.codepoint == 27u   ? AFORC_KEY_ESCAPE
          : report.codepoint == 13u ? AFORC_KEY_ENTER
          : report.codepoint == 9u  ? AFORC_KEY_TAB
          : report.codepoint == 127u
              ? AFORC_KEY_BACKSPACE
              : aforc_input_internal_key_from_codepoint(report.codepoint);
    event_codepoint = report.codepoint >= 0x20u && report.codepoint != 0x7fu
                          ? report.codepoint
                          : 0u;
    if (report.event_type != 3u && report.text_count == 1u)
    {
        event_codepoint = report.text[0];
    }
    aforc_input_internal_emit_protocol_key(
        input,
        key,
        event_codepoint,
        aforc_input_internal_decode_modifiers(report.modifiers),
        report.event_type,
        report.event_type_present ||
            input->key_release_mode == AFORC_INPUT_KEY_RELEASE_EXPLICIT,
        timestamp_ms);
    if (report.event_type == 3u)
    {
        return;
    }
    for (index = 0u; index < report.text_count; ++index)
    {
        aforc_input_internal_emit_text(input, report.text[index], timestamp_ms);
    }
}

static AFORC_MouseButton aforc_input_internal_mouse_button(uint32_t code)
{
    if ((code & 128u) != 0u)
    {
        return (code & 1u) == 0u ? AFORC_MOUSE_BUTTON_4 : AFORC_MOUSE_BUTTON_5;
    }
    switch (code & 3u)
    {
        case 0u:
            return AFORC_MOUSE_LEFT;
        case 1u:
            return AFORC_MOUSE_MIDDLE;
        case 2u:
            return AFORC_MOUSE_RIGHT;
        default:
            return AFORC_MOUSE_NONE;
    }
}

static AFORC_Modifiers aforc_input_internal_mouse_modifiers(uint32_t code)
{
    AFORC_Modifiers modifiers = AFORC_MOD_NONE;

    if ((code & 4u) != 0u)
    {
        modifiers |= AFORC_MOD_SHIFT;
    }
    if ((code & 8u) != 0u)
    {
        modifiers |= AFORC_MOD_ALT;
    }
    if ((code & 16u) != 0u)
    {
        modifiers |= AFORC_MOD_CTRL;
    }
    return modifiers;
}

void aforc_input_internal_handle_mouse(AFORC_Input *input,
                                       uint32_t code,
                                       uint32_t column,
                                       uint32_t row,
                                       bool release,
                                       bool legacy,
                                       uint64_t timestamp_ms)
{
    const AFORC_Modifiers modifiers =
        aforc_input_internal_mouse_modifiers(code);
    const AFORC_MouseButton button = aforc_input_internal_mouse_button(code);
    const bool motion = (code & 32u) != 0u;
    const bool wheel = (code & 64u) != 0u;
    const int32_t previous_x = input->mouse_x;
    const int32_t previous_y = input->mouse_y;

    /* Both SGR and legacy mouse coordinates are one-based terminal cells. */
    if (column == 0u || row == 0u || column - 1u > (uint32_t)INT32_MAX ||
        row - 1u > (uint32_t)INT32_MAX)
    {
        return;
    }
    input->mouse_x = (int32_t)(column - 1u);
    input->mouse_y = (int32_t)(row - 1u);
    if (wheel)
    {
        AFORC_InputEvent event = aforc_input_internal_event(
            AFORC_INPUT_EVENT_MOUSE_WHEEL, timestamp_ms);
        const uint32_t direction = code & 3u;

        event.data.wheel.delta.x = direction == 2u   ? -1
                                   : direction == 3u ? 1
                                                     : 0;
        event.data.wheel.delta.y = direction == 0u   ? 1
                                   : direction == 1u ? -1
                                                     : 0;
        (void)aforc_input_internal_queue_event(input, &event);
        return;
    }
    if (motion || previous_x != input->mouse_x || previous_y != input->mouse_y)
    {
        aforc_input_internal_emit_mouse_move(
            input, button, modifiers, timestamp_ms);
    }
    if ((legacy && (code & 3u) == 3u) ||
        (release && button == AFORC_MOUSE_NONE))
    {
        aforc_input_internal_release_mouse_buttons(
            input, modifiers, timestamp_ms);
    }
    else if (release)
    {
        aforc_input_internal_emit_mouse_button(
            input, button, false, modifiers, timestamp_ms);
    }
    else if (!motion)
    {
        aforc_input_internal_emit_mouse_button(
            input, button, true, modifiers, timestamp_ms);
    }
}

void aforc_input_internal_handle_csi(AFORC_Input *input,
                                     const unsigned char *payload,
                                     size_t payload_size,
                                     unsigned char final_byte,
                                     uint64_t timestamp_ms)
{
    uint32_t parameters[8];
    size_t count = 0u;
    AFORC_Key key = AFORC_KEY_NONE;
    AFORC_Modifiers modifiers = AFORC_MOD_NONE;
    uint32_t event_type = 1u;

    if (final_byte == (unsigned char)'u' && payload_size > 0u)
    {
        uint32_t flags = 0u;

        if (aforc_input_internal_parse_keyboard_flags(
                payload, payload_size, &flags))
        {
            const uint32_t required =
                AFORC_KITTY_EVENT_TYPES | AFORC_KITTY_ALL_KEYS;

            input->key_release_mode = (flags & required) == required
                                          ? AFORC_INPUT_KEY_RELEASE_EXPLICIT
                                          : AFORC_INPUT_KEY_RELEASE_SYNTHETIC;
            return;
        }
        aforc_input_internal_handle_kitty_report(
            input, payload, payload_size, timestamp_ms);
        return;
    }

    if ((final_byte == (unsigned char)'M' ||
         final_byte == (unsigned char)'m') &&
        payload_size > 0u && payload[0] == (unsigned char)'<')
    {
        if (aforc_input_internal_parse_parameters(
                payload, payload_size, true, parameters, 8u, &count) &&
            count == 3u)
        {
            aforc_input_internal_handle_mouse(input,
                                              parameters[0],
                                              parameters[1],
                                              parameters[2],
                                              final_byte == (unsigned char)'m',
                                              false,
                                              timestamp_ms);
        }
        return;
    }
    if (!aforc_input_internal_parse_parameters(
            payload, payload_size, false, parameters, 8u, &count))
    {
        return;
    }
    if (final_byte == (unsigned char)'I' && count == 0u)
    {
        const AFORC_InputEvent event = aforc_input_internal_event(
            AFORC_INPUT_EVENT_FOCUS_IN, timestamp_ms);
        (void)aforc_input_internal_queue_event(input, &event);
        return;
    }
    if (final_byte == (unsigned char)'O' && count == 0u)
    {
        AFORC_InputEvent event = aforc_input_internal_event(
            AFORC_INPUT_EVENT_FOCUS_OUT, timestamp_ms);
        (void)aforc_input_internal_queue_event(input, &event);
        aforc_input_internal_release_all(input, timestamp_ms);
        return;
    }
    if (final_byte == (unsigned char)'Z')
    {
        aforc_input_internal_emit_protocol_key(
            input, AFORC_KEY_TAB, 0u, AFORC_MOD_SHIFT, 1u, false, timestamp_ms);
        return;
    }
    if (final_byte == (unsigned char)'~' && count > 0u)
    {
        if (parameters[0] == 200u)
        {
            const AFORC_InputEvent event = aforc_input_internal_event(
                AFORC_INPUT_EVENT_PASTE_BEGIN, timestamp_ms);
            input->paste_mode = true;
            (void)aforc_input_internal_queue_event(input, &event);
            return;
        }
        if (parameters[0] == 201u)
        {
            const AFORC_InputEvent event = aforc_input_internal_event(
                AFORC_INPUT_EVENT_PASTE_END, timestamp_ms);
            input->paste_mode = false;
            (void)aforc_input_internal_queue_event(input, &event);
            return;
        }
        if (parameters[0] == 27u && count >= 3u)
        {
            const uint32_t codepoint = parameters[2];

            if (!aforc_input_internal_codepoint_valid(codepoint))
            {
                return;
            }

            modifiers = aforc_input_internal_decode_modifiers(parameters[1]);
            key = aforc_input_internal_key_from_codepoint(codepoint);
            aforc_input_internal_emit_protocol_key(
                input, key, codepoint, modifiers, 1u, false, timestamp_ms);
            return;
        }
        key = aforc_input_internal_tilde_key(parameters[0]);
        if (count > 1u)
        {
            modifiers = aforc_input_internal_decode_modifiers(parameters[1]);
        }
        if (count > 2u && parameters[2] != 0u)
        {
            event_type = parameters[2];
        }
    }
    else
    {
        key = aforc_input_internal_final_key(final_byte);
        if (count > 1u)
        {
            modifiers = aforc_input_internal_decode_modifiers(parameters[1]);
        }
        if (count > 2u && parameters[2] != 0u)
        {
            event_type = parameters[2];
        }
    }
    if (event_type > 3u)
    {
        return;
    }
    if (key != AFORC_KEY_NONE)
    {
        aforc_input_internal_emit_protocol_key(
            input,
            key,
            0u,
            modifiers,
            event_type,
            count > 2u ||
                input->key_release_mode == AFORC_INPUT_KEY_RELEASE_EXPLICIT,
            timestamp_ms);
    }
}

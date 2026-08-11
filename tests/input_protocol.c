/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/input.h"

#include <stdio.h>
#include <string.h>

static bool create_input(AFORC_Input **out_input,
                         size_t event_capacity,
                         uint32_t release_timeout_ms)
{
    AFORC_InputConfig config = aforc_input_config_default();

    config.event_capacity = event_capacity;
    config.key_release_timeout_ms = release_timeout_ms;
    return aforc_input_create(out_input, &config) == AFORC_OK;
}

static bool no_events(AFORC_Input *input)
{
    AFORC_InputEvent event;

    return !aforc_input_next_event(input, &event);
}

static bool test_kitty_explicit_release_owns_key_lifetime(void)
{
    static const unsigned char press[] = "\x1b[97;3:1u";
    static const unsigned char release[] = "\x1b[97;1:3u";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed = false;

    if (!create_input(&input, 16U, 10U))
    {
        return false;
    }
    if (aforc_input_feed(input, press, sizeof(press) - 1U, 100U) == AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
        event.data.key.key == AFORC_KEY_A &&
        event.data.key.modifiers == AFORC_MOD_ALT && !event.data.key.repeat &&
        no_events(input) && aforc_input_key_held(input, AFORC_KEY_A) &&
        aforc_input_flush(input, 1000U) == AFORC_OK && no_events(input) &&
        aforc_input_key_held(input, AFORC_KEY_A) &&
        aforc_input_feed(input, release, sizeof(release) - 1U, 1001U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_UP &&
        event.data.key.key == AFORC_KEY_A && no_events(input) &&
        !aforc_input_key_held(input, AFORC_KEY_A) &&
        aforc_input_key_released(input, AFORC_KEY_A))
    {
        passed = true;
    }
    aforc_input_destroy(input);
    return passed;
}

static bool test_release_all_clears_explicit_release_key(void)
{
    static const unsigned char press[] = "\x1b[97;1:1u";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed = false;

    if (!create_input(&input, 8U, 10U))
    {
        return false;
    }
    if (aforc_input_feed(input, press, sizeof(press) - 1U, 100U) == AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
        event.data.key.key == AFORC_KEY_A && no_events(input) &&
        aforc_input_key_held(input, AFORC_KEY_A))
    {
        aforc_input_release_all(input, 101U);
        passed = aforc_input_next_event(input, &event) &&
                 event.type == AFORC_INPUT_EVENT_KEY_UP &&
                 event.data.key.key == AFORC_KEY_A && no_events(input) &&
                 !aforc_input_key_held(input, AFORC_KEY_A) &&
                 aforc_input_key_released(input, AFORC_KEY_A);
    }
    aforc_input_destroy(input);
    return passed;
}

static bool test_kitty_capability_and_text_reporting(void)
{
    static const unsigned char partial_capabilities[] = "\x1b[?1u";
    static const unsigned char full_capabilities[] = "\x1b[?27u";
    static const unsigned char press[] = "\x1b[97;2;65u";
    static const unsigned char repeat[] = "\x1b[97;2:2;65u";
    static const unsigned char release[] = "\x1b[97;1:3u";
    static const unsigned char pure_text[] = "\x1b[0;;229:97u";
    static const unsigned char control_text[] = "\x1b[97;1:1;31u";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed = false;

    if (!create_input(&input, 32U, 10U))
    {
        return false;
    }
    if (aforc_input_key_release_mode(NULL) !=
            AFORC_INPUT_KEY_RELEASE_SYNTHETIC ||
        aforc_input_key_release_mode(input) !=
            AFORC_INPUT_KEY_RELEASE_SYNTHETIC ||
        aforc_input_feed(input,
                         partial_capabilities,
                         sizeof(partial_capabilities) - 1U,
                         1U) != AFORC_OK ||
        aforc_input_key_release_mode(input) !=
            AFORC_INPUT_KEY_RELEASE_SYNTHETIC ||
        !no_events(input) ||
        aforc_input_feed(
            input, full_capabilities, sizeof(full_capabilities) - 1U, 2U) !=
            AFORC_OK ||
        aforc_input_key_release_mode(input) !=
            AFORC_INPUT_KEY_RELEASE_EXPLICIT ||
        !no_events(input))
    {
        aforc_input_destroy(input);
        return false;
    }
    if (aforc_input_feed(input, press, sizeof(press) - 1U, 3U) == AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
        event.data.key.key == AFORC_KEY_A &&
        event.data.key.codepoint == (uint32_t)'A' &&
        event.data.key.modifiers == AFORC_MOD_SHIFT && !event.data.key.repeat &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_TEXT &&
        event.data.text.codepoint == (uint32_t)'A' && no_events(input) &&
        aforc_input_flush(input, 1000U) == AFORC_OK && no_events(input) &&
        aforc_input_key_held(input, AFORC_KEY_A) &&
        aforc_input_feed(input, repeat, sizeof(repeat) - 1U, 1001U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
        event.data.key.key == AFORC_KEY_A &&
        event.data.key.codepoint == (uint32_t)'A' && event.data.key.repeat &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_TEXT &&
        event.data.text.codepoint == (uint32_t)'A' && no_events(input) &&
        aforc_input_feed(input, release, sizeof(release) - 1U, 1002U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_UP &&
        event.data.key.key == AFORC_KEY_A && no_events(input) &&
        !aforc_input_key_held(input, AFORC_KEY_A) &&
        aforc_input_feed(input, pure_text, sizeof(pure_text) - 1U, 1003U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_TEXT &&
        event.data.text.codepoint == UINT32_C(229) &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_TEXT &&
        event.data.text.codepoint == (uint32_t)'a' && no_events(input) &&
        aforc_input_feed(
            input, control_text, sizeof(control_text) - 1U, 1004U) ==
            AFORC_OK &&
        no_events(input))
    {
        passed = true;
    }
    aforc_input_destroy(input);
    return passed;
}

static bool test_kitty_function_key_uses_negotiated_release(void)
{
    static const unsigned char capabilities[] = "\x1b[?27u";
    static const unsigned char press[] = "\x1b[1A";
    static const unsigned char release[] = "\x1b[1;1:3A";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed;

    if (!create_input(&input, 8U, 10U))
    {
        return false;
    }
    passed =
        aforc_input_feed(input, capabilities, sizeof(capabilities) - 1U, 1U) ==
            AFORC_OK &&
        aforc_input_feed(input, press, sizeof(press) - 1U, 2U) == AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
        event.data.key.key == AFORC_KEY_UP && no_events(input) &&
        aforc_input_flush(input, 1000U) == AFORC_OK &&
        aforc_input_key_held(input, AFORC_KEY_UP) && no_events(input) &&
        aforc_input_feed(input, release, sizeof(release) - 1U, 1001U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_UP &&
        event.data.key.key == AFORC_KEY_UP && no_events(input) &&
        !aforc_input_key_held(input, AFORC_KEY_UP);
    aforc_input_destroy(input);
    return passed;
}

static bool test_unsupported_kitty_function_is_not_text(void)
{
    static const unsigned char caps_lock[] = "\x1b[57358;1:1u";
    AFORC_Input *input = NULL;
    bool passed;

    if (!create_input(&input, 8U, 10U))
    {
        return false;
    }
    passed = aforc_input_feed(input, caps_lock, sizeof(caps_lock) - 1U, 50U) ==
                 AFORC_OK &&
             no_events(input);
    aforc_input_destroy(input);
    return passed;
}

static bool test_legacy_alt_key_is_not_text(void)
{
    static const unsigned char alt_a[] = "\x1b"
                                         "a";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed;

    if (!create_input(&input, 8U, 10U))
    {
        return false;
    }
    passed =
        aforc_input_feed(input, alt_a, sizeof(alt_a) - 1U, 50U) == AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
        event.data.key.key == AFORC_KEY_A &&
        event.data.key.modifiers == AFORC_MOD_ALT && no_events(input);
    aforc_input_destroy(input);
    return passed;
}

static bool test_incremental_utf8_and_paste_boundaries(void)
{
    static const unsigned char utf8_first[] = {0xf0U, 0x9fU};
    static const unsigned char utf8_second[] = {0x98U, 0x80U};
    static const unsigned char paste_begin[] = "\x1b[200~";
    static const unsigned char paste_end_first[] = "\x1b[20";
    static const unsigned char paste_end_second[] = "1~";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed = false;

    if (!create_input(&input, 16U, 10U))
    {
        return false;
    }
    if (aforc_input_feed(input, utf8_first, sizeof(utf8_first), 1U) ==
            AFORC_OK &&
        no_events(input) &&
        aforc_input_feed(input, utf8_second, sizeof(utf8_second), 2U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_TEXT &&
        event.data.text.codepoint == UINT32_C(0x1f600) && no_events(input) &&
        aforc_input_feed(input, paste_begin, sizeof(paste_begin) - 1U, 3U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_PASTE_BEGIN && no_events(input) &&
        aforc_input_feed(
            input, paste_end_first, sizeof(paste_end_first) - 1U, 4U) ==
            AFORC_OK &&
        no_events(input) &&
        aforc_input_feed(
            input, paste_end_second, sizeof(paste_end_second) - 1U, 5U) ==
            AFORC_OK &&
        aforc_input_next_event(input, &event) &&
        event.type == AFORC_INPUT_EVENT_PASTE_END && no_events(input))
    {
        passed = true;
    }
    aforc_input_destroy(input);
    return passed;
}

static bool test_bounded_escape_forces_progress(void)
{
    unsigned char bytes[96];
    AFORC_Input *input = NULL;
    AFORC_Status status;
    bool passed;

    if (!create_input(&input, 256U, 10U))
    {
        return false;
    }
    (void)memset(bytes, (unsigned char)'1', sizeof(bytes));
    bytes[0] = 0x1bU;
    bytes[1] = (unsigned char)'[';
    status = aforc_input_feed(input, bytes, sizeof(bytes), 1U);
    passed = status == AFORC_OK && aforc_input_flush(input, 100U) == AFORC_OK &&
             aforc_input_dropped_events(input) == 0U;
    aforc_input_destroy(input);
    return passed;
}

static bool test_queue_overflow_preserves_state_and_order(void)
{
    static const unsigned char bytes[] = "ab";
    AFORC_Input *input = NULL;
    AFORC_InputEvent event;
    bool passed;

    if (!create_input(&input, 1U, 100U))
    {
        return false;
    }
    passed = aforc_input_feed(input, bytes, sizeof(bytes) - 1U, 1U) ==
                 AFORC_ERROR_LIMIT &&
             aforc_input_event_count(input) == 1U &&
             aforc_input_dropped_events(input) == 3U &&
             aforc_input_key_held(input, AFORC_KEY_A) &&
             aforc_input_key_held(input, AFORC_KEY_B) &&
             aforc_input_next_event(input, &event) &&
             event.type == AFORC_INPUT_EVENT_KEY_DOWN &&
             event.data.key.key == AFORC_KEY_A && no_events(input);
    aforc_input_destroy(input);
    return passed;
}

int main(void)
{
    if (!test_kitty_explicit_release_owns_key_lifetime())
    {
        (void)fprintf(stderr, "Kitty explicit-release lifecycle failed\n");
        return 1;
    }
    if (!test_release_all_clears_explicit_release_key())
    {
        (void)fprintf(stderr, "explicit-release release-all failed\n");
        return 2;
    }
    if (!test_kitty_capability_and_text_reporting())
    {
        (void)fprintf(stderr, "Kitty capability/text reporting failed\n");
        return 3;
    }
    if (!test_kitty_function_key_uses_negotiated_release())
    {
        (void)fprintf(stderr, "Kitty function-key release failed\n");
        return 4;
    }
    if (!test_unsupported_kitty_function_is_not_text())
    {
        (void)fprintf(stderr, "Kitty functional-key filtering failed\n");
        return 5;
    }
    if (!test_incremental_utf8_and_paste_boundaries())
    {
        (void)fprintf(stderr, "incremental parser boundary failed\n");
        return 6;
    }
    if (!test_legacy_alt_key_is_not_text())
    {
        (void)fprintf(stderr, "legacy Alt text filtering failed\n");
        return 7;
    }
    if (!test_bounded_escape_forces_progress())
    {
        (void)fprintf(stderr, "bounded escape progress failed\n");
        return 8;
    }
    if (!test_queue_overflow_preserves_state_and_order())
    {
        (void)fprintf(stderr, "input queue overflow contract failed\n");
        return 9;
    }
    (void)puts("input protocol: ok");
    return 0;
}

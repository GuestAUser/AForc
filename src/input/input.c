/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "input_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Public input lifecycle, terminal polling, and frame-state facade.
 *
 * An input instance owns its bounded byte/event buffers. Protocol parsing and
 * held-state transitions live in private components; this facade coordinates
 * them against one monotonic timestamp so event order remains deterministic.
 */

static AFORC_Status aforc_input_internal_now_ms(uint64_t *out_now_ms)
{
    struct timespec now;

    if (out_now_ms == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0 || now.tv_sec < 0 ||
        (uint64_t)now.tv_sec > UINT64_MAX / UINT64_C(1000))
    {
        return AFORC_ERROR_PLATFORM;
    }
    *out_now_ms = (uint64_t)now.tv_sec * UINT64_C(1000) +
                  (uint64_t)now.tv_nsec / UINT64_C(1000000);
    return AFORC_OK;
}

AFORC_InputConfig aforc_input_config_default(void)
{
    AFORC_InputConfig config;

    config.event_capacity = 256u;
    config.byte_capacity = 4096u;
    config.key_release_timeout_ms = 600u;
    config.escape_timeout_ms = 25u;
    config.allocator = aforc_allocator_default();
    return config;
}

AFORC_Status aforc_input_create(AFORC_Input **out_input,
                                const AFORC_InputConfig *config)
{
    AFORC_InputConfig effective_config = aforc_input_config_default();
    AFORC_Input *input = NULL;
    AFORC_Status status;

    if (out_input == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_input = NULL;
    if (config != NULL)
    {
        effective_config = *config;
    }
    if (effective_config.event_capacity == 0u ||
        effective_config.byte_capacity < 64u ||
        effective_config.event_capacity > SIZE_MAX / sizeof(AFORC_InputEvent) ||
        !aforc_allocator_is_valid(&effective_config.allocator))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_alloc_array(
        &effective_config.allocator, 1u, sizeof(*input), (void **)&input);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(input, 0, sizeof(*input));
    input->allocator = effective_config.allocator;
    status = aforc_alloc_array(&input->allocator,
                               effective_config.event_capacity,
                               sizeof(*input->events),
                               (void **)&input->events);
    if (status == AFORC_OK)
    {
        status = aforc_alloc_array(&input->allocator,
                                   effective_config.byte_capacity,
                                   sizeof(*input->bytes),
                                   (void **)&input->bytes);
    }
    if (status != AFORC_OK)
    {
        aforc_input_destroy(input);
        return status;
    }
    input->event_capacity = effective_config.event_capacity;
    input->byte_capacity = effective_config.byte_capacity;
    input->key_release_timeout_ms = effective_config.key_release_timeout_ms;
    input->escape_timeout_ms = effective_config.escape_timeout_ms;
    input->key_release_mode = AFORC_INPUT_KEY_RELEASE_SYNTHETIC;
    input->mouse_x = -1;
    input->mouse_y = -1;
    *out_input = input;
    return AFORC_OK;
}

void aforc_input_destroy(AFORC_Input *input)
{
    AFORC_Allocator allocator;

    if (input == NULL)
    {
        return;
    }
    allocator = input->allocator;
    aforc_free(&allocator, input->events);
    aforc_free(&allocator, input->bytes);
    (void)memset(input, 0, sizeof(*input));
    aforc_free(&allocator, input);
}

AFORC_Status aforc_input_begin_frame(AFORC_Input *input)
{
    uint64_t timestamp_ms = 0u;
    AFORC_Status status;
    size_t index = 0u;

    if (input == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_input_internal_now_ms(&timestamp_ms);
    if (status != AFORC_OK)
    {
        return status;
    }
    input->queue_overflowed = false;
    for (index = 0u; index < AFORC_KEY_COUNT; ++index)
    {
        input->keys[index].pressed = false;
        input->keys[index].released = false;
    }
    for (index = 0u; index < AFORC_MOUSE_BUTTON_COUNT; ++index)
    {
        input->mouse[index].pressed = false;
        input->mouse[index].released = false;
    }
    aforc_input_internal_parse_available(input, timestamp_ms, false);
    aforc_input_internal_release_expired(input, timestamp_ms);
    return input->queue_overflowed ? AFORC_ERROR_LIMIT : AFORC_OK;
}

AFORC_Status
aforc_input_poll(AFORC_Input *input, AFORC_Terminal *terminal, int timeout_ms)
{
    unsigned char buffer[1024];
    uint64_t timestamp_ms = 0u;
    AFORC_Status status = AFORC_OK;
    bool readable = false;
    bool resized = false;
    bool queue_full = false;
    int effective_timeout = timeout_ms;

    if (input == NULL || terminal == NULL || timeout_ms < -1)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_input_internal_now_ms(&timestamp_ms);
    if (status != AFORC_OK)
    {
        return status;
    }
    input->queue_overflowed = false;
    effective_timeout =
        aforc_input_internal_effective_timeout(input, timeout_ms, timestamp_ms);
    status =
        aforc_terminal_poll(terminal, effective_timeout, &readable, &resized);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_input_internal_now_ms(&timestamp_ms);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (resized)
    {
        AFORC_Size size;

        status = aforc_terminal_dimensions(terminal, &size);
        if (status != AFORC_OK)
        {
            return status;
        }
        {
            AFORC_InputEvent event = aforc_input_internal_event(
                AFORC_INPUT_EVENT_RESIZE, timestamp_ms);
            event.data.resize.size = size;
            (void)aforc_input_internal_queue_event(input, &event);
        }
    }
    if (readable)
    {
        for (;;)
        {
            size_t count = 0u;

            status =
                aforc_terminal_read(terminal, buffer, sizeof(buffer), &count);
            if (status == AFORC_OK && count == 0u)
            {
                break;
            }
            if (status != AFORC_OK)
            {
                return status;
            }
            status =
                aforc_input_internal_feed(input, buffer, count, timestamp_ms);
            if (status != AFORC_OK && status != AFORC_ERROR_LIMIT)
            {
                return status;
            }
            if (status == AFORC_ERROR_LIMIT)
            {
                queue_full = true;
            }
        }
    }
    aforc_input_internal_parse_available(input, timestamp_ms, false);
    aforc_input_internal_release_expired(input, timestamp_ms);
    if (input->queue_overflowed)
    {
        queue_full = true;
    }
    return queue_full ? AFORC_ERROR_LIMIT : AFORC_OK;
}

AFORC_Status aforc_input_feed(AFORC_Input *input,
                              const unsigned char *bytes,
                              size_t size,
                              uint64_t timestamp_ms)
{
    if (input == NULL || (bytes == NULL && size != 0u))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    input->queue_overflowed = false;
    return aforc_input_internal_feed(input, bytes, size, timestamp_ms);
}

AFORC_Status aforc_input_flush(AFORC_Input *input, uint64_t timestamp_ms)
{
    if (input == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    input->queue_overflowed = false;
    aforc_input_internal_parse_available(input, timestamp_ms, false);
    aforc_input_internal_release_expired(input, timestamp_ms);
    return input->queue_overflowed ? AFORC_ERROR_LIMIT : AFORC_OK;
}

bool aforc_input_next_event(AFORC_Input *input, AFORC_InputEvent *out_event)
{
    if (input == NULL || out_event == NULL || input->event_count == 0u)
    {
        return false;
    }
    *out_event = input->events[input->event_head];
    input->event_head = (input->event_head + 1u) % input->event_capacity;
    --input->event_count;
    return true;
}

size_t aforc_input_event_count(const AFORC_Input *input)
{
    return input == NULL ? 0u : input->event_count;
}

uint64_t aforc_input_dropped_events(const AFORC_Input *input)
{
    return input == NULL ? 0u : input->dropped_events;
}

AFORC_InputKeyReleaseMode aforc_input_key_release_mode(const AFORC_Input *input)
{
    return input == NULL ? AFORC_INPUT_KEY_RELEASE_SYNTHETIC
                         : input->key_release_mode;
}

bool aforc_input_key_held(const AFORC_Input *input, AFORC_Key key)
{
    return input != NULL && aforc_input_internal_key_valid(key) &&
           input->keys[(size_t)key].held;
}

bool aforc_input_key_pressed(const AFORC_Input *input, AFORC_Key key)
{
    return input != NULL && aforc_input_internal_key_valid(key) &&
           input->keys[(size_t)key].pressed;
}

bool aforc_input_key_released(const AFORC_Input *input, AFORC_Key key)
{
    return input != NULL && aforc_input_internal_key_valid(key) &&
           input->keys[(size_t)key].released;
}

bool aforc_input_mouse_held(const AFORC_Input *input, AFORC_MouseButton button)
{
    return input != NULL && aforc_input_internal_mouse_button_valid(button) &&
           input->mouse[(size_t)button].held;
}

bool aforc_input_mouse_pressed(const AFORC_Input *input,
                               AFORC_MouseButton button)
{
    return input != NULL && aforc_input_internal_mouse_button_valid(button) &&
           input->mouse[(size_t)button].pressed;
}

bool aforc_input_mouse_released(const AFORC_Input *input,
                                AFORC_MouseButton button)
{
    return input != NULL && aforc_input_internal_mouse_button_valid(button) &&
           input->mouse[(size_t)button].released;
}

AFORC_Point aforc_input_mouse_position(const AFORC_Input *input)
{
    const AFORC_Point unknown = {-1, -1};

    return input == NULL ? unknown
                         : (AFORC_Point){input->mouse_x, input->mouse_y};
}

void aforc_input_release_all(AFORC_Input *input, uint64_t timestamp_ms)
{
    if (input == NULL)
    {
        return;
    }
    aforc_input_internal_release_all(input, timestamp_ms);
}

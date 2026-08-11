/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "input_internal.h"

#include <string.h>

/*
 * Incremental byte-buffer parser.
 *
 * Escape, UTF-8, and bracketed-paste sequences may span terminal reads. The
 * parser retains only an incomplete suffix, times out ambiguous bare escapes,
 * and keeps recognized control-sequence prefixes until they complete or hit
 * the bounded storage limit.
 */

static AFORC_ParseResult
aforc_input_internal_parse_paste(AFORC_Input *input,
                                 const unsigned char *bytes,
                                 size_t size,
                                 bool force_escape,
                                 uint64_t timestamp_ms,
                                 size_t *out_consumed)
{
    static const unsigned char paste_end[] = {0x1bu,
                                              (unsigned char)'[',
                                              (unsigned char)'2',
                                              (unsigned char)'0',
                                              (unsigned char)'1',
                                              (unsigned char)'~'};
    size_t prefix_size = size < sizeof(paste_end) ? size : sizeof(paste_end);
    uint32_t codepoint = 0u;
    size_t consumed = 0u;
    AFORC_Utf8Result utf8_result;

    /* Hold a partial terminator so its protocol bytes never leak as text. */
    if (memcmp(bytes, paste_end, prefix_size) == 0)
    {
        if (size < sizeof(paste_end) && !force_escape)
        {
            return AFORC_PARSE_INCOMPLETE;
        }
        if (size >= sizeof(paste_end))
        {
            const AFORC_InputEvent event = aforc_input_internal_event(
                AFORC_INPUT_EVENT_PASTE_END, timestamp_ms);
            input->paste_mode = false;
            (void)aforc_input_internal_queue_event(input, &event);
            *out_consumed = sizeof(paste_end);
            return AFORC_PARSE_COMPLETE;
        }
    }
    utf8_result =
        aforc_input_internal_decode_utf8(bytes, size, &codepoint, &consumed);
    if (utf8_result == AFORC_UTF8_INCOMPLETE && !force_escape)
    {
        return AFORC_PARSE_INCOMPLETE;
    }
    if (utf8_result != AFORC_UTF8_COMPLETE)
    {
        codepoint = 0xfffdu;
        consumed = 1u;
    }
    aforc_input_internal_emit_text(input, codepoint, timestamp_ms);
    *out_consumed = consumed;
    return AFORC_PARSE_COMPLETE;
}

static bool aforc_input_internal_escape_expired(const AFORC_Input *input,
                                                uint64_t timestamp_ms)
{
    return input->escape_pending &&
           (timestamp_ms >= input->escape_started_ms) &&
           timestamp_ms - input->escape_started_ms >=
               (uint64_t)input->escape_timeout_ms;
}

static void aforc_input_internal_mark_escape(AFORC_Input *input,
                                             uint64_t timestamp_ms)
{
    /* Do not restart the deadline when a partial sequence gains more bytes. */
    if (!input->escape_pending)
    {
        input->escape_pending = true;
        input->escape_started_ms = timestamp_ms;
    }
}

static AFORC_ParseResult
aforc_input_internal_defer_protocol(AFORC_Input *input, uint64_t timestamp_ms)
{
    input->escape_pending = true;
    input->escape_started_ms = timestamp_ms;
    return AFORC_PARSE_INCOMPLETE;
}

static void aforc_input_internal_emit_escape(AFORC_Input *input,
                                             uint64_t timestamp_ms)
{
    aforc_input_internal_emit_key_down(input,
                                       AFORC_KEY_ESCAPE,
                                       0u,
                                       AFORC_MOD_NONE,
                                       false,
                                       false,
                                       false,
                                       timestamp_ms);
}

static AFORC_ParseResult
aforc_input_internal_parse_escape(AFORC_Input *input,
                                  const unsigned char *bytes,
                                  size_t size,
                                  bool force_escape,
                                  uint64_t timestamp_ms,
                                  size_t *out_consumed)
{
    size_t final_index = 0u;

    if (size < 2u)
    {
        if (!force_escape)
        {
            aforc_input_internal_mark_escape(input, timestamp_ms);
            return AFORC_PARSE_INCOMPLETE;
        }
        aforc_input_internal_emit_escape(input, timestamp_ms);
        *out_consumed = 1u;
        return AFORC_PARSE_COMPLETE;
    }
    if (bytes[1] == (unsigned char)'[')
    {
        if (size >= 3u && bytes[2] == (unsigned char)'M')
        {
            if (size < 6u)
            {
                if (force_escape)
                {
                    return aforc_input_internal_defer_protocol(input,
                                                               timestamp_ms);
                }
                aforc_input_internal_mark_escape(input, timestamp_ms);
                return AFORC_PARSE_INCOMPLETE;
            }
            if (bytes[3] >= 32u && bytes[4] >= 33u && bytes[5] >= 33u)
            {
                aforc_input_internal_handle_mouse(input,
                                                  (uint32_t)(bytes[3] - 32u),
                                                  (uint32_t)(bytes[4] - 32u),
                                                  (uint32_t)(bytes[5] - 32u),
                                                  false,
                                                  true,
                                                  timestamp_ms);
            }
            *out_consumed = 6u;
            return AFORC_PARSE_COMPLETE;
        }
        for (final_index = 2u; final_index < size && final_index < 64u;
             ++final_index)
        {
            if (bytes[final_index] >= 0x40u && bytes[final_index] <= 0x7eu)
            {
                aforc_input_internal_handle_csi(input,
                                                bytes + 2u,
                                                final_index - 2u,
                                                bytes[final_index],
                                                timestamp_ms);
                *out_consumed = final_index + 1u;
                return AFORC_PARSE_COMPLETE;
            }
        }
        if (final_index == size && size < 64u)
        {
            if (force_escape)
            {
                return aforc_input_internal_defer_protocol(input, timestamp_ms);
            }
            aforc_input_internal_mark_escape(input, timestamp_ms);
            return AFORC_PARSE_INCOMPLETE;
        }
        if (force_escape)
        {
            aforc_input_internal_emit_escape(input, timestamp_ms);
            *out_consumed = 1u;
            return AFORC_PARSE_COMPLETE;
        }
        *out_consumed = final_index < size ? final_index + 1u : 1u;
        return AFORC_PARSE_COMPLETE;
    }
    if (bytes[1] == (unsigned char)'O')
    {
        if (size < 3u)
        {
            if (force_escape)
            {
                return aforc_input_internal_defer_protocol(input, timestamp_ms);
            }
            aforc_input_internal_mark_escape(input, timestamp_ms);
            return AFORC_PARSE_INCOMPLETE;
        }
        aforc_input_internal_handle_ss3(input, bytes[2], timestamp_ms);
        *out_consumed = 3u;
        return AFORC_PARSE_COMPLETE;
    }
    if (bytes[1] == 0x1bu)
    {
        aforc_input_internal_emit_escape(input, timestamp_ms);
        *out_consumed = 1u;
        return AFORC_PARSE_COMPLETE;
    }
    {
        size_t consumed = 0u;
        const AFORC_ParseResult result =
            aforc_input_internal_parse_plain(input,
                                             bytes + 1u,
                                             size - 1u,
                                             AFORC_MOD_ALT,
                                             force_escape,
                                             timestamp_ms,
                                             &consumed);

        if (result == AFORC_PARSE_INCOMPLETE)
        {
            if (!force_escape)
            {
                aforc_input_internal_mark_escape(input, timestamp_ms);
                return result;
            }
            aforc_input_internal_emit_escape(input, timestamp_ms);
            *out_consumed = 1u;
            return AFORC_PARSE_COMPLETE;
        }
        *out_consumed = consumed + 1u;
        return AFORC_PARSE_COMPLETE;
    }
}

static AFORC_ParseResult
aforc_input_internal_parse_one(AFORC_Input *input,
                               const unsigned char *bytes,
                               size_t size,
                               bool force_escape,
                               uint64_t timestamp_ms,
                               size_t *out_consumed)
{
    if (input->paste_mode)
    {
        return aforc_input_internal_parse_paste(
            input, bytes, size, force_escape, timestamp_ms, out_consumed);
    }
    if (bytes[0] == 0x1bu)
    {
        return aforc_input_internal_parse_escape(
            input, bytes, size, force_escape, timestamp_ms, out_consumed);
    }
    return aforc_input_internal_parse_plain(input,
                                            bytes,
                                            size,
                                            AFORC_MOD_NONE,
                                            force_escape,
                                            timestamp_ms,
                                            out_consumed);
}

void aforc_input_internal_parse_available(AFORC_Input *input,
                                          uint64_t timestamp_ms,
                                          bool force_all)
{
    size_t offset = 0u;

    while (offset < input->byte_count)
    {
        size_t consumed = 0u;
        const bool force_escape =
            force_all ||
            aforc_input_internal_escape_expired(input, timestamp_ms);
        const AFORC_ParseResult result =
            aforc_input_internal_parse_one(input,
                                           input->bytes + offset,
                                           input->byte_count - offset,
                                           force_escape,
                                           timestamp_ms,
                                           &consumed);

        if (result == AFORC_PARSE_INCOMPLETE)
        {
            aforc_input_internal_mark_escape(input, timestamp_ms);
            break;
        }
        if (consumed == 0u || consumed > input->byte_count - offset)
        {
            consumed = 1u;
        }
        offset += consumed;
        input->escape_pending = false;
    }
    if (offset > 0u)
    {
        input->byte_count -= offset;
        if (input->byte_count > 0u)
        {
            (void)memmove(
                input->bytes, input->bytes + offset, input->byte_count);
        }
    }
    if (input->byte_count == 0u)
    {
        input->escape_pending = false;
    }
}

AFORC_Status aforc_input_internal_feed(AFORC_Input *input,
                                       const unsigned char *bytes,
                                       size_t size,
                                       uint64_t timestamp_ms)
{
    size_t offset = 0u;

    aforc_input_internal_release_expired(input, timestamp_ms);
    aforc_input_internal_parse_available(input, timestamp_ms, false);
    while (offset < size)
    {
        size_t available = input->byte_capacity - input->byte_count;
        size_t chunk = size - offset;

        if (available == 0u)
        {
            /* A full prefix must be forced so bounded buffering makes progress.
             */
            aforc_input_internal_parse_available(input, timestamp_ms, true);
            available = input->byte_capacity - input->byte_count;
            if (available == 0u)
            {
                return AFORC_ERROR_OVERFLOW;
            }
        }
        if (chunk > available)
        {
            chunk = available;
        }
        (void)memcpy(input->bytes + input->byte_count, bytes + offset, chunk);
        input->byte_count += chunk;
        offset += chunk;
        aforc_input_internal_parse_available(input, timestamp_ms, false);
    }
    return input->queue_overflowed ? AFORC_ERROR_LIMIT : AFORC_OK;
}

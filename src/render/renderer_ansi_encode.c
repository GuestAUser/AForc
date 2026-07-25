/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_ansi_internal.h"

#include <string.h>

static AFORC_Status ansi_reserve(AFORC_Renderer *renderer, size_t additional)
{
    size_t required = 0u;
    size_t capacity;
    void *replacement = NULL;
    AFORC_Status status;

    if (!aforc_size_add(renderer->batch_size, additional, &required)) {
        return AFORC_ERROR_OVERFLOW;
    }
    if (required <= renderer->batch_capacity) {
        return AFORC_OK;
    }
    capacity = renderer->batch_capacity == 0u ? 4096u
                                               : renderer->batch_capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    if (renderer->batch == NULL) {
        status = aforc_alloc_array(&renderer->allocator,
                                   capacity,
                                   sizeof(*renderer->batch),
                                   &replacement);
    } else {
        status = aforc_realloc_array(&renderer->allocator,
                                     renderer->batch,
                                     capacity,
                                     sizeof(*renderer->batch),
                                     &replacement);
    }
    if (status != AFORC_OK) {
        return status;
    }
    renderer->batch = replacement;
    renderer->batch_capacity = capacity;
    return AFORC_OK;
}

static AFORC_Status ansi_append(AFORC_Renderer *renderer,
                                const void *data,
                                size_t size)
{
    AFORC_Status status = ansi_reserve(renderer, size);

    if (status != AFORC_OK) {
        return status;
    }
    if (size > 0u) {
        (void)memcpy(renderer->batch + renderer->batch_size, data, size);
        renderer->batch_size += size;
    }
    return AFORC_OK;
}

AFORC_Status aforc_renderer_ansi_literal(AFORC_Renderer *renderer,
                                         const char *literal)
{
    return ansi_append(renderer, literal, strlen(literal));
}

static AFORC_Status ansi_byte(AFORC_Renderer *renderer, unsigned char byte)
{
    return ansi_append(renderer, &byte, 1u);
}

static AFORC_Status ansi_u32(AFORC_Renderer *renderer, uint32_t value)
{
    char digits[10];
    size_t count = 0u;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    for (size_t index = 0u; index < count / 2u; ++index) {
        const char temporary = digits[index];

        digits[index] = digits[count - index - 1u];
        digits[count - index - 1u] = temporary;
    }
    return ansi_append(renderer, digits, count);
}

static AFORC_Status ansi_parameter(AFORC_Renderer *renderer, uint32_t value)
{
    const AFORC_Status status = ansi_byte(renderer, (unsigned char)';');

    return status == AFORC_OK ? ansi_u32(renderer, value) : status;
}

AFORC_Status aforc_renderer_ansi_cursor(AFORC_Renderer *renderer,
                                        uint32_t row,
                                        uint32_t column)
{
    AFORC_Status status = aforc_renderer_ansi_literal(renderer, "\x1b[");

    if (status == AFORC_OK) {
        status = ansi_u32(renderer, row);
    }
    if (status == AFORC_OK) {
        status = ansi_byte(renderer, (unsigned char)';');
    }
    if (status == AFORC_OK) {
        status = ansi_u32(renderer, column);
    }
    return status == AFORC_OK ? ansi_byte(renderer, (unsigned char)'H')
                              : status;
}

static AFORC_Status ansi_color(AFORC_Renderer *renderer,
                               AFORC_Color color,
                               bool foreground)
{
    AFORC_Status status;

    if (color.mode == AFORC_COLOR_DEFAULT) {
        return ansi_parameter(renderer, foreground ? 39u : 49u);
    }
    status = ansi_parameter(renderer, foreground ? 38u : 48u);
    if (status != AFORC_OK) {
        return status;
    }
    if (color.mode == AFORC_COLOR_INDEXED) {
        status = ansi_parameter(renderer, 5u);
        return status == AFORC_OK ? ansi_parameter(renderer, color.red)
                                  : status;
    }
    status = ansi_parameter(renderer, 2u);
    if (status == AFORC_OK) {
        status = ansi_parameter(renderer, color.red);
    }
    if (status == AFORC_OK) {
        status = ansi_parameter(renderer, color.green);
    }
    return status == AFORC_OK ? ansi_parameter(renderer, color.blue) : status;
}

AFORC_Status aforc_renderer_ansi_style(AFORC_Renderer *renderer,
                                       AFORC_Cell cell)
{
    AFORC_Status status = aforc_renderer_ansi_literal(renderer, "\x1b[0");

    for (size_t index = 0u;
         status == AFORC_OK &&
         index < sizeof(ansi_style_codes) / sizeof(ansi_style_codes[0]);
         ++index) {
        if ((cell.style & ansi_style_codes[index].flag) != 0u) {
            status = ansi_parameter(renderer, ansi_style_codes[index].code);
        }
    }
    if (status == AFORC_OK) {
        status = ansi_color(renderer, cell.foreground, true);
    }
    if (status == AFORC_OK) {
        status = ansi_color(renderer, cell.background, false);
    }
    return status == AFORC_OK ? ansi_byte(renderer, (unsigned char)'m')
                              : status;
}

AFORC_Status aforc_renderer_ansi_codepoint(AFORC_Renderer *renderer,
                                           uint32_t codepoint)
{
    unsigned char bytes[4];
    const size_t count = aforc_renderer_ansi_codepoint_size(codepoint);

    if (count == 1u) {
        bytes[0] = (unsigned char)codepoint;
    } else if (count == 2u) {
        bytes[0] = (unsigned char)(0xc0u | (codepoint >> 6u));
        bytes[1] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else if (count == 3u) {
        bytes[0] = (unsigned char)(0xe0u | (codepoint >> 12u));
        bytes[1] = (unsigned char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        bytes[2] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    } else {
        bytes[0] = (unsigned char)(0xf0u | (codepoint >> 18u));
        bytes[1] = (unsigned char)(0x80u | ((codepoint >> 12u) & 0x3fu));
        bytes[2] = (unsigned char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        bytes[3] = (unsigned char)(0x80u | (codepoint & 0x3fu));
    }
    return ansi_append(renderer, bytes, count);
}

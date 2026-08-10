/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_RENDER_ANSI_INTERNAL_H
#define AFORC_RENDER_ANSI_INTERNAL_H

#include "renderer_internal.h"

typedef struct AnsiStyleCode
{
    AFORC_CellStyle flag;
    uint32_t code;
} AnsiStyleCode;

static const AnsiStyleCode ansi_style_codes[] = {
    {AFORC_STYLE_BOLD, 1u},
    {AFORC_STYLE_DIM, 2u},
    {AFORC_STYLE_ITALIC, 3u},
    {AFORC_STYLE_UNDERLINE, 4u},
    {AFORC_STYLE_BLINK, 5u},
    {AFORC_STYLE_REVERSE, 7u},
    {AFORC_STYLE_HIDDEN, 8u},
    {AFORC_STYLE_STRIKETHROUGH, 9u},
};

#endif

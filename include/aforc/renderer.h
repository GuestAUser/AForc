/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_RENDERER_H
#define AFORC_RENDERER_H

#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AFORC_Renderer AFORC_Renderer;

typedef enum AFORC_ColorMode {
    AFORC_COLOR_DEFAULT = 0,
    AFORC_COLOR_INDEXED,
    AFORC_COLOR_RGB
} AFORC_ColorMode;

typedef struct AFORC_Color {
    AFORC_ColorMode mode;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} AFORC_Color;

typedef uint16_t AFORC_CellStyle;

enum {
    AFORC_STYLE_NONE = 0,
    AFORC_STYLE_BOLD = 1u << 0,
    AFORC_STYLE_DIM = 1u << 1,
    AFORC_STYLE_ITALIC = 1u << 2,
    AFORC_STYLE_UNDERLINE = 1u << 3,
    AFORC_STYLE_BLINK = 1u << 4,
    AFORC_STYLE_REVERSE = 1u << 5,
    AFORC_STYLE_HIDDEN = 1u << 6,
    AFORC_STYLE_STRIKETHROUGH = 1u << 7
};

typedef struct AFORC_Cell {
    uint32_t codepoint;
    AFORC_Color foreground;
    AFORC_Color background;
    AFORC_CellStyle style;
} AFORC_Cell;

typedef struct AFORC_RendererConfig {
    AFORC_Size size;
    AFORC_Allocator allocator;
} AFORC_RendererConfig;

AFORC_API AFORC_Color aforc_color_default(void);
AFORC_API AFORC_Color aforc_color_indexed(uint8_t index);
AFORC_API AFORC_Color aforc_color_rgb(uint8_t red, uint8_t green, uint8_t blue);
AFORC_API AFORC_Cell aforc_cell_default(void);
AFORC_API AFORC_RendererConfig aforc_renderer_config_default(void);

AFORC_API AFORC_Status aforc_renderer_create(
    AFORC_Renderer **out_renderer,
    const AFORC_RendererConfig *config
);

AFORC_API AFORC_Status aforc_renderer_create_for_terminal(
    AFORC_Renderer **out_renderer,
    AFORC_Terminal *terminal,
    const AFORC_Allocator *allocator
);

AFORC_API void aforc_renderer_destroy(AFORC_Renderer *renderer);
AFORC_API AFORC_Status aforc_renderer_resize(
    AFORC_Renderer *renderer,
    AFORC_Size size
);
AFORC_API AFORC_Status aforc_renderer_resize_to_terminal(
    AFORC_Renderer *renderer,
    AFORC_Terminal *terminal,
    bool *out_changed
);
AFORC_API AFORC_Size aforc_renderer_size(const AFORC_Renderer *renderer);
AFORC_API AFORC_Cell *aforc_renderer_back_buffer(
    AFORC_Renderer *renderer,
    size_t *out_stride
);
AFORC_API AFORC_Status aforc_renderer_clear(
    AFORC_Renderer *renderer,
    AFORC_Cell cell
);
AFORC_API AFORC_Status aforc_renderer_put(
    AFORC_Renderer *renderer,
    AFORC_Point position,
    AFORC_Cell cell
);
AFORC_API AFORC_Status aforc_renderer_get(
    const AFORC_Renderer *renderer,
    AFORC_Point position,
    AFORC_Cell *out_cell
);
AFORC_API AFORC_Status aforc_renderer_fill_rect(
    AFORC_Renderer *renderer,
    AFORC_Rect rect,
    AFORC_Cell cell
);
AFORC_API AFORC_Status aforc_renderer_draw_ascii(
    AFORC_Renderer *renderer,
    AFORC_Point position,
    const char *text,
    size_t length,
    AFORC_Cell cell
);
AFORC_API void aforc_renderer_invalidate(AFORC_Renderer *renderer);
AFORC_API AFORC_Status aforc_renderer_present(
    AFORC_Renderer *renderer,
    AFORC_Terminal *terminal
);

#ifdef __cplusplus
}
#endif

#endif

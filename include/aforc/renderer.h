/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_RENDERER_H
#define AFORC_RENDERER_H

#include "terminal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define AFORC_RENDERER_MAX_CELLS ((size_t)1048576u)

typedef struct AFORC_Renderer AFORC_Renderer;

typedef enum AFORC_ColorMode
{
    AFORC_COLOR_DEFAULT = 0,
    AFORC_COLOR_INDEXED,
    AFORC_COLOR_RGB
} AFORC_ColorMode;

typedef struct AFORC_Color
{
    AFORC_ColorMode mode;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} AFORC_Color;

typedef uint16_t AFORC_CellStyle;

enum
{
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

typedef struct AFORC_Cell
{
    uint32_t codepoint;
    AFORC_Color foreground;
    AFORC_Color background;
    AFORC_CellStyle style;
} AFORC_Cell;

typedef struct AFORC_RendererConfig
{
    AFORC_Size size;
    AFORC_Allocator allocator;
} AFORC_RendererConfig;

/*
 * A renderer owns its cell buffers and encoded output batch. Create copies the
 * allocator; its context must outlive the renderer. Destroy accepts NULL.
 * Drawing mutates only the back buffer: put and draw_ascii clip to the surface,
 * and fill_rect intersects its rectangle with the surface. The pointer returned
 * by back_buffer is borrowed, uses the reported cell stride, and is invalidated
 * by resize or destroy. Cells written through it must satisfy the same
 * printable scalar, color-mode, and style constraints as the drawing functions.
 *
 * draw_ascii validates the complete byte span before mutation, rejects bytes
 * above ASCII, treats newline and carriage return as cursor controls, and
 * renders other control bytes as spaces. Present resizes to the terminal,
 * writes one complete diff batch, and advances the front buffer only after a
 * successful write; a failure therefore leaves the frame retryable. Create and
 * resize reject surfaces above AFORC_RENDERER_MAX_CELLS with AFORC_ERROR_LIMIT
 * before allocating either cell buffer. A rejected resize leaves the existing
 * size and buffers unchanged.
 */

AFORC_API AFORC_Color aforc_color_default(void);
AFORC_API AFORC_Color aforc_color_indexed(uint8_t index);
AFORC_API AFORC_Color aforc_color_rgb(uint8_t red, uint8_t green, uint8_t blue);
AFORC_API AFORC_Cell aforc_cell_default(void);
AFORC_API AFORC_RendererConfig aforc_renderer_config_default(void);

AFORC_API AFORC_Status aforc_renderer_create(
    AFORC_Renderer **out_renderer, const AFORC_RendererConfig *config);

AFORC_API AFORC_Status
aforc_renderer_create_for_terminal(AFORC_Renderer **out_renderer,
                                   AFORC_Terminal *terminal,
                                   const AFORC_Allocator *allocator);

AFORC_API void aforc_renderer_destroy(AFORC_Renderer *renderer);
AFORC_API AFORC_Status aforc_renderer_resize(AFORC_Renderer *renderer,
                                             AFORC_Size size);
AFORC_API AFORC_Status aforc_renderer_resize_to_terminal(
    AFORC_Renderer *renderer, AFORC_Terminal *terminal, bool *out_changed);
AFORC_API AFORC_Size aforc_renderer_size(const AFORC_Renderer *renderer);
AFORC_API AFORC_Cell *aforc_renderer_back_buffer(AFORC_Renderer *renderer,
                                                 size_t *out_stride);
AFORC_API AFORC_Status aforc_renderer_clear(AFORC_Renderer *renderer,
                                            AFORC_Cell cell);
AFORC_API AFORC_Status aforc_renderer_put(AFORC_Renderer *renderer,
                                          AFORC_Point position,
                                          AFORC_Cell cell);
AFORC_API AFORC_Status aforc_renderer_get(const AFORC_Renderer *renderer,
                                          AFORC_Point position,
                                          AFORC_Cell *out_cell);
AFORC_API AFORC_Status aforc_renderer_fill_rect(AFORC_Renderer *renderer,
                                                AFORC_Rect rect,
                                                AFORC_Cell cell);
AFORC_API AFORC_Status aforc_renderer_draw_ascii(AFORC_Renderer *renderer,
                                                 AFORC_Point position,
                                                 const char *text,
                                                 size_t length,
                                                 AFORC_Cell cell);
AFORC_API void aforc_renderer_invalidate(AFORC_Renderer *renderer);
AFORC_API AFORC_Status aforc_renderer_present(AFORC_Renderer *renderer,
                                              AFORC_Terminal *terminal);

#ifdef __cplusplus
}
#endif

#endif

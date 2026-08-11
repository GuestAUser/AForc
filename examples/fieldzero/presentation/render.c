#include "fieldzero/presentation.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    FIELDZERO_VISUAL_CANVAS = 0,
    FIELDZERO_VISUAL_INK,
    FIELDZERO_VISUAL_FRAME,
    FIELDZERO_VISUAL_SIGNAL,
    FIELDZERO_VISUAL_ROLE_COUNT,
    FIELDZERO_STAGE_HEIGHT = 22,
    FIELDZERO_CANVAS_RED = 0x17,
    FIELDZERO_CANVAS_GREEN = 0x16,
    FIELDZERO_CANVAS_BLUE = 0x12,
    FIELDZERO_INK_RED = 0xfa,
    FIELDZERO_INK_GREEN = 0xf9,
    FIELDZERO_INK_BLUE = 0xf6,
    FIELDZERO_FRAME_RED = 0x6a,
    FIELDZERO_FRAME_GREEN = 0x65,
    FIELDZERO_FRAME_BLUE = 0x5c,
    FIELDZERO_SIGNAL_RED = 0x62,
    FIELDZERO_SIGNAL_GREEN = 0xd7,
    FIELDZERO_SIGNAL_BLUE = 0x76
};

AFORC_Cell fieldzero_visual_cell(uint32_t codepoint,
                                 uint8_t role,
                                 AFORC_CellStyle style,
                                 bool no_color);
AFORC_Status
fieldzero_visual_plot(void *context, AFORC_Point position, AFORC_Cell cell);
AFORC_Status fieldzero_render_world(AFORC_Renderer *renderer,
                                    const FieldzeroGame *game,
                                    const FieldzeroPresentation *presentation,
                                    AFORC_Rect arena);
AFORC_Status fieldzero_render_ui(AFORC_Renderer *renderer,
                                 const FieldzeroGame *game,
                                 const FieldzeroPresentation *presentation,
                                 const FieldzeroViewState *view,
                                 AFORC_Rect arena);

static AFORC_Color fieldzero_visual_color(uint8_t role)
{
    switch (role)
    {
        case FIELDZERO_VISUAL_CANVAS:
            return aforc_color_rgb(FIELDZERO_CANVAS_RED,
                                   FIELDZERO_CANVAS_GREEN,
                                   FIELDZERO_CANVAS_BLUE);
        case FIELDZERO_VISUAL_FRAME:
            return aforc_color_rgb(FIELDZERO_FRAME_RED,
                                   FIELDZERO_FRAME_GREEN,
                                   FIELDZERO_FRAME_BLUE);
        case FIELDZERO_VISUAL_SIGNAL:
            return aforc_color_rgb(FIELDZERO_SIGNAL_RED,
                                   FIELDZERO_SIGNAL_GREEN,
                                   FIELDZERO_SIGNAL_BLUE);
        case FIELDZERO_VISUAL_INK:
        default:
            return aforc_color_rgb(
                FIELDZERO_INK_RED, FIELDZERO_INK_GREEN, FIELDZERO_INK_BLUE);
    }
}

AFORC_Cell fieldzero_visual_cell(uint32_t codepoint,
                                 uint8_t role,
                                 AFORC_CellStyle style,
                                 bool no_color)
{
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = codepoint;
    cell.style = style;
    if (!no_color)
    {
        cell.foreground = fieldzero_visual_color(role);
        cell.background = fieldzero_visual_color(FIELDZERO_VISUAL_CANVAS);
    }
    return cell;
}

AFORC_Status
fieldzero_visual_plot(void *context, AFORC_Point position, AFORC_Cell cell)
{
    return aforc_renderer_put((AFORC_Renderer *)context, position, cell);
}

static AFORC_Rect fieldzero_arena(AFORC_Size screen)
{
    return (AFORC_Rect){(screen.width - FIELDZERO_ARENA_WIDTH) / 2,
                        (screen.height - FIELDZERO_STAGE_HEIGHT) / 2,
                        FIELDZERO_ARENA_WIDTH,
                        FIELDZERO_ARENA_HEIGHT};
}

static AFORC_Status fieldzero_render_resize_message(AFORC_Renderer *renderer,
                                                    AFORC_Size screen,
                                                    bool no_color)
{
    static const char message[] = "FIELD ZERO REQUIRES 80x24 - RESIZE TERMINAL";
    const int32_t length = (int32_t)(sizeof(message) - 1U);
    const AFORC_Point position = {(screen.width - length) / 2,
                                  screen.height / 2};

    return aforc_renderer_draw_ascii(
        renderer,
        position,
        message,
        sizeof(message) - 1U,
        fieldzero_visual_cell(
            (uint32_t)' ', FIELDZERO_VISUAL_INK, AFORC_STYLE_BOLD, no_color));
}

AFORC_Status fieldzero_render(AFORC_Renderer *renderer,
                              const FieldzeroGame *game,
                              const FieldzeroPresentation *presentation,
                              const FieldzeroViewState *view)
{
    AFORC_Size screen;
    AFORC_Status status;

    if (renderer == NULL || game == NULL || presentation == NULL ||
        view == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    screen = aforc_renderer_size(renderer);
    status =
        aforc_renderer_clear(renderer,
                             fieldzero_visual_cell((uint32_t)' ',
                                                   FIELDZERO_VISUAL_CANVAS,
                                                   AFORC_STYLE_NONE,
                                                   presentation->no_color));
    if (status != AFORC_OK)
    {
        return status;
    }
    if (screen.width < FIELDZERO_MIN_WIDTH ||
        screen.height < FIELDZERO_MIN_HEIGHT || view->terminal_too_small)
    {
        return fieldzero_render_resize_message(
            renderer, screen, presentation->no_color);
    }
    if (view->screen != FIELDZERO_SCREEN_TITLE &&
        view->screen != FIELDZERO_SCREEN_PLAY &&
        view->screen != FIELDZERO_SCREEN_COMPLETE)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    const AFORC_Rect arena = fieldzero_arena(screen);

    if (view->screen == FIELDZERO_SCREEN_PLAY &&
        game->phase != FIELDZERO_PHASE_COMPLETE)
    {
        status = fieldzero_render_world(renderer, game, presentation, arena);
        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return fieldzero_render_ui(renderer, game, presentation, view, arena);
}

static bool fieldzero_color_equal(AFORC_Color left, AFORC_Color right)
{
    if (left.mode != right.mode)
    {
        return false;
    }
    if (left.mode == AFORC_COLOR_DEFAULT)
    {
        return true;
    }
    if (left.mode == AFORC_COLOR_INDEXED)
    {
        return left.red == right.red;
    }
    return left.red == right.red && left.green == right.green &&
           left.blue == right.blue;
}

static bool fieldzero_palette_color(AFORC_Color color)
{
    for (uint8_t role = FIELDZERO_VISUAL_CANVAS;
         role < FIELDZERO_VISUAL_ROLE_COUNT;
         ++role)
    {
        if (fieldzero_color_equal(color, fieldzero_visual_color(role)))
        {
            return true;
        }
    }
    return false;
}

static bool fieldzero_player_glyph(uint32_t codepoint)
{
    return codepoint == (uint32_t)'@' || codepoint == (uint32_t)'^' ||
           codepoint == (uint32_t)'v' || codepoint == (uint32_t)'<' ||
           codepoint == (uint32_t)'>';
}

AFORC_Status fieldzero_render_validate(const AFORC_Renderer *renderer,
                                       const FieldzeroViewState *view,
                                       bool no_color)
{
    const AFORC_Color signal = fieldzero_visual_color(FIELDZERO_VISUAL_SIGNAL);
    AFORC_Size screen;
    size_t signal_cells = 0U;
    size_t inverse_players = 0U;

    if (renderer == NULL || view == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    screen = aforc_renderer_size(renderer);
    if (screen.width <= 0 || screen.height <= 0)
    {
        return AFORC_ERROR_STATE;
    }
    for (int32_t y = 0; y < screen.height; ++y)
    {
        for (int32_t x = 0; x < screen.width; ++x)
        {
            AFORC_Cell cell;
            AFORC_Status status =
                aforc_renderer_get(renderer, (AFORC_Point){x, y}, &cell);

            if (status != AFORC_OK)
            {
                return status;
            }
            if (cell.codepoint < UINT32_C(0x20) ||
                cell.codepoint > UINT32_C(0x7e))
            {
                return AFORC_ERROR_FORMAT;
            }
            if ((cell.style & (AFORC_STYLE_BLINK | AFORC_STYLE_HIDDEN)) != 0U)
            {
                return AFORC_ERROR_STATE;
            }
            if (no_color)
            {
                if (cell.foreground.mode != AFORC_COLOR_DEFAULT ||
                    cell.background.mode != AFORC_COLOR_DEFAULT)
                {
                    return AFORC_ERROR_STATE;
                }
            }
            else if (!fieldzero_palette_color(cell.foreground) ||
                     !fieldzero_palette_color(cell.background))
            {
                return AFORC_ERROR_STATE;
            }
            if (fieldzero_color_equal(cell.foreground, signal) ||
                fieldzero_color_equal(cell.background, signal))
            {
                ++signal_cells;
            }
            if ((cell.style & AFORC_STYLE_REVERSE) != 0U &&
                fieldzero_player_glyph(cell.codepoint))
            {
                ++inverse_players;
            }
        }
    }
    const bool too_small = screen.width < FIELDZERO_MIN_WIDTH ||
                           screen.height < FIELDZERO_MIN_HEIGHT ||
                           view->terminal_too_small;
    const bool overlay = view->help_visible || view->paused ||
                         view->focus_paused || view->quit_confirmation;
    const bool gameplay =
        view->screen == FIELDZERO_SCREEN_PLAY && !too_small && !overlay;

    if (gameplay && inverse_players != 1U)
    {
        return AFORC_ERROR_STATE;
    }
    if (!no_color && gameplay && signal_cells != 1U)
    {
        return AFORC_ERROR_STATE;
    }
    if ((!gameplay || no_color) && signal_cells > (no_color ? 0U : 1U))
    {
        return AFORC_ERROR_STATE;
    }
    return AFORC_OK;
}

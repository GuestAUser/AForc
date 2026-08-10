/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

AFORC_Cell
game_cell(uint32_t codepoint, AFORC_Color foreground, AFORC_CellStyle style)
{
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = codepoint;
    cell.foreground = foreground;
    cell.style = style;
    return cell;
}

AFORC_Status
game_plot_cell(void *context, AFORC_Point position, AFORC_Cell cell)
{
    Game *game = context;

    return aforc_renderer_put(game->renderer, position, cell);
}

AFORC_Status game_scene_render(AFORC_Scene *scene,
                               AFORC_Engine *engine,
                               double interpolation,
                               AFORC_Error *error)
{
    Game *game = scene->user_data;
    AFORC_Size screen = aforc_renderer_size(game->renderer);
    AFORC_Cell background =
        game_cell((uint32_t)' ', aforc_color_default(), AFORC_STYLE_NONE);
    int32_t map_rows;
    AFORC_Status status;

    (void)engine;
    (void)interpolation;
    status = aforc_renderer_clear(game->renderer, background);
    if (status != AFORC_OK)
    {
        return game_error(error, status, "renderer", "clear failed");
    }
    if (screen.width < GAME_MIN_COLUMNS || screen.height < GAME_MIN_ROWS)
    {
        static const char message[] = "Resize terminal to at least 40x14";
        AFORC_Point position = {1, screen.height / 2};

        status = aforc_renderer_draw_ascii(game->renderer,
                                           position,
                                           message,
                                           sizeof(message) - 1U,
                                           game_cell((uint32_t)' ',
                                                     aforc_color_indexed(203U),
                                                     AFORC_STYLE_BOLD));
        return status == AFORC_OK
                   ? AFORC_OK
                   : game_error(
                         error, status, "renderer", "resize message failed");
    }
    map_rows = screen.height - GAME_HUD_ROWS;
    status = game_render_world(game, screen, map_rows);
    if (status == AFORC_OK)
    {
        status = game_render_hud(game, screen, map_rows);
    }
    if (status != AFORC_OK)
    {
        return game_error(
            error, status, "roguelike", "frame composition failed");
    }
    return AFORC_OK;
}

/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static AFORC_Status game_render_help(const AFORC_UICanvas *canvas,
                                     AFORC_Size screen)
{
    static const char *const lines[] = {"MOVE  ARROW / WASD / HJKL",
                                        "WAIT  . / SPACE",
                                        "EXIT  > while standing on exit",
                                        "SAVE S   LOAD L (after run ends)",
                                        "ENEMY S  Tough, sight 12",
                                        "ENEMY g  Frail, sight 7",
                                        "HELP ? close",
                                        "QUIT Q / ESC"};
    AFORC_Rect panel;
    AFORC_UIPanelStyle style = aforc_ui_panel_style_ascii(
        game_cell((uint32_t)' ', aforc_color_indexed(220U), AFORC_STYLE_BOLD),
        game_cell((uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE),
        true);
    int32_t width = screen.width > 56 ? 56 : screen.width;
    int32_t height = screen.height < 12 ? screen.height : 12;
    AFORC_Status status =
        aforc_ui_layout_anchor((AFORC_Rect){0, 0, screen.width, screen.height},
                               (AFORC_Size){width, height},
                               AFORC_UI_ANCHOR_CENTER,
                               &panel);

    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_panel(canvas, panel, &style);
    }
    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 1, panel.y + 1, panel.width - 2, 1},
            "AFORC ROGUELIKE CONTROLS",
            sizeof("AFORC ROGUELIKE CONTROLS") - 1U,
            AFORC_UI_ALIGN_CENTER,
            game_cell(
                (uint32_t)' ', aforc_color_indexed(220U), AFORC_STYLE_BOLD));
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < sizeof(lines) / sizeof(lines[0]);
         ++index)
    {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){
                panel.x + 3, panel.y + 3 + (int32_t)index, panel.width - 6, 1},
            lines[index],
            strlen(lines[index]),
            AFORC_UI_ALIGN_START,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(index == 6U ? 220U : 252U),
                      index == 6U ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE));
    }
    return status;
}

static AFORC_Status
game_render_overlay(Game *game, const AFORC_UICanvas *canvas, AFORC_Size screen)
{
    const bool victory = game->run_state == GAME_VICTORIOUS;
    const char *title = victory ? "VICTORY" : "RUN ENDED";
    const char *instruction = "L Load save   R New run   Q Quit";
    AFORC_Rect panel;
    AFORC_UIPanelStyle style = aforc_ui_panel_style_ascii(
        game_cell((uint32_t)' ',
                  aforc_color_indexed(victory ? 220U : 203U),
                  AFORC_STYLE_BOLD),
        game_cell((uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE),
        true);
    int32_t width = screen.width > 48 ? 48 : screen.width;
    AFORC_Status status =
        aforc_ui_layout_anchor((AFORC_Rect){0, 0, screen.width, screen.height},
                               (AFORC_Size){width, 7},
                               AFORC_UI_ANCHOR_CENTER,
                               &panel);

    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_panel(canvas, panel, &style);
    }
    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 1, panel.y + 2, panel.width - 2, 1},
            title,
            strlen(title),
            AFORC_UI_ALIGN_CENTER,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(victory ? 220U : 203U),
                      AFORC_STYLE_BOLD));
    }
    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 1, panel.y + 4, panel.width - 2, 1},
            instruction,
            strlen(instruction),
            AFORC_UI_ALIGN_CENTER,
            game_cell(
                (uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE));
    }
    return status;
}

AFORC_Status game_render_hud(Game *game, AFORC_Size screen, int32_t map_rows)
{
    GamePosition *position = NULL;
    GameActor *actor = NULL;
    AFORC_UICanvas canvas;
    AFORC_UIPanelStyle panel_style = aforc_ui_panel_style_ascii(
        game_cell((uint32_t)' ', aforc_color_indexed(37U), AFORC_STYLE_NONE),
        game_cell((uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE),
        true);
    AFORC_UIProgressStyle health_style = {
        game_cell((uint32_t)'=', aforc_color_indexed(196U), AFORC_STYLE_BOLD),
        game_cell((uint32_t)'-', aforc_color_indexed(244U), AFORC_STYLE_DIM)};
    char status_text[96];
    AFORC_Status status =
        game_actor_components(game, game->player, &position, &actor);

    if (status != AFORC_OK)
    {
        return status;
    }
    status =
        aforc_ui_canvas_init(&canvas,
                             (AFORC_Rect){0, 0, screen.width, screen.height},
                             game_plot_cell,
                             game);
    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_panel(
            &canvas,
            (AFORC_Rect){0, map_rows, screen.width, GAME_HUD_ROWS},
            &panel_style);
    }
    (void)snprintf(status_text,
                   sizeof(status_text),
                   "AFORC RUINS  Floor %u/%u  Turn %u  Score %u  Seed %" PRIu64,
                   game->floor,
                   game->rules.final_floor,
                   game->turn,
                   game->score,
                   game->seed);
    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_label(
            &canvas,
            (AFORC_Rect){2, map_rows + 1, screen.width - 4, 1},
            status_text,
            strlen(status_text),
            AFORC_UI_ALIGN_START,
            game_cell(
                (uint32_t)' ', aforc_color_indexed(220U), AFORC_STYLE_BOLD));
    }
    if (status == AFORC_OK)
    {
        const int32_t progress_width =
            screen.width > 33 ? 22 : screen.width - 7;

        status = aforc_ui_draw_label(&canvas,
                                     (AFORC_Rect){2, map_rows + 2, 3, 1},
                                     "HP ",
                                     3U,
                                     AFORC_UI_ALIGN_START,
                                     game_cell((uint32_t)' ',
                                               aforc_color_indexed(196U),
                                               AFORC_STYLE_BOLD));
        if (status == AFORC_OK)
        {
            status = aforc_ui_draw_progress(
                &canvas,
                (AFORC_Rect){5, map_rows + 2, progress_width, 1},
                (uint64_t)(uint32_t)actor->health,
                (uint64_t)(uint32_t)actor->maximum_health,
                &health_style);
        }
    }
    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_label(
            &canvas,
            (AFORC_Rect){2, map_rows + 3, screen.width - 4, 1},
            game->message,
            strlen(game->message),
            AFORC_UI_ALIGN_START,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(
                          game->run_state == GAME_DEFEATED ? 203U : 252U),
                      game->run_state == GAME_DEFEATED ? AFORC_STYLE_BOLD
                                                       : AFORC_STYLE_NONE));
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    if (game->help_visible)
    {
        return game_render_help(&canvas, screen);
    }
    if (game->run_state != GAME_PLAYING)
    {
        return game_render_overlay(game, &canvas, screen);
    }
    return AFORC_OK;
}

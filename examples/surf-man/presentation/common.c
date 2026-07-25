/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "aforc/ui.h"
#include "surf_man/app.h"

#include <stdio.h>
#include <string.h>

static uint8_t tone_index(SurfManTone tone) {
    switch (tone) {
    case SURF_MAN_TONE_CANVAS:
        return SURF_MAN_COLOR_CANVAS;
    case SURF_MAN_TONE_INK:
        return SURF_MAN_COLOR_INK;
    case SURF_MAN_TONE_FRAMEWORK:
        return SURF_MAN_COLOR_FRAMEWORK;
    case SURF_MAN_TONE_SIGNAL:
        return SURF_MAN_COLOR_SIGNAL;
    }
    return SURF_MAN_COLOR_INK;
}

static AFORC_Status draw_horizontal(SurfManApp *app,
                                    int32_t left,
                                    int32_t right,
                                    int32_t row,
                                    uint32_t codepoint) {
    AFORC_Status status = AFORC_OK;

    for (int32_t column = left; status == AFORC_OK && column <= right;
         ++column) {
        status = surf_man_draw_char(app,
                                    (AFORC_Point){column, row},
                                    codepoint,
                                    SURF_MAN_TONE_FRAMEWORK,
                                    AFORC_STYLE_NONE);
    }
    return status;
}

SurfManLayout surf_man_layout_for_size(AFORC_Size size) {
    SurfManLayout layout;

    layout.screen = (AFORC_Rect){0, 0, size.width, size.height};
    layout.separator_y = size.height - 7;
    layout.play = (AFORC_Rect){1, 1, size.width - 2, size.height - 8};
    layout.hud = (AFORC_Rect){1, size.height - 6, size.width - 2, 5};
    return layout;
}

AFORC_Cell surf_man_cell(uint32_t codepoint,
                         uint8_t color_index,
                         AFORC_CellStyle style) {
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = codepoint;
    cell.foreground = aforc_color_indexed(color_index);
    cell.background = aforc_color_indexed(SURF_MAN_COLOR_CANVAS);
    cell.style = style;
    return cell;
}

AFORC_Cell surf_man_tone_cell(const SurfManApp *app,
                              uint32_t codepoint,
                              SurfManTone tone,
                              AFORC_CellStyle style) {
    uint8_t index = tone_index(tone);
    AFORC_Cell cell;

    if (app != NULL && app->settings.color_mode == SURF_MAN_COLOR_HIGH_CONTRAST &&
        tone == SURF_MAN_TONE_FRAMEWORK) {
        index = SURF_MAN_COLOR_INK;
    }
    cell = surf_man_cell(codepoint, index, style);
    if (app != NULL && app->settings.color_mode == SURF_MAN_COLOR_NONE) {
        cell.foreground = aforc_color_default();
        cell.background = aforc_color_default();
    }
    return cell;
}

AFORC_Status surf_man_plot_cell(void *context,
                                AFORC_Point position,
                                AFORC_Cell cell) {
    SurfManApp *app = context;

    if (app == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return aforc_renderer_put(app->renderer, position, cell);
}

AFORC_Status surf_man_draw_text(SurfManApp *app,
                                AFORC_Point position,
                                const char *text,
                                SurfManTone tone,
                                AFORC_CellStyle style) {
    AFORC_Size size;
    size_t length;

    if (app == NULL || text == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    size = aforc_renderer_size(app->renderer);
    length = strlen(text);
    if (position.x >= 0 && position.x < size.width && size.width > 1) {
        const size_t available = (size_t)(size.width - position.x - 1);

        if (length > available) {
            length = available;
        }
    }
    return aforc_renderer_draw_ascii(app->renderer,
                                     position,
                                     text,
                                     length,
                                     surf_man_tone_cell(app,
                                                        (uint32_t)' ',
                                                        tone,
                                                        style));
}

AFORC_Status surf_man_draw_char(SurfManApp *app,
                                AFORC_Point position,
                                uint32_t codepoint,
                                SurfManTone tone,
                                AFORC_CellStyle style) {
    if (app == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return aforc_renderer_put(app->renderer,
                              position,
                              surf_man_tone_cell(app,
                                                 codepoint,
                                                 tone,
                                                 style));
}

AFORC_Status surf_man_draw_panel(SurfManApp *app,
                                 AFORC_Rect rect,
                                 const char *title) {
    AFORC_Size size;
    AFORC_UICanvas canvas;
    AFORC_UIPanelStyle panel;
    AFORC_Status status;

    if (app == NULL || rect.width < 2 || rect.height < 2) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    size = aforc_renderer_size(app->renderer);
    status = aforc_ui_canvas_init(&canvas,
                                  (AFORC_Rect){0, 0, size.width, size.height},
                                  surf_man_plot_cell,
                                  app);
    panel = aforc_ui_panel_style_ascii(
        surf_man_tone_cell(app,
                           (uint32_t)' ',
                           SURF_MAN_TONE_FRAMEWORK,
                           AFORC_STYLE_NONE),
        surf_man_tone_cell(app,
                           (uint32_t)' ',
                           SURF_MAN_TONE_CANVAS,
                           AFORC_STYLE_NONE),
        true);
    if (status == AFORC_OK) {
        status = aforc_ui_draw_panel(&canvas, rect, &panel);
    }
    if (status == AFORC_OK && title != NULL && rect.width > 4) {
        char heading[64];
        size_t heading_length;
        size_t limit = (size_t)(rect.width - 4);

        (void)snprintf(heading, sizeof(heading), "[ %s ]", title);
        heading_length = strlen(heading);
        if (heading_length > limit) {
            heading_length = limit;
        }
        status = aforc_renderer_draw_ascii(
            app->renderer,
            (AFORC_Point){rect.x + 2, rect.y},
            heading,
            heading_length,
            surf_man_tone_cell(app,
                               (uint32_t)' ',
                               SURF_MAN_TONE_INK,
                               AFORC_STYLE_BOLD));
    }
    return status;
}

AFORC_Status surf_man_draw_instrument(SurfManApp *app,
                                      const SurfManLayout *layout) {
    char heading[48];
    size_t heading_length;
    size_t heading_limit;
    AFORC_Status status;

    if (app == NULL || layout == NULL || layout->screen.width < 2 ||
        layout->screen.height < 2) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = draw_horizontal(app,
                             0,
                             layout->screen.width - 1,
                             0,
                             (uint32_t)'-');
    if (status == AFORC_OK) {
        status = draw_horizontal(app,
                                 0,
                                 layout->screen.width - 1,
                                 layout->screen.height - 1,
                                 (uint32_t)'-');
    }
    for (int32_t row = 1;
         status == AFORC_OK && row < layout->screen.height - 1;
         ++row) {
        status = surf_man_draw_char(app,
                                    (AFORC_Point){0, row},
                                    (uint32_t)'|',
                                    SURF_MAN_TONE_FRAMEWORK,
                                    AFORC_STYLE_NONE);
        if (status == AFORC_OK) {
            status = surf_man_draw_char(
                app,
                (AFORC_Point){layout->screen.width - 1, row},
                (uint32_t)'|',
                SURF_MAN_TONE_FRAMEWORK,
                AFORC_STYLE_NONE);
        }
    }
    if (status == AFORC_OK) {
        status = draw_horizontal(app,
                                 0,
                                 layout->screen.width - 1,
                                 layout->separator_y,
                                 (uint32_t)'-');
    }
    if (status == AFORC_OK) {
        static const AFORC_Point corner_offsets[] = {
            {0, 0}, {1, 0}, {0, 1}, {1, 1}};

        for (size_t index = 0U;
             status == AFORC_OK &&
             index < sizeof(corner_offsets) / sizeof(corner_offsets[0]);
             ++index) {
            const int32_t x = corner_offsets[index].x == 0
                                  ? 0
                                  : layout->screen.width - 1;
            const int32_t y = corner_offsets[index].y == 0
                                  ? 0
                                  : layout->screen.height - 1;

            status = surf_man_draw_char(app,
                                        (AFORC_Point){x, y},
                                        (uint32_t)'+',
                                        SURF_MAN_TONE_FRAMEWORK,
                                        AFORC_STYLE_NONE);
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(app,
                                    (AFORC_Point){0, layout->separator_y},
                                    (uint32_t)'+',
                                    SURF_MAN_TONE_FRAMEWORK,
                                    AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(
            app,
            (AFORC_Point){layout->screen.width - 1, layout->separator_y},
            (uint32_t)'+',
            SURF_MAN_TONE_FRAMEWORK,
            AFORC_STYLE_NONE);
    }
    (void)snprintf(heading,
                   sizeof(heading),
                   "[ SURF-MAN | %s ]",
                   surf_man_phase_name(app->simulation.phase));
    heading_length = strlen(heading);
    heading_limit = (size_t)(layout->screen.width - 4);
    if (heading_length > heading_limit) {
        heading_length = heading_limit;
    }
    if (status == AFORC_OK) {
        status = aforc_renderer_draw_ascii(
            app->renderer,
            (AFORC_Point){2, 0},
            heading,
            heading_length,
            surf_man_tone_cell(app,
                               (uint32_t)' ',
                               SURF_MAN_TONE_INK,
                               AFORC_STYLE_BOLD));
    }
    return status;
}

const char *surf_man_phase_name(SurfManPhase phase) {
    switch (phase) {
    case SURF_MAN_SHACK:
        return "SHACK";
    case SURF_MAN_COUNT_IN:
        return "COUNT-IN";
    case SURF_MAN_RIDING:
        return "RIDING";
    case SURF_MAN_WIPEOUT_RECOVERY:
        return "WIPEOUT";
    case SURF_MAN_WAVE_RECAP:
        return "WAVE RECAP";
    case SURF_MAN_DAY_RECAP:
        return "DAY RECAP";
    case SURF_MAN_PRACTICE:
        return "PRACTICE";
    }
    return "UNKNOWN";
}

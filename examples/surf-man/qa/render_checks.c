/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man_internal.h"

#include <string.h>

enum {
    SURF_MAN_QA_CANVAS = 234,
    SURF_MAN_QA_INK = 255,
    SURF_MAN_QA_FRAMEWORK = 243,
    SURF_MAN_QA_SIGNAL = 77
};

typedef enum SurfManQARiderCase {
    SURF_MAN_QA_RIDER_NEUTRAL = 0,
    SURF_MAN_QA_RIDER_PADDLE,
    SURF_MAN_QA_RIDER_CARVE_LEFT,
    SURF_MAN_QA_RIDER_CARVE_RIGHT,
    SURF_MAN_QA_RIDER_AIR,
    SURF_MAN_QA_RIDER_GRAB,
    SURF_MAN_QA_RIDER_LANDING,
    SURF_MAN_QA_RIDER_TUBE,
    SURF_MAN_QA_RIDER_WIPEOUT,
    SURF_MAN_QA_RIDER_CASE_COUNT
} SurfManQARiderCase;

typedef struct SurfManQARiderExpectation {
    SurfManQARiderCase rider_case;
    char body[3][12];
    const char *board;
} SurfManQARiderExpectation;

static AFORC_Status surf_man_qa_render_error(AFORC_Error *error,
                                             AFORC_Status status,
                                             const char *message) {
    aforc_error_set(error, status, "surf-man qa", "%s", message);
    return status;
}

static bool surf_man_qa_color_equal(AFORC_Color left, AFORC_Color right) {
    if (left.mode != right.mode) {
        return false;
    }
    if (left.mode == AFORC_COLOR_DEFAULT) {
        return true;
    }
    if (left.mode == AFORC_COLOR_INDEXED) {
        return left.red == right.red;
    }
    return left.red == right.red && left.green == right.green &&
           left.blue == right.blue;
}

static bool surf_man_qa_cell_equal(AFORC_Cell left, AFORC_Cell right) {
    return left.codepoint == right.codepoint && left.style == right.style &&
           surf_man_qa_color_equal(left.foreground, right.foreground) &&
           surf_man_qa_color_equal(left.background, right.background);
}

static bool surf_man_qa_cell_at(const AFORC_Renderer *renderer,
                                AFORC_Point position,
                                AFORC_Cell expected) {
    AFORC_Cell actual;

    return aforc_renderer_get(renderer, position, &actual) == AFORC_OK &&
           surf_man_qa_cell_equal(actual, expected);
}

static bool surf_man_qa_text_at(const AFORC_Renderer *renderer,
                                AFORC_Point position,
                                const char *text,
                                uint8_t color,
                                AFORC_CellStyle style) {
    size_t index;

    for (index = 0U; text[index] != '\0'; ++index) {
        const AFORC_Cell expected = surf_man_cell(
            (uint32_t)(unsigned char)text[index], color, style);

        if (!surf_man_qa_cell_at(
                renderer,
                (AFORC_Point){position.x + (int32_t)index, position.y},
                expected)) {
            return false;
        }
    }
    return true;
}

static bool surf_man_qa_find_text(const AFORC_Renderer *renderer,
                                  const char *text,
                                  AFORC_Point *out_position) {
    const AFORC_Size size = aforc_renderer_size(renderer);
    const size_t length = strlen(text);
    int32_t y;

    for (y = 0; y < size.height; ++y) {
        int32_t x;

        for (x = 0; x + (int32_t)length <= size.width; ++x) {
            size_t index;

            for (index = 0U; index < length; ++index) {
                AFORC_Cell cell;
                const AFORC_Point position = {
                    x + (int32_t)index,
                    y,
                };

                if (aforc_renderer_get(renderer, position, &cell) != AFORC_OK ||
                    cell.codepoint != (uint32_t)(unsigned char)text[index]) {
                    break;
                }
            }
            if (index == length) {
                if (out_position != NULL) {
                    *out_position = (AFORC_Point){x, y};
                }
                return true;
            }
        }
    }
    return false;
}

static bool surf_man_qa_tokens_are_valid(const AFORC_Renderer *renderer) {
    const AFORC_Size size = aforc_renderer_size(renderer);
    int32_t y;

    for (y = 0; y < size.height; ++y) {
        int32_t x;

        for (x = 0; x < size.width; ++x) {
            AFORC_Cell cell;
            uint8_t foreground;

            if (aforc_renderer_get(
                    renderer, (AFORC_Point){x, y}, &cell) != AFORC_OK ||
                cell.codepoint > UINT32_C(127) ||
                (cell.style & (AFORC_STYLE_BLINK | AFORC_STYLE_HIDDEN)) != 0U ||
                cell.foreground.mode != AFORC_COLOR_INDEXED ||
                cell.background.mode != AFORC_COLOR_INDEXED ||
                cell.background.red != SURF_MAN_QA_CANVAS) {
                return false;
            }
            foreground = cell.foreground.red;
            if (foreground != SURF_MAN_QA_CANVAS &&
                foreground != SURF_MAN_QA_INK &&
                foreground != SURF_MAN_QA_FRAMEWORK &&
                foreground != SURF_MAN_QA_SIGNAL) {
                return false;
            }
        }
    }
    return true;
}

static bool surf_man_qa_rider_pose_present(const AFORC_Renderer *renderer,
                                           const char body[][12],
                                           const char *board) {
    const AFORC_Size size = aforc_renderer_size(renderer);
    const size_t board_length = strlen(board);
    int32_t y;

    for (y = 1; y < size.height; ++y) {
        int32_t x;

        for (x = 1; x + (int32_t)board_length <= size.width; ++x) {
            size_t index;
            bool exact = true;
            int32_t body_left;

            for (index = 0U; index < board_length; ++index) {
                const AFORC_Point position = {
                    x + (int32_t)index,
                    y,
                };
                const AFORC_Cell expected = surf_man_cell(
                    (uint32_t)(unsigned char)board[index],
                    SURF_MAN_QA_SIGNAL,
                    AFORC_STYLE_BOLD);

                if (!surf_man_qa_cell_at(renderer, position, expected)) {
                    exact = false;
                    break;
                }
            }
            if (!exact) {
                continue;
            }
            body_left = x - 1;
            if (body_left < 0 || y < 3 || body_left + 11 > size.width) {
                continue;
            }
            for (size_t row = 0U; row < 3U && exact; ++row) {
                for (size_t column = 0U; column < 11U; ++column) {
                    const unsigned char glyph =
                        (unsigned char)body[row][column];
                    const uint8_t color = glyph == (unsigned char)' '
                                              ? SURF_MAN_QA_CANVAS
                                          : glyph == (unsigned char)'O'
                                              ? SURF_MAN_QA_SIGNAL
                                              : SURF_MAN_QA_INK;
                    const AFORC_CellStyle style =
                        glyph == (unsigned char)' ' ? AFORC_STYLE_NONE
                                                    : AFORC_STYLE_BOLD;

                    if (!surf_man_qa_cell_at(
                            renderer,
                            (AFORC_Point){body_left + (int32_t)column,
                                          y - 3 + (int32_t)row},
                            surf_man_cell((uint32_t)glyph, color, style))) {
                        exact = false;
                        break;
                    }
                }
            }
            if (exact) {
                return true;
            }
        }
    }
    return false;
}

static void surf_man_qa_select_rider_case(SurfManApp *app,
                                          SurfManQARiderCase rider_case) {
    app->simulation.phase = SURF_MAN_RIDING;
    app->simulation.airborne = false;
    app->simulation.grabbed = false;
    app->simulation.altitude_q16 = 0;
    app->simulation.vertical_velocity_q16 = 0;
    app->simulation.tube_ticks = 0U;
    app->simulation.bank_ticks = 0U;
    app->simulation.last_maneuver = SURF_MAN_MANEUVER_NONE;
    app->simulation.angle_q16 = 0;
    app->settings.reduced_motion = true;
    app->visuals.visual_tick = 0U;

    switch (rider_case) {
    case SURF_MAN_QA_RIDER_PADDLE:
        app->simulation.phase = SURF_MAN_COUNT_IN;
        break;
    case SURF_MAN_QA_RIDER_CARVE_LEFT:
        app->simulation.bank_ticks = 1U;
        app->simulation.last_maneuver = SURF_MAN_MANEUVER_CARVE_LEFT;
        break;
    case SURF_MAN_QA_RIDER_CARVE_RIGHT:
        app->simulation.bank_ticks = 1U;
        app->simulation.last_maneuver = SURF_MAN_MANEUVER_CARVE_RIGHT;
        break;
    case SURF_MAN_QA_RIDER_AIR:
        app->simulation.airborne = true;
        app->simulation.altitude_q16 = 3 * SURF_MAN_Q16_ONE;
        break;
    case SURF_MAN_QA_RIDER_GRAB:
        app->simulation.airborne = true;
        app->simulation.grabbed = true;
        app->simulation.altitude_q16 = 3 * SURF_MAN_Q16_ONE;
        break;
    case SURF_MAN_QA_RIDER_LANDING:
        app->simulation.airborne = true;
        app->simulation.altitude_q16 = SURF_MAN_Q16_ONE;
        app->simulation.vertical_velocity_q16 = -SURF_MAN_Q16_ONE;
        break;
    case SURF_MAN_QA_RIDER_TUBE:
        app->simulation.tube_ticks = 1U;
        break;
    case SURF_MAN_QA_RIDER_WIPEOUT:
        app->simulation.phase = SURF_MAN_WIPEOUT_RECOVERY;
        break;
    case SURF_MAN_QA_RIDER_NEUTRAL:
    case SURF_MAN_QA_RIDER_CASE_COUNT:
        break;
    }
    app->simulation.settings = app->settings;
}

static AFORC_Status surf_man_qa_all_rider_poses(SurfManApp *app,
                                                AFORC_Error *error) {
    static const SurfManQARiderExpectation expectations[] = {
        {SURF_MAN_QA_RIDER_NEUTRAL,
         {"     O     ", "    /|\\    ", "    / \\    "},
         "<=======>"},
        {SURF_MAN_QA_RIDER_PADDLE,
         {"     O     ", "    /|\\    ", "    / \\    "},
         "<=======>"},
        {SURF_MAN_QA_RIDER_CARVE_LEFT,
         {"    O      ", "   /|\\     ", "   / \\     "},
         "\\=======/"},
        {SURF_MAN_QA_RIDER_CARVE_RIGHT,
         {"      O    ", "     /|\\   ", "     / \\   "},
         "/=======\\"},
        {SURF_MAN_QA_RIDER_AIR,
         {"     O     ", "    \\|/    ", "    / \\    "},
         "/=======\\"},
        {SURF_MAN_QA_RIDER_GRAB,
         {"      O    ", "    _/|/   ", "     / \\   "},
         "/=======\\"},
        {SURF_MAN_QA_RIDER_LANDING,
         {"     O     ", "   \\_|_/   ", "    / \\    "},
         "<=======>"},
        {SURF_MAN_QA_RIDER_TUBE,
         {"    O      ", "   /|_     ", "   / \\     "},
         "<=======>"},
        {SURF_MAN_QA_RIDER_WIPEOUT,
         {"     O     ", "   __|__   ", "   _/ \\_   "},
         "/=======\\"},
    };
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManSettings saved_settings = app->settings;
    const uint64_t saved_visual_tick = app->visuals.visual_tick;
    AFORC_Status status = AFORC_OK;
    const char *failure = NULL;

    for (size_t index = 0U;
         status == AFORC_OK &&
         index < sizeof(expectations) / sizeof(expectations[0]);
         ++index) {
        app->simulation = saved_simulation;
        app->settings = saved_settings;
        surf_man_qa_select_rider_case(app, expectations[index].rider_case);
        surf_man_visuals_mark_dirty(&app->visuals);
        status = surf_man_render_frame(app, 0.0, error);
        if (status != AFORC_OK) {
            failure = "rider pose matrix render did not complete";
        } else if (!surf_man_qa_rider_pose_present(app->renderer,
                                                   expectations[index].body,
                                                   expectations[index].board)) {
            status = AFORC_ERROR_STATE;
            failure = "rider pose matrix contains disconnected or misplaced anatomy";
        }
    }

    app->simulation = saved_simulation;
    app->settings = saved_settings;
    app->visuals.visual_tick = saved_visual_tick;
    if (failure != NULL) {
        return surf_man_qa_render_error(error, status, failure);
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_shack_cells(SurfManApp *app,
                                            AFORC_Error *error) {
    const AFORC_Size size = aforc_renderer_size(app->renderer);
    const AFORC_Cell frame = surf_man_cell(
        (uint32_t)'+', SURF_MAN_QA_FRAMEWORK, AFORC_STYLE_NONE);
    const AFORC_Cell marker = surf_man_cell(
        (uint32_t)'>', SURF_MAN_QA_SIGNAL, AFORC_STYLE_BOLD);
    surf_man_visuals_mark_dirty(&app->visuals);
    AFORC_Status status = surf_man_render_frame(app, 0.0, error);

    if (status != AFORC_OK) {
        return surf_man_qa_render_error(
            error, status, "shack render did not complete");
    }
    if (!surf_man_qa_cell_at(app->renderer, (AFORC_Point){0, 0}, frame) ||
        !surf_man_qa_cell_at(
            app->renderer, (AFORC_Point){size.width - 1, 0}, frame) ||
        !surf_man_qa_cell_at(
            app->renderer, (AFORC_Point){0, size.height - 1}, frame) ||
        !surf_man_qa_cell_at(app->renderer,
                             (AFORC_Point){size.width - 1, size.height - 1},
                             frame)) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "shack instrument frame corners were not exact framework cells");
    }
    if (!surf_man_qa_cell_at(app->renderer, (AFORC_Point){57, 4}, marker) ||
        !surf_man_qa_text_at(app->renderer,
                             (AFORC_Point){59, 5},
                             "PRACTICE",
                             SURF_MAN_QA_FRAMEWORK,
                             AFORC_STYLE_NONE) ||
        !surf_man_qa_text_at(app->renderer,
                             (AFORC_Point){8, 4},
                             "SURF SHACK",
                             SURF_MAN_QA_INK,
                             AFORC_STYLE_BOLD)) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "shack art or menu cells differed from the 80x24 composition");
    }
    if (!surf_man_qa_tokens_are_valid(app->renderer)) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "shack used non-ASCII, blinking, or out-of-contract color cells");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_riding_cells(SurfManApp *app,
                                             AFORC_Error *error) {
    surf_man_visuals_mark_dirty(&app->visuals);
    AFORC_Status status = surf_man_render_frame(app, 0.0, error);

    if (status != AFORC_OK) {
        return surf_man_qa_render_error(
            error, status, "riding render did not complete");
    }
    if (!surf_man_qa_find_text(app->renderer, "SCORE", NULL) ||
        !surf_man_qa_find_text(app->renderer, "FLOW", NULL) ||
        !surf_man_qa_find_text(app->renderer, "WAVE", NULL)) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "riding HUD omitted SCORE, FLOW, or WAVE cells");
    }
    if (!surf_man_qa_text_at(app->renderer,
                             (AFORC_Point){2, 18},
                             "DAY 2",
                             SURF_MAN_QA_INK,
                             AFORC_STYLE_BOLD) ||
        !surf_man_qa_text_at(app->renderer,
                             (AFORC_Point){2, 19},
                             "FLOW [",
                             SURF_MAN_QA_INK,
                             AFORC_STYLE_NONE) ||
        !surf_man_qa_text_at(app->renderer,
                             (AFORC_Point){8, 19},
                             "===",
                             SURF_MAN_QA_SIGNAL,
                             AFORC_STYLE_BOLD) ||
        !surf_man_qa_text_at(app->renderer,
                             (AFORC_Point){11, 19},
                             "--",
                             SURF_MAN_QA_FRAMEWORK,
                             AFORC_STYLE_DIM)) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "riding HUD cells differed from the fixed 80x24 composition");
    }
    status = surf_man_qa_all_rider_poses(app, error);
    if (status != AFORC_OK) {
        return status;
    }
    if (!surf_man_qa_tokens_are_valid(app->renderer)) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "riding scene used non-ASCII, blinking, or invalid color cells");
    }
    return AFORC_OK;
}

AFORC_Status surf_man_render_checks(SurfManApp *app, AFORC_Error *error) {
    SurfManSimulation saved_simulation;
    SurfManSettings saved_settings;
    SurfManOverlay saved_overlay;
    SurfManMenuItem saved_menu_item;
    AFORC_Size saved_terminal_size;
    bool saved_focused;
    bool saved_dirty;
    char saved_message[SURF_MAN_MESSAGE_CAPACITY];
    AFORC_Status status;

    if (app == NULL || app->renderer == NULL || !app->initialized) {
        return surf_man_qa_render_error(
            error, AFORC_ERROR_INVALID_ARGUMENT, "invalid render QA context");
    }
    if (aforc_renderer_size(app->renderer).width != SURF_MAN_TARGET_COLUMNS ||
        aforc_renderer_size(app->renderer).height != SURF_MAN_TARGET_ROWS) {
        return surf_man_qa_render_error(
            error,
            AFORC_ERROR_STATE,
            "render QA requires the deterministic 80x24 target surface");
    }

    saved_simulation = app->simulation;
    saved_settings = app->settings;
    saved_overlay = app->overlay;
    saved_menu_item = app->menu_item;
    saved_terminal_size = app->terminal_size;
    saved_focused = app->focused;
    saved_dirty = app->visuals.dirty;
    (void)memcpy(saved_message, app->message, sizeof(saved_message));

    app->settings = surf_man_settings_default();
    app->simulation.settings = app->settings;
    app->simulation.phase = SURF_MAN_SHACK;
    app->overlay = SURF_MAN_OVERLAY_NONE;
    app->menu_item = SURF_MAN_MENU_SURF;
    app->terminal_size = (AFORC_Size){
        SURF_MAN_TARGET_COLUMNS,
        SURF_MAN_TARGET_ROWS,
    };
    app->focused = true;
    surf_man_set_message(app, "Choose SURF or PRACTICE.");
    status = surf_man_qa_shack_cells(app, error);

    if (status == AFORC_OK) {
        app->simulation.phase = SURF_MAN_RIDING;
        app->simulation.day = 2U;
        app->simulation.wave = 2U;
        app->simulation.day_score = 12345U;
        app->simulation.pending_score = 678U;
        app->simulation.flow = 3U;
        app->simulation.risk_active = true;
        app->simulation.wave_ticks_remaining = 15U * SURF_MAN_FIXED_HZ;
        app->simulation.speed_q16 = 5 * SURF_MAN_Q16_ONE;
        app->simulation.face_q16 = 3 * SURF_MAN_Q16_ONE;
        app->simulation.altitude_q16 = 0;
        app->simulation.airborne = false;
        app->simulation.grabbed = false;
        app->simulation.last_turn = 0;
        app->simulation.last_maneuver = SURF_MAN_MANEUVER_NONE;
        surf_man_set_message(app, "BANK THE LINE");
        status = surf_man_qa_riding_cells(app, error);
    }

    app->simulation = saved_simulation;
    app->settings = saved_settings;
    app->overlay = saved_overlay;
    app->menu_item = saved_menu_item;
    app->terminal_size = saved_terminal_size;
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    (void)memcpy(app->message, saved_message, sizeof(saved_message));
    return status;
}

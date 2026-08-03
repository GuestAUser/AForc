/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "surf_man/app.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum SurfManRiderPose {
    SURF_MAN_RIDER_NEUTRAL = 0,
    SURF_MAN_RIDER_PADDLE,
    SURF_MAN_RIDER_CARVE_LEFT,
    SURF_MAN_RIDER_CARVE_RIGHT,
    SURF_MAN_RIDER_AIR,
    SURF_MAN_RIDER_GRAB,
    SURF_MAN_RIDER_LANDING,
    SURF_MAN_RIDER_TUBE,
    SURF_MAN_RIDER_WIPEOUT,
    SURF_MAN_RIDER_POSE_COUNT
} SurfManRiderPose;

enum {
    SURF_MAN_RIDER_SKY_ROWS = 3,
    SURF_MAN_RIDER_BODY_ROWS = 3,
    SURF_MAN_RIDER_HALF_WIDTH = 4,
    SURF_MAN_RIDER_BODY_WIDTH = SURF_MAN_RIDER_HALF_WIDTH * 2 + 1,
    SURF_MAN_RIDER_BOARD_GAP = 0
};

static const char rider_body[SURF_MAN_RIDER_POSE_COUNT]
                            [SURF_MAN_RIDER_BODY_ROWS]
                            [SURF_MAN_RIDER_BODY_WIDTH + 1] = {
    [SURF_MAN_RIDER_NEUTRAL] = {
        "    O    ", "   /|\\   ", "   / \\   "},
    [SURF_MAN_RIDER_PADDLE] = {
        "    O    ", "   /|\\   ", "   / \\   "},
    [SURF_MAN_RIDER_CARVE_LEFT] = {
        "   O     ", "  /|\\    ", "  / \\    "},
    [SURF_MAN_RIDER_CARVE_RIGHT] = {
        "     O   ", "    /|\\  ", "    / \\  "},
    [SURF_MAN_RIDER_AIR] = {
        "    O    ", "   \\|/   ", "   / \\   "},
    [SURF_MAN_RIDER_GRAB] = {
        "     O   ", "   _/|/  ", "    / \\  "},
    [SURF_MAN_RIDER_LANDING] = {
        "    O    ", "  \\_|_/  ", "   / \\   "},
    [SURF_MAN_RIDER_TUBE] = {
        "   O     ", "  /|_    ", "  / \\    "},
    [SURF_MAN_RIDER_WIPEOUT] = {
        "    O    ", "  __|__  ", "  _/ \\_  "}};

static SurfManRiderPose rider_pose(const SurfManSimulation *simulation) {
    const int32_t carve_threshold =
        simulation->rules.carve_velocity_threshold_q16 / 4;

    if (simulation->phase == SURF_MAN_WIPEOUT_RECOVERY) {
        return SURF_MAN_RIDER_WIPEOUT;
    }
    if (simulation->airborne && simulation->grabbed) {
        return SURF_MAN_RIDER_GRAB;
    }
    if (simulation->airborne) {
        if (simulation->vertical_velocity_q16 < 0 &&
            simulation->altitude_q16 <= 2 * SURF_MAN_Q16_ONE) {
            return SURF_MAN_RIDER_LANDING;
        }
        return SURF_MAN_RIDER_AIR;
    }
    if (simulation->tube_ticks != 0U) {
        return SURF_MAN_RIDER_TUBE;
    }
    if (simulation->last_maneuver == SURF_MAN_MANEUVER_AIR &&
        simulation->bank_ticks != 0U) {
        return SURF_MAN_RIDER_LANDING;
    }
    if (simulation->line_velocity_q16 <= -carve_threshold ||
        simulation->angle_q16 <= -SURF_MAN_Q16_ONE / 32) {
        return SURF_MAN_RIDER_CARVE_LEFT;
    }
    if (simulation->line_velocity_q16 >= carve_threshold ||
        simulation->angle_q16 >= SURF_MAN_Q16_ONE / 32) {
        return SURF_MAN_RIDER_CARVE_RIGHT;
    }
    if (simulation->phase == SURF_MAN_COUNT_IN) {
        return SURF_MAN_RIDER_PADDLE;
    }
    return SURF_MAN_RIDER_NEUTRAL;
}

static const char *rider_board(SurfManRiderPose pose, int32_t angle_q16) {
    switch (pose) {
    case SURF_MAN_RIDER_CARVE_LEFT:
        return "\\=====/";
    case SURF_MAN_RIDER_CARVE_RIGHT:
        return "/=====\\";
    case SURF_MAN_RIDER_AIR:
    case SURF_MAN_RIDER_GRAB:
        return angle_q16 < 0 ? "\\=====/" : "/=====\\";
    case SURF_MAN_RIDER_WIPEOUT:
        return angle_q16 < 0 ? "\\=====/" : "/=====\\";
    case SURF_MAN_RIDER_NEUTRAL:
    case SURF_MAN_RIDER_PADDLE:
    case SURF_MAN_RIDER_LANDING:
    case SURF_MAN_RIDER_TUBE:
    case SURF_MAN_RIDER_POSE_COUNT:
        return "<=====>";
    }
    return "<=====>";
}

static bool rider_cell_visible(const SurfManLayout *layout,
                               AFORC_Point position) {
    return position.x >= layout->play.x && position.y >= layout->play.y &&
           position.x < layout->play.x + layout->play.width &&
           position.y < layout->play.y + layout->play.height;
}

static SurfManTone rider_glyph_tone(unsigned char glyph) {
    if (glyph == (unsigned char)'O') {
        return SURF_MAN_TONE_SIGNAL;
    }
    return SURF_MAN_TONE_INK;
}

static AFORC_Status draw_rider_glyph(SurfManApp *app,
                                     const SurfManLayout *layout,
                                     AFORC_Point position,
                                     unsigned char glyph,
                                     SurfManTone tone) {
    if (!rider_cell_visible(layout, position)) {
        return AFORC_OK;
    }
    if (glyph == (unsigned char)' ') {
        return AFORC_OK;
    }
    return surf_man_draw_char(app,
                              position,
                              (uint32_t)glyph,
                              tone,
                              AFORC_STYLE_BOLD);
}

static AFORC_Status draw_rider_body(SurfManApp *app,
                                    const SurfManLayout *layout,
                                    SurfManRiderPose pose,
                                    AFORC_Point origin) {
    AFORC_Status status = AFORC_OK;

    for (size_t row = 0U;
         status == AFORC_OK && row < SURF_MAN_RIDER_BODY_ROWS;
         ++row) {
        const char *line = rider_body[(size_t)pose][row];

        for (size_t column = 0U;
             status == AFORC_OK && column < SURF_MAN_RIDER_BODY_WIDTH;
             ++column) {
            const unsigned char glyph = (unsigned char)line[column];

            status = draw_rider_glyph(
                app,
                layout,
                (AFORC_Point){origin.x + (int32_t)column,
                              origin.y + (int32_t)row},
                glyph,
                rider_glyph_tone(glyph));
        }
    }
    return status;
}

static size_t rider_text_length(const char *text) {
    size_t length = 0U;

    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static AFORC_Status draw_rider_board(SurfManApp *app,
                                     const SurfManLayout *layout,
                                     const char *board,
                                     int32_t center_x,
                                     int32_t board_y) {
    const size_t length = rider_text_length(board);
    const int32_t left = center_x - (int32_t)(length / 2U);
    AFORC_Status status = AFORC_OK;

    for (size_t column = 0U;
         status == AFORC_OK && column < length;
         ++column) {
        status = draw_rider_glyph(
            app,
            layout,
            (AFORC_Point){left + (int32_t)column, board_y},
            (unsigned char)board[column],
            SURF_MAN_TONE_SIGNAL);
    }
    return status;
}

AFORC_Status surf_man_render_rider(SurfManApp *app,
                                    const SurfManLayout *layout,
                                    int32_t surface_y) {
    const SurfManSimulation *simulation = &app->simulation;
    const SurfManRiderPose pose = rider_pose(simulation);
    const int32_t center_x =
        surf_man_rider_center_x(simulation, layout->play);
    const int32_t minimum_board_y = layout->play.y +
                                     SURF_MAN_RIDER_SKY_ROWS +
                                     SURF_MAN_RIDER_BODY_ROWS +
                                     SURF_MAN_RIDER_BOARD_GAP;
    const int32_t maximum_board_y = layout->play.y + layout->play.height - 1;
    const int64_t vertical_q16 =
        (int64_t)simulation->wave_face_offset_q16 +
        simulation->altitude_q16;
    const int32_t rounded_vertical_q16 =
        vertical_q16 < INT32_MIN
            ? INT32_MIN
        : vertical_q16 > INT32_MAX ? INT32_MAX
                                   : (int32_t)vertical_q16;
    int32_t board_y;
    AFORC_Point body_origin;
    AFORC_Status status;

    board_y = surface_y - surf_man_q16_round_cell(rounded_vertical_q16);
    if (board_y < minimum_board_y) {
        board_y = minimum_board_y;
    } else if (board_y > maximum_board_y) {
        board_y = maximum_board_y;
    }
    body_origin.x = center_x - SURF_MAN_RIDER_HALF_WIDTH;
    body_origin.y = board_y - SURF_MAN_RIDER_BODY_ROWS -
                    SURF_MAN_RIDER_BOARD_GAP;
    status = draw_rider_body(app, layout, pose, body_origin);
    if (status == AFORC_OK) {
        status = draw_rider_board(app,
                                  layout,
                                  rider_board(pose, simulation->angle_q16),
                                  center_x,
                                  board_y);
    }
    return status;
}

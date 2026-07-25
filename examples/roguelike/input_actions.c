/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

static uint32_t game_event_codepoint(const AFORC_InputEvent *event) {
    uint32_t codepoint = event->data.key.codepoint;

    if (codepoint == 0U && event->data.key.key >= AFORC_KEY_A &&
        event->data.key.key <= AFORC_KEY_Z) {
        codepoint = (uint32_t)event->data.key.key;
        if ((event->data.key.modifiers & AFORC_MOD_SHIFT) == 0U) {
            codepoint += (uint32_t)('a' - 'A');
        }
    }
    return codepoint;
}

static bool game_persistence_error_is_recoverable(AFORC_Status status) {
    switch (status) {
        case AFORC_ERROR_IO:
        case AFORC_ERROR_NOT_FOUND:
        case AFORC_ERROR_UNSUPPORTED:
        case AFORC_ERROR_FORMAT:
        case AFORC_ERROR_CHECKSUM:
            return true;
        default:
            return false;
    }
}

static AFORC_Status game_report_persistence_result(Game *game,
                                                   const char *operation,
                                                   AFORC_Status status) {
    if (status == AFORC_OK || !game_persistence_error_is_recoverable(status)) {
        return status;
    }
    game_set_message(game,
                     "%s failed: %s.",
                     operation,
                     aforc_status_string(status));
    return AFORC_OK;
}

static AFORC_Status game_handle_key(Game *game,
                                    AFORC_Engine *engine,
                                    const AFORC_InputEvent *event,
                                    bool *out_consumed) {
    const AFORC_Key key = event->data.key.key;
    const uint32_t codepoint = game_event_codepoint(event);
    AFORC_Point movement = {0, 0};
    bool move = false;
    AFORC_Status status = AFORC_OK;

    *out_consumed = true;
    if (key == AFORC_KEY_ESCAPE || codepoint == (uint32_t)'q' ||
        codepoint == (uint32_t)'Q') {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (codepoint == (uint32_t)'?') {
        game->help_visible = !game->help_visible;
        game_set_message(game,
                         game->help_visible ? "Help opened." : "Help closed.");
        return AFORC_OK;
    }
    if (game->help_visible) {
        return AFORC_OK;
    }
    if (game->run_state != GAME_PLAYING) {
        if (codepoint == (uint32_t)'r' || codepoint == (uint32_t)'R') {
            return game_new_run(game);
        }
        game_set_message(game, "Press R for a new run or Q to quit.");
        return AFORC_OK;
    }
    if (codepoint == (uint32_t)'S') {
        status = game_report_persistence_result(game, "Save", game_save(game));
    } else if (codepoint == (uint32_t)'L') {
        status = game_report_persistence_result(game, "Load", game_load(game));
    } else if (codepoint == (uint32_t)'>') {
        status = game_descend(game);
    } else if (codepoint == (uint32_t)'.' || key == AFORC_KEY_SPACE) {
        status = game_wait_turn(game);
    } else {
        if (key == AFORC_KEY_UP || codepoint == (uint32_t)'w' ||
            codepoint == (uint32_t)'k') {
            movement.y = -1;
            move = true;
        } else if (key == AFORC_KEY_DOWN || codepoint == (uint32_t)'s' ||
                   codepoint == (uint32_t)'j') {
            movement.y = 1;
            move = true;
        } else if (key == AFORC_KEY_LEFT || codepoint == (uint32_t)'a' ||
                   codepoint == (uint32_t)'h') {
            movement.x = -1;
            move = true;
        } else if (key == AFORC_KEY_RIGHT || codepoint == (uint32_t)'d' ||
                   codepoint == (uint32_t)'l') {
            movement.x = 1;
            move = true;
        }
        if (move) {
            status = game_move_player(game, movement);
        } else {
            game_set_message(game, "Unknown key. Press ? for controls.");
        }
    }
    return status;
}

AFORC_Status game_scene_event(AFORC_Scene *scene,
                              AFORC_Engine *engine,
                              const void *event_data,
                              bool *consumed,
                              AFORC_Error *error) {
    Game *game = scene->user_data;
    const AFORC_InputEvent *event = event_data;
    AFORC_Status status = AFORC_OK;

    *consumed = false;
    if (event->type == AFORC_INPUT_EVENT_KEY_DOWN) {
        status = game_handle_key(game, engine, event, consumed);
    }
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "roguelike",
                          "could not process input event");
    }
    return AFORC_OK;
}

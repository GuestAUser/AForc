/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

const AFORC_SceneVTable game_scene_vtable = {
    NULL,
    NULL,
    NULL,
    NULL,
    game_scene_fixed_update,
    game_scene_update,
    game_scene_render,
    game_scene_event
};

AFORC_Status game_dispatch_input_queue(Game *game,
                                       AFORC_Engine *engine,
                                       AFORC_Error *error) {
    AFORC_InputEvent event;
    AFORC_Status status = AFORC_OK;

    while (aforc_input_next_event(game->input, &event)) {
        bool consumed = false;

        status = aforc_engine_dispatch_event(engine,
                                             &event,
                                             &consumed,
                                             error);
        if (status != AFORC_OK) {
            break;
        }
    }
    return status;
}

AFORC_Status game_poll_events(void *context,
                              AFORC_Engine *engine,
                              AFORC_Error *error) {
    Game *game = context;
    AFORC_Status status = aforc_input_begin_frame(game->input);

    if (status == AFORC_OK) {
        status = aforc_input_poll(game->input, game->terminal, 0);
    }
    if (status == AFORC_ERROR_INTERRUPTED ||
        status == AFORC_ERROR_END_OF_STREAM) {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (status != AFORC_OK) {
        return game_error(error, status, "input", "terminal input poll failed");
    }
    return game_dispatch_input_queue(game, engine, error);
}

AFORC_Status game_begin_frame(void *context,
                              AFORC_Engine *engine,
                              AFORC_Error *error) {
    Game *game = context;
    bool changed = false;
    AFORC_Status status;

    (void)engine;
    /* A NULL terminal keeps the same frame hook path for off-screen smoke. */
    if (game->terminal == NULL) {
        return AFORC_OK;
    }
    status = aforc_renderer_resize_to_terminal(game->renderer,
                                               game->terminal,
                                               &changed);
    if (status != AFORC_OK) {
        return game_error(error, status, "renderer", "terminal resize failed");
    }
    if (changed) {
        aforc_renderer_invalidate(game->renderer);
    }
    return AFORC_OK;
}

AFORC_Status game_present(void *context,
                          AFORC_Engine *engine,
                          AFORC_Error *error) {
    Game *game = context;
    AFORC_Status status;

    (void)engine;
    if (game->terminal == NULL) {
        return AFORC_OK;
    }
    status = aforc_renderer_present(game->renderer, game->terminal);
    return status == AFORC_OK
               ? AFORC_OK
               : game_error(error, status, "renderer", "present failed");
}

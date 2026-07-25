/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int game_run_smoke(uint64_t seed) {
    AFORC_Renderer *renderer = NULL;
    AFORC_Input *input = NULL;
    AFORC_Engine *engine = NULL;
    AFORC_RendererConfig renderer_config = aforc_renderer_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Error error;
    Game game = {0};
    AFORC_Status status;
    size_t entity_count = 0U;
    bool game_initialized = false;

    aforc_error_clear(&error);
    renderer_config.size = (AFORC_Size){80, 28};
    status = aforc_renderer_create(&renderer, &renderer_config);
    if (status == AFORC_OK) {
        status = aforc_input_create(&input, &input_config);
    }
    if (status == AFORC_OK) {
        status = game_initialize(&game,
                                 renderer,
                                 input,
                                 NULL,
                                 seed);
        game_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        engine_config.user_data = &game;
        engine_config.target_frames_per_second = 0U;
        engine_config.hooks.context = &game;
        engine_config.hooks.begin_frame = game_begin_frame;
        engine_config.hooks.present = game_present;
        status = aforc_engine_create(&engine_config,
                                     &engine,
                                     &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &game.scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, &error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_checks(&game, engine, &error);
    }
    if (status == AFORC_OK) {
        entity_count = aforc_ecs_entity_count(game.ecs);
        (void)printf("roguelike smoke: ok seed=%" PRIu64
                     " floor=%u entities=%zu\n",
                     game.seed,
                     game.floor,
                     entity_count);
    } else {
        (void)fprintf(stderr,
                      "roguelike smoke: %s%s%s\n",
                      aforc_status_string(status),
                      error.message[0] == '\0' ? "" : ": ",
                      error.message);
    }
    aforc_engine_destroy(engine);
    if (game_initialized) {
        game_dispose(&game);
    }
    aforc_input_destroy(input);
    aforc_renderer_destroy(renderer);
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int game_run_interactive(uint64_t seed) {
    AFORC_Terminal *terminal = NULL;
    AFORC_Renderer *renderer = NULL;
    AFORC_Input *input = NULL;
    AFORC_Engine *engine = NULL;
    AFORC_TerminalConfig terminal_config = aforc_terminal_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Error error;
    Game game = {0};
    bool game_initialized = false;
    AFORC_Status status;

    aforc_error_clear(&error);
    status = aforc_terminal_open(&terminal, &terminal_config);
    if (status == AFORC_OK) {
        status = aforc_renderer_create_for_terminal(&renderer,
                                                    terminal,
                                                    &engine_config.allocator);
    }
    if (status == AFORC_OK) {
        status = aforc_input_create(&input, &input_config);
    }
    if (status == AFORC_OK) {
        status = game_initialize(&game,
                                 renderer,
                                 input,
                                 terminal,
                                 seed);
        game_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        engine_config.user_data = &game;
        engine_config.hooks.context = &game;
        engine_config.hooks.poll_events = game_poll_events;
        engine_config.hooks.begin_frame = game_begin_frame;
        engine_config.hooks.present = game_present;
        status = aforc_engine_create(&engine_config, &engine, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &game.scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_run(engine, &error);
    }
    aforc_engine_destroy(engine);
    if (game_initialized) {
        game_dispose(&game);
    }
    aforc_input_destroy(input);
    aforc_renderer_destroy(renderer);
    aforc_terminal_close(terminal);
    if (status != AFORC_OK) {
        (void)fprintf(stderr,
                      "aforc-roguelike: %s%s%s\n",
                      aforc_status_string(status),
                      error.message[0] == '\0' ? "" : ": ",
                      error.message);
    }
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

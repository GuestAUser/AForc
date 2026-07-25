/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/qa.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void surf_man_report_runtime_error(const char *mode,
                                          AFORC_Status status,
                                          const AFORC_Error *error)
{
    (void)fprintf(stderr,
                  "surf-man %s: %s%s%s\n",
                  mode,
                  aforc_status_string(status),
                  error->message[0] == '\0' ? "" : ": ",
                  error->message);
}

int surf_man_run_smoke(uint64_t seed)
{
    AFORC_Renderer *renderer = NULL;
    AFORC_Input *input = NULL;
    AFORC_Engine *engine = NULL;
    AFORC_RendererConfig renderer_config = aforc_renderer_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Error error;
    SurfManApp app = {0};
    AFORC_Status status;
    bool app_initialized = false;

    aforc_error_clear(&error);
    renderer_config.size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                       SURF_MAN_TARGET_ROWS};
    status = aforc_renderer_create(&renderer, &renderer_config);
    if (status == AFORC_OK) {
        status = aforc_input_create(&input, &input_config);
    }
    if (status == AFORC_OK) {
        status = surf_man_app_init(&app,
                                   renderer,
                                   input,
                                   NULL,
                                   seed,
                                   true);
        app_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        engine_config.fixed_updates_per_second = SURF_MAN_FIXED_HZ;
        engine_config.target_frames_per_second = 0U;
        engine_config.user_data = &app;
        engine_config.hooks.context = &app;
        engine_config.hooks.begin_frame = surf_man_begin_frame;
        engine_config.hooks.present = surf_man_present;
        status = aforc_engine_create(&engine_config, &engine, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &app.scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, &error);
    }
    if (status == AFORC_OK) {
        status = surf_man_smoke_checks(&app, engine, &error);
    }
    if (status == AFORC_OK) {
        (void)printf("surf-man smoke: ok seed=%" PRIu64
                     " hash=%" PRIu64 "\n",
                     seed,
                     surf_man_simulation_hash(&app.simulation));
    } else {
        surf_man_report_runtime_error("smoke", status, &error);
    }

    aforc_engine_destroy(engine);
    if (app_initialized) {
        surf_man_app_dispose(&app);
    }
    aforc_input_destroy(input);
    aforc_renderer_destroy(renderer);
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int surf_man_run_interactive(uint64_t seed)
{
    AFORC_Terminal *terminal = NULL;
    AFORC_Renderer *renderer = NULL;
    AFORC_Input *input = NULL;
    AFORC_Engine *engine = NULL;
    AFORC_TerminalConfig terminal_config = aforc_terminal_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Error error;
    SurfManApp app = {0};
    AFORC_Status status;
    bool app_initialized = false;

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
        status = surf_man_app_init(&app,
                                   renderer,
                                   input,
                                   terminal,
                                   seed,
                                   false);
        app_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        engine_config.fixed_updates_per_second = SURF_MAN_FIXED_HZ;
        engine_config.target_frames_per_second = SURF_MAN_FIXED_HZ;
        engine_config.user_data = &app;
        engine_config.hooks.context = &app;
        engine_config.hooks.poll_events = surf_man_poll_events;
        engine_config.hooks.begin_frame = surf_man_begin_frame;
        engine_config.hooks.present = surf_man_present;
        status = aforc_engine_create(&engine_config, &engine, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &app.scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_run(engine, &error);
    }

    aforc_engine_destroy(engine);
    if (app_initialized) {
        surf_man_app_dispose(&app);
    }
    aforc_input_destroy(input);
    aforc_renderer_destroy(renderer);
    aforc_terminal_close(terminal);
    if (status != AFORC_OK) {
        surf_man_report_runtime_error("interactive", status, &error);
    }
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

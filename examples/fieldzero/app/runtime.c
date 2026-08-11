#include "fieldzero/qa.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    FIELDZERO_NEGOTIATION_ATTEMPTS = 3,
    FIELDZERO_NEGOTIATION_TIMEOUT_MS = 100
};

static AFORC_Status fieldzero_runtime_error(AFORC_Error *error,
                                            AFORC_Status status,
                                            const char *message)
{
    if (error != NULL && error->message[0] == '\0')
    {
        aforc_error_set(error, status, "fieldzero", "%s", message);
    }
    return status;
}

static AFORC_Status fieldzero_negotiate_keyboard(FieldzeroApp *app,
                                                 AFORC_Error *error)
{
    AFORC_InputEvent event;
    AFORC_Status status = aforc_input_begin_frame(app->input);

    for (unsigned int attempt = 0U;
         status == AFORC_OK &&
         attempt < (unsigned int)FIELDZERO_NEGOTIATION_ATTEMPTS &&
         aforc_input_key_release_mode(app->input) !=
             AFORC_INPUT_KEY_RELEASE_EXPLICIT;
         ++attempt)
    {
        status = aforc_input_poll(
            app->input, app->terminal, FIELDZERO_NEGOTIATION_TIMEOUT_MS);
        if (status == AFORC_ERROR_LIMIT)
        {
            while (aforc_input_next_event(app->input, &event))
            {
            }
            status = AFORC_OK;
        }
    }
    if (status != AFORC_OK)
    {
        return fieldzero_runtime_error(
            error, status, "enhanced keyboard negotiation failed");
    }
    if (aforc_input_key_release_mode(app->input) !=
        AFORC_INPUT_KEY_RELEASE_EXPLICIT)
    {
        return fieldzero_runtime_error(
            error,
            AFORC_ERROR_UNSUPPORTED,
            "FIELD ZERO requires explicit key-release reporting; use a "
            "Kitty-compatible terminal or Windows Terminal 1.25+");
    }
    return AFORC_OK;
}

AFORC_Status fieldzero_app_init(FieldzeroApp *app,
                                const FieldzeroOptions *options,
                                bool interactive,
                                AFORC_Error *error)
{
    AFORC_TerminalConfig terminal_config = aforc_terminal_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_RendererConfig renderer_config = aforc_renderer_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Status status = AFORC_OK;
    const char *failure_message = "application initialization failed";
    int pending_signal = 0;
    bool game_initialized = false;
    bool presentation_initialized = false;

    if (app == NULL || options == NULL)
    {
        return fieldzero_runtime_error(error,
                                       AFORC_ERROR_INVALID_ARGUMENT,
                                       "invalid application configuration");
    }
    (void)memset(app, 0, sizeof(*app));
    app->options = *options;
    app->view.screen = FIELDZERO_SCREEN_TITLE;
    app->last_fixed_tick = UINT64_MAX;

    if (interactive)
    {
        terminal_config.mouse_events = false;
        terminal_config.bracketed_paste = false;
        failure_message = "interactive terminal could not be opened";
        status = aforc_terminal_open(&app->terminal, &terminal_config);
    }
    if (status == AFORC_OK)
    {
        failure_message = "input system could not be created";
        status = aforc_input_create(&app->input, &input_config);
    }
    if (status == AFORC_OK && interactive)
    {
        status = fieldzero_negotiate_keyboard(app, error);
    }
    if (status == AFORC_OK)
    {
        failure_message = "renderer could not be created";
        if (interactive)
        {
            status = aforc_renderer_create_for_terminal(
                &app->renderer, app->terminal, NULL);
        }
        else
        {
            renderer_config.size = (AFORC_Size){100, 30};
            status = aforc_renderer_create(&app->renderer, &renderer_config);
        }
    }
    if (status == AFORC_OK)
    {
        failure_message = "game state could not be initialized";
        status = fieldzero_game_init(&app->game, options->seed);
        game_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK)
    {
        failure_message = "presentation state could not be initialized";
        status = fieldzero_presentation_init(&app->presentation, options);
        presentation_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK)
    {
        fieldzero_app_configure_scenes(app);
        engine_config.fixed_updates_per_second =
            FIELDZERO_FIXED_UPDATES_PER_SECOND;
        engine_config.target_frames_per_second = interactive ? 60U : 0U;
        engine_config.user_data = app;
        engine_config.hooks.context = app;
        engine_config.hooks.poll_events =
            interactive ? fieldzero_app_poll_events : NULL;
        engine_config.hooks.begin_frame = fieldzero_app_begin_frame;
        engine_config.hooks.present = fieldzero_app_present;
        failure_message = "engine could not be created";
        status = aforc_engine_create(&engine_config, &app->engine, error);
    }
    if (status == AFORC_OK)
    {
        failure_message = "title scene could not be queued";
        status =
            aforc_engine_request_push(app->engine, &app->title_scene, error);
    }
    if (status == AFORC_OK)
    {
        app->initialized = true;
        return AFORC_OK;
    }

    aforc_engine_destroy(app->engine);
    if (presentation_initialized)
    {
        fieldzero_presentation_dispose(&app->presentation);
    }
    if (game_initialized)
    {
        fieldzero_game_dispose(&app->game);
    }
    aforc_renderer_destroy(app->renderer);
    aforc_input_destroy(app->input);
    pending_signal = aforc_terminal_pending_signal(app->terminal);
    aforc_terminal_close(app->terminal);
    (void)memset(app, 0, sizeof(*app));
    app->pending_signal = pending_signal;
    return fieldzero_runtime_error(error, status, failure_message);
}

void fieldzero_app_dispose(FieldzeroApp *app)
{
    int pending_signal;

    if (app == NULL || !app->initialized)
    {
        return;
    }
    aforc_engine_destroy(app->engine);
    fieldzero_presentation_dispose(&app->presentation);
    fieldzero_game_dispose(&app->game);
    aforc_renderer_destroy(app->renderer);
    aforc_input_destroy(app->input);
    pending_signal = aforc_terminal_pending_signal(app->terminal);
    aforc_terminal_close(app->terminal);
    (void)memset(app, 0, sizeof(*app));
    app->pending_signal = pending_signal;
}

static void fieldzero_print_failure(const char *mode,
                                    AFORC_Status status,
                                    const AFORC_Error *error)
{
    (void)fprintf(stderr,
                  "aforc-fieldzero %s: %s%s%s\n",
                  mode,
                  aforc_status_string(status),
                  error != NULL && error->message[0] != '\0' ? ": " : "",
                  error != NULL ? error->message : "");
}

int fieldzero_run_interactive(const FieldzeroOptions *options)
{
    FieldzeroApp app = {0};
    AFORC_Error error;
    AFORC_Status status;

    aforc_error_clear(&error);
    status = fieldzero_app_init(&app, options, true, &error);
    if (status == AFORC_OK)
    {
        status = aforc_engine_frame(app.engine, 0U, &error);
    }
    if (status == AFORC_OK)
    {
        status = aforc_engine_run(app.engine, &error);
    }
    fieldzero_app_dispose(&app);
    if (app.pending_signal != 0)
    {
        (void)raise(app.pending_signal);
        return EXIT_FAILURE;
    }
    if (status != AFORC_OK)
    {
        fieldzero_print_failure("runtime", status, &error);
    }
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int fieldzero_run_smoke(const FieldzeroOptions *options)
{
    FieldzeroApp app = {0};
    AFORC_Error error;
    AFORC_Status status = AFORC_OK;

    aforc_error_clear(&error);
    if (options == NULL)
    {
        status = fieldzero_runtime_error(&error,
                                         AFORC_ERROR_INVALID_ARGUMENT,
                                         "missing smoke configuration");
    }
    if (status == AFORC_OK && !fieldzero_run_regressions())
    {
        status = fieldzero_runtime_error(
            &error, AFORC_ERROR_STATE, "deterministic regressions failed");
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_app_init(&app, options, false, &error);
    }
    if (status == AFORC_OK)
    {
        status = aforc_engine_frame(app.engine, 0U, &error);
    }
    if (status == AFORC_OK && !fieldzero_smoke_drive(&app, &error))
    {
        status = error.status == AFORC_OK ? AFORC_ERROR_STATE : error.status;
        (void)fieldzero_runtime_error(
            &error, status, "deterministic smoke drive failed");
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_validate(
            app.renderer, &app.view, options->no_color);
        if (status != AFORC_OK)
        {
            (void)fieldzero_runtime_error(
                &error, status, "render validation failed");
        }
    }
    if (status == AFORC_OK)
    {
        (void)printf("fieldzero smoke: ok seed=%" PRIu64
                     " rooms=%u memories=%u state=%" PRIu64
                     " collision=%" PRIu64 "\n",
                     app.game.seed,
                     fieldzero_popcount_u16(app.game.completed_rooms),
                     fieldzero_popcount_u16(app.game.collected_memories),
                     fieldzero_game_state_digest(&app.game),
                     fieldzero_game_collision_digest(&app.game));
    }
    fieldzero_app_dispose(&app);
    if (status != AFORC_OK)
    {
        fieldzero_print_failure("smoke", status, &error);
    }
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <string.h>

static AFORC_Status game_smoke_seed_parser(AFORC_Error *error) {
    static const char *const invalid_seeds[] = {
        "",
        "-1",
        "+1",
        " 1",
        "1 ",
        "1x",
        "18446744073709551616"
    };
    uint64_t seed = UINT64_C(7);
    AFORC_Status status = game_parse_seed("0", &seed);

    if (status != AFORC_OK || seed != 0U) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "zero seed was not parsed");
    }
    status = game_parse_seed("18446744073709551615", &seed);
    if (status != AFORC_OK || seed != UINT64_MAX) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "maximum seed was not parsed");
    }
    for (size_t index = 0U;
         index < sizeof(invalid_seeds) / sizeof(invalid_seeds[0]);
         ++index) {
        seed = UINT64_C(7);
        status = game_parse_seed(invalid_seeds[index], &seed);
        if (status != AFORC_ERROR_FORMAT || seed != UINT64_C(7)) {
            return game_error(error,
                              AFORC_ERROR_STATE,
                              "smoke",
                              "invalid seed was accepted");
        }
    }
    return AFORC_OK;
}

static AFORC_Status game_smoke_repeated_wait_is_dispatched(
    Game *game,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    AFORC_InputEvent event = {0};
    const uint32_t turn = game->turn;
    bool consumed = false;
    AFORC_Status status;

    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_SPACE;
    event.data.key.repeat = true;
    status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    if (status == AFORC_OK && (!consumed || game->turn == turn)) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "repeated wait keydown was ignored");
    }
    if (status == AFORC_OK) {
        game->turn = UINT32_MAX;
        consumed = false;
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
        if (status == AFORC_OK && (!consumed || game->turn != UINT32_MAX)) {
            status = game_error(error,
                                AFORC_ERROR_STATE,
                                "smoke",
                                "turn counter wrapped");
        }
        game->turn = turn + 1U;
    }
    return status;
}

static AFORC_Status game_smoke_repeated_help_is_stable(
    Game *game,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    AFORC_InputEvent event = {0};
    const bool help_visible = game->help_visible;
    char message[GAME_MESSAGE_CAPACITY];
    bool consumed = false;
    AFORC_Status status;

    (void)memcpy(message, game->message, sizeof(message));
    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_NONE;
    event.data.key.codepoint = (uint32_t)'?';
    game->help_visible = false;
    status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    if (status == AFORC_OK && (!consumed || !game->help_visible)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "help keydown did not open help");
    }
    event.data.key.repeat = true;
    consumed = false;
    if (status == AFORC_OK) {
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    }
    if (status == AFORC_OK && (!consumed || !game->help_visible)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "repeated help keydown changed modal state");
    }
    game->help_visible = help_visible;
    (void)memcpy(game->message, message, sizeof(message));
    return status;
}

static AFORC_Status game_smoke_save_failure_is_recoverable(
    Game *game,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    AFORC_InputEvent event = {0};
    const char *save_path = game->save_path;
    bool consumed = false;
    AFORC_Status status;

    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_S;
    event.data.key.codepoint = (uint32_t)'S';
    game->save_path = "aforc-smoke-missing-directory/save.bin";
    status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    game->save_path = save_path;
    if (status == AFORC_OK &&
        (!consumed || strcmp(game->message, "Save failed: not found.") != 0)) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "save failure did not remain in game");
    }
    return status;
}

static AFORC_Status game_smoke_short_save_is_recoverable(
    Game *game,
    AFORC_Error *error) {
    AFORC_SaveWriter writer = {0};
    AFORC_AssetBlob blob = {NULL, 0U};
    const uint64_t seed = game->seed;
    const uint32_t floor = game->floor;
    const uint32_t score = game->score;
    const uint32_t turn = game->turn;
    AFORC_Status status = aforc_save_writer_init(&writer,
                                                 GAME_SAVE_SCHEMA,
                                                 GAME_SAVE_MAX_BYTES);

    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u64(&writer, seed);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_finish(&writer, &blob);
    }
    if (status == AFORC_OK) {
        const AFORC_Status decode_status =
            game_decode_save(game, blob.data, blob.size);

        if (decode_status != AFORC_ERROR_FORMAT || game->seed != seed ||
            game->floor != floor || game->score != score ||
            game->turn != turn) {
            status = game_error(error,
                                AFORC_ERROR_STATE,
                                "smoke",
                                "short save was not rejected transactionally");
        }
    }
    aforc_asset_blob_release(&blob);
    aforc_save_writer_release(&writer);
    return status;
}

static AFORC_Status game_smoke_runtime_ownership(Game *game,
                                                  AFORC_Error *error) {
    if (game->particle_pool.particles != game->particles ||
        game->scene.user_data != game || game->save_path == NULL) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "loaded runtime retained stale ownership pointers");
    }
    return AFORC_OK;
}

AFORC_Status game_runtime_smoke_checks(Game *game,
                                       AFORC_Engine *engine,
                                       AFORC_Error *error) {
    AFORC_Status status = game_smoke_seed_parser(error);

    if (status == AFORC_OK) {
        status = game_smoke_repeated_wait_is_dispatched(game, engine, error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_repeated_help_is_stable(game, engine, error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_save_failure_is_recoverable(game, engine, error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_short_save_is_recoverable(game, error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_runtime_ownership(game, error);
    }
    return status;
}

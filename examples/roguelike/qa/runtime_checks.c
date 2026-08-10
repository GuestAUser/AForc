/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <string.h>

typedef struct GameSmokeActorSnapshot
{
    GamePosition position;
    GameActor actor;
} GameSmokeActorSnapshot;

static bool game_smoke_actor_equal(const GameActor *left,
                                   const GameActor *right)
{
    return left->health == right->health &&
           left->maximum_health == right->maximum_health &&
           left->attack == right->attack && left->glyph == right->glyph &&
           left->color.mode == right->color.mode &&
           left->color.red == right->color.red &&
           left->color.green == right->color.green &&
           left->color.blue == right->color.blue &&
           left->hostile == right->hostile;
}

static AFORC_Status game_smoke_gather_enemies(Game *game,
                                              GameSmokeActorSnapshot *snapshots,
                                              size_t *out_count)
{
    const AFORC_ComponentType types[2] = {game->position_type,
                                          game->actor_type};
    AFORC_EcsView *view = NULL;
    size_t count = 0U;
    AFORC_Status status = aforc_ecs_view_create(game->ecs, types, 2U, &view);

    while (status == AFORC_OK)
    {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        bool has_value = false;

        status = aforc_ecs_view_next(view, &entity, components, &has_value);
        if (status != AFORC_OK || !has_value)
        {
            break;
        }
        if (!((GameActor *)components[1])->hostile)
        {
            continue;
        }
        if (count == GAME_MAX_ENEMIES)
        {
            status = AFORC_ERROR_LIMIT;
            break;
        }
        snapshots[count].position = *(GamePosition *)components[0];
        snapshots[count].actor = *(GameActor *)components[1];
        ++count;
    }
    aforc_ecs_view_destroy(view);
    if (status == AFORC_OK)
    {
        *out_count = count;
    }
    return status;
}

static bool game_smoke_snapshots_equal(const GameSmokeActorSnapshot *left,
                                       const GameSmokeActorSnapshot *right,
                                       size_t count)
{
    for (size_t index = 0U; index < count; ++index)
    {
        if (!aforc_world_point_equal(left[index].position.point,
                                     right[index].position.point) ||
            !game_smoke_actor_equal(&left[index].actor, &right[index].actor))
        {
            return false;
        }
    }
    return true;
}

static AFORC_Status game_smoke_prepare_exact_source(Game *source)
{
    const AFORC_ComponentType types[2] = {source->position_type,
                                          source->actor_type};
    AFORC_EcsView *view = NULL;
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Point previous_player_position;
    AFORC_Entity removed_enemy = AFORC_ENTITY_INVALID;
    bool enemy_adjusted = false;
    AFORC_Status status =
        game_generate_floor(source, 2U, source->rules.player_health);

    if (status == AFORC_OK)
    {
        status = game_actor_components(
            source, source->player, &player_position, &player_actor);
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    previous_player_position = player_position->point;
    player_position->point = source->exit_position;
    if (player_actor->health > 1)
    {
        --player_actor->health;
    }
    for (size_t index = 0U; index < source->cell_count; ++index)
    {
        source->explored[index] = index % 3U == 0U ? 1U : 0U;
    }
    source->score = UINT32_C(0x10203040);
    source->turn = UINT32_C(0x50607080);

    status = aforc_ecs_view_create(source->ecs, types, 2U, &view);
    while (status == AFORC_OK)
    {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        bool has_value = false;
        GameActor *actor;

        status = aforc_ecs_view_next(view, &entity, components, &has_value);
        if (status != AFORC_OK || !has_value)
        {
            break;
        }
        actor = components[1];
        if (actor->hostile)
        {
            if (!enemy_adjusted)
            {
                ((GamePosition *)components[0])->point =
                    previous_player_position;
                if (actor->health > 1)
                {
                    --actor->health;
                }
                enemy_adjusted = true;
            }
            else
            {
                removed_enemy = entity;
                break;
            }
        }
    }
    aforc_ecs_view_destroy(view);
    if (status == AFORC_OK &&
        aforc_ecs_entity_alive(source->ecs, removed_enemy))
    {
        status = aforc_ecs_destroy_entity(source->ecs, removed_enemy);
    }
    return status;
}

static AFORC_Status game_smoke_exact_save_round_trip(Game *game,
                                                     AFORC_Error *error)
{
    GameSmokeActorSnapshot source_enemies[GAME_MAX_ENEMIES];
    GameSmokeActorSnapshot loaded_enemies[GAME_MAX_ENEMIES];
    Game source = {0};
    Game loaded = {0};
    AFORC_AssetBlob blob = {NULL, 0U};
    AFORC_AssetBlob hostile_blob = {NULL, 0U};
    AFORC_SaveReader reader = {0};
    GamePosition *source_player_position = NULL;
    GameActor *source_player_actor = NULL;
    GamePosition *loaded_player_position = NULL;
    GameActor *loaded_player_actor = NULL;
    size_t source_enemy_count = 0U;
    size_t loaded_enemy_count = 0U;
    bool source_initialized = false;
    bool loaded_initialized = false;
    AFORC_Status status = game_initialize(&source,
                                          game->renderer,
                                          game->input,
                                          game->terminal,
                                          UINT64_C(0xc0ffee));

    if (status == AFORC_OK)
    {
        source_initialized = true;
        status = game_smoke_prepare_exact_source(&source);
    }
    if (status == AFORC_OK)
    {
        status = game_encode_save(&source, &blob);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_init(&reader,
                                        blob.data,
                                        blob.size,
                                        GAME_SAVE_MAX_BYTES,
                                        GAME_SAVE_LEGACY_SCHEMA,
                                        GAME_SAVE_SCHEMA);
        if (status == AFORC_OK && (GAME_SAVE_SCHEMA != 2 ||
                                   reader.schema_version != GAME_SAVE_SCHEMA))
        {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK)
    {
        status = game_initialize(
            &loaded, game->renderer, game->input, game->terminal, UINT64_C(1));
        loaded_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK)
    {
        status = game_decode_save(&loaded, blob.data, blob.size);
    }
    if (status == AFORC_OK)
    {
        status = game_actor_components(&source,
                                       source.player,
                                       &source_player_position,
                                       &source_player_actor);
    }
    if (status == AFORC_OK)
    {
        status = game_actor_components(&loaded,
                                       loaded.player,
                                       &loaded_player_position,
                                       &loaded_player_actor);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_gather_enemies(
            &source, source_enemies, &source_enemy_count);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_gather_enemies(
            &loaded, loaded_enemies, &loaded_enemy_count);
    }
    if (status == AFORC_OK &&
        (source.seed != loaded.seed || source.floor != loaded.floor ||
         source.score != loaded.score || source.turn != loaded.turn ||
         source.cell_count != loaded.cell_count ||
         memcmp(source.explored, loaded.explored, source.cell_count) != 0 ||
         !aforc_world_point_equal(source_player_position->point,
                                  loaded_player_position->point) ||
         !game_smoke_actor_equal(source_player_actor, loaded_player_actor) ||
         source_enemy_count != loaded_enemy_count ||
         !game_smoke_snapshots_equal(
             source_enemies, loaded_enemies, source_enemy_count)))
    {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "schema 2 save did not restore exact run state");
    }
    if (status == AFORC_OK)
    {
        AFORC_Ecs *const previous_ecs = loaded.ecs;
        AFORC_TileMap *const previous_map = loaded.map;
        const uint64_t previous_seed = loaded.seed;
        const uint32_t previous_floor = loaded.floor;
        const uint32_t previous_score = loaded.score;
        const uint32_t previous_turn = loaded.turn;
        const AFORC_Entity previous_player = loaded.player;
        const uint8_t previous_explored = loaded.explored[0];

        source_player_position->point.x = -1;
        status = game_encode_save(&source, &hostile_blob);
        if (status == AFORC_OK)
        {
            const AFORC_Status decode_status =
                game_decode_save(&loaded, hostile_blob.data, hostile_blob.size);

            if (decode_status != AFORC_ERROR_FORMAT ||
                loaded.ecs != previous_ecs || loaded.map != previous_map ||
                loaded.seed != previous_seed ||
                loaded.floor != previous_floor ||
                loaded.score != previous_score ||
                loaded.turn != previous_turn ||
                !aforc_entity_equal(loaded.player, previous_player) ||
                loaded.explored[0] != previous_explored)
            {
                status =
                    game_error(error,
                               AFORC_ERROR_STATE,
                               "smoke",
                               "hostile save was not rejected transactionally");
            }
        }
    }
    aforc_asset_blob_release(&hostile_blob);
    aforc_asset_blob_release(&blob);
    if (loaded_initialized)
    {
        game_dispose(&loaded);
    }
    if (source_initialized)
    {
        game_dispose(&source);
    }
    return status;
}

static AFORC_Status game_smoke_legacy_save(Game *game, AFORC_Error *error)
{
    Game loaded = {0};
    AFORC_SaveWriter writer = {0};
    AFORC_AssetBlob blob = {NULL, 0U};
    GamePosition *position = NULL;
    GameActor *actor = NULL;
    const uint64_t seed = UINT64_C(0x123456789abcdef0);
    const uint32_t floor = 2U;
    const int32_t health = 520;
    const int32_t expected_health =
        game->rules.player_health < health ? game->rules.player_health : health;
    const uint32_t score = UINT32_C(0x13579bdf);
    const uint32_t turn = UINT32_C(0x2468ace0);
    bool initialized = false;
    AFORC_Status status = aforc_save_writer_init(
        &writer, GAME_SAVE_LEGACY_SCHEMA, GAME_SAVE_MAX_BYTES);

    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u64(&writer, seed);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u32(&writer, floor);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_i32(&writer, health);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u32(&writer, score);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u32(&writer, turn);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_finish(&writer, &blob);
    }
    if (status == AFORC_OK)
    {
        status = game_initialize(
            &loaded, game->renderer, game->input, game->terminal, UINT64_C(1));
        initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK)
    {
        status = game_decode_save(&loaded, blob.data, blob.size);
    }
    if (status == AFORC_OK)
    {
        status =
            game_actor_components(&loaded, loaded.player, &position, &actor);
    }
    if (status == AFORC_OK && (loaded.seed != seed || loaded.floor != floor ||
                               loaded.score != score || loaded.turn != turn ||
                               actor->health != expected_health))
    {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "schema 1 checkpoint compatibility regressed");
    }
    if (initialized)
    {
        game_dispose(&loaded);
    }
    aforc_asset_blob_release(&blob);
    aforc_save_writer_release(&writer);
    return status;
}

static AFORC_Status game_smoke_seed_parser(AFORC_Error *error)
{
    static const char *const invalid_seeds[] = {
        "", "-1", "+1", " 1", "1 ", "1x", "18446744073709551616"};
    uint64_t seed = UINT64_C(7);
    AFORC_Status status = game_parse_seed("0", &seed);

    if (status != AFORC_OK || seed != 0U)
    {
        return game_error(
            error, AFORC_ERROR_STATE, "smoke", "zero seed was not parsed");
    }
    status = game_parse_seed("18446744073709551615", &seed);
    if (status != AFORC_OK || seed != UINT64_MAX)
    {
        return game_error(
            error, AFORC_ERROR_STATE, "smoke", "maximum seed was not parsed");
    }
    for (size_t index = 0U;
         index < sizeof(invalid_seeds) / sizeof(invalid_seeds[0]);
         ++index)
    {
        seed = UINT64_C(7);
        status = game_parse_seed(invalid_seeds[index], &seed);
        if (status != AFORC_ERROR_FORMAT || seed != UINT64_C(7))
        {
            return game_error(
                error, AFORC_ERROR_STATE, "smoke", "invalid seed was accepted");
        }
    }
    return AFORC_OK;
}

static AFORC_Status game_smoke_repeated_wait_is_dispatched(Game *game,
                                                           AFORC_Engine *engine,
                                                           AFORC_Error *error)
{
    AFORC_InputEvent event = {0};
    const uint32_t turn = game->turn;
    bool consumed = false;
    AFORC_Status status;

    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_SPACE;
    event.data.key.repeat = true;
    status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    if (status == AFORC_OK && (!consumed || game->turn == turn))
    {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "repeated wait keydown was ignored");
    }
    if (status == AFORC_OK)
    {
        game->turn = UINT32_MAX;
        consumed = false;
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
        if (status == AFORC_OK && (!consumed || game->turn != UINT32_MAX))
        {
            status = game_error(
                error, AFORC_ERROR_STATE, "smoke", "turn counter wrapped");
        }
        game->turn = turn + 1U;
    }
    return status;
}

static AFORC_Status game_smoke_repeated_help_is_stable(Game *game,
                                                       AFORC_Engine *engine,
                                                       AFORC_Error *error)
{
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
    if (status == AFORC_OK && (!consumed || !game->help_visible))
    {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "help keydown did not open help");
    }
    event.data.key.repeat = true;
    consumed = false;
    if (status == AFORC_OK)
    {
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    }
    if (status == AFORC_OK && (!consumed || !game->help_visible))
    {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "repeated help keydown changed modal state");
    }
    game->help_visible = help_visible;
    (void)memcpy(game->message, message, sizeof(message));
    return status;
}

static AFORC_Status game_smoke_save_failure_is_recoverable(Game *game,
                                                           AFORC_Engine *engine,
                                                           AFORC_Error *error)
{
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
        (!consumed || strcmp(game->message, "Save failed: not found.") != 0))
    {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "save failure did not remain in game");
    }
    return status;
}

static AFORC_Status game_smoke_short_save_is_recoverable(Game *game,
                                                         AFORC_Error *error)
{
    AFORC_SaveWriter writer = {0};
    AFORC_AssetBlob blob = {NULL, 0U};
    const uint64_t seed = game->seed;
    const uint32_t floor = game->floor;
    const uint32_t score = game->score;
    const uint32_t turn = game->turn;
    AFORC_Status status =
        aforc_save_writer_init(&writer, GAME_SAVE_SCHEMA, GAME_SAVE_MAX_BYTES);

    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u64(&writer, seed);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_finish(&writer, &blob);
    }
    if (status == AFORC_OK)
    {
        const AFORC_Status decode_status =
            game_decode_save(game, blob.data, blob.size);

        if (decode_status != AFORC_ERROR_FORMAT || game->seed != seed ||
            game->floor != floor || game->score != score || game->turn != turn)
        {
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

static AFORC_Status game_smoke_runtime_ownership(Game *game, AFORC_Error *error)
{
    if (game->particle_pool.particles != game->particles ||
        game->scene.user_data != game || game->save_path == NULL)
    {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "loaded runtime retained stale ownership pointers");
    }
    return AFORC_OK;
}

static AFORC_Status
game_smoke_effect_timing(Game *game, AFORC_Engine *engine, AFORC_Error *error)
{
    AFORC_Particle particle_storage[1];
    AFORC_ParticleDesc particle = {0};
    Game probe = *game;
    size_t particle_index = 0U;
    AFORC_Status status;

    probe.scene.user_data = &probe;
    probe.particle_pool = (AFORC_ParticlePool){0};
    probe.exit_tween = (AFORC_Tween){0};
    probe.particle_millisecond_remainder = 0.0;
    probe.tween_millisecond_remainder = 0.0;
    status = aforc_particle_pool_init(
        &probe.particle_pool, particle_storage, 1U, 1U);
    if (status == AFORC_OK)
    {
        status = aforc_tween_init(
            &probe.exit_tween, 0.0, 1.0, 1000U, AFORC_EASING_LINEAR);
    }
    particle.lifetime_ms = 1000U;
    particle.cell = aforc_cell_default();
    if (status == AFORC_OK)
    {
        status = aforc_particle_pool_spawn(
            &probe.particle_pool, &particle, &particle_index);
    }
    for (size_t tick = 0U; status == AFORC_OK && tick < 30U; ++tick)
    {
        status =
            game_scene_fixed_update(&probe.scene, engine, 1.0 / 60.0, error);
        if (status == AFORC_OK)
        {
            status =
                game_scene_update(&probe.scene, engine, 1.0 / 120.0, error);
        }
    }
    if (status == AFORC_OK &&
        (particle_storage[particle_index].age_ms != 500U ||
         probe.exit_tween.elapsed_ms != 250U))
    {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "effect timing accumulated per-frame rounding");
    }
    aforc_tween_dispose(&probe.exit_tween);
    aforc_particle_pool_dispose(&probe.particle_pool);
    return status;
}

AFORC_Status
game_runtime_smoke_checks(Game *game, AFORC_Engine *engine, AFORC_Error *error)
{
    AFORC_Status status = game_smoke_seed_parser(error);

    if (status == AFORC_OK)
    {
        status = game_smoke_repeated_wait_is_dispatched(game, engine, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_repeated_help_is_stable(game, engine, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_save_failure_is_recoverable(game, engine, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_short_save_is_recoverable(game, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_exact_save_round_trip(game, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_legacy_save(game, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_runtime_ownership(game, error);
    }
    if (status == AFORC_OK)
    {
        status = game_smoke_effect_timing(game, engine, error);
    }
    return status;
}

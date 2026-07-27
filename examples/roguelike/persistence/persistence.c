/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <string.h>

enum {
    GAME_SAVE_MAX_ACTOR_STAT = 1000
};

typedef struct GameSaveActorSnapshot {
    AFORC_Point point;
    GameActor actor;
} GameSaveActorSnapshot;

typedef struct GameSaveState {
    uint64_t seed;
    uint32_t floor;
    uint32_t score;
    uint32_t turn;
    uint32_t map_width;
    uint32_t map_height;
    uint8_t *explored;
    size_t explored_count;
    GameSaveActorSnapshot player;
    GameSaveActorSnapshot enemies[GAME_MAX_ENEMIES];
    size_t enemy_count;
} GameSaveState;

static AFORC_Status game_save_write_actor(
    AFORC_SaveWriter *writer,
    const GameSaveActorSnapshot *snapshot) {
    AFORC_Status status = aforc_save_writer_write_i32(writer,
                                                      snapshot->point.x);

    if (status == AFORC_OK) {
        status = aforc_save_writer_write_i32(writer, snapshot->point.y);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_i32(writer, snapshot->actor.health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_i32(writer,
                                             snapshot->actor.maximum_health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_i32(writer, snapshot->actor.attack);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(writer, snapshot->actor.glyph);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(
            writer,
            (uint32_t)snapshot->actor.color.mode);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u8(writer,
                                            snapshot->actor.color.red);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u8(writer,
                                            snapshot->actor.color.green);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u8(writer,
                                            snapshot->actor.color.blue);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u8(writer,
                                            snapshot->actor.hostile ? 1U : 0U);
    }
    return status;
}

static AFORC_Status game_save_read_actor(
    AFORC_SaveReader *reader,
    GameSaveActorSnapshot *snapshot) {
    uint32_t color_mode = 0U;
    uint8_t hostile = 0U;
    AFORC_Status status = aforc_save_reader_read_i32(reader,
                                                     &snapshot->point.x);

    if (status == AFORC_OK) {
        status = aforc_save_reader_read_i32(reader, &snapshot->point.y);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_i32(reader, &snapshot->actor.health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_i32(
            reader,
            &snapshot->actor.maximum_health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_i32(reader, &snapshot->actor.attack);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &snapshot->actor.glyph);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &color_mode);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u8(reader, &snapshot->actor.color.red);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u8(reader,
                                           &snapshot->actor.color.green);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u8(reader, &snapshot->actor.color.blue);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u8(reader, &hostile);
    }
    if (status == AFORC_OK &&
        (color_mode > (uint32_t)AFORC_COLOR_RGB || hostile > 1U)) {
        return AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_OK) {
        snapshot->actor.color.mode = (AFORC_ColorMode)color_mode;
        snapshot->actor.hostile = hostile != 0U;
    }
    return status;
}

static AFORC_Status game_save_capture_enemies(
    const Game *game,
    GameSaveActorSnapshot *snapshots,
    size_t *out_count) {
    const AFORC_ComponentType types[2] = {game->position_type,
                                         game->actor_type};
    AFORC_EcsView *view = NULL;
    size_t count = 0U;
    AFORC_Status status = aforc_ecs_view_create(game->ecs, types, 2U, &view);

    while (status == AFORC_OK) {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        bool has_value = false;
        const GameActor *actor;

        status = aforc_ecs_view_next(view,
                                     &entity,
                                     components,
                                     &has_value);
        if (status != AFORC_OK || !has_value) {
            break;
        }
        actor = components[1];
        if (!actor->hostile) {
            continue;
        }
        if (count == GAME_MAX_ENEMIES) {
            status = AFORC_ERROR_LIMIT;
            break;
        }
        snapshots[count].point = ((GamePosition *)components[0])->point;
        snapshots[count].actor = *actor;
        ++count;
    }
    aforc_ecs_view_destroy(view);
    if (status == AFORC_OK &&
        aforc_ecs_entity_count(game->ecs) != count + 1U) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        *out_count = count;
    }
    return status;
}

AFORC_Status game_encode_save(const Game *game, AFORC_AssetBlob *out_blob) {
    GameSaveActorSnapshot enemies[GAME_MAX_ENEMIES];
    GameSaveActorSnapshot player;
    const void *position_component = NULL;
    const void *actor_component = NULL;
    AFORC_SaveWriter writer = {0};
    AFORC_Size dimensions = {0, 0};
    uint32_t layer_count = 0U;
    size_t enemy_count = 0U;
    AFORC_Status status = aforc_ecs_get_const(game->ecs,
                                              game->player,
                                              game->position_type,
                                              &position_component);

    if (status == AFORC_OK) {
        status = aforc_ecs_get_const(game->ecs,
                                     game->player,
                                     game->actor_type,
                                     &actor_component);
    }
    if (status == AFORC_OK) {
        status = aforc_tilemap_get_dimensions(game->map,
                                              &dimensions,
                                              &layer_count);
    }
    if (status == AFORC_OK &&
        (dimensions.width <= 0 || dimensions.height <= 0 ||
         layer_count != 1U || game->cell_count > UINT32_MAX)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        player.point = ((const GamePosition *)position_component)->point;
        player.actor = *(const GameActor *)actor_component;
        status = game_save_capture_enemies(game, enemies, &enemy_count);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_init(&writer,
                                        GAME_SAVE_SCHEMA,
                                        GAME_SAVE_MAX_BYTES);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u64(&writer, game->seed);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, game->floor);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, game->score);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, game->turn);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer,
                                             (uint32_t)dimensions.width);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer,
                                             (uint32_t)dimensions.height);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer,
                                             (uint32_t)game->cell_count);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_bytes(&writer,
                                               game->explored,
                                               game->cell_count);
    }
    if (status == AFORC_OK) {
        status = game_save_write_actor(&writer, &player);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, (uint32_t)enemy_count);
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < enemy_count;
         ++index) {
        status = game_save_write_actor(&writer, &enemies[index]);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_finish(&writer, out_blob);
    }
    aforc_save_writer_release(&writer);
    return status;
}

static bool game_save_point_in_bounds(const GameSaveState *state,
                                      AFORC_Point point) {
    return point.x >= 0 && point.y >= 0 &&
           (uint32_t)point.x < state->map_width &&
           (uint32_t)point.y < state->map_height;
}

static bool game_save_actor_stats_valid(const GameActor *actor) {
    return actor->health > 0 && actor->maximum_health > 0 &&
           actor->health <= actor->maximum_health && actor->attack > 0 &&
           actor->maximum_health <= GAME_SAVE_MAX_ACTOR_STAT &&
           actor->attack <= GAME_SAVE_MAX_ACTOR_STAT;
}

static AFORC_Status game_save_validate_exact(const Game *game,
                                             const GameSaveState *state) {
    if (state->floor == 0U || state->floor > game->rules.final_floor ||
        state->map_width != (uint32_t)game->rules.map_width ||
        state->map_height != (uint32_t)game->rules.map_height ||
        state->explored_count != game->cell_count ||
        state->enemy_count > GAME_MAX_ENEMIES ||
        !game_save_point_in_bounds(state, state->player.point) ||
        !game_save_actor_stats_valid(&state->player.actor) ||
        state->player.actor.hostile ||
        state->player.actor.glyph != (uint32_t)'@') {
        return AFORC_ERROR_FORMAT;
    }
    for (size_t index = 0U; index < state->explored_count; ++index) {
        if (state->explored[index] > 1U) {
            return AFORC_ERROR_FORMAT;
        }
    }
    for (size_t index = 0U; index < state->enemy_count; ++index) {
        const GameSaveActorSnapshot *enemy = &state->enemies[index];

        if (!game_save_point_in_bounds(state, enemy->point) ||
            !game_save_actor_stats_valid(&enemy->actor) ||
            !enemy->actor.hostile ||
            (enemy->actor.glyph != (uint32_t)'S' &&
             enemy->actor.glyph != (uint32_t)'g') ||
            aforc_world_point_equal(enemy->point, state->player.point)) {
            return AFORC_ERROR_FORMAT;
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (aforc_world_point_equal(enemy->point,
                                        state->enemies[previous].point)) {
                return AFORC_ERROR_FORMAT;
            }
        }
    }
    return AFORC_OK;
}

static void game_save_state_release(const AFORC_Allocator *allocator,
                                    GameSaveState *state) {
    aforc_free(allocator, state->explored);
    state->explored = NULL;
    state->explored_count = 0U;
}

static AFORC_Status game_save_read_exact(Game *game,
                                         AFORC_SaveReader *reader,
                                         GameSaveState *state) {
    uint32_t explored_count = 0U;
    uint32_t enemy_count = 0U;
    AFORC_Status status = aforc_save_reader_read_u64(reader, &state->seed);

    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &state->floor);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &state->score);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &state->turn);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &state->map_width);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &state->map_height);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &explored_count);
    }
    if (status == AFORC_OK &&
        (game->cell_count > UINT32_MAX ||
         explored_count != (uint32_t)game->cell_count ||
         state->map_width != (uint32_t)game->rules.map_width ||
         state->map_height != (uint32_t)game->rules.map_height)) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_OK) {
        state->explored_count = explored_count;
        status = aforc_alloc_array(&game->allocator,
                                   state->explored_count,
                                   sizeof(*state->explored),
                                   (void **)&state->explored);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_bytes(reader,
                                              state->explored,
                                              state->explored_count);
    }
    if (status == AFORC_OK) {
        status = game_save_read_actor(reader, &state->player);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &enemy_count);
    }
    if (status == AFORC_OK && enemy_count > GAME_MAX_ENEMIES) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_OK) {
        state->enemy_count = enemy_count;
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < state->enemy_count;
         ++index) {
        status = game_save_read_actor(reader, &state->enemies[index]);
    }
    if (status == AFORC_OK && !aforc_save_reader_finished(reader)) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_ERROR_END_OF_STREAM) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_OK) {
        status = game_save_validate_exact(game, state);
    }
    return status;
}

static AFORC_Status game_save_validate_tiles(
    const Game *game,
    const GameSaveState *state) {
    AFORC_Tile tile = TILE_WALL;
    AFORC_Status status = aforc_tilemap_get(game->map,
                                            0U,
                                            state->player.point,
                                            &tile);

    if (status == AFORC_OK && tile == TILE_WALL) {
        return AFORC_ERROR_FORMAT;
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < state->enemy_count;
         ++index) {
        status = aforc_tilemap_get(game->map,
                                   0U,
                                   state->enemies[index].point,
                                   &tile);
        if (status == AFORC_OK && tile == TILE_WALL) {
            return AFORC_ERROR_FORMAT;
        }
    }
    return status;
}

static AFORC_Status game_replace_exact(Game *game,
                                       const GameSaveState *state) {
    Game replacement = {0};
    const char *save_path = game->save_path;
    AFORC_Status status = game_initialize(&replacement,
                                          game->renderer,
                                          game->input,
                                          game->terminal,
                                          state->seed);

    if (status != AFORC_OK) {
        return status;
    }
    replacement.save_path = save_path;
    status = game_generate_floor(&replacement,
                                 state->floor,
                                 state->player.actor.health);
    if (status == AFORC_OK) {
        status = game_save_validate_tiles(&replacement, state);
    }
    if (status == AFORC_OK) {
        status = aforc_ecs_clear(replacement.ecs);
    }
    if (status == AFORC_OK) {
        status = game_create_actor(&replacement,
                                   state->player.point,
                                   state->player.actor,
                                   &replacement.player);
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < state->enemy_count;
         ++index) {
        status = game_create_actor(&replacement,
                                   state->enemies[index].point,
                                   state->enemies[index].actor,
                                   NULL);
    }
    if (status != AFORC_OK) {
        game_dispose(&replacement);
        return status;
    }
    (void)memcpy(replacement.explored,
                 state->explored,
                 state->explored_count);
    (void)memset(replacement.visibility, 0, replacement.cell_count);
    replacement.score = state->score;
    replacement.turn = state->turn;
    game_set_message(&replacement, "Run loaded from %s.", save_path);

    game_dispose(game);
    *game = replacement;
    game->particle_pool.particles = game->particles;
    game->scene.user_data = game;
    return AFORC_OK;
}

static AFORC_Status game_replace_legacy(Game *game,
                                        uint64_t seed,
                                        uint32_t floor,
                                        int32_t health,
                                        uint32_t score,
                                        uint32_t turn) {
    Game replacement = {0};
    const char *save_path = game->save_path;
    AFORC_Status status = game_initialize(&replacement,
                                          game->renderer,
                                          game->input,
                                          game->terminal,
                                          seed);

    if (status != AFORC_OK) {
        return status;
    }
    replacement.save_path = save_path;
    status = game_generate_floor(&replacement, floor, health);
    if (status != AFORC_OK) {
        game_dispose(&replacement);
        return status;
    }
    replacement.score = score;
    replacement.turn = turn;
    game_set_message(&replacement, "Run loaded from %s.", save_path);

    game_dispose(game);
    *game = replacement;
    game->particle_pool.particles = game->particles;
    game->scene.user_data = game;
    return AFORC_OK;
}

static AFORC_Status game_decode_legacy(Game *game, AFORC_SaveReader *reader) {
    uint64_t seed = 0U;
    uint32_t floor = 0U;
    int32_t health = 0;
    uint32_t score = 0U;
    uint32_t turn = 0U;
    AFORC_Status status = aforc_save_reader_read_u64(reader, &seed);

    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &floor);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_i32(reader, &health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &score);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(reader, &turn);
    }
    if (status == AFORC_OK && !aforc_save_reader_finished(reader)) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_ERROR_END_OF_STREAM) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_OK &&
        (floor == 0U || floor > game->rules.final_floor || health <= 0 ||
         health > GAME_SAVE_MAX_ACTOR_STAT)) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status != AFORC_OK) {
        return status;
    }
    return game_replace_legacy(game, seed, floor, health, score, turn);
}

AFORC_Status game_decode_save(Game *game, const void *data, size_t size) {
    AFORC_SaveReader reader = {0};
    GameSaveState state = {0};
    const AFORC_Allocator allocator = game->allocator;
    AFORC_Status status = aforc_save_reader_init(&reader,
                                                 data,
                                                 size,
                                                 GAME_SAVE_MAX_BYTES,
                                                 GAME_SAVE_LEGACY_SCHEMA,
                                                 GAME_SAVE_SCHEMA);

    if (status != AFORC_OK) {
        return status;
    }
    if (reader.schema_version == GAME_SAVE_LEGACY_SCHEMA) {
        return game_decode_legacy(game, &reader);
    }
    status = game_save_read_exact(game, &reader, &state);
    if (status == AFORC_OK) {
        status = game_replace_exact(game, &state);
    }
    game_save_state_release(&allocator, &state);
    return status;
}

AFORC_Status game_save(Game *game) {
    AFORC_AssetBlob blob = {NULL, 0U};
    AFORC_AssetPathPolicy policy = aforc_asset_path_policy_default();
    AFORC_Status status = game_encode_save(game, &blob);

    policy.allow_hidden_components = true;
    if (status == AFORC_OK) {
        status = aforc_asset_store_binary_atomic(".",
                                                 game->save_path,
                                                 &policy,
                                                 blob.data,
                                                 blob.size,
                                                 GAME_SAVE_MAX_FILE_BYTES);
    }
    aforc_asset_blob_release(&blob);
    if (status == AFORC_OK) {
        game_set_message(game, "Run saved to %s.", game->save_path);
    }
    return status;
}

AFORC_Status game_load(Game *game) {
    AFORC_AssetBlob blob = {NULL, 0U};
    AFORC_AssetPathPolicy policy = aforc_asset_path_policy_default();
    AFORC_Status status;

    policy.allow_hidden_components = true;
    status = aforc_asset_load_binary(".",
                                   game->save_path,
                                   &policy,
                                   GAME_SAVE_MAX_FILE_BYTES,
                                   &blob);
    if (status == AFORC_OK) {
        status = game_decode_save(game, blob.data, blob.size);
    }
    aforc_asset_blob_release(&blob);
    if (status == AFORC_ERROR_NOT_FOUND) {
        game_set_message(game, "No saved run found at %s.", game->save_path);
        return AFORC_OK;
    }
    return status;
}

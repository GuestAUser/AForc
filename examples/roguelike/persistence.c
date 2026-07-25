/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

AFORC_Status game_encode_save(const Game *game, AFORC_AssetBlob *out_blob) {
    const GameActor *actor = NULL;
    const void *actor_component = NULL;
    AFORC_SaveWriter writer;
    AFORC_Status status = aforc_ecs_get_const(game->ecs,
                                          game->player,
                                          game->actor_type,
                                          &actor_component);

    if (status != AFORC_OK) {
        return status;
    }
    actor = actor_component;
    status = aforc_save_writer_init(&writer,
                                  GAME_SAVE_SCHEMA,
                                  GAME_SAVE_MAX_BYTES);
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u64(&writer, game->seed);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, game->floor);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_i32(&writer, actor->health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, game->score);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, game->turn);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_finish(&writer, out_blob);
    }
    aforc_save_writer_release(&writer);
    return status;
}

AFORC_Status game_decode_save(Game *game, const void *data, size_t size) {
    AFORC_SaveReader reader;
    uint64_t seed = 0U;
    uint32_t floor = 0U;
    int32_t health = 0;
    uint32_t score = 0U;
    uint32_t turn = 0U;
    AFORC_Status status = aforc_save_reader_init(&reader,
                                             data,
                                             size,
                                             GAME_SAVE_MAX_BYTES,
                                             GAME_SAVE_SCHEMA,
                                             GAME_SAVE_SCHEMA);

    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u64(&reader, &seed);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(&reader, &floor);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_i32(&reader, &health);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(&reader, &score);
    }
    if (status == AFORC_OK) {
        status = aforc_save_reader_read_u32(&reader, &turn);
    }
    if (status == AFORC_OK && !aforc_save_reader_finished(&reader)) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status == AFORC_OK &&
        (floor == 0U || floor > game->rules.final_floor || health <= 0 ||
         health > game->rules.player_health)) {
        status = AFORC_ERROR_FORMAT;
    }
    if (status != AFORC_OK) {
        return status;
    }
    game->seed = seed;
    /* Generation rebuilds deterministic state before dynamic fields are restored. */
    status = game_generate_floor(game, floor, health);
    if (status == AFORC_OK) {
        game->score = score;
        game->turn = turn;
        game_set_message(game, "Run loaded from %s.", game->save_path);
    }
    return status;
}

AFORC_Status game_save(Game *game) {
    AFORC_AssetBlob blob = {NULL, 0U};
    AFORC_AssetPathPolicy policy = aforc_asset_path_policy_default();
    AFORC_Status status = game_encode_save(game, &blob);

    policy.allow_hidden_components = true;
    if (status == AFORC_OK) {
        status = aforc_asset_store_binary(".",
                                        game->save_path,
                                        &policy,
                                        blob.data,
                                        blob.size,
                                        GAME_SAVE_MAX_BYTES);
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
                                   GAME_SAVE_MAX_BYTES,
                                   &blob);
    if (status == AFORC_ERROR_NOT_FOUND) {
        game_set_message(game,
                         "No saved run found at %s.",
                         game->save_path);
        return AFORC_OK;
    }
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

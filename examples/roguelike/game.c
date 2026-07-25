/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char game_configuration[] =
    "[game]\n"
    "map_width=72\n"
    "map_height=36\n"
    "room_count=15\n"
    "enemy_count=12\n"
    "fov_radius=12\n"
    "player_health=720\n"
    "player_attack=5\n"
    "enemy_health=8\n"
    "enemy_attack=3\n"
    "final_floor=5\n";

static const char game_save_path[] = ".aforc-roguelike.sav";

AFORC_Status game_error(AFORC_Error *error,
                      AFORC_Status status,
                      const char *subsystem,
                      const char *message) {
    aforc_error_set(error, status, subsystem, "%s", message);
    return status;
}

void game_set_message(Game *game, const char *format, ...) {
    va_list arguments;

    va_start(arguments, format);
    (void)vsnprintf(game->message, sizeof(game->message), format, arguments);
    va_end(arguments);
}

static AFORC_Status game_parse_u32(const char *text,
                                 uint32_t minimum,
                                 uint32_t maximum,
                                 uint32_t *out_value) {
    char *end = NULL;
    unsigned long long value = 0U;

    if (text == NULL || out_value == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < minimum ||
        value > maximum) {
        return AFORC_ERROR_FORMAT;
    }
    *out_value = (uint32_t)value;
    return AFORC_OK;
}

static AFORC_Status game_rule(const AFORC_Config *config,
                            const char *key,
                            uint32_t minimum,
                            uint32_t maximum,
                            uint32_t *out_value) {
    return game_parse_u32(aforc_config_get(config, "game", key),
                          minimum,
                          maximum,
                          out_value);
}

static AFORC_Status game_load_rules(GameRules *rules) {
    AFORC_Config config = {NULL, 0U};
    AFORC_ConfigLimits limits = aforc_config_limits_default();
    uint32_t map_width = 0U;
    uint32_t map_height = 0U;
    uint32_t player_health = 0U;
    uint32_t player_attack = 0U;
    uint32_t enemy_health = 0U;
    uint32_t enemy_attack = 0U;
    AFORC_Status status;

    limits.max_input_bytes = sizeof(game_configuration) - 1U;
    limits.max_line_bytes = 64U;
    limits.max_entries = 16U;
    limits.max_section_bytes = 16U;
    limits.max_key_bytes = 32U;
    limits.max_value_bytes = 16U;
    status = aforc_config_parse(game_configuration,
                              sizeof(game_configuration) - 1U,
                              &limits,
                              &config);
    if (status == AFORC_OK) {
        status = game_rule(&config, "map_width", 40U, 120U, &map_width);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config, "map_height", 20U, 60U, &map_height);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "room_count",
                           4U,
                           GAME_MAX_ROOMS,
                           &rules->room_count);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "enemy_count",
                           1U,
                           GAME_MAX_ENEMIES - 10U,
                           &rules->enemy_count);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config, "fov_radius", 4U, 32U, &rules->fov_radius);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "player_health",
                           1U,
                           1000U,
                           &player_health);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "player_attack",
                           1U,
                           100U,
                           &player_attack);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "enemy_health",
                           1U,
                           100U,
                           &enemy_health);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "enemy_attack",
                           1U,
                           100U,
                           &enemy_attack);
    }
    if (status == AFORC_OK) {
        status = game_rule(&config,
                           "final_floor",
                           1U,
                           20U,
                           &rules->final_floor);
    }
    aforc_config_release(&config);
    if (status != AFORC_OK) {
        return status;
    }
    rules->map_width = (int32_t)map_width;
    rules->map_height = (int32_t)map_height;
    rules->player_health = (int32_t)player_health;
    rules->player_attack = (int32_t)player_attack;
    rules->enemy_health = (int32_t)enemy_health;
    rules->enemy_attack = (int32_t)enemy_attack;
    return AFORC_OK;
}

bool game_tile_blocks(AFORC_Tile tile,
                      uint32_t layer,
                      AFORC_Point position,
                      void *context) {
    (void)layer;
    (void)position;
    (void)context;
    return tile == TILE_WALL;
}

AFORC_Status game_create_actor(Game *game,
                             AFORC_Point position,
                             GameActor actor,
                             AFORC_Entity *out_entity) {
    GamePosition component_position = {position};
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_Status status = aforc_ecs_create_entity(game->ecs, &entity);

    if (status == AFORC_OK) {
        status = aforc_ecs_add(game->ecs,
                             entity,
                             game->position_type,
                             &component_position,
                             NULL);
    }
    if (status == AFORC_OK) {
        status = aforc_ecs_add(game->ecs,
                             entity,
                             game->actor_type,
                             &actor,
                             NULL);
    }
    if (status != AFORC_OK) {
        if (aforc_ecs_entity_alive(game->ecs, entity)) {
            (void)aforc_ecs_destroy_entity(game->ecs, entity);
        }
        return status;
    }
    if (out_entity != NULL) {
        *out_entity = entity;
    }
    return AFORC_OK;
}

AFORC_Status game_actor_components(Game *game,
                                 AFORC_Entity entity,
                                 GamePosition **out_position,
                                 GameActor **out_actor) {
    void *position = NULL;
    void *actor = NULL;
    AFORC_Status status;

    if (out_position == NULL || out_actor == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ecs_get(game->ecs,
                         entity,
                         game->position_type,
                         &position);
    if (status == AFORC_OK) {
        status = aforc_ecs_get(game->ecs, entity, game->actor_type, &actor);
    }
    if (status == AFORC_OK) {
        *out_position = position;
        *out_actor = actor;
    }
    return status;
}

AFORC_Status game_entity_at(Game *game,
                          AFORC_Point point,
                          AFORC_Entity *out_entity,
                          bool *out_found) {
    const AFORC_ComponentType types[2] = {game->position_type,
                                        game->actor_type};
    AFORC_EcsView *view = NULL;
    AFORC_Status status;
    bool has_value = false;

    if (out_entity == NULL || out_found == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_entity = AFORC_ENTITY_INVALID;
    *out_found = false;
    status = aforc_ecs_view_create(game->ecs, types, 2U, &view);
    while (status == AFORC_OK) {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        GamePosition *position;

        status = aforc_ecs_view_next(view,
                                   &entity,
                                   components,
                                   &has_value);
        if (status != AFORC_OK || !has_value) {
            break;
        }
        position = components[0];
        if (aforc_world_point_equal(position->point, point)) {
            *out_entity = entity;
            *out_found = true;
            break;
        }
    }
    aforc_ecs_view_destroy(view);
    return status;
}

void game_dispose(Game *game) {
    aforc_tween_dispose(&game->exit_tween);
    aforc_particle_pool_dispose(&game->particle_pool);
    aforc_path_workspace_destroy(game->path_workspace);
    aforc_tilemap_destroy(game->map);
    aforc_ecs_destroy(game->ecs);
    aforc_free(&game->allocator, game->explored);
    aforc_free(&game->allocator, game->visibility);
    game->map = NULL;
    game->path_workspace = NULL;
    game->ecs = NULL;
    game->explored = NULL;
    game->visibility = NULL;
}

AFORC_Status game_initialize(Game *game,
                           AFORC_Renderer *renderer,
                           AFORC_Input *input,
                           AFORC_Terminal *terminal,
                           uint64_t seed,
                           bool smoke_mode) {
    AFORC_EcsConfig ecs_config = aforc_ecs_config_default();
    AFORC_EcsComponentDesc position_desc;
    AFORC_EcsComponentDesc actor_desc;
    AFORC_Status status;

    (void)memset(game, 0, sizeof(*game));
    game->allocator = aforc_allocator_default();
    game->renderer = renderer;
    game->input = input;
    game->terminal = terminal;
    game->seed = seed;
    game->save_path = game_save_path;
    game->smoke_mode = smoke_mode;
    game->position_type = AFORC_COMPONENT_TYPE_INVALID;
    game->actor_type = AFORC_COMPONENT_TYPE_INVALID;
    game->player = AFORC_ENTITY_INVALID;
    game->scene.vtable = &game_scene_vtable;
    game->scene.user_data = game;
    status = game_load_rules(&game->rules);
    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_size_multiply((size_t)(uint32_t)game->rules.map_width,
                            (size_t)(uint32_t)game->rules.map_height,
                            &game->cell_count)) {
        return AFORC_ERROR_OVERFLOW;
    }
    status = aforc_path_workspace_create(&game->allocator,
                                         &game->path_workspace);
    if (status == AFORC_OK) {
        status = aforc_path_workspace_reserve(game->path_workspace,
                                              game->cell_count);
    }
    if (status == AFORC_OK) {
        status = aforc_alloc_array(&game->allocator,
                                  game->cell_count,
                                  sizeof(*game->visibility),
                                  (void **)&game->visibility);
    }
    if (status == AFORC_OK) {
        status = aforc_alloc_array(&game->allocator,
                                 game->cell_count,
                                 sizeof(*game->explored),
                                 (void **)&game->explored);
    }
    ecs_config.initial_entity_capacity = GAME_MAX_ENEMIES + 1U;
    ecs_config.max_entities = GAME_MAX_ENEMIES + 1U;
    ecs_config.initial_component_capacity = GAME_MAX_ENEMIES + 1U;
    ecs_config.initial_component_type_capacity = 2U;
    ecs_config.max_component_types = 2U;
    if (status == AFORC_OK) {
        status = aforc_ecs_create(&ecs_config, &game->ecs);
    }
    position_desc.size = sizeof(GamePosition);
    position_desc.alignment = _Alignof(GamePosition);
    position_desc.initial_capacity = GAME_MAX_ENEMIES + 1U;
    position_desc.cleanup = NULL;
    position_desc.cleanup_user_data = NULL;
    actor_desc.size = sizeof(GameActor);
    actor_desc.alignment = _Alignof(GameActor);
    actor_desc.initial_capacity = GAME_MAX_ENEMIES + 1U;
    actor_desc.cleanup = NULL;
    actor_desc.cleanup_user_data = NULL;
    if (status == AFORC_OK) {
        status = aforc_ecs_register_component(game->ecs,
                                            &position_desc,
                                            &game->position_type);
    }
    if (status == AFORC_OK) {
        status = aforc_ecs_register_component(game->ecs,
                                            &actor_desc,
                                            &game->actor_type);
    }
    if (status == AFORC_OK) {
        status = aforc_particle_pool_init(&game->particle_pool,
                                        game->particles,
                                        GAME_PARTICLE_CAPACITY,
                                        (uint32_t)(seed ^ (seed >> 32U)));
    }
    if (status == AFORC_OK) {
        status = aforc_tween_init(&game->exit_tween,
                                0.0,
                                1.0,
                                900U,
                                AFORC_EASING_QUADRATIC_IN_OUT);
    }
    if (status == AFORC_OK) {
        status = aforc_camera_init(&game->camera, (AFORC_Size){40, 20});
    }
    if (status == AFORC_OK) {
        status = game_new_run(game);
    }
    if (status != AFORC_OK) {
        game_dispose(game);
    }
    return status;
}

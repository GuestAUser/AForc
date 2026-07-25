/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
                             uint64_t seed) {
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

/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

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

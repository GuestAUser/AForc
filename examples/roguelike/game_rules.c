/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>
#include <stdlib.h>

static const char game_configuration[] =
    "[game]\n"
    "map_width=72\n"
    "map_height=36\n"
    "room_count=15\n"
    "enemy_count=12\n"
    "fov_radius=12\n"
    "player_health=520\n"
    "player_attack=5\n"
    "enemy_health=8\n"
    "enemy_attack=3\n"
    "final_floor=5\n";

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

AFORC_Status game_load_rules(GameRules *rules) {
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

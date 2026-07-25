/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/game.h"

SurfManSettings surf_man_settings_default(void) {
    SurfManSettings settings;

    settings.speed_percent = 100U;
    settings.timing_percent = 100U;
    settings.landing_assist = false;
    settings.reduced_motion = false;
    settings.color_mode = SURF_MAN_COLOR_STANDARD;
    return settings;
}

SurfManRules surf_man_rules_default(void) {
    SurfManRules rules;

    rules.wave_ticks = SURF_MAN_WAVE_SECONDS * SURF_MAN_FIXED_HZ;
    rules.count_in_ticks = 3U * SURF_MAN_FIXED_HZ;
    rules.recovery_ticks = 2U * SURF_MAN_FIXED_HZ;
    rules.bank_delay_ticks = (5U * SURF_MAN_FIXED_HZ) / 2U;
    rules.minimum_speed_q16 = 2 * SURF_MAN_Q16_ONE;
    rules.maximum_speed_q16 = 8 * SURF_MAN_Q16_ONE;
    rules.gravity_q16 = 6 * SURF_MAN_Q16_ONE;
    rules.pop_velocity_q16 = 4 * SURF_MAN_Q16_ONE;
    rules.landing_tolerance_q16 = SURF_MAN_Q16_ONE / 8;
    return rules;
}

void surf_man_command_clear(SurfManCommand *command) {
    if (command != NULL) {
        *command = (SurfManCommand){0};
    }
}

const char *surf_man_wave_kind_name(SurfManWaveKind kind) {
    switch (kind) {
        case SURF_MAN_WAVE_OPEN:
            return "OPEN";
        case SURF_MAN_WAVE_STEEP:
            return "STEEP";
        case SURF_MAN_WAVE_TUBE:
            return "TUBE";
        case SURF_MAN_WAVE_CHOP:
            return "CHOP";
        case SURF_MAN_WAVE_CLOSEOUT:
            return "CLOSEOUT";
        default:
            return "UNKNOWN";
    }
}

static void surf_man_hash_u64(uint64_t *hash, uint64_t value) {
    for (uint32_t shift = 0U; shift < 64U; shift += 8U) {
        *hash ^= (value >> shift) & UINT64_C(0xff);
        *hash *= UINT64_C(1099511628211);
    }
}

static void surf_man_hash_u32(uint64_t *hash, uint32_t value) {
    surf_man_hash_u64(hash, value);
}

static void surf_man_hash_i32(uint64_t *hash, int32_t value) {
    surf_man_hash_u32(hash, (uint32_t)value);
}

uint64_t surf_man_simulation_hash(const SurfManSimulation *simulation) {
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t award_length = 0U;

    if (simulation == NULL) {
        return UINT64_C(0);
    }
    surf_man_hash_u32(&hash, simulation->rules.wave_ticks);
    surf_man_hash_u32(&hash, simulation->rules.count_in_ticks);
    surf_man_hash_u32(&hash, simulation->rules.recovery_ticks);
    surf_man_hash_u32(&hash, simulation->rules.bank_delay_ticks);
    surf_man_hash_i32(&hash, simulation->rules.minimum_speed_q16);
    surf_man_hash_i32(&hash, simulation->rules.maximum_speed_q16);
    surf_man_hash_i32(&hash, simulation->rules.gravity_q16);
    surf_man_hash_i32(&hash, simulation->rules.pop_velocity_q16);
    surf_man_hash_i32(&hash, simulation->rules.landing_tolerance_q16);
    surf_man_hash_u32(&hash, simulation->settings.speed_percent);
    surf_man_hash_u32(&hash, simulation->settings.timing_percent);
    surf_man_hash_u32(&hash, simulation->settings.landing_assist ? 1U : 0U);
    surf_man_hash_u32(&hash, simulation->settings.reduced_motion ? 1U : 0U);
    surf_man_hash_u32(&hash, (uint32_t)simulation->settings.color_mode);
    surf_man_hash_u64(&hash, simulation->mechanics_rng.state);
    surf_man_hash_u64(&hash, simulation->mechanics_rng.increment);
    surf_man_hash_u64(&hash, simulation->seed);
    surf_man_hash_u64(&hash, simulation->day_score);
    surf_man_hash_u64(&hash, simulation->pending_score);
    surf_man_hash_u64(&hash, simulation->best_score);
    surf_man_hash_u32(&hash, simulation->day);
    surf_man_hash_u32(&hash, simulation->wave);
    surf_man_hash_u32(&hash, simulation->phase_tick);
    surf_man_hash_u32(&hash, simulation->active_tick);
    surf_man_hash_u32(&hash, simulation->wave_ticks_remaining);
    surf_man_hash_u32(&hash, simulation->segment_ticks_remaining);
    surf_man_hash_u32(&hash, simulation->bank_ticks);
    surf_man_hash_u32(&hash, simulation->tube_ticks);
    surf_man_hash_u32(&hash, simulation->air_half_turns);
    surf_man_hash_u32(&hash, simulation->maneuver_count);
    surf_man_hash_i32(&hash, simulation->distance_q16);
    surf_man_hash_i32(&hash, simulation->speed_q16);
    surf_man_hash_i32(&hash, simulation->face_q16);
    surf_man_hash_i32(&hash, simulation->face_velocity_q16);
    surf_man_hash_i32(&hash, simulation->altitude_q16);
    surf_man_hash_i32(&hash, simulation->vertical_velocity_q16);
    surf_man_hash_i32(&hash, simulation->angle_q16);
    surf_man_hash_i32(&hash, simulation->angular_velocity_q16);
    surf_man_hash_u32(&hash, (uint32_t)simulation->phase);
    surf_man_hash_u32(&hash, (uint32_t)simulation->wave_kind);
    surf_man_hash_u32(&hash, (uint32_t)simulation->last_maneuver);
    surf_man_hash_u32(&hash, (uint8_t)simulation->last_turn);
    surf_man_hash_u32(&hash, simulation->flow);
    surf_man_hash_u32(&hash, simulation->practice ? 1U : 0U);
    surf_man_hash_u32(&hash, simulation->airborne ? 1U : 0U);
    surf_man_hash_u32(&hash, simulation->grabbed ? 1U : 0U);
    surf_man_hash_u32(&hash, simulation->risk_active ? 1U : 0U);
    surf_man_hash_u32(&hash, simulation->initialized ? 1U : 0U);
    while (award_length < SURF_MAN_AWARD_CAPACITY &&
           simulation->award[award_length] != '\0') {
        surf_man_hash_u32(&hash, (uint8_t)simulation->award[award_length]);
        ++award_length;
    }
    surf_man_hash_u64(&hash, award_length);
    return hash;
}

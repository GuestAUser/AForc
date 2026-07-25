/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EXAMPLES_SURF_MAN_GAME_H
#define AFORC_EXAMPLES_SURF_MAN_GAME_H

#include "aforc/assets.h"
#include "aforc/common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    SURF_MAN_FIXED_HZ = 60,
    SURF_MAN_VISUAL_HZ = 60,
    SURF_MAN_WAVES_PER_DAY = 3,
    SURF_MAN_WAVE_SECONDS = 45,
    SURF_MAN_TARGET_COLUMNS = 80,
    SURF_MAN_TARGET_ROWS = 24,
    SURF_MAN_MIN_COLUMNS = 60,
    SURF_MAN_MIN_ROWS = 20,
    SURF_MAN_PARTICLE_CAPACITY = 160,
    SURF_MAN_HAZARD_CAPACITY = 64,
    SURF_MAN_MESSAGE_CAPACITY = 160,
    SURF_MAN_AWARD_CAPACITY = 128,
    SURF_MAN_COMMAND_LEASE_TICKS = 8,
    SURF_MAN_FLOW_MAX = 5,
    SURF_MAN_Q16_SHIFT = 16
};

#define SURF_MAN_Q16_ONE INT32_C(65536)

typedef enum SurfManPhase {
    SURF_MAN_SHACK = 0,
    SURF_MAN_COUNT_IN,
    SURF_MAN_RIDING,
    SURF_MAN_WIPEOUT_RECOVERY,
    SURF_MAN_WAVE_RECAP,
    SURF_MAN_DAY_RECAP,
    SURF_MAN_PRACTICE
} SurfManPhase;

typedef enum SurfManOverlay {
    SURF_MAN_OVERLAY_NONE = 0,
    SURF_MAN_OVERLAY_PAUSE,
    SURF_MAN_OVERLAY_HELP,
    SURF_MAN_OVERLAY_ACCESSIBILITY,
    SURF_MAN_OVERLAY_RESIZE
} SurfManOverlay;

typedef enum SurfManWaveKind {
    SURF_MAN_WAVE_OPEN = 0,
    SURF_MAN_WAVE_STEEP,
    SURF_MAN_WAVE_TUBE,
    SURF_MAN_WAVE_CHOP,
    SURF_MAN_WAVE_CLOSEOUT
} SurfManWaveKind;

typedef enum SurfManManeuver {
    SURF_MAN_MANEUVER_NONE = 0,
    SURF_MAN_MANEUVER_CARVE_LEFT,
    SURF_MAN_MANEUVER_CARVE_RIGHT,
    SURF_MAN_MANEUVER_LIP_SNAP,
    SURF_MAN_MANEUVER_AIR,
    SURF_MAN_MANEUVER_TUBE
} SurfManManeuver;

typedef enum SurfManColorMode {
    SURF_MAN_COLOR_STANDARD = 0,
    SURF_MAN_COLOR_HIGH_CONTRAST,
    SURF_MAN_COLOR_NONE
} SurfManColorMode;

typedef struct SurfManSettings {
    uint32_t speed_percent;
    uint32_t timing_percent;
    bool landing_assist;
    bool reduced_motion;
    SurfManColorMode color_mode;
} SurfManSettings;

typedef struct SurfManRules {
    uint32_t wave_ticks;
    uint32_t count_in_ticks;
    uint32_t recovery_ticks;
    uint32_t bank_delay_ticks;
    int32_t minimum_speed_q16;
    int32_t maximum_speed_q16;
    int32_t line_position_limit_q16;
    int32_t line_maximum_velocity_q16;
    int32_t line_acceleration_q16;
    int32_t line_drag_q16;
    int32_t carve_velocity_threshold_q16;
    int32_t wave_face_offset_limit_q16;
    int32_t wave_face_maximum_velocity_q16;
    int32_t wave_face_acceleration_q16;
    int32_t wave_face_drag_q16;
    int32_t air_face_threshold_q16;
    int32_t hazard_face_threshold_q16;
    int32_t gravity_q16;
    int32_t pop_velocity_q16;
    int32_t landing_tolerance_q16;
} SurfManRules;

typedef struct SurfManCommand {
    int8_t vertical;
    int8_t horizontal;
    bool action;
    bool confirm;
    bool back;
} SurfManCommand;

typedef struct SurfManWaveSample {
    int32_t face_q16;
    int32_t slope_q16;
    int32_t push_q16;
    bool lip;
    bool pocket;
    bool tube;
    bool foam;
    bool hazard;
} SurfManWaveSample;

typedef struct SurfManSimulation {
    SurfManRules rules;
    SurfManSettings settings;
    AFORC_Rng mechanics_rng;
    uint64_t seed;
    uint64_t day_score;
    uint64_t pending_score;
    uint64_t best_score;
    uint32_t day;
    uint32_t wave;
    uint32_t phase_tick;
    uint32_t active_tick;
    uint32_t wave_ticks_remaining;
    uint32_t segment_ticks_remaining;
    uint32_t bank_ticks;
    uint32_t tube_ticks;
    uint32_t air_half_turns;
    uint32_t maneuver_count;
    int32_t distance_q16;
    int32_t speed_q16;
    int32_t line_position_q16;
    int32_t line_velocity_q16;
    int32_t wave_face_offset_q16;
    int32_t wave_face_velocity_q16;
    int32_t face_q16;
    int32_t face_velocity_q16;
    int32_t altitude_q16;
    int32_t vertical_velocity_q16;
    int32_t angle_q16;
    int32_t angular_velocity_q16;
    SurfManPhase phase;
    SurfManWaveKind wave_kind;
    SurfManManeuver last_maneuver;
    int8_t last_turn;
    uint8_t flow;
    bool practice;
    bool airborne;
    bool grabbed;
    bool risk_active;
    bool initialized;
    char award[SURF_MAN_AWARD_CAPACITY];
} SurfManSimulation;

SurfManSettings surf_man_settings_default(void);
SurfManRules surf_man_rules_default(void);
void surf_man_command_clear(SurfManCommand *command);
AFORC_Status surf_man_simulation_init(SurfManSimulation *simulation,
                                      uint64_t seed,
                                      const SurfManSettings *settings);
AFORC_Status surf_man_simulation_start_day(SurfManSimulation *simulation,
                                           bool practice);
AFORC_Status surf_man_simulation_start_wave(SurfManSimulation *simulation);
void surf_man_ride_reset(SurfManSimulation *simulation);
AFORC_Status surf_man_ride_step(SurfManSimulation *simulation,
                                const SurfManCommand *command);
AFORC_Status surf_man_ride_recovery_step(SurfManSimulation *simulation);
AFORC_Status surf_man_simulation_step(SurfManSimulation *simulation,
                                      const SurfManCommand *command);
AFORC_Status surf_man_wave_sample(const SurfManSimulation *simulation,
                                  int32_t distance_offset_q16,
                                  SurfManWaveSample *out_sample);
AFORC_Status surf_man_score_maneuver(SurfManSimulation *simulation,
                                     SurfManManeuver maneuver,
                                     uint32_t modifiers,
                                     bool risky);
void surf_man_score_bank(SurfManSimulation *simulation);
void surf_man_score_wipeout(SurfManSimulation *simulation);
uint64_t surf_man_simulation_hash(const SurfManSimulation *simulation);
const char *surf_man_wave_kind_name(SurfManWaveKind kind);

#endif

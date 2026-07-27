/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/game.h"

#include <limits.h>

enum {
    SURF_MAN_SEGMENT_TICKS = 5 * SURF_MAN_FIXED_HZ
};

static int32_t surf_man_sim_clamp(int64_t value,
                                  int32_t minimum,
                                  int32_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return (int32_t)value;
}

static int32_t surf_man_scaled_q16(int32_t value, uint32_t percent) {
    return surf_man_sim_clamp(((int64_t)value * percent) / 100,
                              INT32_MIN,
                              INT32_MAX);
}

static int32_t surf_man_inverse_scaled_q16(int32_t value,
                                           uint32_t percent) {
    return surf_man_sim_clamp(((int64_t)value * 100) / percent,
                              INT32_MIN,
                              INT32_MAX);
}

static void surf_man_tick_u32(uint32_t *value) {
    if (*value < UINT32_MAX) {
        ++*value;
    }
}

static SurfManManeuver surf_man_carve_maneuver(int8_t horizontal) {
    return horizontal < 0 ? SURF_MAN_MANEUVER_CARVE_LEFT
                          : SURF_MAN_MANEUVER_CARVE_RIGHT;
}

static int32_t surf_man_neutral_wave_face_offset_q16(
    const SurfManSimulation *simulation) {
    return simulation->rules.air_face_threshold_q16;
}

void surf_man_ride_reset(SurfManSimulation *simulation) {
    simulation->pending_score = 0U;
    simulation->phase_tick = 0U;
    simulation->active_tick = 0U;
    simulation->wave_ticks_remaining = simulation->rules.wave_ticks;
    simulation->segment_ticks_remaining = SURF_MAN_SEGMENT_TICKS;
    simulation->bank_ticks = 0U;
    simulation->award_ticks = 0U;
    simulation->tube_ticks = 0U;
    simulation->air_half_turns = 0U;
    simulation->maneuver_count = 0U;
    simulation->distance_q16 = 0;
    simulation->speed_q16 = surf_man_scaled_q16(
        simulation->rules.minimum_speed_q16, simulation->settings.speed_percent);
    simulation->line_position_q16 = 0;
    simulation->line_velocity_q16 = 0;
    simulation->wave_face_offset_q16 =
        surf_man_neutral_wave_face_offset_q16(simulation);
    simulation->wave_face_velocity_q16 = 0;
    simulation->face_q16 = 0;
    simulation->face_velocity_q16 = 0;
    simulation->altitude_q16 = 0;
    simulation->vertical_velocity_q16 = 0;
    simulation->angle_q16 = 0;
    simulation->angular_velocity_q16 = 0;
    simulation->last_maneuver = SURF_MAN_MANEUVER_NONE;
    simulation->last_turn = 0;
    simulation->carve_direction = 0;
    simulation->flow = 0U;
    simulation->airborne = false;
    simulation->grabbed = false;
    simulation->risk_active = false;
    simulation->award[0] = '\0';
}

static void surf_man_begin_wipeout(SurfManSimulation *simulation) {
    surf_man_score_wipeout(simulation);
    simulation->phase = SURF_MAN_WIPEOUT_RECOVERY;
    simulation->phase_tick = 0U;
    simulation->airborne = false;
    simulation->altitude_q16 = 0;
    simulation->vertical_velocity_q16 = 0;
    simulation->angle_q16 = 0;
    simulation->angular_velocity_q16 = 0;
    simulation->last_turn = 0;
    simulation->carve_direction = 0;
}

static AFORC_Status surf_man_finish_tube(SurfManSimulation *simulation) {
    AFORC_Status status = AFORC_OK;

    if (simulation->tube_ticks != 0U) {
        status = surf_man_score_maneuver(simulation,
                                         SURF_MAN_MANEUVER_TUBE,
                                         simulation->tube_ticks / 30U,
                                         true);
        simulation->tube_ticks = 0U;
    }
    return status;
}

static AFORC_Status surf_man_finish_wave(SurfManSimulation *simulation) {
    AFORC_Status status = surf_man_finish_tube(simulation);

    if (status != AFORC_OK) {
        return status;
    }
    surf_man_score_bank(simulation);
    simulation->phase_tick = 0U;
    if (simulation->practice) {
        simulation->phase = SURF_MAN_PRACTICE;
        simulation->active_tick = 0U;
        simulation->wave_ticks_remaining = simulation->rules.wave_ticks;
        simulation->distance_q16 = 0;
        simulation->segment_ticks_remaining = SURF_MAN_SEGMENT_TICKS;
        simulation->line_position_q16 = 0;
        simulation->line_velocity_q16 = 0;
        simulation->wave_face_offset_q16 =
            surf_man_neutral_wave_face_offset_q16(simulation);
        simulation->wave_face_velocity_q16 = 0;
        simulation->last_turn = 0;
        simulation->carve_direction = 0;
    } else {
        simulation->phase = SURF_MAN_WAVE_RECAP;
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_advance_wave_clock(SurfManSimulation *simulation) {
    surf_man_tick_u32(&simulation->active_tick);
    if (simulation->segment_ticks_remaining > 1U) {
        --simulation->segment_ticks_remaining;
    } else {
        simulation->segment_ticks_remaining = SURF_MAN_SEGMENT_TICKS;
    }
    if (simulation->wave_ticks_remaining > 0U) {
        --simulation->wave_ticks_remaining;
    }
    if (simulation->wave_ticks_remaining == 0U) {
        return surf_man_finish_wave(simulation);
    }
    return AFORC_OK;
}

static int32_t surf_man_landing_error(int32_t angle_q16) {
    int32_t remainder = angle_q16 % SURF_MAN_Q16_ONE;
    int64_t magnitude = remainder;

    if (magnitude < 0) {
        magnitude = -magnitude;
    }
    if (magnitude > SURF_MAN_Q16_ONE / 2) {
        magnitude = SURF_MAN_Q16_ONE - magnitude;
    }
    return (int32_t)magnitude;
}

static AFORC_Status surf_man_tick_air(SurfManSimulation *simulation,
                                      const SurfManCommand *command) {
    int32_t tolerance;
    uint64_t turns;
    AFORC_Status status;

    if (command->action) {
        simulation->grabbed = true;
    }
    simulation->angular_velocity_q16 =
        (int32_t)command->horizontal * (3 * SURF_MAN_Q16_ONE / 4);
    simulation->vertical_velocity_q16 = surf_man_sim_clamp(
        (int64_t)simulation->vertical_velocity_q16 -
            simulation->rules.gravity_q16 / SURF_MAN_FIXED_HZ,
        -simulation->rules.maximum_speed_q16,
        simulation->rules.maximum_speed_q16);
    simulation->altitude_q16 = surf_man_sim_clamp(
        (int64_t)simulation->altitude_q16 +
            simulation->vertical_velocity_q16 / SURF_MAN_FIXED_HZ,
        0,
        8 * SURF_MAN_Q16_ONE);
    simulation->angle_q16 = surf_man_sim_clamp(
        (int64_t)simulation->angle_q16 +
            simulation->angular_velocity_q16 / SURF_MAN_FIXED_HZ,
        INT32_MIN,
        INT32_MAX);
    turns = simulation->angle_q16 < 0
                ? (uint64_t)(-(int64_t)simulation->angle_q16)
                : (uint64_t)simulation->angle_q16;
    turns /= (uint32_t)(SURF_MAN_Q16_ONE / 2);
    simulation->air_half_turns =
        turns > UINT32_MAX ? UINT32_MAX : (uint32_t)turns;
    if (simulation->altitude_q16 > 0 ||
        simulation->vertical_velocity_q16 >= 0) {
        return AFORC_OK;
    }

    tolerance = surf_man_scaled_q16(
        simulation->rules.landing_tolerance_q16,
        simulation->settings.timing_percent);
    if (simulation->settings.landing_assist) {
        tolerance = surf_man_sim_clamp((int64_t)tolerance * 2,
                                       0,
                                       SURF_MAN_Q16_ONE / 2);
    }
    if (surf_man_landing_error(simulation->angle_q16) > tolerance) {
        surf_man_begin_wipeout(simulation);
        return AFORC_OK;
    }
    status = surf_man_score_maneuver(
        simulation,
        SURF_MAN_MANEUVER_AIR,
        simulation->air_half_turns + (simulation->grabbed ? 1U : 0U),
        true);
    simulation->airborne = false;
    simulation->grabbed = false;
    simulation->altitude_q16 = 0;
    simulation->vertical_velocity_q16 = 0;
    simulation->angle_q16 = 0;
    simulation->angular_velocity_q16 = 0;
    simulation->air_half_turns = 0U;
    return status;
}

static AFORC_Status surf_man_ground_actions(SurfManSimulation *simulation,
                                             const SurfManCommand *command,
                                             const SurfManWaveSample *sample) {
    AFORC_Status status = AFORC_OK;

    if (simulation->tube_ticks != 0U && !sample->tube) {
        status = surf_man_finish_tube(simulation);
    }
    if (status != AFORC_OK) {
        return status;
    }
    if (sample->tube && command->action && simulation->tube_ticks == 0U) {
        simulation->tube_ticks = 1U;
        simulation->risk_active = true;
    } else if (simulation->tube_ticks != 0U && sample->tube) {
        surf_man_tick_u32(&simulation->tube_ticks);
    }
    if (sample->lip && command->action && simulation->tube_ticks == 0U) {
        const int32_t air_face_threshold_q16 = surf_man_inverse_scaled_q16(
            simulation->rules.air_face_threshold_q16,
            simulation->settings.timing_percent);

        if (simulation->wave_face_offset_q16 >=
            air_face_threshold_q16) {
            simulation->airborne = true;
            simulation->vertical_velocity_q16 =
                simulation->rules.pop_velocity_q16;
            simulation->risk_active = true;
        } else {
            status = surf_man_score_maneuver(
                simulation, SURF_MAN_MANEUVER_LIP_SNAP, 0U, true);
        }
    }
    return status;
}

static int32_t surf_man_abs_q16(int32_t value) {
    const int64_t magnitude = value < 0 ? -(int64_t)value : value;

    return magnitude > INT32_MAX ? INT32_MAX : (int32_t)magnitude;
}

static int8_t surf_man_sign_q16(int32_t value) {
    if (value < 0) {
        return -1;
    }
    return value > 0 ? 1 : 0;
}

static int32_t surf_man_approach_q16(int32_t value,
                                     int32_t target,
                                     int32_t maximum_delta) {
    if (value < target) {
        return surf_man_sim_clamp(
            (int64_t)value + maximum_delta, value, target);
    }
    if (value > target) {
        return surf_man_sim_clamp(
            (int64_t)value - maximum_delta, target, value);
    }
    return value;
}

static AFORC_Status surf_man_integrate_line(
    SurfManSimulation *simulation,
    const SurfManCommand *command) {
    const int32_t threshold =
        simulation->rules.carve_velocity_threshold_q16;
    const int32_t acceleration =
        simulation->rules.line_acceleration_q16 / SURF_MAN_FIXED_HZ;
    const int32_t drag =
        simulation->rules.line_drag_q16 / SURF_MAN_FIXED_HZ;
    const int32_t previous_velocity_q16 = simulation->line_velocity_q16;
    int8_t steering_direction = command->horizontal;
    int32_t magnitude = surf_man_abs_q16(simulation->line_velocity_q16);
    AFORC_Status status = AFORC_OK;

    if (simulation->last_turn != 0 && magnitude < threshold / 2) {
        simulation->last_turn = 0;
    }
    if (simulation->carve_direction == 0 && command->horizontal != 0 &&
        simulation->last_turn != 0 &&
        command->horizontal != simulation->last_turn &&
        simulation->tube_ticks == 0U) {
        simulation->carve_direction = command->horizontal;
    }
    if (simulation->carve_direction != 0) {
        steering_direction = simulation->tube_ticks == 0U
                                 ? simulation->carve_direction
                                 : 0;
    }

    if (steering_direction != 0) {
        simulation->line_velocity_q16 = surf_man_sim_clamp(
            (int64_t)simulation->line_velocity_q16 +
                (int64_t)steering_direction * acceleration,
            -simulation->rules.line_maximum_velocity_q16,
            simulation->rules.line_maximum_velocity_q16);
    } else {
        simulation->line_velocity_q16 = surf_man_approach_q16(
            simulation->line_velocity_q16, 0, drag);
    }
    simulation->line_position_q16 = surf_man_sim_clamp(
        (int64_t)simulation->line_position_q16 +
            simulation->line_velocity_q16 / SURF_MAN_FIXED_HZ,
        -simulation->rules.line_position_limit_q16,
        simulation->rules.line_position_limit_q16);
    if ((simulation->line_position_q16 ==
             simulation->rules.line_position_limit_q16 &&
         simulation->line_velocity_q16 > 0) ||
        (simulation->line_position_q16 ==
             -simulation->rules.line_position_limit_q16 &&
         simulation->line_velocity_q16 < 0)) {
        simulation->line_velocity_q16 = 0;
    }

    if (simulation->carve_direction != 0 &&
        simulation->tube_ticks == 0U &&
        surf_man_sign_q16(previous_velocity_q16) !=
            simulation->carve_direction &&
        surf_man_sign_q16(simulation->line_velocity_q16) ==
            simulation->carve_direction) {
        const int8_t completed_direction = simulation->carve_direction;

        simulation->carve_direction = 0;
        simulation->last_turn = 0;
        status = surf_man_score_maneuver(
            simulation,
            surf_man_carve_maneuver(completed_direction),
            0U,
            false);
        if (status != AFORC_OK) {
            return status;
        }
    }

    magnitude = surf_man_abs_q16(simulation->line_velocity_q16);
    if (simulation->last_turn == 0 &&
        simulation->carve_direction == 0 && steering_direction != 0 &&
        magnitude >= threshold &&
        surf_man_sign_q16(simulation->line_velocity_q16) ==
            steering_direction) {
        simulation->last_turn = steering_direction;
    }
    return AFORC_OK;
}

static void surf_man_integrate_wave_face(
    SurfManSimulation *simulation,
    const SurfManCommand *command) {
    const int32_t acceleration =
        simulation->rules.wave_face_acceleration_q16 / SURF_MAN_FIXED_HZ;
    const int32_t drag =
        simulation->rules.wave_face_drag_q16 / SURF_MAN_FIXED_HZ;

    if (command->vertical != 0) {
        simulation->wave_face_velocity_q16 = surf_man_sim_clamp(
            (int64_t)simulation->wave_face_velocity_q16 -
                (int64_t)command->vertical * acceleration,
            -simulation->rules.wave_face_maximum_velocity_q16,
            simulation->rules.wave_face_maximum_velocity_q16);
    } else {
        simulation->wave_face_velocity_q16 = surf_man_approach_q16(
            simulation->wave_face_velocity_q16, 0, drag);
    }
    simulation->wave_face_offset_q16 = surf_man_sim_clamp(
        (int64_t)simulation->wave_face_offset_q16 +
            simulation->wave_face_velocity_q16 / SURF_MAN_FIXED_HZ,
        -simulation->rules.wave_face_offset_limit_q16,
        simulation->rules.wave_face_offset_limit_q16);
    if ((simulation->wave_face_offset_q16 ==
             simulation->rules.wave_face_offset_limit_q16 &&
         simulation->wave_face_velocity_q16 > 0) ||
        (simulation->wave_face_offset_q16 ==
             -simulation->rules.wave_face_offset_limit_q16 &&
         simulation->wave_face_velocity_q16 < 0)) {
        simulation->wave_face_velocity_q16 = 0;
    }
}

static void surf_man_integrate_face(SurfManSimulation *simulation,
                                    const SurfManCommand *command,
                                    const SurfManWaveSample *sample) {
    const int32_t maximum_speed = surf_man_scaled_q16(
        simulation->rules.maximum_speed_q16, simulation->settings.speed_percent);
    const int32_t minimum_speed = surf_man_scaled_q16(
        simulation->rules.minimum_speed_q16, simulation->settings.speed_percent);
    int64_t acceleration = sample->push_q16;
    int64_t face_delta;
    int64_t target_angle;
    int32_t target_angular_velocity;

    acceleration += (int64_t)command->vertical * 2 * SURF_MAN_Q16_ONE;
    acceleration -= (int64_t)simulation->wave_face_velocity_q16 * 2;
    if (sample->pocket) {
        acceleration += SURF_MAN_Q16_ONE;
    }
    if (sample->foam) {
        acceleration -= SURF_MAN_Q16_ONE;
    }
    simulation->speed_q16 = surf_man_sim_clamp(
        (int64_t)simulation->speed_q16 + acceleration / SURF_MAN_FIXED_HZ,
        minimum_speed,
        maximum_speed);
    simulation->distance_q16 = surf_man_sim_clamp(
        (int64_t)simulation->distance_q16 +
            simulation->speed_q16 / SURF_MAN_FIXED_HZ,
        0,
        INT32_MAX);
    face_delta = (int64_t)sample->face_q16 - simulation->face_q16;
    simulation->face_velocity_q16 = surf_man_sim_clamp(
        face_delta * (SURF_MAN_FIXED_HZ / 4), INT32_MIN, INT32_MAX);
    simulation->face_q16 = surf_man_sim_clamp(
        (int64_t)simulation->face_q16 + face_delta / 4,
        0,
        8 * SURF_MAN_Q16_ONE);
    target_angle = surf_man_sim_clamp(
        sample->slope_q16 / 4 +
            (int64_t)command->horizontal * (SURF_MAN_Q16_ONE / 16),
        -SURF_MAN_Q16_ONE / 2,
        SURF_MAN_Q16_ONE / 2);
    /*
     * Convert lean error into a capped Q16 angular velocity, then remove half
     * the velocity error per fixed tick. This critically damped response keeps
     * steering immediate without letting frame cadence alter turn integration.
     */
    target_angular_velocity = surf_man_sim_clamp(
        (target_angle - simulation->angle_q16) * 10,
        -3 * SURF_MAN_Q16_ONE / 4,
        3 * SURF_MAN_Q16_ONE / 4);
    simulation->angular_velocity_q16 = surf_man_sim_clamp(
        (int64_t)simulation->angular_velocity_q16 +
            ((int64_t)target_angular_velocity -
             simulation->angular_velocity_q16) /
                2,
        -3 * SURF_MAN_Q16_ONE / 4,
        3 * SURF_MAN_Q16_ONE / 4);
    simulation->angle_q16 = surf_man_sim_clamp(
        (int64_t)simulation->angle_q16 +
            simulation->angular_velocity_q16 / SURF_MAN_FIXED_HZ,
        -SURF_MAN_Q16_ONE / 2,
        SURF_MAN_Q16_ONE / 2);
}

AFORC_Status surf_man_ride_step(SurfManSimulation *simulation,
                                 const SurfManCommand *command) {
    SurfManWaveSample sample;
    AFORC_Status status = surf_man_wave_sample(
        simulation, simulation->line_position_q16, &sample);

    if (status != AFORC_OK) {
        return status;
    }
    if (simulation->airborne) {
        status = surf_man_tick_air(simulation, command);
    } else {
        status = surf_man_ground_actions(simulation, command, &sample);
        if (status == AFORC_OK && sample.hazard &&
            simulation->wave_face_offset_q16 <
                surf_man_inverse_scaled_q16(
                    simulation->rules.hazard_face_threshold_q16,
                    simulation->settings.timing_percent)) {
            surf_man_begin_wipeout(simulation);
        }
    }
    if (status != AFORC_OK) {
        return status;
    }
    if (simulation->phase != SURF_MAN_WIPEOUT_RECOVERY) {
        if (!simulation->airborne) {
            status = surf_man_integrate_line(simulation, command);
            if (status == AFORC_OK) {
                surf_man_integrate_wave_face(simulation, command);
                surf_man_integrate_face(simulation, command, &sample);
            }
        }
        if (status != AFORC_OK) {
            return status;
        }
        if (simulation->bank_ticks > 0U && !simulation->airborne &&
            simulation->tube_ticks == 0U) {
            --simulation->bank_ticks;
            if (simulation->bank_ticks == 0U) {
                surf_man_score_bank(simulation);
            }
        }
    }
    return surf_man_advance_wave_clock(simulation);
}

static int32_t surf_man_recovery_approach_q16(int32_t value,
                                              int32_t target,
                                              uint32_t ticks_remaining) {
    int64_t magnitude = (int64_t)value - target;
    int64_t delta;

    if (magnitude < 0) {
        magnitude = -magnitude;
    }
    delta = (magnitude + ticks_remaining - 1U) / ticks_remaining;
    return surf_man_approach_q16(
        value,
        target,
        surf_man_sim_clamp(delta, 0, INT32_MAX));
}

AFORC_Status surf_man_ride_recovery_step(SurfManSimulation *simulation) {
    uint32_t ticks_remaining = 1U;

    if (simulation->phase_tick < simulation->rules.recovery_ticks) {
        ticks_remaining =
            simulation->rules.recovery_ticks - simulation->phase_tick;
    }
    simulation->line_position_q16 = surf_man_recovery_approach_q16(
        simulation->line_position_q16, 0, ticks_remaining);
    simulation->line_velocity_q16 = surf_man_recovery_approach_q16(
        simulation->line_velocity_q16, 0, ticks_remaining);
    simulation->wave_face_offset_q16 = surf_man_recovery_approach_q16(
        simulation->wave_face_offset_q16,
        surf_man_neutral_wave_face_offset_q16(simulation),
        ticks_remaining);
    simulation->wave_face_velocity_q16 = surf_man_recovery_approach_q16(
        simulation->wave_face_velocity_q16, 0, ticks_remaining);

    surf_man_tick_u32(&simulation->phase_tick);
    if (simulation->phase == SURF_MAN_WIPEOUT_RECOVERY &&
        simulation->phase_tick >= simulation->rules.recovery_ticks) {
        simulation->phase = simulation->practice ? SURF_MAN_PRACTICE
                                                  : SURF_MAN_RIDING;
        simulation->phase_tick = 0U;
        simulation->line_position_q16 = 0;
        simulation->line_velocity_q16 = 0;
        simulation->wave_face_offset_q16 =
            surf_man_neutral_wave_face_offset_q16(simulation);
        simulation->wave_face_velocity_q16 = 0;
    }
    return AFORC_OK;
}

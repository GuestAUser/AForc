/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/game.h"

static void surf_man_tick_u32(uint32_t *value) {
    if (*value < UINT32_MAX) {
        ++*value;
    }
}

static void surf_man_tick_award(SurfManSimulation *simulation) {
    if (simulation->award_ticks == 0U) {
        return;
    }
    --simulation->award_ticks;
    if (simulation->award_ticks == 0U) {
        simulation->award[0] = '\0';
    }
}

static AFORC_Status surf_man_select_wave_kind(
    SurfManSimulation *simulation,
    uint32_t *out_kind) {
    uint32_t choice;
    AFORC_Status status;

    if (simulation->practice) {
        return aforc_rng_bounded_u32(&simulation->mechanics_rng,
                                     SURF_MAN_WAVE_CLOSEOUT + 1U,
                                     out_kind);
    }
    if (simulation->wave == 0U) {
        *out_kind = SURF_MAN_WAVE_OPEN;
        return AFORC_OK;
    }
    status = aforc_rng_bounded_u32(&simulation->mechanics_rng, 2U, &choice);
    if (status != AFORC_OK) {
        return status;
    }
    *out_kind = simulation->wave == 1U
                    ? SURF_MAN_WAVE_STEEP + choice
                    : SURF_MAN_WAVE_CHOP + choice;
    return AFORC_OK;
}

static bool surf_man_settings_valid(const SurfManSettings *settings) {
    return settings->speed_percent >= 50U &&
           settings->speed_percent <= 200U &&
           settings->timing_percent >= 50U &&
           settings->timing_percent <= 200U &&
           settings->color_mode >= SURF_MAN_COLOR_STANDARD &&
           settings->color_mode <= SURF_MAN_COLOR_NONE;
}

AFORC_Status surf_man_simulation_init(SurfManSimulation *simulation,
                                      uint64_t seed,
                                      const SurfManSettings *settings) {
    SurfManSettings selected;
    AFORC_Status status;

    if (simulation == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    selected = settings == NULL ? surf_man_settings_default() : *settings;
    if (!surf_man_settings_valid(&selected)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *simulation = (SurfManSimulation){0};
    simulation->rules = surf_man_rules_default();
    simulation->settings = selected;
    simulation->seed = seed;
    simulation->phase = SURF_MAN_SHACK;
    simulation->wave_kind = SURF_MAN_WAVE_OPEN;
    status = aforc_rng_seed(&simulation->mechanics_rng,
                            seed,
                            UINT64_C(0x853c49e6748fea9b));
    if (status != AFORC_OK) {
        return status;
    }
    simulation->initialized = true;
    return AFORC_OK;
}

AFORC_Status surf_man_simulation_start_day(SurfManSimulation *simulation,
                                           bool practice) {
    AFORC_Rng rng;
    uint32_t next_day;
    AFORC_Status status;

    if (simulation == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!simulation->initialized) {
        return AFORC_ERROR_STATE;
    }
    if (!practice && simulation->day == UINT32_MAX) {
        return AFORC_ERROR_LIMIT;
    }
    next_day = practice ? simulation->day : simulation->day + 1U;
    status = aforc_rng_seed(
        &rng,
        simulation->seed ^
            (UINT64_C(0x9e3779b97f4a7c15) * next_day) ^
            (practice ? UINT64_C(0xd1b54a32d192ed03) : UINT64_C(0)),
        UINT64_C(0x853c49e6748fea9b));
    if (status != AFORC_OK) {
        return status;
    }
    simulation->mechanics_rng = rng;
    simulation->day = next_day;
    simulation->day_score = 0U;
    simulation->pending_score = 0U;
    simulation->wave = 0U;
    simulation->phase = SURF_MAN_SHACK;
    simulation->practice = practice;
    surf_man_ride_reset(simulation);
    return AFORC_OK;
}

AFORC_Status surf_man_simulation_start_wave(SurfManSimulation *simulation) {
    SurfManWaveSample sample;
    uint32_t kind;
    AFORC_Status status;

    if (simulation == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!simulation->initialized) {
        return AFORC_ERROR_STATE;
    }
    if (simulation->wave >= SURF_MAN_WAVES_PER_DAY) {
        return AFORC_ERROR_LIMIT;
    }
    status = surf_man_select_wave_kind(simulation, &kind);
    if (status != AFORC_OK) {
        return status;
    }
    ++simulation->wave;
    simulation->wave_kind = (SurfManWaveKind)kind;
    surf_man_ride_reset(simulation);
    simulation->phase = SURF_MAN_COUNT_IN;
    status = surf_man_wave_sample(simulation, 0, &sample);
    if (status == AFORC_OK) {
        simulation->face_q16 = sample.face_q16;
    }
    return status;
}

AFORC_Status surf_man_simulation_end_session(SurfManSimulation *simulation) {
    if (simulation == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!simulation->initialized) {
        return AFORC_ERROR_STATE;
    }
    if (simulation->practice) {
        surf_man_score_bank(simulation);
    }
    surf_man_ride_reset(simulation);
    simulation->day_score = 0U;
    simulation->wave = 0U;
    simulation->wave_ticks_remaining = 0U;
    simulation->segment_ticks_remaining = 0U;
    simulation->phase = SURF_MAN_SHACK;
    simulation->wave_kind = SURF_MAN_WAVE_OPEN;
    simulation->practice = false;
    return AFORC_OK;
}

AFORC_Status surf_man_simulation_step(SurfManSimulation *simulation,
                                      const SurfManCommand *command) {
    if (simulation == NULL || command == NULL || command->vertical < -1 ||
        command->vertical > 1 || command->horizontal < -1 ||
        command->horizontal > 1) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!simulation->initialized) {
        return AFORC_ERROR_STATE;
    }
    surf_man_tick_award(simulation);
    switch (simulation->phase) {
        case SURF_MAN_COUNT_IN:
            surf_man_tick_u32(&simulation->phase_tick);
            if (command->back) {
                return surf_man_simulation_end_session(simulation);
            } else if (simulation->phase_tick >=
                       simulation->rules.count_in_ticks) {
                simulation->phase = simulation->practice
                                        ? SURF_MAN_PRACTICE
                                        : SURF_MAN_RIDING;
                simulation->phase_tick = 0U;
            }
            return AFORC_OK;
        case SURF_MAN_RIDING:
        case SURF_MAN_PRACTICE:
            if (simulation->practice && command->back) {
                return surf_man_simulation_end_session(simulation);
            }
            return surf_man_ride_step(simulation, command);
        case SURF_MAN_WIPEOUT_RECOVERY:
            return surf_man_ride_recovery_step(simulation);
        case SURF_MAN_WAVE_RECAP:
            surf_man_tick_u32(&simulation->phase_tick);
            if (command->back) {
                return surf_man_simulation_end_session(simulation);
            } else if (command->confirm) {
                if (simulation->wave < SURF_MAN_WAVES_PER_DAY) {
                    return surf_man_simulation_start_wave(simulation);
                }
                simulation->phase = SURF_MAN_DAY_RECAP;
                simulation->phase_tick = 0U;
            }
            return AFORC_OK;
        case SURF_MAN_DAY_RECAP:
            surf_man_tick_u32(&simulation->phase_tick);
            if (command->confirm || command->back) {
                return surf_man_simulation_end_session(simulation);
            }
            return AFORC_OK;
        case SURF_MAN_SHACK:
            return AFORC_OK;
        default:
            return AFORC_ERROR_STATE;
    }
}

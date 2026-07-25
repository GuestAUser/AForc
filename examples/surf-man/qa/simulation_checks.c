/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man_internal.h"

#include <limits.h>
#include <string.h>

enum {
    SURF_MAN_QA_SCHEDULE_TICKS = 240
};

typedef struct SurfManQaSchedule {
    AFORC_Scene scene;
    SurfManSimulation simulation;
    uint64_t ticks;
} SurfManQaSchedule;

static AFORC_Status surf_man_qa_error(AFORC_Error *error,
                                      AFORC_Status status,
                                      const char *message) {
    aforc_error_set(error, status, "surf-man qa", "%s", message);
    return status;
}

static AFORC_Status surf_man_qa_schedule_update(AFORC_Scene *scene,
                                                AFORC_Engine *engine,
                                                double seconds,
                                                AFORC_Error *error) {
    SurfManQaSchedule *schedule = scene->user_data;
    SurfManCommand command;
    AFORC_Status status;

    (void)engine;
    (void)seconds;
    (void)error;
    surf_man_command_clear(&command);
    if (schedule->simulation.phase == SURF_MAN_RIDING) {
        command.horizontal = ((schedule->ticks / 24U) & 1U) == 0U ? 1 : -1;
        command.vertical = ((schedule->ticks / 40U) & 1U) == 0U ? 1 : -1;
        command.action = schedule->ticks % 53U == 0U;
    }
    status = surf_man_simulation_step(&schedule->simulation, &command);
    if (status == AFORC_OK) {
        ++schedule->ticks;
    }
    return status;
}

static const AFORC_SceneVTable surf_man_qa_schedule_vtable = {
    .fixed_update = surf_man_qa_schedule_update,
};

static AFORC_Status surf_man_qa_schedule_init(SurfManQaSchedule *schedule,
                                              uint64_t seed) {
    const SurfManSettings settings = surf_man_settings_default();
    AFORC_Status status;

    memset(schedule, 0, sizeof(*schedule));
    status = surf_man_simulation_init(&schedule->simulation, seed, &settings);
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&schedule->simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&schedule->simulation);
    }
    schedule->scene = (AFORC_Scene){
        &surf_man_qa_schedule_vtable,
        schedule,
        0U,
    };
    return status;
}

static AFORC_Status surf_man_qa_run_schedule(uint64_t seed,
                                             const uint8_t *pattern,
                                             size_t pattern_count,
                                             uint64_t *out_hash,
                                             AFORC_Error *error) {
    const uint64_t fixed_step_ns =
        UINT64_C(1000000000) / SURF_MAN_FIXED_HZ;
    AFORC_EngineConfig config = aforc_engine_config_default();
    SurfManQaSchedule schedule;
    AFORC_Engine *engine = NULL;
    uint64_t elapsed_ticks = 0U;
    uint64_t now_ns = 0U;
    size_t pattern_index = 0U;
    AFORC_Status status = surf_man_qa_schedule_init(&schedule, seed);

    config.fixed_updates_per_second = SURF_MAN_FIXED_HZ;
    config.maximum_fixed_updates_per_frame = 8U;
    config.target_frames_per_second = 0U;
    config.quit_when_scene_stack_empty = false;
    if (status == AFORC_OK) {
        status = aforc_engine_create(&config, &engine, error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &schedule.scene, error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, error);
    }
    while (status == AFORC_OK &&
           elapsed_ticks < SURF_MAN_QA_SCHEDULE_TICKS) {
        const uint64_t increment = pattern[pattern_index % pattern_count];

        ++pattern_index;
        if (increment > SURF_MAN_QA_SCHEDULE_TICKS - elapsed_ticks) {
            status = AFORC_ERROR_STATE;
            break;
        }
        elapsed_ticks += increment;
        now_ns += increment * fixed_step_ns;
        status = aforc_engine_frame(engine, now_ns, error);
    }
    if (status == AFORC_OK &&
        (schedule.ticks != SURF_MAN_QA_SCHEDULE_TICKS ||
         aforc_engine_fixed_tick(engine) != SURF_MAN_QA_SCHEDULE_TICKS)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        *out_hash = surf_man_simulation_hash(&schedule.simulation);
    }
    aforc_engine_destroy(engine);
    return status;
}

static AFORC_Status surf_man_qa_schedule_checks(uint64_t seed,
                                                AFORC_Error *error) {
    static const uint8_t steady[] = {1U};
    static const uint8_t alternating[] = {1U, 3U};
    static const uint8_t stall[] = {0U, 0U, 8U};
    uint64_t steady_hash = 0U;
    uint64_t alternating_hash = 0U;
    uint64_t stall_hash = 0U;
    AFORC_Status status;

    status = surf_man_qa_run_schedule(
        seed, steady, sizeof(steady), &steady_hash, error);
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "steady fixed-step schedule did not complete");
    }
    status = surf_man_qa_run_schedule(
        seed, alternating, sizeof(alternating), &alternating_hash, error);
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "alternating fixed-step schedule did not complete");
    }
    status = surf_man_qa_run_schedule(
        seed, stall, sizeof(stall), &stall_hash, error);
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "bounded-stall fixed-step schedule did not complete");
    }
    if (steady_hash != alternating_hash || steady_hash != stall_hash) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "same seed diverged across fixed-step frame schedules");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_score_checks(uint64_t seed,
                                             AFORC_Error *error) {
    const SurfManSettings settings = surf_man_settings_default();
    SurfManSimulation simulation;
    uint64_t maneuver_score;
    AFORC_Status status;

    status = surf_man_simulation_init(&simulation, seed, &settings);
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&simulation);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "score fixture could not start a normal wave");
    }
    simulation.phase = SURF_MAN_RIDING;
    status = surf_man_score_maneuver(
        &simulation, SURF_MAN_MANEUVER_CARVE_LEFT, 1U, false);
    if (status != AFORC_OK || simulation.pending_score == 0U ||
        strcmp(simulation.award, "LEFT CARVE") != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "valid carve did not create pending score");
    }
    maneuver_score = simulation.pending_score;
    surf_man_score_bank(&simulation);
    if (simulation.pending_score != 0U ||
        simulation.day_score != maneuver_score) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "bank did not transfer pending score exactly once");
    }

    simulation.flow = 0U;
    simulation.last_maneuver = SURF_MAN_MANEUVER_NONE;
    status = surf_man_score_maneuver(
        &simulation, SURF_MAN_MANEUVER_CARVE_LEFT, 0U, false);
    maneuver_score = simulation.pending_score;
    if (status == AFORC_OK) {
        status = surf_man_score_maneuver(
            &simulation, SURF_MAN_MANEUVER_CARVE_LEFT, 0U, false);
    }
    if (status != AFORC_OK || maneuver_score != UINT64_C(100) ||
        simulation.pending_score != UINT64_C(150) || simulation.flow != 1U ||
        simulation.bank_ticks != (5U * SURF_MAN_FIXED_HZ) / 2U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "repeated maneuver did not halve base score or preserve flow");
    }
    surf_man_score_wipeout(&simulation);

    status = surf_man_score_maneuver(
        &simulation, SURF_MAN_MANEUVER_CARVE_LEFT, 0U, false);
    if (status == AFORC_OK) {
        status = surf_man_score_maneuver(
            &simulation, SURF_MAN_MANEUVER_CARVE_RIGHT, 0U, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_score_maneuver(
            &simulation, SURF_MAN_MANEUVER_CARVE_LEFT, 0U, false);
    }
    if (status != AFORC_OK || simulation.pending_score != UINT64_C(400) ||
        simulation.flow != 3U ||
        strcmp(simulation.award, "LEFT CARVE") != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "alternating carve rails did not build flow");
    }
    surf_man_score_wipeout(&simulation);

    simulation.air_half_turns = 3U;
    simulation.grabbed = true;
    status = surf_man_score_maneuver(
        &simulation, SURF_MAN_MANEUVER_AIR, 4U, true);
    if (status != AFORC_OK ||
        strcmp(simulation.award, "GRAB 540 AIR") != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "air award did not include grab and rotation state");
    }
    surf_man_score_wipeout(&simulation);

    status = surf_man_score_maneuver(
        &simulation, (SurfManManeuver)(SURF_MAN_MANEUVER_TUBE + 1), 0U, false);
    if (status != AFORC_ERROR_INVALID_ARGUMENT) {
        return surf_man_qa_error(
            error, AFORC_ERROR_STATE, "invalid maneuver enum was accepted");
    }

    simulation.pending_score = 123U;
    simulation.flow = 4U;
    simulation.risk_active = true;
    surf_man_score_wipeout(&simulation);
    if (simulation.pending_score != 0U || simulation.flow != 0U ||
        simulation.risk_active) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "wipeout did not clear pending score, flow, and risk");
    }

    simulation.day_score = UINT64_MAX - 4U;
    simulation.pending_score = 9U;
    surf_man_score_bank(&simulation);
    if (simulation.day_score != UINT64_MAX || simulation.pending_score != 0U) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "score bank did not saturate at UINT64_MAX");
    }
    simulation.pending_score = UINT64_MAX - 1U;
    status = surf_man_score_maneuver(
        &simulation, SURF_MAN_MANEUVER_CARVE_RIGHT, 1U, true);
    if (status != AFORC_OK || simulation.pending_score != UINT64_MAX) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "pending maneuver score did not saturate at UINT64_MAX");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_transition_checks(uint64_t seed,
                                                  AFORC_Error *error) {
    const SurfManSettings settings = surf_man_settings_default();
    SurfManSimulation simulation;
    uint32_t first_day;
    AFORC_Status status =
        surf_man_simulation_init(&simulation, seed, &settings);

    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&simulation, false);
    }
    if (status != AFORC_OK || simulation.practice || simulation.day == 0U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "normal day did not enter a non-practice day state");
    }
    first_day = simulation.day;
    simulation.pending_score = 17U;
    simulation.flow = 3U;
    status = surf_man_simulation_start_day(&simulation, true);
    if (status != AFORC_OK || !simulation.practice ||
        simulation.pending_score != 0U || simulation.flow != 0U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "practice transition did not reset transient run state");
    }
    status = surf_man_simulation_start_day(&simulation, false);
    if (status != AFORC_OK || simulation.practice ||
        simulation.day <= first_day) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "practice exit did not advance to the next normal day");
    }
    return AFORC_OK;
}

AFORC_Status surf_man_simulation_checks(uint64_t seed, AFORC_Error *error) {
    AFORC_Status status;

    if (SURF_MAN_VISUAL_HZ != SURF_MAN_FIXED_HZ) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "visual cadence is below the fixed simulation cadence");
    }
    status = surf_man_qa_schedule_checks(seed, error);

    if (status == AFORC_OK) {
        status = surf_man_qa_score_checks(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_transition_checks(seed, error);
    }
    return status;
}

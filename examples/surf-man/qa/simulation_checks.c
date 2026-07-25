/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man_internal.h"

#include <limits.h>
#include <string.h>

enum {
    SURF_MAN_QA_SCHEDULE_TICKS = 20 * SURF_MAN_FIXED_HZ,
    SURF_MAN_QA_MIN_RIDING_TICKS = 5 * SURF_MAN_FIXED_HZ,
    SURF_MAN_QA_SAMPLE_SEARCH_STEPS = 1024,
    SURF_MAN_QA_CARVE_HOLD_TICKS = SURF_MAN_FIXED_HZ / 3
};

typedef struct SurfManQaSchedule {
    AFORC_Scene scene;
    SurfManSimulation simulation;
    uint64_t ticks;
    uint64_t riding_ticks;
} SurfManQaSchedule;

static AFORC_Status surf_man_qa_error(AFORC_Error *error,
                                       AFORC_Status status,
                                       const char *message) {
    aforc_error_set(error, status, "surf-man qa", "%s", message);
    return status;
}

static int64_t surf_man_qa_distance_i32(int32_t value, int32_t target) {
    int64_t distance = (int64_t)value - target;

    return distance < 0 ? -distance : distance;
}

static AFORC_Status surf_man_qa_schedule_update(AFORC_Scene *scene,
                                                AFORC_Engine *engine,
                                                double seconds,
                                                AFORC_Error *error) {
    SurfManQaSchedule *schedule = scene->user_data;
    SurfManCommand command;
    bool was_riding;
    AFORC_Status status;

    (void)engine;
    (void)seconds;
    (void)error;
    surf_man_command_clear(&command);
    was_riding = schedule->simulation.phase == SURF_MAN_RIDING;
    if (was_riding) {
        command.horizontal = ((schedule->ticks / 24U) & 1U) == 0U ? 1 : -1;
        command.vertical = ((schedule->ticks / 40U) & 1U) == 0U ? 1 : -1;
        command.action = schedule->ticks % 53U == 0U;
    }
    status = surf_man_simulation_step(&schedule->simulation, &command);
    if (status == AFORC_OK) {
        if (was_riding) {
            ++schedule->riding_ticks;
        }
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
         aforc_engine_fixed_tick(engine) != SURF_MAN_QA_SCHEDULE_TICKS ||
         schedule.riding_ticks < SURF_MAN_QA_MIN_RIDING_TICKS)) {
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

static AFORC_Status surf_man_qa_riding_init(
    SurfManSimulation *simulation,
    uint64_t seed,
    SurfManWaveKind wave_kind) {
    const SurfManSettings settings = surf_man_settings_default();
    AFORC_Status status = surf_man_simulation_init(simulation, seed, &settings);

    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(simulation);
    }
    if (status == AFORC_OK) {
        simulation->phase = SURF_MAN_RIDING;
        simulation->phase_tick = 0U;
        simulation->wave_kind = wave_kind;
    }
    return status;
}

static AFORC_Status surf_man_qa_seek_ground_sample(
    SurfManSimulation *simulation,
    bool require_lip) {
    const int32_t search_step_q16 = SURF_MAN_Q16_ONE / 4;

    for (uint32_t step = 0U;
         step < SURF_MAN_QA_SAMPLE_SEARCH_STEPS;
         ++step) {
        const int64_t distance_q16 = (int64_t)step * search_step_q16;
        SurfManWaveSample sample;
        AFORC_Status status;

        if (distance_q16 > INT32_MAX) {
            return AFORC_ERROR_LIMIT;
        }
        simulation->distance_q16 = (int32_t)distance_q16;
        status = surf_man_wave_sample(simulation, 0, &sample);
        if (status != AFORC_OK) {
            return status;
        }
        if (!sample.hazard && !sample.tube && sample.lip == require_lip) {
            return AFORC_OK;
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

static AFORC_Status surf_man_qa_steering_contract(uint64_t seed,
                                                   AFORC_Error *error) {
    SurfManSimulation simulation;
    SurfManSimulation bounded;
    SurfManCommand command;
    uint64_t pending_score;
    uint32_t maneuver_count;
    int32_t established_position;
    int32_t established_velocity;
    int32_t coast_position;
    int32_t coast_velocity;
    AFORC_Status status = surf_man_qa_riding_init(
        &simulation, seed, SURF_MAN_WAVE_OPEN);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_ground_sample(&simulation, false);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "steering fixture could not find safe open water");
    }

    maneuver_count = simulation.maneuver_count;
    pending_score = simulation.pending_score;
    bounded = simulation;
    surf_man_command_clear(&command);
    command.horizontal = 1;
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "first steering input did not complete");
    }
    if (simulation.maneuver_count != maneuver_count ||
        simulation.pending_score != pending_score ||
        simulation.last_maneuver != SURF_MAN_MANEUVER_NONE ||
        simulation.line_velocity_q16 <= 0 ||
        simulation.line_position_q16 <= 0 ||
        simulation.line_position_q16 >
            simulation.rules.line_maximum_velocity_q16 /
                SURF_MAN_FIXED_HZ) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "first steering press lacked immediate bounded motion");
    }

    for (uint32_t tick = 1U;
         tick < SURF_MAN_QA_CARVE_HOLD_TICKS;
         ++tick) {
        const int32_t previous_position = simulation.line_position_q16;

        status = surf_man_simulation_step(&simulation, &command);
        if (status != AFORC_OK ||
            surf_man_qa_distance_i32(
                simulation.line_position_q16, previous_position) >
                simulation.rules.line_maximum_velocity_q16 /
                    SURF_MAN_FIXED_HZ) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "sustained steering exceeded its per-tick travel bound");
        }
    }
    if (simulation.maneuver_count != maneuver_count ||
        simulation.pending_score != pending_score ||
        simulation.last_turn != 1 ||
        simulation.line_velocity_q16 <
            simulation.rules.carve_velocity_threshold_q16) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "sustained steering did not establish an unscored carve rail");
    }

    established_position = simulation.line_position_q16;
    established_velocity = simulation.line_velocity_q16;
    surf_man_command_clear(&command);
    status = surf_man_simulation_step(&simulation, &command);
    coast_position = simulation.line_position_q16;
    coast_velocity = simulation.line_velocity_q16;
    if (status != AFORC_OK || coast_velocity <= 0 ||
        coast_velocity >= established_velocity ||
        coast_position <= established_position ||
        coast_position - established_position >
            simulation.rules.line_maximum_velocity_q16 /
                SURF_MAN_FIXED_HZ) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "neutral steering did not coast with bounded drag");
    }

    command.horizontal = -1;
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK ||
        simulation.maneuver_count != maneuver_count + 1U ||
        simulation.pending_score <= pending_score ||
        simulation.last_maneuver != SURF_MAN_MANEUVER_CARVE_LEFT ||
        simulation.line_velocity_q16 <= 0 ||
        simulation.line_velocity_q16 >= coast_velocity ||
        simulation.line_position_q16 < coast_position ||
        simulation.line_position_q16 - coast_position >
            simulation.rules.line_maximum_velocity_q16 /
                SURF_MAN_FIXED_HZ) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "turn reversal did not score once while preserving momentum");
    }
    for (uint32_t tick = 0U;
         tick < SURF_MAN_FIXED_HZ && simulation.line_velocity_q16 >= 0;
         ++tick) {
        const int32_t previous_position = simulation.line_position_q16;

        status = surf_man_simulation_step(&simulation, &command);
        if (status != AFORC_OK ||
            simulation.line_position_q16 <
                -simulation.rules.line_position_limit_q16 ||
            simulation.line_position_q16 >
                simulation.rules.line_position_limit_q16 ||
            surf_man_qa_distance_i32(
                simulation.line_position_q16, previous_position) >
                simulation.rules.line_maximum_velocity_q16 /
                    SURF_MAN_FIXED_HZ) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "reversal motion exceeded its deterministic travel bounds");
        }
    }
    if (simulation.line_velocity_q16 >= 0 ||
        simulation.maneuver_count != maneuver_count + 1U) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "reversal did not ease through zero without duplicate scoring");
    }

    bounded.line_position_q16 = bounded.rules.line_position_limit_q16 - 1;
    bounded.line_velocity_q16 = bounded.rules.line_maximum_velocity_q16;
    surf_man_command_clear(&command);
    command.horizontal = 1;
    status = surf_man_simulation_step(&bounded, &command);
    if (status != AFORC_OK ||
        bounded.line_position_q16 != bounded.rules.line_position_limit_q16 ||
        bounded.line_velocity_q16 != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "line motion did not stop at its configured boundary");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_wave_face_contract(uint64_t seed,
                                                    AFORC_Error *error) {
    SurfManSimulation baseline;
    SurfManSimulation climb;
    SurfManSimulation drop;
    SurfManSimulation bounded;
    SurfManCommand climb_command;
    SurfManCommand drop_command;
    int32_t initial_offset;
    AFORC_Status status = surf_man_qa_riding_init(
        &baseline, seed, SURF_MAN_WAVE_OPEN);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_ground_sample(&baseline, false);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "wave-face fixture could not find safe open water");
    }

    climb = baseline;
    drop = baseline;
    bounded = baseline;
    initial_offset = baseline.wave_face_offset_q16;
    surf_man_command_clear(&climb_command);
    surf_man_command_clear(&drop_command);
    climb_command.vertical = -1;
    drop_command.vertical = 1;
    for (uint32_t tick = 0U; tick < SURF_MAN_FIXED_HZ / 2U; ++tick) {
        const int32_t previous_climb_offset = climb.wave_face_offset_q16;
        const int32_t previous_drop_offset = drop.wave_face_offset_q16;

        status = surf_man_simulation_step(&climb, &climb_command);
        if (status == AFORC_OK) {
            status = surf_man_simulation_step(&drop, &drop_command);
        }
        if (status != AFORC_OK || climb.phase != SURF_MAN_RIDING ||
            drop.phase != SURF_MAN_RIDING ||
            climb.wave_face_offset_q16 <
                -climb.rules.wave_face_offset_limit_q16 ||
            climb.wave_face_offset_q16 >
                climb.rules.wave_face_offset_limit_q16 ||
            drop.wave_face_offset_q16 <
                -drop.rules.wave_face_offset_limit_q16 ||
            drop.wave_face_offset_q16 >
                drop.rules.wave_face_offset_limit_q16 ||
            climb.wave_face_velocity_q16 <
                -climb.rules.wave_face_maximum_velocity_q16 ||
            climb.wave_face_velocity_q16 >
                climb.rules.wave_face_maximum_velocity_q16 ||
            drop.wave_face_velocity_q16 <
                -drop.rules.wave_face_maximum_velocity_q16 ||
            drop.wave_face_velocity_q16 >
                drop.rules.wave_face_maximum_velocity_q16 ||
            surf_man_qa_distance_i32(
                climb.wave_face_offset_q16, previous_climb_offset) >
                climb.rules.wave_face_maximum_velocity_q16 /
                    SURF_MAN_FIXED_HZ ||
            surf_man_qa_distance_i32(
                drop.wave_face_offset_q16, previous_drop_offset) >
                drop.rules.wave_face_maximum_velocity_q16 /
                    SURF_MAN_FIXED_HZ) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "wave-face travel escaped its configured bounds");
        }
        if (tick == 0U &&
            (climb.wave_face_velocity_q16 <= 0 ||
             climb.wave_face_offset_q16 <= initial_offset ||
             drop.wave_face_velocity_q16 >= 0 ||
             drop.wave_face_offset_q16 >= initial_offset)) {
            return surf_man_qa_error(
                error,
                AFORC_ERROR_STATE,
                "vertical input lacked immediate bidirectional response");
        }
    }
    if (climb.wave_face_offset_q16 <= initial_offset ||
        drop.wave_face_offset_q16 >= initial_offset ||
        drop.speed_q16 <= climb.speed_q16) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "climb and drop motion did not couple intuitively to speed");
    }

    bounded.wave_face_offset_q16 =
        bounded.rules.wave_face_offset_limit_q16 - 1;
    bounded.wave_face_velocity_q16 =
        bounded.rules.wave_face_maximum_velocity_q16;
    status = surf_man_simulation_step(&bounded, &climb_command);
    if (status != AFORC_OK ||
        bounded.wave_face_offset_q16 !=
            bounded.rules.wave_face_offset_limit_q16 ||
        bounded.wave_face_velocity_q16 != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "wave-face motion did not stop at its configured boundary");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_seek_local_difference(
    SurfManSimulation *simulation,
    int32_t line_offset_q16,
    SurfManWaveSample *out_center,
    SurfManWaveSample *out_local) {
    const int32_t search_step_q16 = SURF_MAN_Q16_ONE / 4;

    for (uint32_t step = 0U;
         step < SURF_MAN_QA_SAMPLE_SEARCH_STEPS;
         ++step) {
        const int64_t distance_q16 = (int64_t)step * search_step_q16;
        AFORC_Status status;

        if (distance_q16 > INT32_MAX) {
            return AFORC_ERROR_LIMIT;
        }
        simulation->distance_q16 = (int32_t)distance_q16;
        status = surf_man_wave_sample(simulation, 0, out_center);
        if (status == AFORC_OK) {
            status = surf_man_wave_sample(
                simulation, line_offset_q16, out_local);
        }
        if (status != AFORC_OK) {
            return status;
        }
        if (out_local->face_q16 > 0 &&
            out_local->face_q16 / 4 != out_center->face_q16 / 4) {
            return AFORC_OK;
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

static AFORC_Status surf_man_qa_local_sample_contract(uint64_t seed,
                                                       AFORC_Error *error) {
    SurfManSimulation simulation;
    SurfManWaveSample center;
    SurfManWaveSample local;
    SurfManCommand command;
    int32_t expected_face;
    AFORC_Status status = surf_man_qa_riding_init(
        &simulation, seed, SURF_MAN_WAVE_OPEN);

    if (status == AFORC_OK) {
        simulation.line_position_q16 =
            simulation.rules.line_position_limit_q16;
        simulation.line_velocity_q16 = 0;
        status = surf_man_qa_seek_local_difference(
            &simulation, simulation.line_position_q16, &center, &local);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "local wave fixture found no distinct rider sample");
    }

    simulation.face_q16 = 0;
    expected_face = local.face_q16 / 4;
    surf_man_command_clear(&command);
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK || simulation.face_q16 != expected_face ||
        simulation.face_q16 == center.face_q16 / 4) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "ride physics did not consume the rider-local wave sample");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_seek_hazard(
    SurfManSimulation *simulation) {
    const int32_t search_step_q16 = SURF_MAN_Q16_ONE / 4;

    for (uint32_t step = 0U;
         step < SURF_MAN_QA_SAMPLE_SEARCH_STEPS;
         ++step) {
        const int64_t distance_q16 = (int64_t)step * search_step_q16;
        SurfManWaveSample sample;
        AFORC_Status status;

        if (distance_q16 > INT32_MAX) {
            return AFORC_ERROR_LIMIT;
        }
        simulation->distance_q16 = (int32_t)distance_q16;
        status = surf_man_wave_sample(simulation, 0, &sample);
        if (status != AFORC_OK) {
            return status;
        }
        if (sample.hazard) {
            return AFORC_OK;
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

static AFORC_Status surf_man_qa_hazard_contract(uint64_t seed,
                                                 AFORC_Error *error) {
    SurfManSimulation baseline;
    SurfManSimulation high_line;
    SurfManSimulation low_line;
    SurfManCommand command;
    AFORC_Status status = surf_man_qa_riding_init(
        &baseline, seed, SURF_MAN_WAVE_CHOP);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_hazard(&baseline);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "hazard fixture found no deterministic hazard");
    }

    high_line = baseline;
    low_line = baseline;
    high_line.wave_face_offset_q16 =
        high_line.rules.hazard_face_threshold_q16 + SURF_MAN_Q16_ONE / 2;
    low_line.wave_face_offset_q16 =
        low_line.rules.hazard_face_threshold_q16 - SURF_MAN_Q16_ONE / 2;
    high_line.wave_face_velocity_q16 = 0;
    low_line.wave_face_velocity_q16 = 0;
    surf_man_command_clear(&command);
    status = surf_man_simulation_step(&high_line, &command);
    if (status == AFORC_OK) {
        status = surf_man_simulation_step(&low_line, &command);
    }
    if (status != AFORC_OK || high_line.phase != SURF_MAN_RIDING ||
        low_line.phase != SURF_MAN_WIPEOUT_RECOVERY) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "hazard outcome did not depend on established face position");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_timing_threshold_contract(
    uint64_t seed,
    AFORC_Error *error) {
    SurfManSimulation lip_standard;
    SurfManSimulation lip_wide;
    SurfManSimulation hazard_standard;
    SurfManSimulation hazard_wide;
    SurfManCommand command;
    const uint32_t wide_timing_percent = 150U;
    int32_t wide_air_threshold;
    int32_t wide_hazard_threshold;
    AFORC_Status status = surf_man_qa_riding_init(
        &lip_standard, seed, SURF_MAN_WAVE_STEEP);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_ground_sample(&lip_standard, true);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "timing fixture could not find an unobstructed lip");
    }
    wide_air_threshold = (int32_t)(
        ((int64_t)lip_standard.rules.air_face_threshold_q16 * 100) /
        wide_timing_percent);
    wide_hazard_threshold = (int32_t)(
        ((int64_t)lip_standard.rules.hazard_face_threshold_q16 * 100) /
        wide_timing_percent);
    if (wide_air_threshold <= 0 ||
        wide_air_threshold >= lip_standard.rules.air_face_threshold_q16 ||
        wide_hazard_threshold <= 0 ||
        wide_hazard_threshold >=
            lip_standard.rules.hazard_face_threshold_q16) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "wide timing did not lower positive face thresholds");
    }

    lip_wide = lip_standard;
    lip_standard.wave_face_offset_q16 = wide_air_threshold;
    lip_wide.wave_face_offset_q16 = wide_air_threshold;
    lip_standard.wave_face_velocity_q16 = 0;
    lip_wide.wave_face_velocity_q16 = 0;
    lip_wide.settings.timing_percent = wide_timing_percent;
    surf_man_command_clear(&command);
    command.action = true;
    status = surf_man_simulation_step(&lip_standard, &command);
    if (status == AFORC_OK) {
        status = surf_man_simulation_step(&lip_wide, &command);
    }
    if (status != AFORC_OK || lip_standard.airborne ||
        lip_standard.last_maneuver != SURF_MAN_MANEUVER_LIP_SNAP ||
        !lip_wide.airborne) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "wide timing did not widen unchorded air eligibility");
    }

    status = surf_man_qa_riding_init(
        &hazard_standard, seed, SURF_MAN_WAVE_CHOP);
    if (status == AFORC_OK) {
        status = surf_man_qa_seek_hazard(&hazard_standard);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "timing fixture found no deterministic hazard");
    }
    hazard_wide = hazard_standard;
    hazard_standard.wave_face_offset_q16 = wide_hazard_threshold;
    hazard_wide.wave_face_offset_q16 = wide_hazard_threshold;
    hazard_standard.wave_face_velocity_q16 = 0;
    hazard_wide.wave_face_velocity_q16 = 0;
    hazard_wide.settings.timing_percent = wide_timing_percent;
    surf_man_command_clear(&command);
    status = surf_man_simulation_step(&hazard_standard, &command);
    if (status == AFORC_OK) {
        status = surf_man_simulation_step(&hazard_wide, &command);
    }
    if (status != AFORC_OK ||
        hazard_standard.phase != SURF_MAN_WIPEOUT_RECOVERY ||
        hazard_wide.phase != SURF_MAN_RIDING) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "wide timing did not widen positional hazard safety");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_neutral_air_contract(uint64_t seed,
                                                      AFORC_Error *error) {
    SurfManSimulation simulation;
    SurfManCommand command;
    AFORC_Status status = surf_man_qa_riding_init(
        &simulation, seed, SURF_MAN_WAVE_STEEP);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_ground_sample(&simulation, true);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "air fixture could not find an unobstructed lip");
    }

    surf_man_command_clear(&command);
    command.action = true;
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK || !simulation.airborne ||
        simulation.vertical_velocity_q16 <= 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "neutral Space at a lip did not launch an air");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_recovery_contract(uint64_t seed,
                                                   AFORC_Error *error) {
    SurfManSimulation simulation;
    SurfManCommand command;
    uint32_t active_tick;
    uint32_t wave_ticks_remaining;
    int32_t neutral_face_offset;
    bool moved_toward_center = false;
    AFORC_Status status = surf_man_qa_riding_init(
        &simulation, seed, SURF_MAN_WAVE_CHOP);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_hazard(&simulation);
    }
    if (status != AFORC_OK || simulation.rules.recovery_ticks == 0U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "recovery fixture could not start a timed recovery");
    }

    simulation.phase = SURF_MAN_WIPEOUT_RECOVERY;
    simulation.settings.timing_percent = 50U;
    simulation.phase_tick = 0U;
    simulation.active_tick = 17U;
    simulation.wave_ticks_remaining = simulation.rules.recovery_ticks + 23U;
    simulation.line_position_q16 =
        simulation.rules.line_position_limit_q16 / 2;
    simulation.line_velocity_q16 =
        -simulation.rules.line_maximum_velocity_q16 / 2;
    simulation.wave_face_offset_q16 =
        -simulation.rules.wave_face_offset_limit_q16 / 2;
    simulation.wave_face_velocity_q16 =
        simulation.rules.wave_face_maximum_velocity_q16 / 2;
    neutral_face_offset = simulation.rules.air_face_threshold_q16;
    active_tick = simulation.active_tick;
    wave_ticks_remaining = simulation.wave_ticks_remaining;
    surf_man_command_clear(&command);

    for (uint32_t tick = 0U; tick < simulation.rules.recovery_ticks; ++tick) {
        const int32_t previous_line_position = simulation.line_position_q16;
        const int32_t previous_line_velocity = simulation.line_velocity_q16;
        const int32_t previous_face_offset =
            simulation.wave_face_offset_q16;
        const int32_t previous_face_velocity =
            simulation.wave_face_velocity_q16;

        status = surf_man_simulation_step(&simulation, &command);
        if (status != AFORC_OK) {
            return surf_man_qa_error(
                error, status, "wipeout recovery tick did not complete");
        }
        if (surf_man_qa_distance_i32(simulation.line_position_q16, 0) >
                surf_man_qa_distance_i32(previous_line_position, 0) ||
            surf_man_qa_distance_i32(simulation.line_velocity_q16, 0) >
                surf_man_qa_distance_i32(previous_line_velocity, 0) ||
            surf_man_qa_distance_i32(
                simulation.wave_face_offset_q16, neutral_face_offset) >
                surf_man_qa_distance_i32(
                    previous_face_offset, neutral_face_offset) ||
            surf_man_qa_distance_i32(simulation.wave_face_velocity_q16, 0) >
                surf_man_qa_distance_i32(previous_face_velocity, 0)) {
            return surf_man_qa_error(
                error,
                AFORC_ERROR_STATE,
                "wipeout recovery moved authoritative motion away from center");
        }
        if (simulation.line_position_q16 != previous_line_position ||
            simulation.line_velocity_q16 != previous_line_velocity ||
            simulation.wave_face_offset_q16 != previous_face_offset ||
            simulation.wave_face_velocity_q16 != previous_face_velocity) {
            moved_toward_center = true;
        }
        if (simulation.active_tick != active_tick ||
            simulation.wave_ticks_remaining != wave_ticks_remaining) {
            return surf_man_qa_error(
                error,
                AFORC_ERROR_STATE,
                "wipeout recovery consumed active wave time");
        }
        if (tick + 1U < simulation.rules.recovery_ticks) {
            if (simulation.phase != SURF_MAN_WIPEOUT_RECOVERY ||
                simulation.phase_tick != tick + 1U) {
                return surf_man_qa_error(
                    error,
                    AFORC_ERROR_STATE,
                    "wipeout recovery ended before recovery_ticks");
            }
        } else if (simulation.phase != SURF_MAN_RIDING ||
                   simulation.phase_tick != 0U ||
                   simulation.line_position_q16 != 0 ||
                   simulation.line_velocity_q16 != 0 ||
                   simulation.wave_face_offset_q16 != neutral_face_offset ||
                   simulation.wave_face_velocity_q16 != 0) {
            return surf_man_qa_error(
                error,
                AFORC_ERROR_STATE,
                "wipeout recovery did not resume centered at recovery_ticks");
        }
    }
    if (!moved_toward_center) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "wipeout recovery did not visibly recenter authoritative motion");
    }
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK || simulation.phase != SURF_MAN_RIDING ||
        simulation.active_tick != active_tick + 1U ||
        simulation.wave_ticks_remaining != wave_ticks_remaining - 1U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "centered recovery did not resume safely on the next ride tick");
    }
    return AFORC_OK;
}

typedef struct SurfManQaHashField {
    size_t offset;
    const char *message;
} SurfManQaHashField;

static AFORC_Status surf_man_qa_hash_contract(uint64_t seed,
                                               AFORC_Error *error) {
    static const SurfManQaHashField fields[] = {
        {offsetof(SurfManSimulation, line_position_q16),
         "line position is absent from the simulation hash"},
        {offsetof(SurfManSimulation, line_velocity_q16),
         "line velocity is absent from the simulation hash"},
        {offsetof(SurfManSimulation, wave_face_offset_q16),
         "wave-face offset is absent from the simulation hash"},
        {offsetof(SurfManSimulation, wave_face_velocity_q16),
         "wave-face velocity is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, line_position_limit_q16),
         "line position limit is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, line_maximum_velocity_q16),
         "line velocity limit is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, line_acceleration_q16),
         "line acceleration is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, line_drag_q16),
         "line drag is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, carve_velocity_threshold_q16),
         "carve threshold is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, wave_face_offset_limit_q16),
         "wave-face offset limit is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, wave_face_maximum_velocity_q16),
         "wave-face velocity limit is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, wave_face_acceleration_q16),
         "wave-face acceleration is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, wave_face_drag_q16),
         "wave-face drag is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, air_face_threshold_q16),
         "air face threshold is absent from the simulation hash"},
        {offsetof(SurfManSimulation, rules) +
             offsetof(SurfManRules, hazard_face_threshold_q16),
         "hazard face threshold is absent from the simulation hash"},
    };
    SurfManSimulation simulation;
    const SurfManSettings settings = surf_man_settings_default();
    uint64_t baseline_hash;
    AFORC_Status status =
        surf_man_simulation_init(&simulation, seed, &settings);

    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "hash fixture could not initialize simulation");
    }
    baseline_hash = surf_man_simulation_hash(&simulation);
    for (size_t index = 0U; index < sizeof(fields) / sizeof(fields[0]); ++index) {
        SurfManSimulation mutated = simulation;
        unsigned char *bytes = (unsigned char *)(void *)&mutated;
        int32_t value;

        memcpy(&value, bytes + fields[index].offset, sizeof(value));
        value = value == INT32_MAX ? value - 1 : value + 1;
        memcpy(bytes + fields[index].offset, &value, sizeof(value));
        if (surf_man_simulation_hash(&mutated) == baseline_hash) {
            return surf_man_qa_error(
                error, AFORC_ERROR_STATE, fields[index].message);
        }
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_gameplay_contracts(uint64_t seed,
                                                    AFORC_Error *error) {
    AFORC_Status status = surf_man_qa_steering_contract(seed, error);

    if (status == AFORC_OK) {
        status = surf_man_qa_wave_face_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_local_sample_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_hazard_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_timing_threshold_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_neutral_air_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_recovery_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_hash_contract(seed, error);
    }
    return status;
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
    status = surf_man_qa_gameplay_contracts(seed, error);

    if (status == AFORC_OK) {
        status = surf_man_qa_schedule_checks(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_score_checks(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_transition_checks(seed, error);
    }
    return status;
}

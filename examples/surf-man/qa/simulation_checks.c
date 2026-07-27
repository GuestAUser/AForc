/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/qa.h"

#include <limits.h>
#include <string.h>

enum {
    SURF_MAN_QA_SCHEDULE_TICKS = 20 * SURF_MAN_FIXED_HZ,
    SURF_MAN_QA_MIN_RIDING_TICKS = 5 * SURF_MAN_FIXED_HZ,
    SURF_MAN_QA_SAMPLE_SEARCH_STEPS = 1024,
    SURF_MAN_QA_CARVE_HOLD_TICKS = SURF_MAN_FIXED_HZ / 3,
    SURF_MAN_QA_AWARD_TICKS = 2 * SURF_MAN_FIXED_HZ
};

typedef enum SurfManQaZone {
    SURF_MAN_QA_ZONE_CLEAR = 0,
    SURF_MAN_QA_ZONE_POCKET,
    SURF_MAN_QA_ZONE_FOAM,
    SURF_MAN_QA_ZONE_TUBE,
    SURF_MAN_QA_ZONE_HAZARD
} SurfManQaZone;

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

static uint32_t surf_man_qa_sample_role_count(
    const SurfManWaveSample *sample) {
    return (sample->pocket ? 1U : 0U) + (sample->foam ? 1U : 0U) +
           (sample->tube ? 1U : 0U) + (sample->hazard ? 1U : 0U);
}

static bool surf_man_qa_sample_is_zone(const SurfManWaveSample *sample,
                                       SurfManQaZone zone) {
    if (surf_man_qa_sample_role_count(sample) !=
        (zone == SURF_MAN_QA_ZONE_CLEAR ? 0U : 1U)) {
        return false;
    }
    switch (zone) {
        case SURF_MAN_QA_ZONE_CLEAR:
            return !sample->lip;
        case SURF_MAN_QA_ZONE_POCKET:
            return sample->pocket;
        case SURF_MAN_QA_ZONE_FOAM:
            return sample->foam;
        case SURF_MAN_QA_ZONE_TUBE:
            return sample->tube;
        case SURF_MAN_QA_ZONE_HAZARD:
            return sample->hazard;
        default:
            return false;
    }
}

static AFORC_Status surf_man_qa_seek_zone(
    SurfManSimulation *simulation,
    SurfManQaZone zone,
    SurfManWaveSample *out_sample) {
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
        if (surf_man_qa_sample_is_zone(&sample, zone)) {
            if (out_sample != NULL) {
                *out_sample = sample;
            }
            return AFORC_OK;
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

static AFORC_Status surf_man_qa_tube_commit_contract(uint64_t seed,
                                                      AFORC_Error *error) {
    SurfManSimulation simulation;
    SurfManCommand command;
    uint64_t pending_score;
    uint32_t maneuver_count;
    AFORC_Status status = surf_man_qa_riding_init(
        &simulation, seed, SURF_MAN_WAVE_TUBE);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_zone(
            &simulation, SURF_MAN_QA_ZONE_TUBE, NULL);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "tube fixture found no exclusive tube section");
    }

    pending_score = simulation.pending_score;
    maneuver_count = simulation.maneuver_count;
    surf_man_command_clear(&command);
    command.action = true;
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK || simulation.tube_ticks != 1U ||
        simulation.pending_score != pending_score ||
        simulation.maneuver_count != maneuver_count ||
        simulation.last_maneuver != SURF_MAN_MANEUVER_NONE ||
        !simulation.risk_active) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "tube tap did not start an unscored committed ride");
    }

    surf_man_command_clear(&command);
    for (uint32_t tick = 0U; tick < 4U; ++tick) {
        status = surf_man_simulation_step(&simulation, &command);
        if (status != AFORC_OK || simulation.tube_ticks != tick + 2U ||
            simulation.pending_score != pending_score ||
            simulation.maneuver_count != maneuver_count) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "tube tap did not stay committed through the tube section");
        }
    }
    status = surf_man_qa_seek_zone(
        &simulation, SURF_MAN_QA_ZONE_CLEAR, NULL);
    if (status == AFORC_OK) {
        status = surf_man_simulation_step(&simulation, &command);
    }
    if (status != AFORC_OK || simulation.tube_ticks != 0U ||
        simulation.pending_score != pending_score + UINT64_C(40) ||
        simulation.maneuver_count != maneuver_count + 1U ||
        simulation.flow != 1U ||
        simulation.last_maneuver != SURF_MAN_MANEUVER_TUBE ||
        strcmp(simulation.award, "TUBE") != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "first committed tube did not score fully and build flow on exit");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_zone_role_contract(uint64_t seed,
                                                    AFORC_Error *error) {
    bool saw_pocket = false;
    bool saw_foam = false;
    bool saw_tube = false;
    bool saw_hazard = false;
    SurfManSimulation pocket;
    SurfManSimulation foam;
    SurfManWaveSample pocket_sample;
    SurfManWaveSample foam_sample;
    SurfManCommand command;
    int32_t pocket_without_role;
    int32_t foam_without_role;
    AFORC_Status status;

    for (uint32_t kind = SURF_MAN_WAVE_OPEN;
         kind <= SURF_MAN_WAVE_CLOSEOUT;
         ++kind) {
        SurfManSimulation simulation;

        status = surf_man_qa_riding_init(
            &simulation, seed, (SurfManWaveKind)kind);
        if (status != AFORC_OK) {
            return surf_man_qa_error(
                error, status, "zone fixture could not initialize wave kind");
        }
        for (uint32_t step = 0U;
             step < SURF_MAN_QA_SAMPLE_SEARCH_STEPS;
             ++step) {
            SurfManWaveSample sample;

            simulation.distance_q16 =
                (int32_t)step * (SURF_MAN_Q16_ONE / 4);
            status = surf_man_wave_sample(&simulation, 0, &sample);
            if (status != AFORC_OK) {
                return surf_man_qa_error(
                    error, status, "zone fixture could not sample wave roles");
            }
            if (surf_man_qa_sample_role_count(&sample) > 1U) {
                return surf_man_qa_error(
                    error,
                    AFORC_ERROR_STATE,
                    "pocket, foam, tube, and hazard roles overlapped");
            }
            saw_pocket = saw_pocket || sample.pocket;
            saw_foam = saw_foam || sample.foam;
            saw_tube = saw_tube || sample.tube;
            saw_hazard = saw_hazard || sample.hazard;
        }
    }
    if (!saw_pocket || !saw_foam || !saw_tube || !saw_hazard) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "wave roles did not expose pocket, foam, tube, and hazard play");
    }

    status = surf_man_qa_riding_init(
        &pocket, seed, SURF_MAN_WAVE_OPEN);
    if (status == AFORC_OK) {
        status = surf_man_qa_seek_zone(
            &pocket, SURF_MAN_QA_ZONE_POCKET, &pocket_sample);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_riding_init(
            &foam, seed, SURF_MAN_WAVE_CHOP);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_seek_zone(
            &foam, SURF_MAN_QA_ZONE_FOAM, &foam_sample);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "zone fixture could not find pocket and foam roles");
    }

    pocket.speed_q16 = 4 * SURF_MAN_Q16_ONE;
    pocket.wave_face_velocity_q16 = 0;
    foam.speed_q16 = 4 * SURF_MAN_Q16_ONE;
    foam.wave_face_velocity_q16 = 0;
    pocket_without_role =
        pocket.speed_q16 + pocket_sample.push_q16 / SURF_MAN_FIXED_HZ;
    foam_without_role =
        foam.speed_q16 + foam_sample.push_q16 / SURF_MAN_FIXED_HZ;
    surf_man_command_clear(&command);
    status = surf_man_simulation_step(&pocket, &command);
    if (status == AFORC_OK) {
        status = surf_man_simulation_step(&foam, &command);
    }
    if (status != AFORC_OK || pocket.speed_q16 <= pocket_without_role ||
        foam.speed_q16 >= foam_without_role) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "pocket did not add drive or foam did not remove speed");
    }
    return AFORC_OK;
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
        simulation.maneuver_count != maneuver_count ||
        simulation.pending_score != pending_score ||
        simulation.last_maneuver != SURF_MAN_MANEUVER_NONE ||
        simulation.line_velocity_q16 <= 0 ||
        simulation.line_velocity_q16 >= coast_velocity ||
        simulation.line_position_q16 < coast_position ||
        simulation.line_position_q16 - coast_position >
            simulation.rules.line_maximum_velocity_q16 /
                SURF_MAN_FIXED_HZ) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "opposite tap scored before line velocity reversed");
    }
    surf_man_command_clear(&command);
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
        if (simulation.line_velocity_q16 >= 0 &&
            (simulation.maneuver_count != maneuver_count ||
             simulation.pending_score != pending_score ||
             simulation.last_maneuver != SURF_MAN_MANEUVER_NONE)) {
            return surf_man_qa_error(
                error,
                AFORC_ERROR_STATE,
                "committed carve scored before crossing into its direction");
        }
    }
    if (simulation.line_velocity_q16 >= 0 ||
        simulation.maneuver_count != maneuver_count + 1U ||
        simulation.pending_score <= pending_score ||
        simulation.last_maneuver != SURF_MAN_MANEUVER_CARVE_LEFT ||
        strcmp(simulation.award, "LEFT CARVE") != 0) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "opposite tap did not score when line velocity crossed direction");
    }
    status = surf_man_simulation_step(&simulation, &command);
    if (status != AFORC_OK ||
        simulation.maneuver_count != maneuver_count + 1U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "committed carve scored more than once after direction crossing");
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

static AFORC_Status surf_man_qa_award_expiry_contract(uint64_t seed,
                                                       AFORC_Error *error) {
    SurfManSimulation simulation;
    SurfManCommand command;
    AFORC_Status status = surf_man_qa_riding_init(
        &simulation, seed, SURF_MAN_WAVE_OPEN);

    if (status == AFORC_OK) {
        status = surf_man_qa_seek_ground_sample(&simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_score_maneuver(
            &simulation, SURF_MAN_MANEUVER_CARVE_LEFT, 0U, false);
    }
    if (status != AFORC_OK || strcmp(simulation.award, "LEFT CARVE") != 0) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "award expiry fixture could not create a carve award");
    }

    surf_man_command_clear(&command);
    for (uint32_t tick = 0U; tick < SURF_MAN_QA_AWARD_TICKS; ++tick) {
        status = surf_man_simulation_step(&simulation, &command);
        if (status != AFORC_OK) {
            return surf_man_qa_error(
                error, status, "award expiry fixture could not advance");
        }
        if (tick + 1U < SURF_MAN_QA_AWARD_TICKS &&
            simulation.award[0] == '\0') {
            return surf_man_qa_error(
                error, AFORC_ERROR_STATE, "award expired before two seconds");
        }
    }
    if (simulation.award[0] != '\0') {
        return surf_man_qa_error(
            error, AFORC_ERROR_STATE, "award remained stale after two seconds");
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
        {offsetof(SurfManSimulation, award_ticks),
         "award lifetime is absent from the simulation hash"},
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
    simulation.carve_direction = -1;
    if (surf_man_simulation_hash(&simulation) == baseline_hash) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "committed carve direction is absent from the simulation hash");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_gameplay_contracts(uint64_t seed,
                                                    AFORC_Error *error) {
    AFORC_Status status = surf_man_qa_steering_contract(seed, error);

    if (status == AFORC_OK) {
        status = surf_man_qa_tube_commit_contract(seed, error);
    }
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
        status = surf_man_qa_zone_role_contract(seed, error);
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
        status = surf_man_qa_award_expiry_contract(seed, error);
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

static AFORC_Status surf_man_qa_practice_best_contract(uint64_t seed,
                                                        AFORC_Error *error) {
    const SurfManSettings settings = surf_man_settings_default();
    SurfManSimulation simulation;
    uint64_t finite_best;
    AFORC_Status status =
        surf_man_simulation_init(&simulation, seed, &settings);

    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&simulation, false);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "best-score fixture could not start normal day");
    }
    simulation.pending_score = UINT64_C(500);
    surf_man_score_bank(&simulation);
    finite_best = simulation.best_score;
    if (finite_best != UINT64_C(500)) {
        return surf_man_qa_error(
            error, AFORC_ERROR_STATE, "normal day did not establish best score");
    }

    status = surf_man_simulation_start_day(&simulation, true);
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "best-score fixture could not start Practice");
    }
    simulation.pending_score = finite_best + UINT64_C(500);
    surf_man_score_bank(&simulation);
    if (simulation.day_score <= finite_best ||
        simulation.best_score != finite_best) {
        return surf_man_qa_error(
            error,
            AFORC_ERROR_STATE,
            "Practice score mutated finite-day best score");
    }

    status = surf_man_simulation_start_day(&simulation, false);
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "best-score fixture could not resume normal day");
    }
    simulation.pending_score = finite_best + UINT64_C(1);
    surf_man_score_bank(&simulation);
    if (simulation.best_score != finite_best + UINT64_C(1)) {
        return surf_man_qa_error(
            error, AFORC_ERROR_STATE, "later normal day did not raise best score");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_wave_progression_contract(
    uint64_t seed,
    AFORC_Error *error) {
    const SurfManSettings settings = surf_man_settings_default();

    for (uint32_t trial = 0U; trial < 16U; ++trial) {
        SurfManSimulation simulation;
        const uint64_t trial_seed =
            seed ^ (UINT64_C(0x9e3779b97f4a7c15) * trial);
        AFORC_Status status =
            surf_man_simulation_init(&simulation, trial_seed, &settings);

        if (status == AFORC_OK) {
            status = surf_man_simulation_start_day(&simulation, false);
        }
        if (status == AFORC_OK) {
            status = surf_man_simulation_start_wave(&simulation);
        }
        if (status != AFORC_OK || simulation.wave != 1U ||
            simulation.wave_kind != SURF_MAN_WAVE_OPEN) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "wave one was not a readable OPEN introduction");
        }

        status = surf_man_simulation_start_wave(&simulation);
        if (status != AFORC_OK || simulation.wave != 2U ||
            (simulation.wave_kind != SURF_MAN_WAVE_STEEP &&
             simulation.wave_kind != SURF_MAN_WAVE_TUBE)) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "wave two did not introduce STEEP or TUBE play");
        }

        status = surf_man_simulation_start_wave(&simulation);
        if (status != AFORC_OK || simulation.wave != 3U ||
            (simulation.wave_kind != SURF_MAN_WAVE_CHOP &&
             simulation.wave_kind != SURF_MAN_WAVE_CLOSEOUT)) {
            return surf_man_qa_error(
                error,
                status == AFORC_OK ? AFORC_ERROR_STATE : status,
                "wave three did not escalate to CHOP or CLOSEOUT");
        }
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_end_session_contract(uint64_t seed,
                                                      AFORC_Error *error) {
    SurfManSettings settings = surf_man_settings_default();
    SurfManSimulation simulation;
    SurfManSimulation uninitialized = {0};
    uint32_t day;
    uint64_t best_score;
    AFORC_Status status;

    if (surf_man_simulation_end_session(NULL) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        surf_man_simulation_end_session(&uninitialized) != AFORC_ERROR_STATE) {
        return surf_man_qa_error(
            error, AFORC_ERROR_STATE, "end-session accepted invalid state");
    }

    settings.speed_percent = 75U;
    status = surf_man_simulation_init(&simulation, seed, &settings);
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&simulation);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "end-session fixture could not start normal day");
    }
    simulation.phase = SURF_MAN_RIDING;
    simulation.day_score = UINT64_C(70);
    simulation.pending_score = UINT64_C(30);
    simulation.best_score = UINT64_C(70);
    simulation.flow = 3U;
    simulation.risk_active = true;
    day = simulation.day;
    status = surf_man_simulation_end_session(&simulation);
    if (status != AFORC_OK || simulation.phase != SURF_MAN_SHACK ||
        simulation.practice || simulation.wave != 0U ||
        simulation.wave_kind != SURF_MAN_WAVE_OPEN ||
        simulation.day_score != 0U ||
        simulation.pending_score != 0U || simulation.best_score != 70U ||
        simulation.day != day || simulation.seed != seed ||
        simulation.settings.speed_percent != 75U ||
        simulation.wave_ticks_remaining != 0U ||
        simulation.segment_ticks_remaining != 0U || simulation.flow != 0U ||
        simulation.risk_active || simulation.award[0] != '\0') {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "normal end-session banked pending score or left ride telemetry");
    }

    best_score = simulation.best_score;
    status = surf_man_simulation_start_day(&simulation, true);
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&simulation);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_error(
            error, status, "end-session fixture could not start Practice");
    }
    simulation.phase = SURF_MAN_PRACTICE;
    simulation.day_score = UINT64_C(40);
    simulation.pending_score = UINT64_C(20);
    status = surf_man_simulation_end_session(&simulation);
    if (status != AFORC_OK || simulation.phase != SURF_MAN_SHACK ||
        simulation.practice || simulation.day_score != 0U ||
        simulation.pending_score != 0U ||
        simulation.best_score != best_score || simulation.day != day ||
        simulation.wave != 0U || simulation.wave_ticks_remaining != 0U) {
        return surf_man_qa_error(
            error,
            status == AFORC_OK ? AFORC_ERROR_STATE : status,
            "Practice end-session did not bank pending score and clear state");
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
        status = surf_man_qa_practice_best_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_wave_progression_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_end_session_contract(seed, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_transition_checks(seed, error);
    }
    return status;
}

/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man_internal.h"

#include <string.h>

enum { SURF_MAN_QA_LIP_SEARCH_STEPS = 1024 };

static AFORC_Status surf_man_qa_smoke_error(AFORC_Error *error,
                                            AFORC_Status status,
                                            const char *message) {
    aforc_error_set(error, status, "surf-man qa", "%s", message);
    return status;
}

static AFORC_Status surf_man_qa_command_checks(SurfManApp *app,
                                               AFORC_Engine *engine,
                                               AFORC_Error *error) {
    const SurfManInputState saved_controls = app->controls;
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManOverlay saved_overlay = app->overlay;
    const bool saved_focused = app->focused;
    const bool saved_dirty = app->visuals.dirty;
    SurfManCommand first;
    SurfManCommand second;
    SurfManCommand third;
    AFORC_InputEvent event = {0};
    AFORC_Status status = AFORC_OK;

    app->controls = (SurfManInputState){0};
    app->controls.vertical = 1;
    app->controls.horizontal = -1;
    app->controls.vertical_lease = 2U;
    app->controls.horizontal_lease = 1U;
    app->controls.confirm_latched = true;
    app->controls.back_latched = true;
    surf_man_input_take_command(app, &first);
    surf_man_input_take_command(app, &second);
    surf_man_input_take_command(app, &third);
    if (first.vertical != 1 || first.horizontal != -1 || first.action ||
        !first.confirm || !first.back || second.vertical != 1 ||
        second.horizontal != 0 || second.action || second.confirm ||
        second.back || third.vertical != 0 || third.horizontal != 0 ||
        third.action || third.confirm || third.back) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        app->simulation.phase = SURF_MAN_RIDING;
        app->overlay = SURF_MAN_OVERLAY_NONE;
        app->focused = true;
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_LEFT;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.type = AFORC_INPUT_EVENT_KEY_UP;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        surf_man_input_take_command(app, &second);
        if (first.horizontal != -1 || second.horizontal != 0) {
            status = AFORC_ERROR_STATE;
        }
    }

    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.type = AFORC_INPUT_EVENT_KEY_UP;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        surf_man_input_take_command(app, &second);
        if (!first.action || second.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_COMMAND_LEASE_TICKS;
         ++tick) {
        surf_man_input_take_command(app, &first);
        if (!first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        if (first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_COMMAND_LEASE_TICKS;
         ++tick) {
        surf_man_input_take_command(app, &first);
        if (!first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.type = AFORC_INPUT_EVENT_KEY_UP;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        if (first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_P;
        event.data.key.codepoint = (uint32_t)'p';
        status = surf_man_app_handle_event(app, engine, &event);
        if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_app_handle_event(app, engine, &event);
        if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_NONE) {
            status = AFORC_ERROR_STATE;
        }
    }

    app->controls = saved_controls;
    app->simulation = saved_simulation;
    app->overlay = saved_overlay;
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "input taps, leases, releases, or one-shot controls were incorrect");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_seek_safe_lip(
    SurfManSimulation *simulation) {
    const int32_t search_step_q16 = SURF_MAN_Q16_ONE / 4;

    for (uint32_t step = 0U; step < SURF_MAN_QA_LIP_SEARCH_STEPS; ++step) {
        SurfManWaveSample sample;
        AFORC_Status status;

        simulation->distance_q16 = (int32_t)step * search_step_q16;
        status = surf_man_wave_sample(
            simulation, simulation->line_position_q16, &sample);
        if (status != AFORC_OK) {
            return status;
        }
        if (sample.lip && !sample.hazard && !sample.tube) {
            return AFORC_OK;
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

static AFORC_Status surf_man_qa_prepare_action_probe(
    SurfManApp *probe,
    const SurfManApp *app) {
    AFORC_Status status;

    probe->controls = (SurfManInputState){0};
    status = surf_man_simulation_init(
        &probe->simulation, app->seed, &probe->settings);
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&probe->simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&probe->simulation);
    }
    if (status == AFORC_OK) {
        probe->simulation.phase = SURF_MAN_RIDING;
        probe->simulation.phase_tick = 0U;
        probe->simulation.wave_kind = SURF_MAN_WAVE_STEEP;
        status = surf_man_qa_seek_safe_lip(&probe->simulation);
    }
    return status;
}

static AFORC_Status surf_man_qa_action_transition_checks(
    const SurfManApp *app,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    SurfManApp probe = {0};
    AFORC_InputEvent event = {0};
    uint32_t initial_maneuver_count = 0U;
    uint32_t snapped_maneuver_count = 0U;
    uint64_t snapped_score = 0U;
    bool visuals_initialized = false;
    AFORC_Status status;

    probe.scene.vtable = &surf_man_scene_vtable;
    probe.scene.user_data = &probe;
    probe.settings = surf_man_settings_default();
    probe.settings.reduced_motion = true;
    probe.overlay = SURF_MAN_OVERLAY_NONE;
    probe.terminal_size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                      SURF_MAN_TARGET_ROWS};
    probe.focused = true;
    probe.initialized = true;
    status = surf_man_qa_prepare_action_probe(&probe, app);
    if (status == AFORC_OK) {
        status = surf_man_visuals_init(&probe.visuals, UINT32_C(0xa17e57));
        visuals_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        probe.simulation.wave_face_offset_q16 =
            (probe.simulation.rules.air_face_threshold_q16 +
             probe.simulation.rules.hazard_face_threshold_q16) /
            2;
        initial_maneuver_count = probe.simulation.maneuver_count;
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.airborne ||
         probe.simulation.last_maneuver != SURF_MAN_MANEUVER_LIP_SNAP ||
         probe.simulation.maneuver_count != initial_maneuver_count + 1U ||
         probe.controls.action_lease != 0U || probe.controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        snapped_maneuver_count = probe.simulation.maneuver_count;
        snapped_score = probe.simulation.pending_score;
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_COMMAND_LEASE_TICKS;
         ++tick) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
        if (status == AFORC_OK &&
            (probe.simulation.maneuver_count != snapped_maneuver_count ||
             probe.simulation.pending_score != snapped_score)) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_seek_safe_lip(&probe.simulation);
    }
    if (status == AFORC_OK) {
        probe.simulation.wave_face_offset_q16 =
            (probe.simulation.rules.air_face_threshold_q16 +
             probe.simulation.rules.hazard_face_threshold_q16) /
            2;
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.airborne ||
         probe.simulation.last_maneuver != SURF_MAN_MANEUVER_LIP_SNAP ||
         probe.simulation.maneuver_count != snapped_maneuver_count + 1U ||
         probe.simulation.pending_score <= snapped_score ||
         probe.controls.action_lease != 0U || probe.controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        status = surf_man_qa_prepare_action_probe(&probe, app);
    }
    if (status == AFORC_OK) {
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (!probe.simulation.airborne || probe.simulation.grabbed ||
         probe.controls.action_lease != 0U || probe.controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (!probe.simulation.airborne || probe.simulation.grabbed)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK && !probe.simulation.grabbed) {
        status = AFORC_ERROR_STATE;
    }

    if (visuals_initialized) {
        surf_man_visuals_dispose(&probe.visuals);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "snap or launch action leaked across fixed ticks");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_visual_timing_checks(SurfManApp *app,
                                                      AFORC_Engine *engine,
                                                      AFORC_Error *error) {
    SurfManApp probe = {0};
    AFORC_ParticleDesc particle = {0};
    size_t particle_index = 0U;
    AFORC_Status status;

    probe.scene.vtable = &surf_man_scene_vtable;
    probe.scene.user_data = &probe;
    probe.simulation = app->simulation;
    probe.simulation.phase = SURF_MAN_SHACK;
    probe.settings = surf_man_settings_default();
    probe.settings.reduced_motion = true;
    probe.simulation.settings = probe.settings;
    probe.overlay = SURF_MAN_OVERLAY_NONE;
    probe.terminal_size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                      SURF_MAN_TARGET_ROWS};
    probe.focused = true;
    probe.initialized = true;

    status = surf_man_visuals_init(&probe.visuals, UINT32_C(0x51f15e));
    if (status == AFORC_OK) {
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK && probe.visuals.dirty) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_COUNT_IN;
        probe.simulation.phase_tick = 0U;
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase_tick != 1U || !probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_WAVE_RECAP;
        probe.simulation.phase_tick = 0U;
        probe.controls = (SurfManInputState){0};
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase_tick != 1U || probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_DAY_RECAP;
        probe.simulation.phase_tick = 0U;
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase_tick != 1U || probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_WAVE_RECAP;
        probe.simulation.phase_tick = 0U;
        probe.simulation.wave = SURF_MAN_WAVES_PER_DAY;
        probe.controls.confirm_latched = true;
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase != SURF_MAN_DAY_RECAP ||
         !probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.settings.reduced_motion = false;
        probe.simulation.settings = probe.settings;
        probe.simulation.phase = SURF_MAN_RIDING;
        probe.visuals.visual_tick = 0U;
        probe.visuals.dirty = false;
        particle.lifetime_ms = 2000U;
        particle.cell = aforc_cell_default();
        status = aforc_particle_pool_spawn(&probe.visuals.particle_pool,
                                           &particle,
                                           &particle_index);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_VISUAL_HZ;
         ++tick) {
        status = surf_man_visuals_step(&probe);
    }
    if (status == AFORC_OK &&
        (!probe.visuals.particles[particle_index].active ||
         probe.visuals.particles[particle_index].age_ms != 1000U)) {
        status = AFORC_ERROR_STATE;
    }
    surf_man_visuals_dispose(&probe.visuals);
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "visual timing, recap dirtiness, or reduced-motion tracking was incorrect");
    }
    return AFORC_OK;
}

static bool surf_man_qa_renderer_uses_color_mode(
    const AFORC_Renderer *renderer,
    AFORC_ColorMode mode) {
    const AFORC_Size size = aforc_renderer_size(renderer);

    for (int32_t y = 0; y < size.height; ++y) {
        for (int32_t x = 0; x < size.width; ++x) {
            AFORC_Cell cell;

            if (aforc_renderer_get(
                    renderer, (AFORC_Point){x, y}, &cell) != AFORC_OK ||
                cell.foreground.mode != mode || cell.background.mode != mode) {
                return false;
            }
        }
    }
    return true;
}

static AFORC_Status surf_man_qa_color_particle_checks(
    const SurfManApp *app,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    SurfManApp probe = {0};
    SurfManInputKey input = {0};
    AFORC_ParticleDesc particle = {0};
    bool visuals_initialized = false;
    AFORC_Status status;

    probe.scene.vtable = &surf_man_scene_vtable;
    probe.scene.user_data = &probe;
    probe.renderer = app->renderer;
    probe.simulation = app->simulation;
    probe.simulation.phase = SURF_MAN_RIDING;
    probe.settings = surf_man_settings_default();
    probe.settings.color_mode = SURF_MAN_COLOR_HIGH_CONTRAST;
    probe.simulation.settings = probe.settings;
    probe.overlay = SURF_MAN_OVERLAY_ACCESSIBILITY;
    probe.menu_item = SURF_MAN_MENU_QUIT;
    probe.terminal_size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                      SURF_MAN_TARGET_ROWS};
    probe.focused = true;
    probe.initialized = true;

    status = surf_man_visuals_init(&probe.visuals, UINT32_C(0xc0104));
    visuals_initialized = status == AFORC_OK;
    particle.position.x =
        (SURF_MAN_TARGET_COLUMNS - 10) * AFORC_EFFECT_FIXED_ONE;
    particle.position.y = AFORC_EFFECT_FIXED_ONE;
    particle.lifetime_ms = 1000U;
    particle.cell = surf_man_cell(
        (uint32_t)'*', UINT8_C(77), AFORC_STYLE_BOLD);
    if (status == AFORC_OK) {
        status = aforc_particle_pool_spawn(
            &probe.visuals.particle_pool, &particle, NULL);
    }
    input.horizontal = 1;
    if (status == AFORC_OK) {
        status = surf_man_menu_handle_modal_key(&probe, engine, &input);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_frame(&probe, 0.0, error);
    }
    if (status == AFORC_OK &&
        (probe.settings.color_mode != SURF_MAN_COLOR_NONE ||
         probe.visuals.particle_pool.active_count != 0U ||
         !surf_man_qa_renderer_uses_color_mode(
             probe.renderer, AFORC_COLOR_DEFAULT))) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        status = surf_man_menu_handle_modal_key(&probe, engine, &input);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_frame(&probe, 0.0, error);
    }
    if (status == AFORC_OK &&
        (probe.settings.color_mode != SURF_MAN_COLOR_STANDARD ||
         !surf_man_qa_renderer_uses_color_mode(
             probe.renderer, AFORC_COLOR_INDEXED))) {
        status = AFORC_ERROR_STATE;
    }

    if (visuals_initialized) {
        surf_man_visuals_dispose(&probe.visuals);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "color-mode transition retained stale particles or cell colors");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_pause_checks(SurfManApp *app,
                                              AFORC_Engine *engine,
                                             AFORC_Error *error) {
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManInputState saved_controls = app->controls;
    const SurfManOverlay saved_overlay = app->overlay;
    const AFORC_Size saved_size = app->terminal_size;
    const bool saved_focused = app->focused;
    const bool saved_dirty = app->visuals.dirty;
    char saved_message[SURF_MAN_MESSAGE_CAPACITY];
    AFORC_InputEvent event = {0};
    uint64_t paused_hash;
    AFORC_Status status;

    (void)memcpy(saved_message, app->message, sizeof(saved_message));
    app->simulation.phase = SURF_MAN_RIDING;
    app->overlay = SURF_MAN_OVERLAY_NONE;
    app->focused = true;
    app->controls = (SurfManInputState){
        .vertical = 1,
        .horizontal = -1,
        .vertical_lease = SURF_MAN_COMMAND_LEASE_TICKS,
        .horizontal_lease = SURF_MAN_COMMAND_LEASE_TICKS,
        .action_lease = SURF_MAN_COMMAND_LEASE_TICKS,
        .action_tap = true,
    };
    event.type = AFORC_INPUT_EVENT_FOCUS_OUT;
    status = surf_man_app_handle_event(app, engine, &event);
    paused_hash = surf_man_simulation_hash(&app->simulation);
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &app->scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (app->focused || app->overlay != SURF_MAN_OVERLAY_PAUSE ||
         surf_man_simulation_hash(&app->simulation) != paused_hash ||
         app->controls.vertical != 0 || app->controls.horizontal != 0 ||
         app->controls.action_lease != 0U || app->controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->focused = true;
        app->overlay = SURF_MAN_OVERLAY_NONE;
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_RESIZE;
        event.data.resize.size = (AFORC_Size){
            SURF_MAN_MIN_COLUMNS - 1,
            SURF_MAN_MIN_ROWS - 1,
        };
        status = surf_man_app_handle_event(app, engine, &event);
        paused_hash = surf_man_simulation_hash(&app->simulation);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &app->scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (app->overlay != SURF_MAN_OVERLAY_RESIZE ||
         surf_man_simulation_hash(&app->simulation) != paused_hash)) {
        status = AFORC_ERROR_STATE;
    }

    app->simulation = saved_simulation;
    app->controls = saved_controls;
    app->overlay = saved_overlay;
    app->terminal_size = saved_size;
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    (void)memcpy(app->message, saved_message, sizeof(saved_message));
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "focus loss or undersized resize advanced authoritative state");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_offscreen_scene_checks(SurfManApp *app,
                                                       AFORC_Engine *engine,
                                                       AFORC_Error *error) {
    AFORC_Status status;

    if (!app->smoke || app->terminal != NULL ||
        aforc_engine_scene(engine) != &app->scene ||
        app->scene.vtable != &surf_man_scene_vtable ||
        app->scene.user_data != app) {
        return surf_man_qa_smoke_error(
            error,
            AFORC_ERROR_STATE,
            "smoke mode did not use the real app scene off-screen");
    }
    status = surf_man_scene_render(&app->scene, engine, 0.0, error);
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error, status, "off-screen real-scene render failed");
    }
    return AFORC_OK;
}

AFORC_Status surf_man_smoke_checks(SurfManApp *app,
                                   AFORC_Engine *engine,
                                   AFORC_Error *error) {
    AFORC_Status status;

    if (app == NULL || engine == NULL || !app->initialized) {
        return surf_man_qa_smoke_error(
            error, AFORC_ERROR_INVALID_ARGUMENT, "invalid smoke QA context");
    }
    status = surf_man_simulation_checks(app->seed, error);
    if (status == AFORC_OK) {
        status = surf_man_qa_command_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_pause_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_visual_timing_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_action_transition_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_color_particle_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_offscreen_scene_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_checks(app, error);
    }
    return status;
}

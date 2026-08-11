#include "fieldzero/presentation.h"

#include <string.h>

enum
{
    FIELDZERO_VISUAL_CANVAS = 0,
    FIELDZERO_VISUAL_INK,
    FIELDZERO_VISUAL_FRAME,
    FIELDZERO_VISUAL_SIGNAL,
    FIELDZERO_MILLISECONDS_PER_SECOND = 1000,
    FIELDZERO_AMBIENT_INTERVAL = 15,
    FIELDZERO_RAIN_INTERVAL = 6,
    FIELDZERO_DASH_INTERVAL = 3,
    FIELDZERO_PHASE_PARTICLE_COUNT = 8,
    FIELDZERO_REDUCED_PHASE_PARTICLE_COUNT = 2,
    FIELDZERO_RAIN_MIN_SPEED = 14,
    FIELDZERO_RAIN_MAX_SPEED = 20,
    FIELDZERO_RAIN_MIN_ACCELERATION = 2,
    FIELDZERO_RAIN_MAX_ACCELERATION = 3,
    FIELDZERO_RAIN_MIN_LIFETIME = 700,
    FIELDZERO_RAIN_MAX_LIFETIME = 1250,
    FIELDZERO_PARTICLE_SEED = 0x465a5041U
};

static const uint64_t fieldzero_decorative_stream =
    UINT64_C(0x465a50524553454e);

AFORC_Cell fieldzero_visual_cell(uint32_t codepoint,
                                 uint8_t role,
                                 AFORC_CellStyle style,
                                 bool no_color);

static int32_t fieldzero_effect_fixed_to_cell(int32_t value)
{
    const int64_t wide = value;
    const int64_t half = FIELDZERO_FIXED_ONE / 2;

    if (wide >= 0)
    {
        return (int32_t)((wide + half) / FIELDZERO_FIXED_ONE);
    }
    return (int32_t)-((-wide + half) / FIELDZERO_FIXED_ONE);
}

static AFORC_Point fieldzero_effect_player_cell(const FieldzeroGame *game)
{
    return (AFORC_Point){fieldzero_effect_fixed_to_cell(game->player.x),
                         fieldzero_effect_fixed_to_cell(game->player.y)};
}

static bool fieldzero_effect_is_feature(const FieldzeroGame *game,
                                        AFORC_Point point)
{
    if (aforc_world_point_equal(point, game->room->exit) ||
        (game->room->has_memory &&
         aforc_world_point_equal(point, game->room->memory)))
    {
        return true;
    }
    for (size_t index = 0U; index < game->room->mark_count; ++index)
    {
        if (aforc_world_point_equal(point, game->room->marks[index]))
        {
            return true;
        }
    }
    return false;
}

static bool fieldzero_effect_spawnable(const FieldzeroGame *game,
                                       AFORC_Point point)
{
    return point.x >= 0 && point.x < FIELDZERO_ARENA_WIDTH && point.y >= 0 &&
           point.y < FIELDZERO_ARENA_HEIGHT &&
           !fieldzero_effect_is_feature(game, point) &&
           !fieldzero_game_cell_blocked(game, point.x, point.y);
}

static AFORC_Status fieldzero_effect_emit(FieldzeroPresentation *presentation,
                                          AFORC_Point point,
                                          AFORC_ParticleI32Range velocity_x,
                                          AFORC_ParticleI32Range velocity_y,
                                          AFORC_ParticleI32Range acceleration_x,
                                          AFORC_ParticleI32Range acceleration_y,
                                          AFORC_ParticleU32Range lifetime,
                                          const AFORC_Cell *cells,
                                          size_t cell_count,
                                          size_t count)
{
    const int32_t x = point.x * AFORC_EFFECT_FIXED_ONE;
    const int32_t y = point.y * AFORC_EFFECT_FIXED_ONE;
    const AFORC_ParticleEmitter emitter = {
        .x = {x, x},
        .y = {y, y},
        .velocity_x = velocity_x,
        .velocity_y = velocity_y,
        .acceleration_x = acceleration_x,
        .acceleration_y = acceleration_y,
        .lifetime_ms = lifetime,
        .cells = cells,
        .cell_count = cell_count,
    };
    size_t spawned = 0U;
    const AFORC_Status status = aforc_particle_pool_emit(
        &presentation->particle_pool, &emitter, count, &spawned);

    return status == AFORC_ERROR_LIMIT ? AFORC_OK : status;
}

static AFORC_Status
fieldzero_effect_emit_ambient(FieldzeroPresentation *presentation,
                              const FieldzeroGame *game)
{
    const bool rain = game->room->sector == FIELDZERO_SECTOR_WELL;
    AFORC_Cell cells[3];
    const size_t cell_count = rain ? 3U : 2U;
    AFORC_Point point;
    uint32_t coordinate;
    AFORC_Status status = aforc_rng_bounded_u32(
        &presentation->decorative_rng, FIELDZERO_ARENA_WIDTH, &coordinate);

    if (status != AFORC_OK)
    {
        return status;
    }
    point.x = (int32_t)coordinate;
    if (rain)
    {
        point.y = 0;
    }
    else
    {
        status = aforc_rng_bounded_u32(
            &presentation->decorative_rng, FIELDZERO_ARENA_HEIGHT, &coordinate);
        if (status != AFORC_OK)
        {
            return status;
        }
        point.y = (int32_t)coordinate;
    }
    if (!fieldzero_effect_spawnable(game, point))
    {
        return AFORC_OK;
    }
    cells[0] = fieldzero_visual_cell(rain ? (uint32_t)'|' : (uint32_t)'.',
                                     rain ? FIELDZERO_VISUAL_INK
                                          : FIELDZERO_VISUAL_FRAME,
                                     AFORC_STYLE_DIM,
                                     presentation->no_color);
    cells[1] = fieldzero_visual_cell(rain ? (uint32_t)':' : (uint32_t)'\'',
                                     FIELDZERO_VISUAL_FRAME,
                                     rain ? AFORC_STYLE_NONE : AFORC_STYLE_DIM,
                                     presentation->no_color);
    cells[2] = fieldzero_visual_cell((uint32_t)'\'',
                                     FIELDZERO_VISUAL_FRAME,
                                     AFORC_STYLE_DIM,
                                     presentation->no_color);
    return fieldzero_effect_emit(
        presentation,
        point,
        (AFORC_ParticleI32Range){-AFORC_EFFECT_FIXED_ONE,
                                 rain ? 0 : AFORC_EFFECT_FIXED_ONE},
        (AFORC_ParticleI32Range){
            rain ? FIELDZERO_RAIN_MIN_SPEED * AFORC_EFFECT_FIXED_ONE
                 : -AFORC_EFFECT_FIXED_ONE,
            rain ? FIELDZERO_RAIN_MAX_SPEED * AFORC_EFFECT_FIXED_ONE
                 : AFORC_EFFECT_FIXED_ONE},
        (AFORC_ParticleI32Range){0, 0},
        (AFORC_ParticleI32Range){
            rain ? FIELDZERO_RAIN_MIN_ACCELERATION * AFORC_EFFECT_FIXED_ONE : 0,
            rain ? FIELDZERO_RAIN_MAX_ACCELERATION * AFORC_EFFECT_FIXED_ONE
                 : 0},
        (AFORC_ParticleU32Range){rain ? FIELDZERO_RAIN_MIN_LIFETIME : 700U,
                                 rain ? FIELDZERO_RAIN_MAX_LIFETIME : 1200U},
        cells,
        cell_count,
        1U);
}

static AFORC_Status
fieldzero_effect_emit_dash(FieldzeroPresentation *presentation,
                           const FieldzeroGame *game)
{
    AFORC_Cell cells[2];
    const AFORC_Point point = fieldzero_effect_player_cell(game);
    const int32_t direction = game->player.facing < 0 ? -1 : 1;
    const int32_t velocity_center = -direction * 4 * AFORC_EFFECT_FIXED_ONE;

    cells[0] = fieldzero_visual_cell((uint32_t)'-',
                                     FIELDZERO_VISUAL_FRAME,
                                     AFORC_STYLE_DIM,
                                     presentation->no_color);
    cells[1] = fieldzero_visual_cell((uint32_t)'.',
                                     FIELDZERO_VISUAL_FRAME,
                                     AFORC_STYLE_DIM,
                                     presentation->no_color);
    return fieldzero_effect_emit(
        presentation,
        point,
        (AFORC_ParticleI32Range){velocity_center - AFORC_EFFECT_FIXED_ONE,
                                 velocity_center + AFORC_EFFECT_FIXED_ONE},
        (AFORC_ParticleI32Range){0, 0},
        (AFORC_ParticleI32Range){0, 0},
        (AFORC_ParticleI32Range){0, 0},
        (AFORC_ParticleU32Range){100U, 180U},
        cells,
        sizeof(cells) / sizeof(cells[0]),
        1U);
}

static AFORC_Status
fieldzero_effect_emit_phase(FieldzeroPresentation *presentation,
                            const FieldzeroGame *game)
{
    AFORC_Cell cells[3];
    AFORC_Point point = fieldzero_effect_player_cell(game);
    size_t count = presentation->reduced_motion
                       ? FIELDZERO_REDUCED_PHASE_PARTICLE_COUNT
                       : FIELDZERO_PHASE_PARTICLE_COUNT;

    if (game->phase == FIELDZERO_PHASE_REGISTERING &&
        game->room_state < game->room->mark_count)
    {
        point = game->room->marks[game->room_state];
    }
    cells[0] = fieldzero_visual_cell((uint32_t)'*',
                                     FIELDZERO_VISUAL_INK,
                                     AFORC_STYLE_BOLD,
                                     presentation->no_color);
    cells[1] = fieldzero_visual_cell((uint32_t)'+',
                                     FIELDZERO_VISUAL_INK,
                                     AFORC_STYLE_DIM,
                                     presentation->no_color);
    cells[2] = fieldzero_visual_cell((uint32_t)'.',
                                     FIELDZERO_VISUAL_FRAME,
                                     AFORC_STYLE_DIM,
                                     presentation->no_color);
    return fieldzero_effect_emit(
        presentation,
        point,
        (AFORC_ParticleI32Range){-3 * AFORC_EFFECT_FIXED_ONE,
                                 3 * AFORC_EFFECT_FIXED_ONE},
        (AFORC_ParticleI32Range){-3 * AFORC_EFFECT_FIXED_ONE,
                                 2 * AFORC_EFFECT_FIXED_ONE},
        (AFORC_ParticleI32Range){0, 0},
        (AFORC_ParticleI32Range){AFORC_EFFECT_FIXED_ONE,
                                 AFORC_EFFECT_FIXED_ONE},
        (AFORC_ParticleU32Range){180U, 420U},
        cells,
        sizeof(cells) / sizeof(cells[0]),
        count);
}

static void fieldzero_effect_update_impulse(FieldzeroPresentation *presentation,
                                            const FieldzeroGame *game)
{
    presentation->camera_impulse_x = 0;
    presentation->camera_impulse_y = 0;
    if (presentation->reduced_motion || game->phase_ticks != 1U)
    {
        return;
    }
    if (game->phase == FIELDZERO_PHASE_REGISTERING)
    {
        presentation->camera_impulse_x = game->player.facing < 0 ? -1 : 1;
    }
    else if (game->phase == FIELDZERO_PHASE_DISSOLVING)
    {
        presentation->camera_impulse_y = 1;
    }
    else if (game->phase == FIELDZERO_PHASE_ROOM_TRANSITION ||
             game->phase == FIELDZERO_PHASE_SECTOR_TRANSITION)
    {
        presentation->camera_impulse_x =
            game->transition_direction < 0 ? -1 : 1;
    }
}

AFORC_Status fieldzero_presentation_init(FieldzeroPresentation *presentation,
                                         const FieldzeroOptions *options)
{
    AFORC_Status status;

    if (presentation == NULL || options == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(presentation, 0, sizeof(*presentation));
    presentation->reduced_motion = options->reduced_motion;
    presentation->no_color = options->no_color;
    status = aforc_rng_seed(&presentation->decorative_rng,
                            options->seed,
                            fieldzero_decorative_stream);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_particle_pool_init(&presentation->particle_pool,
                                      presentation->particles,
                                      FIELDZERO_PARTICLE_CAPACITY,
                                      (uint32_t)(options->seed ^
                                                 (options->seed >> 32U) ^
                                                 FIELDZERO_PARTICLE_SEED));
    if (status != AFORC_OK)
    {
        (void)memset(presentation, 0, sizeof(*presentation));
    }
    return status;
}

void fieldzero_presentation_dispose(FieldzeroPresentation *presentation)
{
    if (presentation == NULL)
    {
        return;
    }
    aforc_particle_pool_dispose(&presentation->particle_pool);
    (void)memset(presentation, 0, sizeof(*presentation));
}

AFORC_Status fieldzero_presentation_update(FieldzeroPresentation *presentation,
                                           const FieldzeroGame *game,
                                           uint64_t fixed_tick)
{
    uint32_t interval;
    uint32_t milliseconds;
    AFORC_Status status;

    if (presentation == NULL || game == NULL || game->room == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    interval = game->room->sector == FIELDZERO_SECTOR_WELL
                   ? FIELDZERO_RAIN_INTERVAL
                   : FIELDZERO_AMBIENT_INTERVAL;
    presentation->frame_index = fixed_tick;
    presentation->particle_millisecond_remainder +=
        FIELDZERO_MILLISECONDS_PER_SECOND;
    milliseconds = presentation->particle_millisecond_remainder /
                   FIELDZERO_FIXED_UPDATES_PER_SECOND;
    presentation->particle_millisecond_remainder %=
        FIELDZERO_FIXED_UPDATES_PER_SECOND;
    status =
        aforc_particle_pool_update(&presentation->particle_pool, milliseconds);
    if (status != AFORC_OK)
    {
        return status;
    }
    fieldzero_effect_update_impulse(presentation, game);
    if (!presentation->reduced_motion && fixed_tick % interval == 0U)
    {
        status = fieldzero_effect_emit_ambient(presentation, game);
    }
    if (status == AFORC_OK && game->player.dash_ticks != 0U &&
        fixed_tick % FIELDZERO_DASH_INTERVAL == 0U)
    {
        status = fieldzero_effect_emit_dash(presentation, game);
    }
    if (status == AFORC_OK && game->phase_ticks == 1U &&
        (game->phase == FIELDZERO_PHASE_REGISTERING ||
         game->phase == FIELDZERO_PHASE_DISSOLVING ||
         game->phase == FIELDZERO_PHASE_ROOM_TRANSITION ||
         game->phase == FIELDZERO_PHASE_SECTOR_TRANSITION))
    {
        status = fieldzero_effect_emit_phase(presentation, game);
    }
    return status;
}

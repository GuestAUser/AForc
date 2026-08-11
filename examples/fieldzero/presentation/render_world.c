#include "fieldzero/presentation.h"

#include <stdint.h>

enum
{
    FIELDZERO_VISUAL_CANVAS = 0,
    FIELDZERO_VISUAL_INK,
    FIELDZERO_VISUAL_FRAME,
    FIELDZERO_VISUAL_SIGNAL,
    FIELDZERO_BAND_QUANTUM = 4,
    FIELDZERO_PLAYER_HALF = FIELDZERO_FIXED_ONE / 2,
    FIELDZERO_BACKGROUND_TICK_INTERVAL = 30,
    FIELDZERO_BACKGROUND_ACCENT_PERIOD = 5,
    FIELDZERO_BACKGROUND_ORIGIN_DENSITY = 47,
    FIELDZERO_BACKGROUND_SPAN_DENSITY = 43,
    FIELDZERO_BACKGROUND_WELL_DENSITY = 61,
    FIELDZERO_BACKGROUND_SHEAR_DENSITY = 41,
    FIELDZERO_BACKGROUND_HORIZON_DENSITY = 53,
    FIELDZERO_RAIN_FAR_LANE_PERIOD = 7,
    FIELDZERO_RAIN_MIDDLE_LANE_PERIOD = 13,
    FIELDZERO_RAIN_FAR_TICK_INTERVAL = 8,
    FIELDZERO_RAIN_MIDDLE_TICK_INTERVAL = 3,
    FIELDZERO_RAIN_REST_STEPS = 3
};

static const uint64_t fieldzero_background_stream =
    UINT64_C(0x6669656c64626163);
static const uint64_t fieldzero_rain_far_stream = UINT64_C(0x7261696e66617230);
static const uint64_t fieldzero_rain_middle_stream =
    UINT64_C(0x7261696e6d696430);

AFORC_Cell fieldzero_visual_cell(uint32_t codepoint,
                                 uint8_t role,
                                 AFORC_CellStyle style,
                                 bool no_color);
AFORC_Status
fieldzero_visual_plot(void *context, AFORC_Point position, AFORC_Cell cell);
AFORC_Status fieldzero_render_world(AFORC_Renderer *renderer,
                                    const FieldzeroGame *game,
                                    const FieldzeroPresentation *presentation,
                                    AFORC_Rect arena);

static uint64_t fieldzero_mix64(uint64_t value)
{
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

static uint64_t fieldzero_decorative_hash(const FieldzeroGame *game,
                                          int32_t x,
                                          int32_t y,
                                          uint64_t layer)
{
    uint64_t value =
        game->seed ^ (uint64_t)game->room_index * UINT64_C(0x9e3779b97f4a7c15);

    value ^= (uint64_t)(uint32_t)x * UINT64_C(0xd6e8feb86659fd93);
    value ^= (uint64_t)(uint32_t)y * UINT64_C(0xa5a3564e27f8862f);
    return fieldzero_mix64(value ^ layer);
}

static AFORC_Status fieldzero_put_local(AFORC_Renderer *renderer,
                                        AFORC_Rect arena,
                                        AFORC_Point point,
                                        AFORC_Point impulse,
                                        AFORC_Cell cell)
{
    const AFORC_Point position = {arena.x + point.x + impulse.x,
                                  arena.y + point.y + impulse.y};

    if (!aforc_rect_contains(arena, position))
    {
        return AFORC_OK;
    }
    return aforc_renderer_put(renderer, position, cell);
}

static AFORC_Point
fieldzero_camera_impulse(const FieldzeroPresentation *presentation)
{
    return (AFORC_Point){presentation->camera_impulse_x,
                         presentation->camera_impulse_y};
}

static AFORC_Status
fieldzero_render_background(AFORC_Renderer *renderer,
                            const FieldzeroGame *game,
                            const FieldzeroPresentation *presentation,
                            AFORC_Rect arena)
{
    const uint64_t motion =
        presentation->reduced_motion
            ? 0U
            : presentation->frame_index / FIELDZERO_BACKGROUND_TICK_INTERVAL;
    uint32_t density = FIELDZERO_BACKGROUND_ORIGIN_DENSITY;
    uint32_t accent = (uint32_t)':';
    int32_t drift_x = (int32_t)(motion % 4U);
    int32_t drift_y = 0;

    switch (game->room->sector)
    {
        case FIELDZERO_SECTOR_SPAN:
            density = FIELDZERO_BACKGROUND_SPAN_DENSITY;
            accent = (uint32_t)'-';
            drift_x = (int32_t)(motion % (uint64_t)arena.width);
            break;
        case FIELDZERO_SECTOR_WELL:
            density = FIELDZERO_BACKGROUND_WELL_DENSITY;
            accent = (uint32_t)'\'';
            drift_x = 0;
            drift_y = (int32_t)(motion % (uint64_t)arena.height);
            break;
        case FIELDZERO_SECTOR_SHEAR:
            density = FIELDZERO_BACKGROUND_SHEAR_DENSITY;
            accent = (uint32_t)'_';
            drift_x = (int32_t)(motion % (uint64_t)arena.width);
            drift_y = (int32_t)((motion / 2U) % (uint64_t)arena.height);
            break;
        case FIELDZERO_SECTOR_HORIZON:
            density = FIELDZERO_BACKGROUND_HORIZON_DENSITY;
            drift_x = (int32_t)((motion / 2U) % 4U);
            break;
        case FIELDZERO_SECTOR_ORIGIN:
        default:
            break;
    }

    for (int32_t y = 0; y < arena.height; ++y)
    {
        for (int32_t x = 0; x < arena.width; ++x)
        {
            const int32_t sample_x = (x + drift_x) % arena.width;
            const int32_t sample_y = (y + drift_y) % arena.height;
            const uint64_t hash = fieldzero_decorative_hash(
                game,
                sample_x,
                sample_y,
                fieldzero_background_stream + (uint64_t)game->room->sector);

            if (hash % density == 0U)
            {
                const uint32_t glyph =
                    hash % FIELDZERO_BACKGROUND_ACCENT_PERIOD == 0U
                        ? accent
                        : (uint32_t)'.';
                const AFORC_Status status = fieldzero_put_local(
                    renderer,
                    arena,
                    (AFORC_Point){x, y},
                    (AFORC_Point){0, 0},
                    fieldzero_visual_cell(glyph,
                                          FIELDZERO_VISUAL_FRAME,
                                          AFORC_STYLE_DIM,
                                          presentation->no_color));

                if (status != AFORC_OK)
                {
                    return status;
                }
            }
        }
    }
    return AFORC_OK;
}

static bool fieldzero_midground_cell(const FieldzeroGame *game,
                                     int32_t x,
                                     int32_t y,
                                     uint64_t phase,
                                     uint32_t *out_glyph)
{
    const int32_t offset =
        (int32_t)(fieldzero_decorative_hash(game, 0, 0, UINT64_C(0x6d6964)) %
                  8U);

    switch (game->room->sector)
    {
        case FIELDZERO_SECTOR_ORIGIN:
            if ((x == 12 + offset && y % 2 == 0) ||
                (y == 5 + offset / 2 && x % 4 == 0))
            {
                *out_glyph = x == 12 + offset ? (uint32_t)':' : (uint32_t)'-';
                return true;
            }
            break;
        case FIELDZERO_SECTOR_SPAN:
            if ((y == 4 || y == 13) && (x + offset + (int32_t)phase) % 6 < 3)
            {
                *out_glyph = (uint32_t)'-';
                return true;
            }
            break;
        case FIELDZERO_SECTOR_WELL:
            if ((x == 10 + offset || x == 44 + offset) && y % 3 != 1)
            {
                *out_glyph = (uint32_t)'|';
                return true;
            }
            break;
        case FIELDZERO_SECTOR_SHEAR:
            if ((x + offset) % 16 < 5 && (y + x / 16) % 6 == 2)
            {
                *out_glyph = (uint32_t)'_';
                return true;
            }
            break;
        case FIELDZERO_SECTOR_HORIZON:
            if (((x + offset) % 12 == 0 && y % 2 == 0) ||
                ((y == 4 || y == 12) && x % 3 == 0))
            {
                *out_glyph =
                    (x + offset) % 12 == 0 ? (uint32_t)':' : (uint32_t)'-';
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

static AFORC_Status
fieldzero_render_midground(AFORC_Renderer *renderer,
                           const FieldzeroGame *game,
                           const FieldzeroPresentation *presentation,
                           AFORC_Rect arena)
{
    const uint64_t phase = presentation->reduced_motion
                               ? 0U
                               : (presentation->frame_index / 45U) % 6U;

    for (int32_t y = 0; y < arena.height; ++y)
    {
        for (int32_t x = 0; x < arena.width; ++x)
        {
            uint32_t glyph = (uint32_t)'.';

            if (fieldzero_midground_cell(game, x, y, phase, &glyph))
            {
                const AFORC_Status status = fieldzero_put_local(
                    renderer,
                    arena,
                    (AFORC_Point){x, y},
                    (AFORC_Point){0, 0},
                    fieldzero_visual_cell(glyph,
                                          FIELDZERO_VISUAL_FRAME,
                                          AFORC_STYLE_DIM,
                                          presentation->no_color));

                if (status != AFORC_OK)
                {
                    return status;
                }
            }
        }
    }
    return AFORC_OK;
}

static int32_t
fieldzero_quantized_axis(int32_t from, int32_t to, uint8_t phase_ticks)
{
    const uint32_t ticks = phase_ticks > FIELDZERO_REGISTRATION_TICKS
                               ? FIELDZERO_REGISTRATION_TICKS
                               : phase_ticks;
    const int64_t delta = (int64_t)to - from;
    int64_t travelled = delta * ticks / FIELDZERO_REGISTRATION_TICKS;

    if (travelled >= 0)
    {
        travelled = travelled / FIELDZERO_BAND_QUANTUM * FIELDZERO_BAND_QUANTUM;
    }
    else
    {
        travelled =
            -((-travelled / FIELDZERO_BAND_QUANTUM) * FIELDZERO_BAND_QUANTUM);
    }
    return (int32_t)((int64_t)from + travelled);
}

static AFORC_Point fieldzero_band_offset(const FieldzeroGame *game,
                                         const FieldzeroBand *band)
{
    uint8_t state = game->room_state;

    if (state >= game->room->state_count)
    {
        state = (uint8_t)(game->room->state_count - 1U);
    }
    AFORC_Point offset = band->offsets[state];

    if (game->phase == FIELDZERO_PHASE_REGISTERING &&
        game->registration_target_state < game->room->state_count)
    {
        const AFORC_Point target =
            band->offsets[game->registration_target_state];

        offset.x =
            fieldzero_quantized_axis(offset.x, target.x, game->phase_ticks);
        offset.y =
            fieldzero_quantized_axis(offset.y, target.y, game->phase_ticks);
    }
    return offset;
}

static bool fieldzero_world_cell_solid(const FieldzeroGame *game,
                                       AFORC_Point point)
{
    for (size_t index = 0U; index < game->room->static_rectangle_count; ++index)
    {
        if (aforc_rect_contains(game->room->static_rectangles[index], point))
        {
            return true;
        }
    }
    for (size_t band_index = 0U; band_index < game->room->band_count;
         ++band_index)
    {
        const FieldzeroBand *band = &game->room->bands[band_index];
        const AFORC_Point offset = fieldzero_band_offset(game, band);

        for (size_t rectangle_index = 0U;
             rectangle_index < band->rectangle_count;
             ++rectangle_index)
        {
            AFORC_Rect rectangle;

            if (aforc_world_rect_translate(band->rectangles[rectangle_index],
                                           offset,
                                           &rectangle) != AFORC_OK ||
                aforc_rect_contains(rectangle, point))
            {
                return true;
            }
        }
    }
    return false;
}

static int32_t fieldzero_rain_surface_y(const FieldzeroGame *game, int32_t x)
{
    for (int32_t y = 0; y < FIELDZERO_ARENA_HEIGHT; ++y)
    {
        if (fieldzero_world_cell_solid(game, (AFORC_Point){x, y}))
        {
            return y;
        }
    }
    return FIELDZERO_ARENA_HEIGHT;
}

static AFORC_Status
fieldzero_render_rain_layer(AFORC_Renderer *renderer,
                            const FieldzeroGame *game,
                            const FieldzeroPresentation *presentation,
                            AFORC_Rect arena,
                            AFORC_Point impulse,
                            bool middle_depth)
{
    const uint32_t lane_period = middle_depth
                                     ? FIELDZERO_RAIN_MIDDLE_LANE_PERIOD
                                     : FIELDZERO_RAIN_FAR_LANE_PERIOD;
    const uint32_t tick_interval = middle_depth
                                       ? FIELDZERO_RAIN_MIDDLE_TICK_INTERVAL
                                       : FIELDZERO_RAIN_FAR_TICK_INTERVAL;
    const uint64_t stream =
        middle_depth ? fieldzero_rain_middle_stream : fieldzero_rain_far_stream;
    const uint64_t step = presentation->reduced_motion
                              ? 0U
                              : presentation->frame_index / tick_interval;
    const AFORC_Point layer_impulse =
        middle_depth ? impulse : (AFORC_Point){0, 0};
    const AFORC_Cell drop =
        fieldzero_visual_cell(middle_depth ? (uint32_t)':' : (uint32_t)'.',
                              FIELDZERO_VISUAL_FRAME,
                              middle_depth ? AFORC_STYLE_NONE : AFORC_STYLE_DIM,
                              presentation->no_color);

    if (game->room->sector != FIELDZERO_SECTOR_WELL)
    {
        return AFORC_OK;
    }
    for (int32_t x = 0; x < arena.width; ++x)
    {
        const uint64_t hash = fieldzero_decorative_hash(game, x, 0, stream);
        int32_t surface_y;
        uint64_t phase;
        AFORC_Status status;

        if (hash % lane_period != 0U)
        {
            continue;
        }
        surface_y = fieldzero_rain_surface_y(game, x);
        phase = ((hash >> 8U) + step) %
                ((uint64_t)surface_y + FIELDZERO_RAIN_REST_STEPS);
        if (phase < (uint64_t)surface_y)
        {
            const AFORC_Point point = {x, (int32_t)phase};

            if (middle_depth && point.y > 0)
            {
                status = fieldzero_put_local(
                    renderer,
                    arena,
                    (AFORC_Point){point.x, point.y - 1},
                    layer_impulse,
                    fieldzero_visual_cell((uint32_t)'\'',
                                          FIELDZERO_VISUAL_FRAME,
                                          AFORC_STYLE_DIM,
                                          presentation->no_color));
                if (status != AFORC_OK)
                {
                    return status;
                }
            }
            status = fieldzero_put_local(
                renderer, arena, point, layer_impulse, drop);
        }
        else if (middle_depth && !presentation->reduced_motion &&
                 phase == (uint64_t)surface_y && surface_y > 0 &&
                 surface_y < arena.height)
        {
            status = fieldzero_put_local(
                renderer,
                arena,
                (AFORC_Point){x, surface_y - 1},
                layer_impulse,
                fieldzero_visual_cell((uint32_t)'_',
                                      FIELDZERO_VISUAL_FRAME,
                                      AFORC_STYLE_DIM,
                                      presentation->no_color));
        }
        else
        {
            status = AFORC_OK;
        }
        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}

static AFORC_Status
fieldzero_draw_rect(AFORC_Renderer *renderer,
                    const FieldzeroPresentation *presentation,
                    AFORC_Rect arena,
                    AFORC_Rect rect,
                    AFORC_Point offset,
                    AFORC_Point impulse,
                    uint32_t edge_glyph,
                    uint32_t detail_glyph,
                    bool band)
{
    for (int32_t y = 0; y < rect.height; ++y)
    {
        for (int32_t x = 0; x < rect.width; ++x)
        {
            const bool edge =
                y == 0 || y + 1 == rect.height || x == 0 || x + 1 == rect.width;
            const uint32_t glyph = edge || band ? edge_glyph : detail_glyph;
            const AFORC_CellStyle style =
                edge ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE;
            const AFORC_Status status = fieldzero_put_local(
                renderer,
                arena,
                (AFORC_Point){rect.x + offset.x + x, rect.y + offset.y + y},
                impulse,
                fieldzero_visual_cell(glyph,
                                      FIELDZERO_VISUAL_INK,
                                      style,
                                      presentation->no_color));

            if (status != AFORC_OK)
            {
                return status;
            }
        }
    }
    return AFORC_OK;
}

static AFORC_Status
fieldzero_render_registration_sweep(AFORC_Renderer *renderer,
                                    const FieldzeroGame *game,
                                    const FieldzeroPresentation *presentation,
                                    AFORC_Rect arena)
{
    if (game->phase != FIELDZERO_PHASE_REGISTERING)
    {
        return AFORC_OK;
    }
    const uint32_t ticks = game->phase_ticks > FIELDZERO_REGISTRATION_TICKS
                               ? FIELDZERO_REGISTRATION_TICKS
                               : game->phase_ticks;
    const int32_t sweep_x =
        (int32_t)(((uint32_t)(arena.width - 1) * ticks /
                   FIELDZERO_REGISTRATION_TICKS) /
                  FIELDZERO_BAND_QUANTUM * FIELDZERO_BAND_QUANTUM);
    const int32_t sweep_y =
        (int32_t)(((uint32_t)(arena.height - 1) * ticks /
                   FIELDZERO_REGISTRATION_TICKS) /
                  FIELDZERO_BAND_QUANTUM * FIELDZERO_BAND_QUANTUM);

    for (int32_t y = 0; y < arena.height; ++y)
    {
        AFORC_Status status =
            fieldzero_put_local(renderer,
                                arena,
                                (AFORC_Point){sweep_x, y},
                                (AFORC_Point){0, 0},
                                fieldzero_visual_cell((uint32_t)'|',
                                                      FIELDZERO_VISUAL_FRAME,
                                                      AFORC_STYLE_DIM,
                                                      presentation->no_color));

        if (status != AFORC_OK)
        {
            return status;
        }
    }
    for (int32_t x = 0; x < arena.width; ++x)
    {
        const AFORC_Status status = fieldzero_put_local(
            renderer,
            arena,
            (AFORC_Point){x, sweep_y},
            (AFORC_Point){0, 0},
            fieldzero_visual_cell(
                x == sweep_x ? (uint32_t)'+' : (uint32_t)'-',
                x == sweep_x ? FIELDZERO_VISUAL_INK : FIELDZERO_VISUAL_FRAME,
                x == sweep_x ? AFORC_STYLE_BOLD : AFORC_STYLE_DIM,
                presentation->no_color));

        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}

static AFORC_Status
fieldzero_render_terrain(AFORC_Renderer *renderer,
                         const FieldzeroGame *game,
                         const FieldzeroPresentation *presentation,
                         AFORC_Rect arena,
                         AFORC_Point impulse)
{
    for (size_t index = 0U; index < game->room->static_rectangle_count; ++index)
    {
        const AFORC_Status status = fieldzero_draw_rect(
            renderer,
            presentation,
            arena,
            game->room->static_rectangles[index],
            (AFORC_Point){0, 0},
            impulse,
            (uint32_t)(unsigned char)game->room->terrain_glyph,
            (uint32_t)(unsigned char)game->room->detail_glyph,
            false);

        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}

static AFORC_Status
fieldzero_render_bands(AFORC_Renderer *renderer,
                       const FieldzeroGame *game,
                       const FieldzeroPresentation *presentation,
                       AFORC_Rect arena,
                       AFORC_Point impulse)
{
    for (size_t band_index = 0U; band_index < game->room->band_count;
         ++band_index)
    {
        const FieldzeroBand *band = &game->room->bands[band_index];
        const AFORC_Point offset = fieldzero_band_offset(game, band);

        for (size_t rectangle_index = 0U;
             rectangle_index < band->rectangle_count;
             ++rectangle_index)
        {
            const AFORC_Status status =
                fieldzero_draw_rect(renderer,
                                    presentation,
                                    arena,
                                    band->rectangles[rectangle_index],
                                    offset,
                                    impulse,
                                    (uint32_t)(unsigned char)band->glyph,
                                    (uint32_t)(unsigned char)band->glyph,
                                    true);

            if (status != AFORC_OK)
            {
                return status;
            }
        }
    }
    return AFORC_OK;
}

static AFORC_Status
fieldzero_render_marks(AFORC_Renderer *renderer,
                       const FieldzeroGame *game,
                       const FieldzeroPresentation *presentation,
                       AFORC_Rect arena,
                       AFORC_Point impulse)
{
    for (size_t index = 0U; index < game->room->mark_count; ++index)
    {
        const bool inert =
            index < game->room_state ||
            (game->phase == FIELDZERO_PHASE_REGISTERING &&
             index == game->room_state &&
             game->phase_ticks >= FIELDZERO_REGISTRATION_TICKS / 2);
        const bool current = index == game->room_state && !inert;
        const AFORC_Status status = fieldzero_put_local(
            renderer,
            arena,
            game->room->marks[index],
            impulse,
            fieldzero_visual_cell(inert ? (uint32_t)'x' : (uint32_t)'+',
                                  current ? FIELDZERO_VISUAL_INK
                                          : FIELDZERO_VISUAL_FRAME,
                                  current ? AFORC_STYLE_BOLD : AFORC_STYLE_DIM,
                                  presentation->no_color));

        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}

static AFORC_Status
fieldzero_render_memory(AFORC_Renderer *renderer,
                        const FieldzeroGame *game,
                        const FieldzeroPresentation *presentation,
                        AFORC_Rect arena,
                        AFORC_Point impulse)
{
    if (!game->room->has_memory)
    {
        return AFORC_OK;
    }
    return fieldzero_put_local(
        renderer,
        arena,
        game->room->memory,
        impulse,
        fieldzero_visual_cell(
            game->memory_collected_here ? (uint32_t)'.' : (uint32_t)'o',
            game->memory_collected_here ? FIELDZERO_VISUAL_FRAME
                                        : FIELDZERO_VISUAL_INK,
            game->memory_collected_here ? AFORC_STYLE_DIM : AFORC_STYLE_BOLD,
            presentation->no_color));
}

static AFORC_Status
fieldzero_render_exit(AFORC_Renderer *renderer,
                      const FieldzeroGame *game,
                      const FieldzeroPresentation *presentation,
                      AFORC_Rect arena,
                      AFORC_Point impulse)
{
    return fieldzero_put_local(renderer,
                               arena,
                               game->room->exit,
                               impulse,
                               fieldzero_visual_cell((uint32_t)'>',
                                                     FIELDZERO_VISUAL_INK,
                                                     AFORC_STYLE_BOLD,
                                                     presentation->no_color));
}

static int32_t fieldzero_fixed_round(int32_t value)
{
    if (value >= 0)
    {
        return (value + FIELDZERO_PLAYER_HALF) / FIELDZERO_FIXED_ONE;
    }
    return (int32_t)(-((-(int64_t)value + FIELDZERO_PLAYER_HALF) /
                       FIELDZERO_FIXED_ONE));
}

static uint32_t fieldzero_player_motion_glyph(const FieldzeroPlayer *player)
{
    if (player->dash_ticks != 0U)
    {
        return player->facing < 0 ? (uint32_t)'<' : (uint32_t)'>';
    }
    if (player->velocity_y <= -FIELDZERO_FIXED_ONE)
    {
        return (uint32_t)'^';
    }
    if (player->velocity_y >= FIELDZERO_FIXED_ONE)
    {
        return (uint32_t)'v';
    }
    if (!player->grounded && (player->wall_left || player->wall_right))
    {
        return player->wall_left ? (uint32_t)'<' : (uint32_t)'>';
    }
    if (player->velocity_x <= -FIELDZERO_FIXED_ONE)
    {
        return (uint32_t)'<';
    }
    if (player->velocity_x >= FIELDZERO_FIXED_ONE)
    {
        return (uint32_t)'>';
    }
    return (uint32_t)'@';
}

static AFORC_Point fieldzero_player_cell(const FieldzeroGame *game,
                                         AFORC_Point impulse)
{
    AFORC_Point point = {fieldzero_fixed_round(game->player.x) + impulse.x,
                         fieldzero_fixed_round(game->player.y) + impulse.y};

    if (point.x < 0)
    {
        point.x = 0;
    }
    else if (point.x >= FIELDZERO_ARENA_WIDTH)
    {
        point.x = FIELDZERO_ARENA_WIDTH - 1;
    }
    if (point.y < 0)
    {
        point.y = 0;
    }
    else if (point.y >= FIELDZERO_ARENA_HEIGHT)
    {
        point.y = FIELDZERO_ARENA_HEIGHT - 1;
    }
    return point;
}

static AFORC_Status
fieldzero_render_dash_trace(AFORC_Renderer *renderer,
                            const FieldzeroGame *game,
                            const FieldzeroPresentation *presentation,
                            AFORC_Rect arena,
                            AFORC_Point impulse)
{
    if (game->player.dash_ticks == 0U)
    {
        return AFORC_OK;
    }
    const AFORC_Point player = fieldzero_player_cell(game, impulse);
    const int32_t direction = game->player.facing < 0 ? 1 : -1;
    const int32_t trace_count = presentation->reduced_motion ? 1 : 2;

    for (int32_t index = 1; index <= trace_count; ++index)
    {
        const AFORC_Status status = fieldzero_put_local(
            renderer,
            arena,
            (AFORC_Point){player.x + direction * index, player.y},
            (AFORC_Point){0, 0},
            fieldzero_visual_cell(index == 1 ? (uint32_t)'-' : (uint32_t)'.',
                                  FIELDZERO_VISUAL_FRAME,
                                  AFORC_STYLE_DIM,
                                  presentation->no_color));

        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}

typedef struct FieldzeroParticleRenderContext
{
    AFORC_Renderer *renderer;
    const FieldzeroGame *game;
    AFORC_Rect arena;
    AFORC_Point impulse;
} FieldzeroParticleRenderContext;

static bool fieldzero_particle_over_feature(const FieldzeroGame *game,
                                            AFORC_Point point)
{
    if (aforc_world_point_equal(point, game->room->exit) ||
        (game->room->has_memory &&
         aforc_world_point_equal(point, game->room->memory)) ||
        aforc_world_point_equal(
            point, fieldzero_player_cell(game, (AFORC_Point){0, 0})))
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
    return fieldzero_world_cell_solid(game, point);
}

static AFORC_Status
fieldzero_particle_plot(void *opaque, AFORC_Point position, AFORC_Cell cell)
{
    const FieldzeroParticleRenderContext *context = opaque;
    const int64_t local_x =
        (int64_t)position.x - context->arena.x - context->impulse.x;
    const int64_t local_y =
        (int64_t)position.y - context->arena.y - context->impulse.y;
    AFORC_Point point;

    if (local_x < 0 || local_y < 0 || local_x >= context->arena.width ||
        local_y >= context->arena.height)
    {
        return AFORC_OK;
    }
    point = (AFORC_Point){(int32_t)local_x, (int32_t)local_y};
    if ((context->game->room->sector == FIELDZERO_SECTOR_WELL &&
         (cell.codepoint == (uint32_t)'|' || cell.codepoint == (uint32_t)':' ||
          cell.codepoint == (uint32_t)'\'') &&
         point.y >= fieldzero_rain_surface_y(context->game, point.x)) ||
        fieldzero_particle_over_feature(context->game, point))
    {
        return AFORC_OK;
    }
    return aforc_renderer_put(context->renderer, position, cell);
}

static AFORC_Status
fieldzero_render_particles(AFORC_Renderer *renderer,
                           const FieldzeroGame *game,
                           const FieldzeroPresentation *presentation,
                           AFORC_Rect arena,
                           AFORC_Point impulse)
{
    AFORC_ParticleDrawOptions options = aforc_particle_draw_options_default();
    FieldzeroParticleRenderContext context = {renderer, game, arena, impulse};

    options.offset = (AFORC_Point){arena.x + impulse.x, arena.y + impulse.y};
    options.clip = arena;
    options.clip_enabled = true;
    return aforc_particle_pool_draw(&presentation->particle_pool,
                                    &options,
                                    fieldzero_particle_plot,
                                    &context);
}

static AFORC_Status
fieldzero_render_player(AFORC_Renderer *renderer,
                        const FieldzeroGame *game,
                        const FieldzeroPresentation *presentation,
                        AFORC_Rect arena,
                        AFORC_Point impulse)
{
    const AFORC_Point player = fieldzero_player_cell(game, impulse);

    return fieldzero_put_local(
        renderer,
        arena,
        player,
        (AFORC_Point){0, 0},
        fieldzero_visual_cell(fieldzero_player_motion_glyph(&game->player),
                              FIELDZERO_VISUAL_SIGNAL,
                              AFORC_STYLE_BOLD | AFORC_STYLE_REVERSE,
                              presentation->no_color));
}

AFORC_Status fieldzero_render_world(AFORC_Renderer *renderer,
                                    const FieldzeroGame *game,
                                    const FieldzeroPresentation *presentation,
                                    AFORC_Rect arena)
{
    AFORC_Status status;
    AFORC_Point impulse;

    if (renderer == NULL || game == NULL || game->room == NULL ||
        presentation == NULL || arena.width != FIELDZERO_ARENA_WIDTH ||
        arena.height != FIELDZERO_ARENA_HEIGHT)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    impulse = fieldzero_camera_impulse(presentation);
    status = fieldzero_render_background(renderer, game, presentation, arena);
    if (status == AFORC_OK)
    {
        status = fieldzero_render_rain_layer(
            renderer, game, presentation, arena, impulse, false);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_render_midground(renderer, game, presentation, arena);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_rain_layer(
            renderer, game, presentation, arena, impulse, true);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_registration_sweep(
            renderer, game, presentation, arena);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_terrain(
            renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_bands(
            renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_marks(
            renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_memory(
            renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_render_exit(renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_particles(
            renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_dash_trace(
            renderer, game, presentation, arena, impulse);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_render_player(
            renderer, game, presentation, arena, impulse);
    }
    return status;
}

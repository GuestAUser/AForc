#ifndef FIELDZERO_TYPES_H
#define FIELDZERO_TYPES_H

#include "aforc/common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    FIELDZERO_ROOM_COUNT = 12,
    FIELDZERO_SECTOR_COUNT = 5,
    FIELDZERO_ARENA_WIDTH = 72,
    FIELDZERO_ARENA_HEIGHT = 18,
    FIELDZERO_MIN_WIDTH = 80,
    FIELDZERO_MIN_HEIGHT = 24,
    FIELDZERO_FIXED_UPDATES_PER_SECOND = 60,
    FIELDZERO_FIXED_ONE = 1 << 16,
    FIELDZERO_REGISTRATION_TICKS = 27,
    FIELDZERO_DISSOLVE_TICKS = 30,
    FIELDZERO_ROOM_TRANSITION_TICKS = 18,
    FIELDZERO_SECTOR_TRANSITION_TICKS = 45,
    FIELDZERO_MAX_STATIC_RECTS = 16,
    FIELDZERO_MAX_BANDS = 3,
    FIELDZERO_MAX_BAND_RECTS = 4,
    FIELDZERO_MAX_ROOM_STATES = 3,
    FIELDZERO_MAX_MARKS = 2,
    FIELDZERO_MEMORY_COUNT = 10
};

typedef enum FieldzeroSector
{
    FIELDZERO_SECTOR_ORIGIN = 0,
    FIELDZERO_SECTOR_SPAN,
    FIELDZERO_SECTOR_WELL,
    FIELDZERO_SECTOR_SHEAR,
    FIELDZERO_SECTOR_HORIZON
} FieldzeroSector;

typedef enum FieldzeroGamePhase
{
    FIELDZERO_PHASE_ACTIVE = 0,
    FIELDZERO_PHASE_REGISTERING,
    FIELDZERO_PHASE_DISSOLVING,
    FIELDZERO_PHASE_ROOM_TRANSITION,
    FIELDZERO_PHASE_SECTOR_TRANSITION,
    FIELDZERO_PHASE_COMPLETE
} FieldzeroGamePhase;

typedef enum FieldzeroScreen
{
    FIELDZERO_SCREEN_TITLE = 0,
    FIELDZERO_SCREEN_PLAY,
    FIELDZERO_SCREEN_COMPLETE
} FieldzeroScreen;

typedef struct FieldzeroBand
{
    AFORC_Rect rectangles[FIELDZERO_MAX_BAND_RECTS];
    size_t rectangle_count;
    AFORC_Point offsets[FIELDZERO_MAX_ROOM_STATES];
    char glyph;
} FieldzeroBand;

typedef struct FieldzeroRoomDefinition
{
    const char *name;
    const char *memory_text;
    FieldzeroSector sector;
    uint8_t state_count;
    AFORC_Point entry;
    AFORC_Point spawn;
    AFORC_Point exit;
    AFORC_Point marks[FIELDZERO_MAX_MARKS];
    AFORC_Point memory;
    AFORC_Rect static_rectangles[FIELDZERO_MAX_STATIC_RECTS];
    size_t static_rectangle_count;
    FieldzeroBand bands[FIELDZERO_MAX_BANDS];
    size_t band_count;
    char terrain_glyph;
    char detail_glyph;
} FieldzeroRoomDefinition;

static inline size_t
fieldzero_room_mark_count(const FieldzeroRoomDefinition *room)
{
    return (size_t)room->state_count - 1U;
}

static inline bool
fieldzero_room_has_memory(const FieldzeroRoomDefinition *room)
{
    return room->memory_text != NULL;
}

static inline int32_t fieldzero_fixed_to_cell(int32_t value)
{
    const int64_t wide = value;
    const int64_t half = FIELDZERO_FIXED_ONE / 2;

    return wide >= 0 ? (int32_t)((wide + half) / FIELDZERO_FIXED_ONE)
                     : (int32_t)-((-wide + half) / FIELDZERO_FIXED_ONE);
}

static inline unsigned int fieldzero_popcount_u16(uint16_t bits)
{
    unsigned int count = 0U;

    while (bits != 0U)
    {
        count += (unsigned int)(bits & UINT16_C(1));
        bits = (uint16_t)(bits >> 1U);
    }
    return count;
}

typedef struct FieldzeroActions
{
    bool left;
    bool right;
    bool jump_held;
    bool jump_pressed;
    bool jump_released;
    bool dash_pressed;
} FieldzeroActions;

typedef struct FieldzeroPlayer
{
    int32_t x;
    int32_t y;
    int32_t velocity_x;
    int32_t velocity_y;
    int8_t facing;
    uint8_t coyote_ticks;
    uint8_t jump_buffer_ticks;
    uint8_t wall_lock_ticks;
    uint8_t dash_ticks;
    bool grounded;
    bool wall_left;
    bool wall_right;
    bool dash_available;
} FieldzeroPlayer;

typedef struct FieldzeroCheckpoint
{
    int32_t x;
    int32_t y;
    uint8_t room_state;
} FieldzeroCheckpoint;

typedef struct FieldzeroOptions
{
    uint64_t seed;
    bool smoke;
    bool reduced_motion;
    bool no_color;
} FieldzeroOptions;

typedef struct FieldzeroViewState
{
    FieldzeroScreen screen;
    bool help_visible;
    bool paused;
    bool focus_paused;
    bool quit_confirmation;
    bool terminal_too_small;
} FieldzeroViewState;

static inline bool fieldzero_view_has_overlay(const FieldzeroViewState *view)
{
    return view->help_visible || view->paused || view->focus_paused ||
           view->quit_confirmation;
}

#endif

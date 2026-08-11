#include "fieldzero/content.h"

static const FieldzeroRoomDefinition well_rooms[] = {
    {
        .name = "RAIN SHAFT",
        .memory_text = "[WATERLINE: NOT FOUND]",
        .sector = FIELDZERO_SECTOR_WELL,
        .state_count = 2U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {67, 4},
        .marks = {{32, 10}},
        .mark_count = 1U,
        .memory = {20, 13},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 12, 1},
                {17, 14, 8, 1},
                {29, 11, 7, 1},
                {42, 8, 7, 1},
                {59, 5, 12, 1},
            },
        .static_rectangle_count = 7U,
        .bands =
            {
                {
                    .rectangles = {{36, 14, 10, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, -4}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{50, 12, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, -4}},
                    .glyph = '=',
                },
            },
        .band_count = 2U,
        .terrain_glyph = '#',
        .detail_glyph = ':',
    },
    {
        .name = "PLUMB FALL",
        .memory_text = "[DEPTH READING: UNSTABLE]",
        .sector = FIELDZERO_SECTOR_WELL,
        .state_count = 2U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {67, 13},
        .marks = {{29, 3}},
        .mark_count = 1U,
        .memory = {32, 6},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 11, 1},
                {14, 8, 2, 6},
                {10, 14, 4, 1},
                {3, 11, 6, 1},
                {9, 8, 5, 1},
                {19, 7, 5, 1},
                {24, 4, 2, 14},
                {26, 4, 8, 1},
                {29, 7, 7, 1},
                {57, 11, 7, 1},
                {61, 14, 10, 1},
            },
        .static_rectangle_count = 13U,
        .bands =
            {
                {
                    .rectangles = {{36, 3, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 4}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{48, 5, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 4}},
                    .glyph = '=',
                },
            },
        .band_count = 2U,
        .terrain_glyph = '#',
        .detail_glyph = '|',
    },
    {
        .name = "SOUNDING",
        .memory_text = "[PLUMB LINE: MOVING]",
        .sector = FIELDZERO_SECTOR_WELL,
        .state_count = 3U,
        .entry = {2, 2},
        .spawn = {4, 2},
        .exit = {68, 2},
        .marks = {{31, 14}, {50, 8}},
        .mark_count = 2U,
        .memory = {20, 11},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {1, 3, 11, 1},
                {15, 8, 8, 1},
                {17, 12, 7, 1},
                {27, 15, 10, 1},
                {47, 9, 8, 1},
                {64, 3, 7, 1},
            },
        .static_rectangle_count = 8U,
        .bands =
            {
                {
                    .rectangles = {{38, 16, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, -4}, {0, -8}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{57, 14, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {0, -8}},
                    .glyph = '=',
                },
            },
        .band_count = 2U,
        .terrain_glyph = '#',
        .detail_glyph = ';',
    },
};

const FieldzeroRoomDefinition *fieldzero_well_rooms(size_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = sizeof(well_rooms) / sizeof(well_rooms[0]);
    }
    return well_rooms;
}

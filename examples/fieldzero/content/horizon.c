#include "fieldzero/content.h"

static const FieldzeroRoomDefinition horizon_rooms[] = {
    {
        .name = "MERIDIAN ARRAY",
        .memory_text = NULL,
        .sector = FIELDZERO_SECTOR_HORIZON,
        .state_count = 3U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {68, 3},
        .marks = {{28, 6}, {47, 10}},
        .mark_count = 2U,
        .memory = {0, 0},
        .has_memory = false,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 11, 1},
                {14, 8, 2, 6},
                {10, 14, 4, 1},
                {3, 11, 6, 1},
                {9, 8, 5, 1},
                {17, 8, 6, 1},
                {23, 7, 2, 11},
                {25, 7, 7, 1},
                {43, 11, 9, 1},
                {62, 4, 9, 1},
            },
        .static_rectangle_count = 12U,
        .bands =
            {
                {
                    .rectangles = {{50, 10, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {-12, 0}, {-16, 0}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{53, 14, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {0, -4}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{62, 11, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {0, -4}},
                    .glyph = '=',
                },
            },
        .band_count = 3U,
        .terrain_glyph = '#',
        .detail_glyph = '.',
    },
    {
        .name = "CONTROL POINT",
        .memory_text = "[ORIGIN: OPERATOR POSITION]",
        .sector = FIELDZERO_SECTOR_HORIZON,
        .state_count = 3U,
        .entry = {2, 11},
        .spawn = {4, 11},
        .exit = {68, 2},
        .marks = {{30, 15}, {44, 9}},
        .mark_count = 2U,
        .memory = {50, 9},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {1, 12, 10, 1},
                {14, 15, 8, 1},
                {26, 16, 9, 2},
                {43, 10, 9, 1},
                {66, 3, 5, 1},
            },
        .static_rectangle_count = 7U,
        .bands =
            {
                {
                    .rectangles = {{35, 17, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, -4}, {0, -8}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{53, 15, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {0, -8}},
                    .glyph = '=',
                },
                {
                    .rectangles = {{62, 14, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {0, -8}},
                    .glyph = '=',
                },
            },
        .band_count = 3U,
        .terrain_glyph = '#',
        .detail_glyph = ':',
    },
};

const FieldzeroRoomDefinition *fieldzero_horizon_rooms(size_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = sizeof(horizon_rooms) / sizeof(horizon_rooms[0]);
    }
    return horizon_rooms;
}

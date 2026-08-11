#include "fieldzero/content.h"

static const FieldzeroRoomDefinition origin_rooms[] = {
    {
        .name = "LIVE DATUM",
        .memory_text = "[ORIGIN LOCK: ONE SIGNAL]",
        .sector = FIELDZERO_SECTOR_ORIGIN,
        .state_count = 2U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {68, 9},
        .marks = {{27, 11}},
        .memory = {15, 14},
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 33, 1},
                {59, 17, 13, 1},
                {11, 15, 8, 1},
                {22, 12, 10, 1},
                {59, 10, 12, 1},
            },
        .static_rectangle_count = 7U,
        .bands =
            {
                {
                    .rectangles = {{27, 14, 9, 1}, {39, 12, 9, 1}},
                    .rectangle_count = 2U,
                    .offsets = {{0, 0}, {8, 0}},
                    .glyph = '=',
                },
            },
        .band_count = 1U,
        .terrain_glyph = '#',
        .detail_glyph = '.',
    },
    {
        .name = "REFERENCE SHIFT",
        .memory_text = "[DATUM DRIFT: DETECTED]",
        .sector = FIELDZERO_SECTOR_ORIGIN,
        .state_count = 2U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {68, 6},
        .marks = {{32, 10}},
        .memory = {13, 14},
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 28, 1},
                {66, 17, 6, 1},
                {9, 15, 9, 1},
                {19, 13, 7, 1},
                {28, 11, 8, 1},
                {66, 7, 5, 1},
            },
        .static_rectangle_count = 8U,
        .bands =
            {
                {
                    .rectangles =
                        {
                            {38, 17, 8, 1},
                            {48, 15, 8, 1},
                            {58, 13, 7, 1},
                        },
                    .rectangle_count = 3U,
                    .offsets = {{0, 0}, {0, -4}},
                    .glyph = '=',
                },
            },
        .band_count = 1U,
        .terrain_glyph = '#',
        .detail_glyph = ':',
    },
};

const FieldzeroRoomDefinition *fieldzero_origin_rooms(size_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = sizeof(origin_rooms) / sizeof(origin_rooms[0]);
    }
    return origin_rooms;
}

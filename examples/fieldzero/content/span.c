#include "fieldzero/content.h"

static const FieldzeroRoomDefinition span_rooms[] = {
    {
        .name = "OPEN INTERVAL",
        .memory_text = "[BASELINE RETURN: NONE]",
        .sector = FIELDZERO_SECTOR_SPAN,
        .state_count = 2U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {67, 10},
        .marks = {{18, 10}},
        .mark_count = 1U,
        .memory = {14, 13},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 24, 1},
                {61, 17, 11, 1},
                {9, 14, 10, 1},
                {12, 11, 11, 1},
                {24, 10, 2, 7},
                {61, 11, 10, 1},
            },
        .static_rectangle_count = 8U,
        .bands =
            {
                {
                    .rectangles = {{27, 13, 9, 1}, {41, 11, 10, 1}},
                    .rectangle_count = 2U,
                    .offsets = {{0, 0}, {4, 0}},
                    .glyph = '_',
                },
            },
        .band_count = 1U,
        .terrain_glyph = '#',
        .detail_glyph = '-',
    },
    {
        .name = "BROKEN BASELINE",
        .memory_text = "[SPAN ERROR: PERSISTENT]",
        .sector = FIELDZERO_SECTOR_SPAN,
        .state_count = 3U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {68, 5},
        .marks = {{19, 12}, {51, 9}},
        .mark_count = 2U,
        .memory = {10, 14},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 16, 1},
                {66, 17, 6, 1},
                {7, 15, 7, 1},
                {15, 13, 8, 1},
                {47, 10, 8, 1},
                {66, 6, 5, 1},
            },
        .static_rectangle_count = 8U,
        .bands =
            {
                {
                    .rectangles = {{18, 14, 8, 1}, {29, 12, 7, 1}},
                    .rectangle_count = 2U,
                    .offsets = {{0, 0}, {8, 0}, {8, 0}},
                    .glyph = '_',
                },
                {
                    .rectangles = {{57, 13, 7, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {0, -4}},
                    .glyph = '_',
                },
            },
        .band_count = 2U,
        .terrain_glyph = '#',
        .detail_glyph = '|',
    },
};

const FieldzeroRoomDefinition *fieldzero_span_rooms(size_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = sizeof(span_rooms) / sizeof(span_rooms[0]);
    }
    return span_rooms;
}

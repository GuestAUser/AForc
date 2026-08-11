#include "fieldzero/content.h"

static const FieldzeroRoomDefinition shear_rooms[] = {
    {
        .name = "CROSSCUT",
        .memory_text = NULL,
        .sector = FIELDZERO_SECTOR_SHEAR,
        .state_count = 2U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {67, 4},
        .marks = {{32, 11}},
        .mark_count = 1U,
        .memory = {0, 0},
        .has_memory = false,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 11, 1},
                {14, 14, 7, 1},
                {23, 12, 14, 1},
                {59, 5, 12, 1},
            },
        .static_rectangle_count = 6U,
        .bands =
            {
                {
                    .rectangles = {{44, 11, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {-8, 0}},
                    .glyph = '_',
                },
                {
                    .rectangles = {{51, 8, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {-4, 0}},
                    .glyph = '_',
                },
            },
        .band_count = 2U,
        .terrain_glyph = '#',
        .detail_glyph = '/',
    },
    {
        .name = "FAULT INDEX",
        .memory_text = "[GRID ANGLE: UNSTABLE]",
        .sector = FIELDZERO_SECTOR_SHEAR,
        .state_count = 3U,
        .entry = {2, 16},
        .spawn = {4, 16},
        .exit = {68, 1},
        .marks = {{28, 12}, {60, 6}},
        .mark_count = 2U,
        .memory = {46, 9},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {0, 17, 11, 1},
                {13, 15, 7, 1},
                {24, 13, 9, 1},
                {45, 10, 7, 1},
                {57, 7, 8, 1},
                {61, 2, 10, 1},
            },
        .static_rectangle_count = 8U,
        .bands =
            {
                {
                    .rectangles = {{40, 11, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {-4, 0}, {4, 0}},
                    .glyph = '_',
                },
                {
                    .rectangles = {{51, 8, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {-4, 0}, {4, 0}},
                    .glyph = '_',
                },
                {
                    .rectangles = {{41, 5, 6, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {4, 0}, {16, 0}},
                    .glyph = '_',
                },
            },
        .band_count = 3U,
        .terrain_glyph = '#',
        .detail_glyph = '/',
    },
    {
        .name = "SHIFT REGISTER",
        .memory_text = "[POSITION FIX: MANUAL]",
        .sector = FIELDZERO_SECTOR_SHEAR,
        .state_count = 3U,
        .entry = {2, 3},
        .spawn = {4, 3},
        .exit = {68, 2},
        .marks = {{34, 14}, {53, 8}},
        .mark_count = 2U,
        .memory = {22, 10},
        .has_memory = true,
        .static_rectangles =
            {
                {0, 0, 1, 18},
                {71, 0, 1, 18},
                {1, 4, 10, 1},
                {14, 8, 7, 1},
                {19, 11, 7, 1},
                {30, 15, 8, 1},
                {50, 9, 8, 1},
                {65, 3, 6, 1},
            },
        .static_rectangle_count = 8U,
        .bands =
            {
                {
                    .rectangles = {{39, 16, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, -4}, {0, -8}},
                    .glyph = '_',
                },
                {
                    .rectangles = {{55, 10, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {-8, 0}, {4, 0}},
                    .glyph = '_',
                },
                {
                    .rectangles = {{38, 6, 8, 1}},
                    .rectangle_count = 1U,
                    .offsets = {{0, 0}, {0, 0}, {20, 0}},
                    .glyph = '_',
                },
            },
        .band_count = 3U,
        .terrain_glyph = '#',
        .detail_glyph = '/',
    },
};

const FieldzeroRoomDefinition *fieldzero_shear_rooms(size_t *out_count)
{
    if (out_count != NULL)
    {
        *out_count = sizeof(shear_rooms) / sizeof(shear_rooms[0]);
    }
    return shear_rooms;
}

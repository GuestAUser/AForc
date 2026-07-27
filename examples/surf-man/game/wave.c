/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/game.h"

#include <limits.h>

enum {
    SURF_MAN_SECTION_UNITS = 8
};

typedef struct SurfManWaveSection {
    int32_t height_q16;
    int32_t accent_q16;
    int32_t push_q16;
    bool lip;
    bool pocket;
    bool tube;
    bool foam;
    bool hazard;
} SurfManWaveSection;

static int32_t surf_man_clamp_i64(int64_t value,
                                  int32_t minimum,
                                  int32_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return (int32_t)value;
}

static int64_t surf_man_floor_div(int64_t value, int64_t divisor) {
    int64_t quotient = value / divisor;

    if (value % divisor < 0) {
        --quotient;
    }
    return quotient;
}

static int32_t surf_man_lerp_q16(int32_t start,
                                 int32_t end,
                                 int32_t fraction_q16) {
    const int64_t difference = (int64_t)end - start;
    const int64_t offset =
        (difference * fraction_q16) / SURF_MAN_Q16_ONE;

    return surf_man_clamp_i64((int64_t)start + offset, INT32_MIN, INT32_MAX);
}

static AFORC_Status surf_man_wave_section(const SurfManSimulation *simulation,
                                          int64_t section_index,
                                          SurfManWaveSection *out_section) {
    static const int32_t base_heights[] = {
        2 * SURF_MAN_Q16_ONE,
        3 * SURF_MAN_Q16_ONE,
        3 * SURF_MAN_Q16_ONE,
        2 * SURF_MAN_Q16_ONE,
        4 * SURF_MAN_Q16_ONE,
    };
    const uint64_t index = (uint64_t)section_index;
    const uint64_t section_seed =
        simulation->seed ^ (UINT64_C(0x9e3779b97f4a7c15) * index) ^
        (UINT64_C(0xd1b54a32d192ed03) * simulation->day) ^
        (UINT64_C(0x94d049bb133111eb) * simulation->wave);
    AFORC_Rng rng;
    uint32_t shape;
    uint32_t features;
    uint32_t force;
    AFORC_Status status;

    status = aforc_rng_seed(&rng,
                            section_seed,
                            UINT64_C(0xda3e39cb94b95bdb) +
                                (uint64_t)simulation->wave_kind);
    if (status == AFORC_OK) {
        status = aforc_rng_next_u32(&rng, &shape);
    }
    if (status == AFORC_OK) {
        status = aforc_rng_next_u32(&rng, &features);
    }
    if (status == AFORC_OK) {
        status = aforc_rng_next_u32(&rng, &force);
    }
    if (status != AFORC_OK) {
        return status;
    }

    out_section->height_q16 =
        base_heights[simulation->wave_kind] +
        (int32_t)(shape % 3U) * (SURF_MAN_Q16_ONE / 2);
    out_section->accent_q16 =
        (int32_t)(1U + ((shape >> 8U) % 4U)) * (SURF_MAN_Q16_ONE / 8);
    out_section->push_q16 =
        SURF_MAN_Q16_ONE / 2 +
        (int32_t)(force % 5U) * (SURF_MAN_Q16_ONE / 8);
    out_section->lip = simulation->wave_kind == SURF_MAN_WAVE_STEEP ||
                       simulation->wave_kind == SURF_MAN_WAVE_CLOSEOUT ||
                       (features & UINT32_C(3)) == 0U;
    out_section->pocket = simulation->wave_kind != SURF_MAN_WAVE_CHOP &&
                          ((features >> 2U) & UINT32_C(3)) != 0U;
    out_section->tube = simulation->wave_kind == SURF_MAN_WAVE_TUBE ||
                        (simulation->wave_kind == SURF_MAN_WAVE_STEEP &&
                         ((features >> 4U) & UINT32_C(7)) == 0U);
    out_section->foam = simulation->wave_kind == SURF_MAN_WAVE_CLOSEOUT ||
                        simulation->wave_kind == SURF_MAN_WAVE_CHOP ||
                        ((features >> 7U) & UINT32_C(3)) == 0U;
    out_section->hazard =
        (simulation->wave_kind == SURF_MAN_WAVE_CHOP ||
         simulation->wave_kind == SURF_MAN_WAVE_CLOSEOUT) &&
        ((features >> 9U) & UINT32_C(3)) == 0U;
    return AFORC_OK;
}

AFORC_Status surf_man_wave_sample(const SurfManSimulation *simulation,
                                  int32_t distance_offset_q16,
                                  SurfManWaveSample *out_sample) {
    const int64_t section_size_q16 =
        (int64_t)SURF_MAN_SECTION_UNITS * SURF_MAN_Q16_ONE;
    SurfManWaveSection current;
    SurfManWaveSection next;
    int64_t distance_q16;
    int64_t section_index;
    int64_t section_start_q16;
    int32_t fraction_q16;
    int32_t triangle_q16;
    int32_t face_q16;
    int32_t slope_q16;
    int32_t accent_slope_q16;
    bool pocket;
    bool tube;
    bool foam;
    bool hazard;
    AFORC_Status status;

    if (simulation == NULL || out_sample == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!simulation->initialized ||
        simulation->wave_kind < SURF_MAN_WAVE_OPEN ||
        simulation->wave_kind > SURF_MAN_WAVE_CLOSEOUT) {
        return AFORC_ERROR_STATE;
    }

    distance_q16 =
        (int64_t)simulation->distance_q16 + distance_offset_q16;
    section_index = surf_man_floor_div(distance_q16, section_size_q16);
    section_start_q16 = section_index * section_size_q16;
    fraction_q16 = (int32_t)(((distance_q16 - section_start_q16) *
                              SURF_MAN_Q16_ONE) /
                             section_size_q16);
    status = surf_man_wave_section(simulation, section_index, &current);
    if (status == AFORC_OK) {
        status = surf_man_wave_section(simulation, section_index + 1, &next);
    }
    if (status != AFORC_OK) {
        return status;
    }

    triangle_q16 = fraction_q16 <= SURF_MAN_Q16_ONE / 2
                       ? fraction_q16 * 2
                       : (SURF_MAN_Q16_ONE - fraction_q16) * 2;
    face_q16 = surf_man_lerp_q16(current.height_q16,
                                 next.height_q16,
                                 fraction_q16);
    face_q16 = surf_man_clamp_i64(
        (int64_t)face_q16 +
            ((int64_t)current.accent_q16 * triangle_q16) /
                SURF_MAN_Q16_ONE,
        0,
        8 * SURF_MAN_Q16_ONE);
    accent_slope_q16 = current.accent_q16 /
                       (SURF_MAN_SECTION_UNITS / 2);
    if (fraction_q16 > SURF_MAN_Q16_ONE / 2) {
        accent_slope_q16 = -accent_slope_q16;
    }
    slope_q16 = surf_man_clamp_i64(
        ((int64_t)next.height_q16 - current.height_q16) /
                SURF_MAN_SECTION_UNITS +
            accent_slope_q16,
        -2 * SURF_MAN_Q16_ONE,
        2 * SURF_MAN_Q16_ONE);
    tube = current.tube &&
           fraction_q16 >= SURF_MAN_Q16_ONE / 8 &&
           fraction_q16 <= (7 * SURF_MAN_Q16_ONE) / 8;
    hazard = !tube && current.hazard &&
             fraction_q16 >= (2 * SURF_MAN_Q16_ONE) / 5 &&
             fraction_q16 <= (3 * SURF_MAN_Q16_ONE) / 5;
    foam = !tube && !hazard && current.foam &&
           fraction_q16 >= SURF_MAN_Q16_ONE / 2;
    pocket = !tube && !hazard && !foam && current.pocket &&
             fraction_q16 >= SURF_MAN_Q16_ONE / 4 &&
             fraction_q16 <= (3 * SURF_MAN_Q16_ONE) / 4;

    *out_sample = (SurfManWaveSample){0};
    out_sample->face_q16 = face_q16;
    out_sample->slope_q16 = slope_q16;
    out_sample->push_q16 = current.push_q16;
    out_sample->lip = current.lip && !tube && !hazard &&
                      fraction_q16 >= (3 * SURF_MAN_Q16_ONE) / 4;
    out_sample->pocket = pocket;
    out_sample->tube = tube;
    out_sample->foam = foam;
    out_sample->hazard = hazard;
    return AFORC_OK;
}

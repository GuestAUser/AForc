/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/game.h"

#include <limits.h>

static uint64_t surf_man_add_score(uint64_t left, uint64_t right) {
    if (UINT64_MAX - left < right) {
        return UINT64_MAX;
    }
    return left + right;
}

static void surf_man_award_set(SurfManSimulation *simulation,
                               const char *text) {
    size_t index = 0U;

    while (index + 1U < SURF_MAN_AWARD_CAPACITY && text[index] != '\0') {
        simulation->award[index] = text[index];
        ++index;
    }
    simulation->award[index] = '\0';
}

static uint64_t surf_man_maneuver_base(SurfManManeuver maneuver) {
    switch (maneuver) {
        case SURF_MAN_MANEUVER_CARVE_LEFT:
        case SURF_MAN_MANEUVER_CARVE_RIGHT:
            return UINT64_C(100);
        case SURF_MAN_MANEUVER_LIP_SNAP:
            return UINT64_C(250);
        case SURF_MAN_MANEUVER_AIR:
            return UINT64_C(400);
        case SURF_MAN_MANEUVER_TUBE:
            return UINT64_C(20);
        default:
            return UINT64_C(0);
    }
}

static const char *surf_man_air_award(const SurfManSimulation *simulation) {
    switch (simulation->air_half_turns) {
        case 0U:
            return simulation->grabbed ? "GRAB AIR" : "AIR";
        case 1U:
            return simulation->grabbed ? "GRAB 180 AIR" : "180 AIR";
        case 2U:
            return simulation->grabbed ? "GRAB 360 AIR" : "360 AIR";
        case 3U:
            return simulation->grabbed ? "GRAB 540 AIR" : "540 AIR";
        default:
            return simulation->grabbed ? "GRAB SPIN AIR" : "SPIN AIR";
    }
}

static const char *surf_man_maneuver_award(
    const SurfManSimulation *simulation,
    SurfManManeuver maneuver) {
    switch (maneuver) {
        case SURF_MAN_MANEUVER_CARVE_LEFT:
            return "LEFT CARVE";
        case SURF_MAN_MANEUVER_CARVE_RIGHT:
            return "RIGHT CARVE";
        case SURF_MAN_MANEUVER_LIP_SNAP:
            return "LIP SNAP";
        case SURF_MAN_MANEUVER_AIR:
            return surf_man_air_award(simulation);
        case SURF_MAN_MANEUVER_TUBE:
            return "TUBE";
        default:
            return "";
    }
}

AFORC_Status surf_man_score_maneuver(SurfManSimulation *simulation,
                                     SurfManManeuver maneuver,
                                     uint32_t modifiers,
                                     bool risky) {
    const bool repeated = simulation != NULL &&
                          simulation->last_maneuver == maneuver;
    uint64_t points;
    uint64_t modifier_factor;
    uint64_t flow_factor;

    if (simulation == NULL || maneuver <= SURF_MAN_MANEUVER_NONE ||
        maneuver > SURF_MAN_MANEUVER_TUBE) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!simulation->initialized) {
        return AFORC_ERROR_STATE;
    }

    modifier_factor = (uint64_t)modifiers + UINT64_C(1);
    flow_factor = simulation->flow == 0U
                      ? UINT64_C(1)
                      : (uint64_t)simulation->flow;
    points = surf_man_maneuver_base(maneuver) * modifier_factor * flow_factor;
    if (risky) {
        points *= UINT64_C(2);
    }
    if (repeated) {
        points /= UINT64_C(2);
    }
    simulation->pending_score =
        surf_man_add_score(simulation->pending_score, points);
    if (!repeated && simulation->flow < SURF_MAN_FLOW_MAX) {
        ++simulation->flow;
    }
    if (simulation->maneuver_count < UINT32_MAX) {
        ++simulation->maneuver_count;
    }
    simulation->bank_ticks = simulation->rules.bank_delay_ticks;
    simulation->last_maneuver = maneuver;
    simulation->risk_active = simulation->risk_active || risky;
    surf_man_award_set(simulation,
                       surf_man_maneuver_award(simulation, maneuver));
    return AFORC_OK;
}

void surf_man_score_bank(SurfManSimulation *simulation) {
    if (simulation == NULL || !simulation->initialized) {
        return;
    }
    if (simulation->pending_score != 0U) {
        simulation->day_score =
            surf_man_add_score(simulation->day_score,
                               simulation->pending_score);
        if (simulation->day_score > simulation->best_score) {
            simulation->best_score = simulation->day_score;
        }
        surf_man_award_set(simulation, "BANKED");
    }
    simulation->pending_score = 0U;
    simulation->bank_ticks = 0U;
    simulation->risk_active = false;
}

void surf_man_score_wipeout(SurfManSimulation *simulation) {
    if (simulation == NULL || !simulation->initialized) {
        return;
    }
    simulation->pending_score = 0U;
    simulation->bank_ticks = 0U;
    simulation->tube_ticks = 0U;
    simulation->air_half_turns = 0U;
    simulation->flow = 0U;
    simulation->grabbed = false;
    simulation->risk_active = false;
    simulation->last_maneuver = SURF_MAN_MANEUVER_NONE;
    surf_man_award_set(simulation, "WIPEOUT - PENDING LOST");
}

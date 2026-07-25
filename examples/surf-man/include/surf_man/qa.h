/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EXAMPLES_SURF_MAN_QA_H
#define AFORC_EXAMPLES_SURF_MAN_QA_H

#include "surf_man/app.h"

AFORC_Status surf_man_simulation_checks(uint64_t seed, AFORC_Error *error);
AFORC_Status surf_man_render_checks(SurfManApp *app, AFORC_Error *error);
AFORC_Status surf_man_smoke_checks(SurfManApp *app,
                                   AFORC_Engine *engine,
                                   AFORC_Error *error);

#endif

/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_TESTS_RENDERER_DIFF_MODEL_H
#define AFORC_TESTS_RENDERER_DIFF_MODEL_H

#include "../include/aforc/renderer.h"

bool renderer_diff_apply_ansi(AFORC_Cell *cells,
                              AFORC_Size size,
                              const char *batch,
                              size_t batch_size);

#endif

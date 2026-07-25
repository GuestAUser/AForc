/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/effects.h"

#include <float.h>

/*
 * Bounded scalar tween state machine.
 *
 * Elapsed time saturates at the configured duration; sampling is pure and
 * never advances state. Inputs reject NaN and infinities so every easing mode
 * has defined endpoints and portable comparison behavior.
 */

static bool easing_valid(AFORC_Easing easing) {
    return easing == AFORC_EASING_LINEAR ||
           easing == AFORC_EASING_QUADRATIC_IN ||
           easing == AFORC_EASING_QUADRATIC_OUT ||
           easing == AFORC_EASING_QUADRATIC_IN_OUT ||
           easing == AFORC_EASING_CUBIC_IN ||
           easing == AFORC_EASING_CUBIC_OUT ||
           easing == AFORC_EASING_CUBIC_IN_OUT;
}

static bool double_finite(double value) {
    return value == value && value >= -DBL_MAX && value <= DBL_MAX;
}

static bool tween_ready(const AFORC_Tween *tween) {
    return tween != NULL && tween->initialized && tween->duration_ms > 0U &&
           tween->elapsed_ms <= tween->duration_ms &&
           easing_valid(tween->easing) && double_finite(tween->start) &&
           double_finite(tween->end);
}

AFORC_Status aforc_easing_sample(AFORC_Easing easing,
                             double progress,
                             double *out_value) {
    double value = progress;

    if (out_value == NULL || !easing_valid(easing) ||
        !double_finite(progress)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    /* Curves never extrapolate; clamping also gives every easing mode exact
       and consistent endpoint samples. */
    if (value < 0.0) {
        value = 0.0;
    } else if (value > 1.0) {
        value = 1.0;
    }

    switch (easing) {
    case AFORC_EASING_LINEAR:
        *out_value = value;
        break;
    case AFORC_EASING_QUADRATIC_IN:
        *out_value = value * value;
        break;
    case AFORC_EASING_QUADRATIC_OUT: {
        const double inverse = 1.0 - value;
        *out_value = 1.0 - inverse * inverse;
        break;
    }
    case AFORC_EASING_QUADRATIC_IN_OUT:
        if (value < 0.5) {
            *out_value = 2.0 * value * value;
        } else {
            const double inverse = -2.0 * value + 2.0;
            *out_value = 1.0 - inverse * inverse / 2.0;
        }
        break;
    case AFORC_EASING_CUBIC_IN:
        *out_value = value * value * value;
        break;
    case AFORC_EASING_CUBIC_OUT: {
        const double inverse = 1.0 - value;
        *out_value = 1.0 - inverse * inverse * inverse;
        break;
    }
    case AFORC_EASING_CUBIC_IN_OUT:
        if (value < 0.5) {
            *out_value = 4.0 * value * value * value;
        } else {
            const double inverse = -2.0 * value + 2.0;
            *out_value = 1.0 - inverse * inverse * inverse / 2.0;
        }
        break;
    default:
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return AFORC_OK;
}

AFORC_Status aforc_tween_init(AFORC_Tween *tween,
                          double start,
                          double end,
                          uint64_t duration_ms,
                          AFORC_Easing easing) {
    if (tween == NULL || !double_finite(start) || !double_finite(end) ||
        duration_ms == 0U || !easing_valid(easing)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    tween->start = start;
    tween->end = end;
    tween->duration_ms = duration_ms;
    tween->elapsed_ms = 0U;
    tween->easing = easing;
    tween->paused = false;
    tween->finished = false;
    tween->initialized = true;
    return AFORC_OK;
}

void aforc_tween_dispose(AFORC_Tween *tween) {
    if (tween == NULL) {
        return;
    }
    tween->start = 0.0;
    tween->end = 0.0;
    tween->duration_ms = 0U;
    tween->elapsed_ms = 0U;
    tween->easing = AFORC_EASING_LINEAR;
    tween->paused = false;
    tween->finished = false;
    tween->initialized = false;
}

AFORC_Status aforc_tween_restart(AFORC_Tween *tween) {
    if (tween == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!tween_ready(tween)) {
        return AFORC_ERROR_STATE;
    }
    tween->elapsed_ms = 0U;
    tween->paused = false;
    tween->finished = false;
    return AFORC_OK;
}

AFORC_Status aforc_tween_set_paused(AFORC_Tween *tween, bool paused) {
    if (tween == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!tween_ready(tween)) {
        return AFORC_ERROR_STATE;
    }
    tween->paused = paused;
    return AFORC_OK;
}

AFORC_Status aforc_tween_update(AFORC_Tween *tween, uint64_t delta_ms) {
    uint64_t remaining;

    if (tween == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!tween_ready(tween)) {
        return AFORC_ERROR_STATE;
    }
    if (tween->paused || tween->finished || delta_ms == 0U) {
        return AFORC_OK;
    }
    remaining = tween->duration_ms - tween->elapsed_ms;
    if (delta_ms >= remaining) {
        tween->elapsed_ms = tween->duration_ms;
        tween->finished = true;
    } else {
        tween->elapsed_ms += delta_ms;
    }
    return AFORC_OK;
}

AFORC_Status aforc_tween_sample(const AFORC_Tween *tween, double *out_value) {
    double eased;
    double progress;
    AFORC_Status status;

    if (tween == NULL || out_value == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!tween_ready(tween)) {
        return AFORC_ERROR_STATE;
    }
    progress = (double)tween->elapsed_ms / (double)tween->duration_ms;
    status = aforc_easing_sample(tween->easing, progress, &eased);
    if (status != AFORC_OK) {
        return status;
    }
    /* Weighted endpoints avoid the cancellation of start + (end - start). */
    *out_value = tween->start * (1.0 - eased) + tween->end * eased;
    return AFORC_OK;
}

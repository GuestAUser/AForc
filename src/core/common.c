/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "common_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Owns process-independent allocation, error, logging, and geometry helpers;
 * it retains no caller pointers or subsystem state. */

static void *default_allocate(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void *default_reallocate(void *context, void *memory, size_t size) {
    (void)context;
    return realloc(memory, size);
}

static void default_deallocate(void *context, void *memory) {
    (void)context;
    free(memory);
}

AFORC_Allocator aforc_allocator_default(void) {
    AFORC_Allocator allocator = {NULL, default_allocate, default_reallocate,
                               default_deallocate};
    return allocator;
}

bool aforc_size_multiply(size_t left, size_t right, size_t *out) {
    if (out == NULL) {
        return false;
    }
    if (left != 0U && right > SIZE_MAX / left) {
        return false;
    }
    *out = left * right;
    return true;
}

bool aforc_size_add(size_t left, size_t right, size_t *out) {
    if (out == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *out = left + right;
    return true;
}

AFORC_Status aforc_alloc_array(const AFORC_Allocator *allocator, size_t count,
                           size_t element_size, void **out_memory) {
    size_t bytes = 0U;
    if (!aforc_allocator_is_valid(allocator) || out_memory == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    if (!aforc_size_multiply(count, element_size, &bytes)) {
        return AFORC_ERROR_OVERFLOW;
    }
    if (bytes == 0U) {
        return AFORC_OK;
    }
    *out_memory = allocator->allocate(allocator->context, bytes);
    return *out_memory == NULL ? AFORC_ERROR_OUT_OF_MEMORY : AFORC_OK;
}

AFORC_Status aforc_realloc_array(const AFORC_Allocator *allocator, void *memory,
                             size_t count, size_t element_size,
                             void **out_memory) {
    size_t bytes = 0U;
    void *replacement = NULL;
    if (!aforc_allocator_is_valid(allocator) || out_memory == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_size_multiply(count, element_size, &bytes)) {
        return AFORC_ERROR_OVERFLOW;
    }
    if (bytes == 0U) {
        allocator->deallocate(allocator->context, memory);
        *out_memory = NULL;
        return AFORC_OK;
    }
    replacement = allocator->reallocate(allocator->context, memory, bytes);
    if (replacement == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    *out_memory = replacement;
    return AFORC_OK;
}

void aforc_free(const AFORC_Allocator *allocator, void *memory) {
    if (aforc_allocator_is_valid(allocator) && memory != NULL) {
        allocator->deallocate(allocator->context, memory);
    }
}

void aforc_error_clear(AFORC_Error *error) {
    if (error != NULL) {
        error->status = AFORC_OK;
        error->subsystem = NULL;
        error->message[0] = '\0';
    }
}

void aforc_error_set(AFORC_Error *error, AFORC_Status status,
                   const char *subsystem, const char *format, ...) {
    va_list arguments;
    if (error == NULL) {
        return;
    }
    error->status = status;
    error->subsystem = subsystem;
    error->message[0] = '\0';
    if (format == NULL) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error->message, sizeof(error->message), format, arguments);
    va_end(arguments);
}

const char *aforc_status_string(AFORC_Status status) {
    static const char *const names[] = {
        "ok", "invalid argument", "out of memory", "integer overflow",
        "I/O error", "platform error", "unsupported", "limit reached",
        "invalid state", "not found", "stale handle", "already exists",
        "invalid format", "checksum mismatch", "end of stream", "interrupted"};
    const size_t index = (size_t)status;
    return index < sizeof(names) / sizeof(names[0]) ? names[index]
                                                    : "unknown status";
}

void aforc_log(const AFORC_Logger *logger, AFORC_LogLevel level,
             const char *subsystem, const char *message) {
    if (logger != NULL && logger->write != NULL &&
        level >= logger->minimum_level) {
        logger->write(logger->context, level, subsystem, message);
    }
}

bool aforc_rect_contains(AFORC_Rect rect, AFORC_Point point) {
    const int64_t right = (int64_t)rect.x + (int64_t)rect.width;
    const int64_t bottom = (int64_t)rect.y + (int64_t)rect.height;
    return rect.width > 0 && rect.height > 0 && point.x >= rect.x &&
           point.y >= rect.y && (int64_t)point.x < right &&
           (int64_t)point.y < bottom;
}

AFORC_Rect aforc_rect_intersection(AFORC_Rect left, AFORC_Rect right) {
    const int64_t left_right = (int64_t)left.x + left.width;
    const int64_t right_right = (int64_t)right.x + right.width;
    const int64_t left_bottom = (int64_t)left.y + left.height;
    const int64_t right_bottom = (int64_t)right.y + right.height;
    const int64_t x0 = left.x > right.x ? left.x : right.x;
    const int64_t y0 = left.y > right.y ? left.y : right.y;
    const int64_t x1 = left_right < right_right ? left_right : right_right;
    const int64_t y1 = left_bottom < right_bottom ? left_bottom : right_bottom;
    AFORC_Rect result = {(int32_t)x0, (int32_t)y0, 0, 0};
    if (left.width <= 0 || left.height <= 0 || right.width <= 0 ||
        right.height <= 0 || x1 <= x0 || y1 <= y0) {
        return result;
    }
    result.width = (int32_t)(x1 - x0);
    result.height = (int32_t)(y1 - y0);
    return result;
}

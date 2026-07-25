/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "config_parser_scale_support.h"

#include <stdio.h>
#include <stdlib.h>

static size_t test_allocation_attempts;
static size_t test_fail_at;
static size_t test_live_allocations;

void *aforc_config_test_malloc(size_t size)
{
    void *memory;

    ++test_allocation_attempts;
    if (test_fail_at != 0u && test_allocation_attempts == test_fail_at) {
        return NULL;
    }
    memory = malloc(size);
    if (memory != NULL) {
        ++test_live_allocations;
    }
    return memory;
}

void *aforc_config_test_realloc(void *memory, size_t size)
{
    void *replacement;

    ++test_allocation_attempts;
    if (test_fail_at != 0u && test_allocation_attempts == test_fail_at) {
        return NULL;
    }
    replacement = realloc(memory, size);
    if (replacement != NULL && memory == NULL) {
        ++test_live_allocations;
    }
    return replacement;
}

void aforc_config_test_free(void *memory)
{
    if (memory != NULL) {
        --test_live_allocations;
    }
    free(memory);
}

void aforc_config_scale_reset_allocations(size_t failure)
{
    test_allocation_attempts = 0u;
    test_fail_at = failure;
    test_live_allocations = 0u;
}

size_t aforc_config_scale_allocation_attempts(void)
{
    return test_allocation_attempts;
}

size_t aforc_config_scale_live_allocations(void)
{
    return test_live_allocations;
}

char *aforc_config_scale_make_config(
    size_t entry_count,
    bool append_duplicate,
    size_t *output_size)
{
    const size_t bytes_per_entry = 32u;
    const size_t fixed_bytes = 64u;
    size_t capacity;
    size_t used;
    size_t index;
    char *text;
    int written;

    if (output_size == NULL ||
        entry_count > (SIZE_MAX - fixed_bytes) / bytes_per_entry) {
        return NULL;
    }
    capacity = fixed_bytes + (entry_count * bytes_per_entry);
    text = malloc(capacity);
    if (text == NULL) {
        return NULL;
    }
    written = snprintf(text, capacity, "[dense]\n");
    if (written < 0 || (size_t)written >= capacity) {
        free(text);
        return NULL;
    }
    used = (size_t)written;
    for (index = 0u; index < entry_count; ++index) {
        written = snprintf(text + used,
                           capacity - used,
                           "key%zu=value%zu\n",
                           index,
                           index);
        if (written < 0 || (size_t)written >= capacity - used) {
            free(text);
            return NULL;
        }
        used += (size_t)written;
    }
    if (append_duplicate) {
        written = snprintf(text + used, capacity - used, "key0=duplicate\n");
        if (written < 0 || (size_t)written >= capacity - used) {
            free(text);
            return NULL;
        }
        used += (size_t)written;
    }
    *output_size = used;
    return text;
}

bool aforc_config_scale_parse_status(
    const char *text,
    size_t text_size,
    AFORC_Status expected)
{
    AFORC_ConfigLimits limits = aforc_config_limits_default();
    AFORC_Config config = {0};
    const AFORC_Status status =
        aforc_config_parse(text, text_size, &limits, &config);
    const bool passed = status == expected && config.entries == NULL &&
                        config.count == 0u;

    aforc_config_release(&config);
    return passed;
}

double aforc_config_scale_elapsed_milliseconds(
    const struct timespec *start,
    const struct timespec *end)
{
    const double seconds = (double)(end->tv_sec - start->tv_sec);
    const double nanoseconds = (double)(end->tv_nsec - start->tv_nsec);

    return (seconds * 1000.0) + (nanoseconds / 1000000.0);
}

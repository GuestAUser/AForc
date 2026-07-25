/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_CONFIG_PARSER_SCALE_SUPPORT_H
#define AFORC_CONFIG_PARSER_SCALE_SUPPORT_H

#include "aforc/assets.h"

#include <time.h>

void *aforc_config_test_malloc(size_t size);
void *aforc_config_test_realloc(void *memory, size_t size);
void aforc_config_test_free(void *memory);

void aforc_config_scale_reset_allocations(size_t failure);
size_t aforc_config_scale_allocation_attempts(void);
size_t aforc_config_scale_live_allocations(void);
char *aforc_config_scale_make_config(
    size_t entry_count,
    bool append_duplicate,
    size_t *output_size);
bool aforc_config_scale_parse_status(
    const char *text,
    size_t text_size,
    AFORC_Status expected);
double aforc_config_scale_elapsed_milliseconds(
    const struct timespec *start,
    const struct timespec *end);

#endif

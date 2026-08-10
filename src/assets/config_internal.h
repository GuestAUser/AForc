/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_CONFIG_INTERNAL_H
#define AFORC_CONFIG_INTERNAL_H

#include "assets_internal.h"

typedef struct AFORC_ConfigRange
{
    const char *data;
    size_t size;
} AFORC_ConfigRange;

typedef struct AFORC_ConfigIndex
{
    size_t *slots;
    size_t capacity;
} AFORC_ConfigIndex;

typedef struct AFORC_ConfigBuilder
{
    AFORC_ConfigEntry *entries;
    size_t count;
    size_t capacity;
    size_t maximum;
    AFORC_ConfigIndex index;
} AFORC_ConfigBuilder;

AFORC_ASSETS_INTERNAL bool
aforc_config_index_contains(const AFORC_ConfigIndex *index,
                            const AFORC_ConfigEntry *entries,
                            AFORC_ConfigRange section,
                            AFORC_ConfigRange key);
AFORC_ASSETS_INTERNAL void
aforc_config_index_insert(AFORC_ConfigIndex *index,
                          const AFORC_ConfigEntry *entries,
                          size_t entry_index);
AFORC_ASSETS_INTERNAL AFORC_Status aforc_config_index_reserve(
    AFORC_ConfigIndex *index, const AFORC_ConfigEntry *entries, size_t count);
AFORC_ASSETS_INTERNAL void aforc_config_index_release(AFORC_ConfigIndex *index);

AFORC_ASSETS_INTERNAL AFORC_Status
aforc_config_duplicate_range(AFORC_ConfigRange range, char **output);
AFORC_ASSETS_INTERNAL void
aforc_config_entries_release(AFORC_ConfigEntry *entries, size_t count);
AFORC_ASSETS_INTERNAL AFORC_Status
aforc_config_builder_append(AFORC_ConfigBuilder *builder,
                            AFORC_ConfigRange section,
                            AFORC_ConfigRange key,
                            AFORC_ConfigRange value);

#endif

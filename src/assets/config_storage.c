/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns heap-backed config entries and public lookup/release operations. */

#include "config_internal.h"

#include "assets_internal.h"

#include <stdlib.h>
#include <string.h>

AFORC_Status aforc_config_duplicate_range(AFORC_ConfigRange range,
                                          char **output)
{
    char *copy;
    size_t allocation_size;

    if ((range.data == NULL && range.size != 0u) || output == NULL ||
        !aforc_size_add(range.size, 1u, &allocation_size))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    copy = malloc(allocation_size);
    if (copy == NULL)
    {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    if (range.size > 0u)
    {
        memcpy(copy, range.data, range.size);
    }
    copy[range.size] = '\0';
    *output = copy;
    return AFORC_OK;
}

void aforc_config_entries_release(AFORC_ConfigEntry *entries, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index)
    {
        free(entries[index].section);
        free(entries[index].key);
        free(entries[index].value);
    }
    free(entries);
}

static AFORC_Status aforc_config_grow_entries(AFORC_ConfigBuilder *builder)
{
    size_t required;
    size_t next_capacity;
    size_t allocation_size;
    AFORC_ConfigEntry *resized;

    if (builder->count >= builder->maximum ||
        !aforc_size_add(builder->count, 1u, &required) ||
        !aforc_assets_growth_capacity(builder->capacity,
                                      required,
                                      builder->maximum,
                                      8u,
                                      &next_capacity) ||
        next_capacity == 0u ||
        !aforc_size_multiply(
            next_capacity, sizeof(AFORC_ConfigEntry), &allocation_size))
    {
        return AFORC_ERROR_LIMIT;
    }
    resized = realloc(builder->entries, allocation_size);
    if (resized == NULL)
    {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    builder->entries = resized;
    builder->capacity = next_capacity;
    return AFORC_OK;
}

AFORC_Status aforc_config_builder_append(AFORC_ConfigBuilder *builder,
                                         AFORC_ConfigRange section,
                                         AFORC_ConfigRange key,
                                         AFORC_ConfigRange value)
{
    AFORC_ConfigEntry entry;
    AFORC_Status status;

    if (aforc_config_index_contains(
            &builder->index, builder->entries, section, key))
    {
        return AFORC_ERROR_EXISTS;
    }
    if (builder->count >= builder->maximum)
    {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_config_index_reserve(
        &builder->index, builder->entries, builder->count);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (builder->count == builder->capacity)
    {
        status = aforc_config_grow_entries(builder);
        if (status != AFORC_OK)
        {
            return status;
        }
    }

    entry.section = NULL;
    entry.key = NULL;
    entry.value = NULL;
    status = aforc_config_duplicate_range(section, &entry.section);
    if (status == AFORC_OK)
    {
        status = aforc_config_duplicate_range(key, &entry.key);
    }
    if (status == AFORC_OK)
    {
        status = aforc_config_duplicate_range(value, &entry.value);
    }
    if (status != AFORC_OK)
    {
        free(entry.section);
        free(entry.key);
        free(entry.value);
        return status;
    }

    builder->entries[builder->count] = entry;
    aforc_config_index_insert(
        &builder->index, builder->entries, builder->count);
    ++builder->count;
    return AFORC_OK;
}

const char *aforc_config_get(const AFORC_Config *config,
                             const char *section,
                             const char *key)
{
    size_t index;

    if (config == NULL || section == NULL || key == NULL ||
        (config->entries == NULL && config->count != 0u))
    {
        return NULL;
    }
    for (index = 0u; index < config->count; ++index)
    {
        if (strcmp(config->entries[index].section, section) == 0 &&
            strcmp(config->entries[index].key, key) == 0)
        {
            return config->entries[index].value;
        }
    }
    return NULL;
}

void aforc_config_release(AFORC_Config *config)
{
    if (config == NULL)
    {
        return;
    }
    aforc_config_entries_release(config->entries, config->count);
    config->entries = NULL;
    config->count = 0u;
}

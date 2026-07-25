/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/*
 * Owns bounded parsing of caller-supplied bytes into heap-owned entries. Input
 * is untrusted and output is committed only after the full document validates.
 */

#include "../../include/aforc/assets.h"

#include "assets_internal.h"

#include <stdlib.h>
#include <string.h>

AFORC_ConfigLimits aforc_config_limits_default(void)
{
    AFORC_ConfigLimits limits;

    limits.max_input_bytes = AFORC_CONFIG_DEFAULT_MAX_INPUT_BYTES;
    limits.max_line_bytes = AFORC_CONFIG_DEFAULT_MAX_LINE_BYTES;
    limits.max_entries = AFORC_CONFIG_DEFAULT_MAX_ENTRIES;
    limits.max_section_bytes = AFORC_CONFIG_DEFAULT_MAX_SECTION_BYTES;
    limits.max_key_bytes = AFORC_CONFIG_DEFAULT_MAX_KEY_BYTES;
    limits.max_value_bytes = AFORC_CONFIG_DEFAULT_MAX_VALUE_BYTES;
    return limits;
}

static bool aforc_config_space(unsigned char character)
{
    return character == (unsigned char)' ' || character == (unsigned char)'\t';
}

static void aforc_config_trim(
    const char *input,
    size_t *start,
    size_t *end)
{
    while (*start < *end &&
           aforc_config_space((unsigned char)input[*start])) {
        ++(*start);
    }
    while (*end > *start &&
           aforc_config_space((unsigned char)input[*end - 1u])) {
        --(*end);
    }
}

static bool aforc_config_name_character(unsigned char character)
{
    return (character >= (unsigned char)'a' && character <= (unsigned char)'z') ||
           (character >= (unsigned char)'A' && character <= (unsigned char)'Z') ||
           (character >= (unsigned char)'0' && character <= (unsigned char)'9') ||
           character == (unsigned char)'_' || character == (unsigned char)'-'   ||
           character == (unsigned char)'.';
}

static bool aforc_config_name_valid(
    const char *input,
    size_t start,
    size_t end)
{
    size_t index;

    if (start == end) {
        return false;
    }
    for (index = start; index < end; ++index) {
        if (!aforc_config_name_character((unsigned char)input[index])) {
            return false;
        }
    }
    return true;
}

static AFORC_Status aforc_duplicate_range(
    const char *input,
    size_t size,
    char **output)
{
    char *copy;
    size_t allocation_size;

    if ((input == NULL && size != 0u) || output == NULL ||
        !aforc_size_add(size, 1u, &allocation_size)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    copy = malloc(allocation_size);
    if (copy == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    if (size > 0u) {
        memcpy(copy, input, size);
    }
    copy[size] = '\0';
    *output = copy;
    return AFORC_OK;
}

static bool aforc_string_matches_range(
    const char *string,
    const char *range,
    size_t range_size)
{
    size_t string_size = strlen(string);

    return string_size == range_size &&
            (range_size == 0u || memcmp(string, range, range_size) == 0);
}

typedef struct AFORC_ConfigIndex {
    size_t *slots;
    size_t capacity;
} AFORC_ConfigIndex;

static size_t aforc_config_hash_range(const char *input, size_t size)
{
    size_t hash = (size_t)UINT64_C(1469598103934665603);
    size_t index;

    for (index = 0u; index < size; ++index) {
        hash ^= (size_t)(unsigned char)input[index];
        hash *= (size_t)UINT64_C(1099511628211);
    }
    return hash;
}

static size_t aforc_config_pair_hash(
    const char *section,
    size_t section_size,
    const char *key,
    size_t key_size)
{
    const size_t section_hash = aforc_config_hash_range(section, section_size);
    const size_t key_hash = aforc_config_hash_range(key, key_size);

    return section_hash ^
           (key_hash + (size_t)UINT64_C(0x9e3779b97f4a7c15) +
            (section_hash << 6u) + (section_hash >> 2u));
}

static size_t aforc_config_entry_hash(const AFORC_ConfigEntry *entry)
{
    return aforc_config_pair_hash(entry->section,
                                  strlen(entry->section),
                                  entry->key,
                                  strlen(entry->key));
}

static bool aforc_config_index_contains(
    const AFORC_ConfigIndex *index,
    const AFORC_ConfigEntry *entries,
    const char *section,
    size_t section_size,
    const char *key,
    size_t key_size)
{
    size_t slot;
    size_t mask;

    if (index->capacity == 0u) {
        return false;
    }
    mask = index->capacity - 1u;
    slot = aforc_config_pair_hash(section, section_size, key, key_size) & mask;
    while (index->slots[slot] != SIZE_MAX) {
        const AFORC_ConfigEntry *entry = &entries[index->slots[slot]];

        if (aforc_string_matches_range(entry->section, section, section_size) &&
            aforc_string_matches_range(entry->key, key, key_size)) {
            return true;
        }
        slot = (slot + 1u) & mask;
    }
    return false;
}

static void aforc_config_index_insert(
    AFORC_ConfigIndex *index,
    const AFORC_ConfigEntry *entries,
    size_t entry_index)
{
    const size_t mask = index->capacity - 1u;
    size_t slot = aforc_config_entry_hash(&entries[entry_index]) & mask;

    while (index->slots[slot] != SIZE_MAX) {
        slot = (slot + 1u) & mask;
    }
    index->slots[slot] = entry_index;
}

static AFORC_Status aforc_config_index_reserve(
    AFORC_ConfigIndex *index,
    const AFORC_ConfigEntry *entries,
    size_t count)
{
    size_t required;
    size_t minimum_capacity;
    size_t next_capacity = 8u;
    size_t allocation_size;
    size_t slot;
    size_t *next_slots;
    AFORC_ConfigIndex next_index;

    if (index->capacity != 0u && count < index->capacity / 2u) {
        return AFORC_OK;
    }
    if (!aforc_size_add(count, 1u, &required) ||
        !aforc_size_multiply(required, 2u, &minimum_capacity)) {
        return AFORC_ERROR_LIMIT;
    }
    if (minimum_capacity < next_capacity) {
        minimum_capacity = next_capacity;
    }
    while (next_capacity < minimum_capacity) {
        if (next_capacity > SIZE_MAX / 2u) {
            return AFORC_ERROR_LIMIT;
        }
        next_capacity *= 2u;
    }
    if (!aforc_size_multiply(
            next_capacity,
            sizeof(*next_slots),
            &allocation_size)) {
        return AFORC_ERROR_LIMIT;
    }
    next_slots = malloc(allocation_size);
    if (next_slots == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    for (slot = 0u; slot < next_capacity; ++slot) {
        next_slots[slot] = SIZE_MAX;
    }
    next_index.slots = next_slots;
    next_index.capacity = next_capacity;
    for (slot = 0u; slot < count; ++slot) {
        aforc_config_index_insert(&next_index, entries, slot);
    }
    free(index->slots);
    *index = next_index;
    return AFORC_OK;
}

static void aforc_config_index_release(AFORC_ConfigIndex *index)
{
    free(index->slots);
    index->slots = NULL;
    index->capacity = 0u;
}

static void aforc_config_entries_release(AFORC_ConfigEntry *entries, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        free(entries[index].section);
        free(entries[index].key);
        free(entries[index].value);
    }
    free(entries);
}

static AFORC_Status aforc_config_grow_entries(
    AFORC_ConfigEntry **entries,
    size_t count,
    size_t *capacity,
    size_t maximum)
{
    size_t required;
    size_t next_capacity;
    size_t allocation_size;
    AFORC_ConfigEntry *resized;

    if (count >= maximum || !aforc_size_add(count, 1u, &required) ||
        !aforc_assets_growth_capacity(
            *capacity,
            required,
            maximum,
            8u,
            &next_capacity) ||
        next_capacity == 0u ||
        !aforc_size_multiply(
            next_capacity,
            sizeof(AFORC_ConfigEntry),
            &allocation_size)) {
        return AFORC_ERROR_LIMIT;
    }
    resized = realloc(*entries, allocation_size);
    if (resized == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    *entries = resized;
    *capacity = next_capacity;
    return AFORC_OK;
}

static AFORC_Status aforc_config_append(
    AFORC_ConfigEntry **entries,
    size_t *count,
    size_t *capacity,
    AFORC_ConfigIndex *index,
    size_t maximum,
    const char *section,
    size_t section_size,
    const char *key,
    size_t key_size,
    const char *value,
    size_t value_size)
{
    AFORC_ConfigEntry entry;
    AFORC_Status status;

    if (aforc_config_index_contains(
            index,
            *entries,
            section,
            section_size,
            key,
            key_size)) {
        return AFORC_ERROR_EXISTS;
    }
    if (*count >= maximum) {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_config_index_reserve(index, *entries, *count);
    if (status != AFORC_OK) {
        return status;
    }
    if (*count == *capacity) {
        status = aforc_config_grow_entries(entries, *count, capacity, maximum);
        if (status != AFORC_OK) {
            return status;
        }
    }

    entry.section = NULL;
    entry.key = NULL;
    entry.value = NULL;
    status = aforc_duplicate_range(section, section_size, &entry.section);
    if (status == AFORC_OK) {
        status = aforc_duplicate_range(key, key_size, &entry.key);
    }
    if (status == AFORC_OK) {
        status = aforc_duplicate_range(value, value_size, &entry.value);
    }
    if (status != AFORC_OK) {
        free(entry.section);
        free(entry.key);
        free(entry.value);
        return status;
    }

    (*entries)[*count] = entry;
    aforc_config_index_insert(index, *entries, *count);
    ++(*count);
    return AFORC_OK;
}

static AFORC_Status aforc_config_validate_input(const char *input, size_t input_size)
{
    size_t index;

    for (index = 0u; index < input_size; ++index) {
        unsigned char character = (unsigned char)input[index];

        if (character == UINT8_C(0) || character == UINT8_C(127) ||
            (character < UINT8_C(32) && character != (unsigned char)'\n' &&
             character != (unsigned char)'\r' &&
             character != (unsigned char)'\t')) {
            return AFORC_ERROR_FORMAT;
        }
        if (character == (unsigned char)'\r' &&
            (index == input_size - 1u || input[index + 1u] != '\n')) {
            return AFORC_ERROR_FORMAT;
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_config_parse(
    const char *input,
    size_t input_size,
    const AFORC_ConfigLimits *limits,
    AFORC_Config *output)
{
    AFORC_ConfigEntry *entries = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    AFORC_ConfigIndex index = {NULL, 0u};
    char *section = NULL;
    size_t section_size = 0u;
    size_t position = 0u;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    output->entries = NULL;
    output->count = 0u;
    if ((input == NULL && input_size != 0u) || limits == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (input_size > limits->max_input_bytes) {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_config_validate_input(input, input_size);
    if (status != AFORC_OK) {
        return status;
    }

    while (position < input_size) {
        size_t raw_start = position;
        size_t raw_end;
        size_t start;
        size_t end;

        while (position < input_size && input[position] != '\n') {
            ++position;
        }
        raw_end = position;
        if (position < input_size) {
            ++position;
        }
        if (raw_end > raw_start && input[raw_end - 1u] == '\r') {
            --raw_end;
        }

        /* Raw lines are bounded before trimming; token limits apply afterward. */
        if (raw_end - raw_start > limits->max_line_bytes) {
            status = AFORC_ERROR_LIMIT;
            goto fail;
        }
        start = raw_start;
        end = raw_end;
        aforc_config_trim(input, &start, &end);
        if (start == end || input[start] == '#' || input[start] == ';') {
            continue;
        }

        if (input[start] == '[') {
            size_t name_start;
            size_t name_end;
            size_t name_size;
            char *new_section;

            if (end - start < 3u || input[end - 1u] != ']') {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            name_start = start + 1u;
            name_end = end - 1u;
            aforc_config_trim(input, &name_start, &name_end);
            name_size = name_end - name_start;
            if (name_size > limits->max_section_bytes) {
                status = AFORC_ERROR_LIMIT;
                goto fail;
            }
            if (!aforc_config_name_valid(input, name_start, name_end)) {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            status = aforc_duplicate_range(
                input + name_start,
                name_size,
                &new_section);
            if (status != AFORC_OK) {
                goto fail;
            }
            free(section);
            section = new_section;
            section_size = name_size;
        } else {
            size_t equals = start;
            size_t key_start = start;
            size_t key_end;
            size_t value_start;
            size_t value_end = end;
            size_t key_size;
            size_t value_size;

            while (equals < end && input[equals] != '=') {
                ++equals;
            }
            if (equals == end) {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            key_end = equals;
            value_start = equals + 1u;
            aforc_config_trim(input, &key_start, &key_end);
            aforc_config_trim(input, &value_start, &value_end);
            key_size = key_end - key_start;
            value_size = value_end - value_start;

            if (key_size > limits->max_key_bytes ||
                value_size > limits->max_value_bytes) {
                status = AFORC_ERROR_LIMIT;
                goto fail;
            }
            if (!aforc_config_name_valid(input, key_start, key_end)) {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            status = aforc_config_append(
                &entries,
                &count,
                &capacity,
                &index,
                limits->max_entries,
                section,
                section_size,
                input + key_start,
                key_size,
                input + value_start,
                value_size);
            if (status != AFORC_OK) {
                goto fail;
            }
        }
    }

    free(section);
    aforc_config_index_release(&index);
    output->entries = entries;
    output->count = count;
    return AFORC_OK;

fail:
    free(section);
    aforc_config_index_release(&index);
    aforc_config_entries_release(entries, count);
    return status;
}

const char *aforc_config_get(
    const AFORC_Config *config,
    const char *section,
    const char *key)
{
    size_t index;

    if (config == NULL || section == NULL || key == NULL ||
        (config->entries == NULL && config->count != 0u)) {
        return NULL;
    }
    for (index = 0u; index < config->count; ++index) {
        if (strcmp(config->entries[index].section, section) == 0 &&
            strcmp(config->entries[index].key, key) == 0) {
            return config->entries[index].value;
        }
    }
    return NULL;
}

void aforc_config_release(AFORC_Config *config)
{
    if (config == NULL) {
        return;
    }
    aforc_config_entries_release(config->entries, config->count);
    config->entries = NULL;
    config->count = 0u;
}

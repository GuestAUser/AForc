/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns the transient open-addressed index used for duplicate detection. */

#include "config_internal.h"

#include <stdlib.h>
#include <string.h>

static bool aforc_string_matches_range(
    const char *string,
    AFORC_ConfigRange range)
{
    size_t string_size = strlen(string);

    return string_size == range.size &&
           (range.size == 0u || memcmp(string, range.data, range.size) == 0);
}

static size_t aforc_config_hash_range(AFORC_ConfigRange range)
{
    size_t hash = (size_t)UINT64_C(1469598103934665603);
    size_t index;

    for (index = 0u; index < range.size; ++index) {
        hash ^= (size_t)(unsigned char)range.data[index];
        hash *= (size_t)UINT64_C(1099511628211);
    }
    return hash;
}

static size_t aforc_config_pair_hash(
    AFORC_ConfigRange section,
    AFORC_ConfigRange key)
{
    const size_t section_hash = aforc_config_hash_range(section);
    const size_t key_hash = aforc_config_hash_range(key);

    return section_hash ^
           (key_hash + (size_t)UINT64_C(0x9e3779b97f4a7c15) +
            (section_hash << 6u) + (section_hash >> 2u));
}

static size_t aforc_config_entry_hash(const AFORC_ConfigEntry *entry)
{
    const AFORC_ConfigRange section = {entry->section, strlen(entry->section)};
    const AFORC_ConfigRange key = {entry->key, strlen(entry->key)};

    return aforc_config_pair_hash(section, key);
}

bool aforc_config_index_contains(
    const AFORC_ConfigIndex *index,
    const AFORC_ConfigEntry *entries,
    AFORC_ConfigRange section,
    AFORC_ConfigRange key)
{
    size_t slot;
    size_t mask;

    if (index->capacity == 0u) {
        return false;
    }
    mask = index->capacity - 1u;
    slot = aforc_config_pair_hash(section, key) & mask;
    while (index->slots[slot] != SIZE_MAX) {
        const AFORC_ConfigEntry *entry = &entries[index->slots[slot]];

        if (aforc_string_matches_range(entry->section, section) &&
            aforc_string_matches_range(entry->key, key)) {
            return true;
        }
        slot = (slot + 1u) & mask;
    }
    return false;
}

void aforc_config_index_insert(
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

AFORC_Status aforc_config_index_reserve(
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

void aforc_config_index_release(AFORC_ConfigIndex *index)
{
    free(index->slots);
    index->slots = NULL;
    index->capacity = 0u;
}

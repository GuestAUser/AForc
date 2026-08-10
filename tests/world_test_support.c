/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_test_support.h"

#include <stdlib.h>

static void *world_test_allocate(void *context, size_t size)
{
    WorldTestAllocatorState *state = context;
    void *memory;

    ++state->attempts;
    if (state->fail_at != 0U && state->attempts == state->fail_at)
    {
        return NULL;
    }
    memory = malloc(size);
    if (memory != NULL)
    {
        ++state->allocations;
        ++state->live;
    }
    return memory;
}

static void *world_test_reallocate(void *context, void *memory, size_t size)
{
    WorldTestAllocatorState *state = context;
    const bool new_allocation = memory == NULL;
    void *replacement;

    ++state->attempts;
    if (state->fail_at != 0U && state->attempts == state->fail_at)
    {
        return NULL;
    }
    replacement = realloc(memory, size);
    if (replacement != NULL && new_allocation)
    {
        ++state->allocations;
        ++state->live;
    }
    return replacement;
}

static void world_test_free(void *context, void *memory)
{
    WorldTestAllocatorState *state = context;

    if (memory != NULL)
    {
        free(memory);
        ++state->frees;
        --state->live;
    }
}

AFORC_Allocator world_test_tracking_allocator(WorldTestAllocatorState *state)
{
    const AFORC_Allocator allocator = {
        state, world_test_allocate, world_test_reallocate, world_test_free};
    return allocator;
}

bool world_test_tile_nonzero(AFORC_Tile tile,
                             uint32_t layer,
                             AFORC_Point position,
                             void *context)
{
    (void)layer;
    (void)position;
    (void)context;
    return tile != 0U;
}

bool world_test_trace_nonzero(AFORC_Tile tile,
                              uint32_t layer,
                              AFORC_Point position,
                              void *context)
{
    WorldTestTrace *trace = context;

    (void)layer;
    if (trace->count < WORLD_TEST_TRACE_CAPACITY)
    {
        trace->points[trace->count++] = position;
    }
    else
    {
        trace->overflowed = true;
    }
    return tile != 0U;
}

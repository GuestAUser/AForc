/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_TESTS_WORLD_TEST_SUPPORT_H
#define AFORC_TESTS_WORLD_TEST_SUPPORT_H

#include "aforc/world.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#define WORLD_TEST_CHECK(condition)                                            \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)fprintf(stderr,                                              \
                          "check failed at %s:%d: %s\n",                       \
                          __FILE__,                                            \
                          __LINE__,                                            \
                          #condition);                                         \
            return false;                                                      \
        }                                                                      \
    } while (false)

#define WORLD_TEST_TRACE_CAPACITY 32U

typedef struct WorldTestAllocatorState
{
    size_t attempts;
    size_t allocations;
    size_t frees;
    size_t live;
    size_t fail_at;
} WorldTestAllocatorState;

typedef struct WorldTestTrace
{
    AFORC_Point points[WORLD_TEST_TRACE_CAPACITY];
    size_t count;
    bool overflowed;
} WorldTestTrace;

AFORC_Allocator world_test_tracking_allocator(WorldTestAllocatorState *state);
bool world_test_tile_nonzero(AFORC_Tile tile,
                             uint32_t layer,
                             AFORC_Point position,
                             void *context);
bool world_test_trace_nonzero(AFORC_Tile tile,
                              uint32_t layer,
                              AFORC_Point position,
                              void *context);

bool world_test_core_cases(void);
bool world_test_visibility_cases(void);
bool world_test_astar_cases(void);

#endif

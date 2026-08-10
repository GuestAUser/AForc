/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "input_internal.h"

#include <limits.h>
#include <string.h>

/*
 * Bounded FIFO event storage.
 *
 * The queue copies complete event values into caller-configured capacity and
 * never allocates after input creation. Overflow is observable but cannot
 * reorder or evict events already accepted by consumers.
 */

bool aforc_input_internal_queue_event(AFORC_Input *input,
                                      const AFORC_InputEvent *event)
{
    size_t tail = 0u;

    if (input->event_count == input->event_capacity)
    {
        /* Drop the newest event so already-observed ordering never changes. */
        if (input->dropped_events < UINT64_MAX)
        {
            ++input->dropped_events;
        }
        input->queue_overflowed = true;
        return false;
    }
    tail = (input->event_head + input->event_count) % input->event_capacity;
    input->events[tail] = *event;
    ++input->event_count;
    return true;
}

AFORC_InputEvent aforc_input_internal_event(AFORC_InputEventType type,
                                            uint64_t timestamp_ms)
{
    AFORC_InputEvent event;

    (void)memset(&event, 0, sizeof(event));
    event.type = type;
    event.timestamp_ms = timestamp_ms;
    return event;
}

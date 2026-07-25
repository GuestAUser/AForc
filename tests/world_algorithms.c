/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_test_support.h"

#include <stdio.h>

int main(void) {
    if (!world_test_core_cases()) return 1;
    if (!world_test_astar_cases()) return 2;
    if (!world_test_visibility_cases()) return 3;
    (void)puts("world algorithms: ok");
    return 0;
}

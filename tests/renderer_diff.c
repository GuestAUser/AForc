/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_diff_cases.h"

#include <stdio.h>

int main(void)
{
    const int result = renderer_diff_run_cases();

    if (result == 0) {
        (void)puts("renderer diff: ok");
    }
    return result;
}

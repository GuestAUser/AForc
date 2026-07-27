/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_diff_cases.h"

#include <stdio.h>
#include <string.h>

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 &&
        strcmp(arguments[1], "--benchmark") == 0) {
        return renderer_diff_run_benchmark();
    }
    const int result = renderer_diff_run_cases();

    if (result == 0) {
        (void)puts("renderer diff: ok");
    }
    return result;
}

/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/aforc.h"

#include <string.h>

int main(void)
{
    return strcmp(aforc_status_string(AFORC_OK), "ok") == 0 ? 0 : 1;
}

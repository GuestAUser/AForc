#include "fieldzero/qa.h"

#include <stdio.h>

int main(void)
{
    if (!fieldzero_run_regressions())
    {
        (void)fprintf(stderr, "FIELD ZERO regression failed\n");
        return 1;
    }
    (void)puts("FIELD ZERO regression: ok");
    return 0;
}

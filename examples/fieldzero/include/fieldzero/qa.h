#ifndef FIELDZERO_QA_H
#define FIELDZERO_QA_H

#include "fieldzero/app.h"

bool fieldzero_run_regressions(void);
bool fieldzero_smoke_drive(FieldzeroApp *app, AFORC_Error *error);

#endif

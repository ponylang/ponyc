#ifndef PONYC_H
#define PONYC_H

#include <platform.h>
#include "pass/pass.h"

PONY_EXTERN_C_BEGIN

bool ponyc_init(pass_opt_t *options);
void ponyc_shutdown(pass_opt_t *options);

PONY_EXTERN_C_END

#endif

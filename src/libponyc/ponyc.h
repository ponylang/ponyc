#ifndef PONYC_H
#define PONYC_H

#include <platform.h>
#include "pass/pass.h"

PONY_EXTERN_C_BEGIN

bool ponyc_init(pass_opt_t* options);

/** Tear down what ponyc_init set up, ending with LLVM's global state.
 *
 * Does not print the collected errors: the caller decides where they go
 * relative to its own output, and LLVM must still be alive for anything the
 * caller does beforehand that touches it.
 */
void ponyc_shutdown(pass_opt_t* options);

PONY_EXTERN_C_END

#endif

#ifndef CODEGEN_GENEXE_H
#define CODEGEN_GENEXE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

bool genexe(compile_t* c, pass_opt_t* opt, ast_t* program);

PONY_EXTERN_C_END

#endif

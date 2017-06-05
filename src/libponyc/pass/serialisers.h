#ifndef PASS_SERIALISERS_H
#define PASS_SERIALISERS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool pass_serialisers(ast_t* program, pass_opt_t* options);

PONY_EXTERN_C_END

#endif

#ifndef PASS_FINALISERS_H
#define PASS_FINALISERS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool pass_finalisers(ast_t* program);

PONY_EXTERN_C_END

#endif

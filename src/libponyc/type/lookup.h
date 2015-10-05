#ifndef TYPE_LOOKUP_H
#define TYPE_LOOKUP_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_t* lookup(pass_opt_t* opt, ast_t* from, ast_t* type, const char* name);

ast_t* lookup_try(pass_opt_t* opt, ast_t* from, ast_t* type, const char* name);

PONY_EXTERN_C_END

#endif

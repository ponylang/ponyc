#ifndef TYPE_SAFETO_H
#define TYPE_SAFETO_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool safe_to_write(ast_t* ast, ast_t* type);

bool safe_to_autorecover(ast_t* receiver, ast_t* type);

PONY_EXTERN_C_END

#endif

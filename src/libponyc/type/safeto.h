#ifndef TYPE_SAFETO_H
#define TYPE_SAFETO_H

#include <platform.h>
#include "../ast/ast.h"
#include "cap.h"

PONY_EXTERN_C_BEGIN

bool safe_to_move(ast_t* ast, ast_t* type, direction direction);

bool safe_to_autorecover(ast_t* receiver, ast_t* type, direction direction);

PONY_EXTERN_C_END

#endif

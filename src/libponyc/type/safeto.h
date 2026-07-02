#ifndef TYPE_SAFETO_H
#define TYPE_SAFETO_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"
#include "cap.h"

PONY_EXTERN_C_BEGIN

bool safe_to_move(ast_t* ast, ast_t* type, direction direction,
  pass_opt_t* opt);
bool safe_to_mutate(ast_t* ast);

bool safe_to_autorecover(ast_t* receiver, ast_t* type, direction direction,
  pass_opt_t* opt);

PONY_EXTERN_C_END

#endif

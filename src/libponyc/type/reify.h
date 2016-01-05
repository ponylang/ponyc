#ifndef REIFY_H
#define REIFY_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors);

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs);

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors);

PONY_EXTERN_C_END

#endif

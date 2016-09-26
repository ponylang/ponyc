#ifndef REIFY_H
#define REIFY_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors,
  pass_opt_t* opt);

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt,
  bool duplicate);

ast_t* reify_method_def(ast_t* ast, ast_t* typeparams, ast_t* typeargs,
  pass_opt_t* opt);

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif

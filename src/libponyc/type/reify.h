#ifndef REIFY_H
#define REIFY_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

typedef struct deferred_reification_t
{
  ast_t* ast;
  ast_t* type_typeparams;
  ast_t* type_typeargs;
  ast_t* method_typeparams;
  ast_t* method_typeargs;
  ast_t* thistype;
} deferred_reification_t;

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors,
  pass_opt_t* opt);

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt,
  bool duplicate);

ast_t* reify_method_def(ast_t* ast, ast_t* typeparams, ast_t* typeargs,
  pass_opt_t* opt);

deferred_reification_t* deferred_reify_new(ast_t* ast, ast_t* typeparams,
  ast_t* typeargs, ast_t* thistype);

void deferred_reify_add_method_typeparams(deferred_reification_t* deferred,
  ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt);

ast_t* deferred_reify(deferred_reification_t* deferred, ast_t* ast,
  pass_opt_t* opt);

ast_t* deferred_reify_method_def(deferred_reification_t* deferred, ast_t* ast,
  pass_opt_t* opt);

deferred_reification_t* deferred_reify_dup(deferred_reification_t* deferred);

void deferred_reify_free(deferred_reification_t* deferred);

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors, pass_opt_t* opt);

pony_type_t* deferred_reification_pony_type();

PONY_EXTERN_C_END

#endif

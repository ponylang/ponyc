#ifndef PASS_EXPR_H
#define PASS_EXPR_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool is_result_needed(ast_t* ast);

bool is_method_result(typecheck_t* t, ast_t* ast);

bool is_method_return(typecheck_t* t, ast_t* ast);

bool is_typecheck_error(ast_t* type);

// Returns true (and reports an error) if `ast` is a control expression that
// jumps away with no value (`error`, `return`, `break`, `continue`), which
// can't be used in a value-operand position described by `what` (e.g. "a
// condition", "an argument"). Call this BEFORE is_typecheck_error at such
// positions: a jump-away expression has a NULL type, which is_typecheck_error
// reports as an error even though none was raised, so bailing on it without
// this check trips the "errors must be > 0" assertion in pass_expr.
// Only use this at value-operand positions. Do not call it on branch or tail
// positions (an `if`/`match` branch, a loop body, the last expression of a
// sequence) where jumping away is legal — those skip a jump-away child instead.
bool jumps_away_no_value(pass_opt_t* opt, ast_t* ast, const char* what);

ast_t* find_antecedent_type(pass_opt_t* opt, ast_t* ast, bool* is_recovered);

ast_result_t pass_pre_expr(ast_t** astp, pass_opt_t* options);

ast_result_t pass_expr(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif

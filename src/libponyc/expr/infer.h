#ifndef EXPR_INFER_H
#define EXPR_INFER_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Infer type arguments for a generic call from its positional arguments.
// typeparams: the type parameters being inferred (TK_TYPEPARAMS).
// other_typeparams: the callee's OTHER still-open type parameters, or NULL.
// params: the callee's parameter list (TK_PARAMS).
// positional: the positional arguments after normalisation (TK_POSITIONALARGS).
// error_at: the node that anchors error messages (the call or TYPEREF).
// callee_name: the callee as written (for messages).
// Returns a TK_TYPEARGS node on success, NULL on failure (one error reported,
// or silent when an argument has no type).
ast_t* infer_typeargs(pass_opt_t* opt, ast_t* typeparams,
  ast_t* other_typeparams, ast_t* params, ast_t* positional,
  ast_t* error_at, const char* callee_name);

// True when a parameter at this position is bindable: the type mentions at
// least one type parameter from the given list in a direct or nested position.
bool infer_bindable_position(ast_t* type, ast_t* typeparams);

// True when type mentions any type parameter from the given list.
bool type_mentions_typeparams(ast_t* type, ast_t* typeparams);

// Return the parameter type pruned of parts that mention an open type
// parameter inside structure. Returns a borrowed pointer (the parameter type,
// one of its members, or NULL).
ast_t* antecedent_prune(ast_t* type, ast_t* typeparams);

// True when the argument subtree contains a dependent expression that lacks
// explicit types and will fail if visited before the parameter type is
// reified. Unlike is_antecedent_dependent (which marks any dependent
// expression), this returns false for arrays with an `as` type and lambdas
// with all parameters typed.
bool infer_needs_antecedent_type(ast_t* arg);

PONY_EXTERN_C_END

#endif

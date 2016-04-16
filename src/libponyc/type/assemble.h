#ifndef TYPE_ASSEMBLE_H
#define TYPE_ASSEMBLE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/**
 * Creates an AST node for a builtin type.
 * Validates the type.
 */
ast_t* type_builtin(pass_opt_t* opt, ast_t* from, const char* name);

/**
 * Same as above, but with typeargs.
 * Validates the type.
 */
ast_t* type_builtin_args(pass_opt_t* opt, ast_t* from, const char* name,
  ast_t* typeargs);

/**
 * For some type A, creates the nominal type Pointer[A].
 */
ast_t* type_pointer_to(pass_opt_t* opt, ast_t* to);

/**
 * Creates an AST node for a package and type.
 * Does not validate the type.
 */
ast_t* type_sugar(ast_t* from, const char* package, const char* name);

/**
* Add a branch type to a control structure type.
*/
ast_t* control_type_add_branch(pass_opt_t* opt, ast_t* control_type,
  ast_t* branch);

/**
 * Build a type that is the union of these two types.
 */
ast_t* type_union(pass_opt_t* opt, ast_t* l_type, ast_t* r_type);

/**
 * Build a type that is the intersection of these two types.
 */
ast_t* type_isect(pass_opt_t* opt, ast_t* l_type, ast_t* r_type);

/**
 * Build a type to describe the current class/actor.
 */
ast_t* type_for_this(pass_opt_t* opt, ast_t* ast, token_id cap,
  token_id ephemeral, bool defs);

/**
 * Build a type to describe a function signature.
 */
ast_t* type_for_fun(ast_t* ast);

/**
 * Force the cap and ephemeral to the given value.
 */
ast_t* set_cap_and_ephemeral(ast_t* type, token_id cap, token_id ephemeral);

PONY_EXTERN_C_END

#endif

#ifndef EXPR_LITERAL_H
#define EXPR_LITERAL_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN


bool expr_literal(pass_opt_t* opt, ast_t* ast, const char* name);

/// Set the given AST node to have literal type
void make_literal_type(ast_t* ast);

/// Report whether the given type AST is a literal type
bool is_type_literal(ast_t* type);

/// Coerce the given AST to be of the given target type, if it is a literal.
/// @return True on success, false on error. If false is returned an error will
///   already have been reported.
bool coerce_literals(ast_t** astp, ast_t* target_type, pass_opt_t* options);

/// Handle a member access, possibly involving literals.
/// On exit all literals will have been removed from the expression or the type
/// type of the expression will have been set.
bool literal_member_access(ast_t* ast, pass_opt_t* options);

/// Handle a call, possibly involving literals.
/// On exit all literals will have been removed from the expression or the type
/// type of the expression will have been set.
bool literal_call(ast_t* ast, pass_opt_t* options);

/// Combine the literal branches in the given control block, if possible.
/// Should only be called once all branches have been type checked.
void literal_unify_control(ast_t* ast, pass_opt_t* options);

PONY_EXTERN_C_END

#endif

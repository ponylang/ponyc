#ifndef EXPR_LITERAL_H
#define EXPR_LITERAL_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool expr_literal(ast_t* ast, const char* name);
bool expr_this(ast_t* ast);
bool expr_tuple(ast_t* ast);
bool expr_error(ast_t* ast);
bool expr_compiler_intrinsic(ast_t* ast);
bool expr_nominal(ast_t** astp);
bool expr_fun(ast_t* ast);

/// Coerce the given AST to be of the given target type, if it is a literal.
/// @param out_type_changed Report whether the type of ast has been changed.
///   May be NULL.
/// @return True on success, false on error
bool coerce_literals(ast_t* ast, ast_t* target_type, bool* out_type_changed);

/// If the given type is a literal, turn it into a concrete type.
/// Must only be called after reification.
ast_t* concrete_literal(ast_t* type);

PONY_EXTERN_C_END

#endif

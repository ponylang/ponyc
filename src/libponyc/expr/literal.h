#ifndef EXPR_LITERAL_H
#define EXPR_LITERAL_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

void literals_init();

bool expr_literal(ast_t* ast, const char* name);
bool expr_this(ast_t* ast);
bool expr_tuple(ast_t* ast);
bool expr_error(ast_t* ast);
bool expr_compiler_intrinsic(ast_t* ast);
bool expr_nominal(ast_t** astp);
bool expr_fun(ast_t* ast);

/// Report whether the given type AST is a literal type
bool is_type_literal(ast_t* type);

/// Coerce the given AST to be of the given target type, if it is a literal.
/// @param out_type_changed Report whether the type of ast has been changed.
///   May be NULL.
/// @return True on success, false on error. If false is returned an error will
///   already have been reported.
bool coerce_literals(ast_t* literals, ast_t* target_type);

void reify_literals(ast_t* ast);

bool coerce_literal_member(ast_t* ast);

bool coerce_literal_operator(ast_t* ast);

/// If the type of the given AST is a literal, turn it into a concrete type.
/// Must only be called after reification.
//void concrete_literal(ast_t* ast);

PONY_EXTERN_C_END

#endif

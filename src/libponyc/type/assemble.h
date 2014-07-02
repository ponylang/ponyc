#ifndef TYPE_ASSEMBLE_H
#define TYPE_ASSEMBLE_H

#include "../ast/ast.h"

/**
 * If the ast node is a subtype of the named type, return the ast for the type
 * of the ast node. Otherwise, return NULL.
 */
ast_t* type_builtin(ast_t* ast, const char* name);

/**
 * If the ast node is a subtype of Bool, return the ast for the type of the ast
 * node. Otherwise, return NULL.
 */
ast_t* type_bool(ast_t* ast);

/**
 * If the ast node is a subtype of Integer, return the ast for the type of the
 * ast node. Otherwise, return NULL.
 */
ast_t* type_int(ast_t* ast);

/**
 * If the ast node is a subtype of Bool or a subtype of Integer, return the ast
 * for the type of the ast node. Otherwise, return NULL.
 */
ast_t* type_int_or_bool(ast_t* ast);

/**
 * If the ast node is a subtype of Arithmetic, return the ast for the type of
 * the ast node. Otherwise, return NULL.
 */
ast_t* type_arithmetic(ast_t* ast);

/**
 * If one of the two types is a super type of the other, return it. Otherwise,
 * return NULL.
 */
ast_t* type_super(ast_t* scope, ast_t* l_type, ast_t* r_type);

/**
 * Build a type that is the union of these two types.
 */
ast_t* type_union(ast_t* ast, ast_t* l_type, ast_t* r_type);

/**
 * Build a type that is the base type without any error types. Used to get the
 * type of the left side of a try expression.
 */
ast_t* type_strip_error(ast_t* ast, ast_t* type);

/**
 * Build a type to describe the current class/actor.
 */
ast_t* type_for_this(ast_t* ast, token_id cap, bool ephemeral);

/**
 * Build a type to describe a function signature.
 */
ast_t* type_for_fun(ast_t* ast);

/**
 * Takes an IDSEQ and a TUPLETYPE and assigns each id in idseq a type from the
 * tuple type.
 */
bool type_for_idseq(ast_t* idseq, ast_t* type);

#endif

#ifndef TYPE_ASSEMBLE_H
#define TYPE_ASSEMBLE_H

#include "../ast/ast.h"

/**
 * Creates an AST node for a builtin type
 * Validates the type.
 */
ast_t* type_builtin(ast_t* from, const char* name);

/**
 * Creates an AST node for a package and type.
 * Does not validate the type.
 */
ast_t* type_sugar(ast_t* from, const char* package, const char* name);

/**
 * Build a type that is the union of these two types.
 */
ast_t* type_union(ast_t* l_type, ast_t* r_type);

/**
 * Build a type that is the intersection of these two types.
 */
ast_t* type_isect(ast_t* l_type, ast_t* r_type);

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

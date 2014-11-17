#ifndef TYPE_ASSEMBLE_H
#define TYPE_ASSEMBLE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Creates an AST node for a builtin type.
 * Validates the type.
 */
ast_t* type_builtin(ast_t* from, const char* name);

/**
 * For some type A, creates the nominal type Pointer[A].
 */
ast_t* type_pointer_to(ast_t* to);

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
ast_t* type_for_this(ast_t* ast, token_id cap, token_id ephemeral);

/**
 * Build a type to describe a function signature.
 */
ast_t* type_for_fun(ast_t* ast);

/**
 * Takes an IDSEQ and a TUPLETYPE and assigns each id in idseq a type from the
 * tuple type.
 */
bool type_for_idseq(ast_t* idseq, ast_t* type);

/**
 * Replaces astp with an ast that removes any elements that are subtypes of
 * other elements and does a capability union on interface types.
 */
bool flatten_union(ast_t** astp);

/**
 * Replaces astp with an ast that removes any elements that are supertypes of
 * other elements.
 */
bool flatten_isect(ast_t** astp);

/**
 * Change cap and ephemeral on a nominal type.
 */
ast_t* set_cap_and_ephemeral(ast_t* type, token_id cap, token_id ephemeral);

PONY_EXTERN_C_END

#endif

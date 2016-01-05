#ifndef TYPE_VIEWPOINT_H
#define TYPE_VIEWPOINT_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Returns the l_type->r_type.
 */
ast_t* viewpoint_type(ast_t* l_type, ast_t* r_type);

/**
 * Returns the upper bounds of an arrow type.
 */
ast_t* viewpoint_upper(ast_t* type);

/**
 * Returns the lower bounds of an arrow type.
 */
ast_t* viewpoint_lower(ast_t* type);

/**
 * Replace all instances of target with some type. The target must either be
 * `this` or a typeparamref.
 */
ast_t* viewpoint_replace(ast_t* ast, ast_t* target, ast_t* with);

/**
 * Replace all instances of `this` with some type.
 */
ast_t* viewpoint_replacethis(ast_t* ast, ast_t* with);

/**
 * Returns a tuple of type reified with every possible instantiation of
 * typeparamref. If there is only one possible instantiation, this returns
 * NULL.
 */
ast_t* viewpoint_reifytypeparam(ast_t* type, ast_t* typeparamref);

/**
 * Returns a tuple of type reified with every possible instantiation of
 * `this`. If `this` doesn't appear in type, this returns NULL.
 */
ast_t* viewpoint_reifythis(ast_t* type);

PONY_EXTERN_C_END

#endif

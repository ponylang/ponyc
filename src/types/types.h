#ifndef TYPES_H
#define TYPES_H

#include "../ast/ast.h"
#include "../ds/list.h"
#include <stdbool.h>

typedef struct type_t type_t;

/**
 * A typelist_t is a list of type_t.
 */
DECLARE_LIST(typelist, type_t);

/**
 * Each element in list A is a subtype of the corresponding element in list B.
 */
bool typelist_sub(typelist_t* a, typelist_t* b);

/**
 * A type_t for an unqualified nominal type in the specified scope.
 * FIX: is this needed?
 */
type_t* type_name(ast_t* scope, const char* name);

/**
 * A type_t for a TK_TYPE, TK_TRAIT, TK_CLASS, TK_ACTOR, or TK_TYPEDEF
 */
type_t* type_create(ast_t* ast);

/**
 * Is A invariant with B?
 */
bool type_eq(type_t* a, type_t* b);

/**
 * Is A covariant with B?
 */
bool type_sub(type_t* a, type_t* b);

/**
 * Generate a hash of the type. Exported for use in hashing a function_t.
 */
uint64_t type_hash(type_t* t);

/**
 * Clean up the global type table.
 */
void type_done();

#endif

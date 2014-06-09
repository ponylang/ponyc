#ifndef TYPES_H
#define TYPES_H

#include "cap.h"
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
 * Given a list of type parameters and a list of constraints, check that a
 * supplied list of type arguments is valid.
 */
bool typelist_constraints(typelist_t* typeparams, typelist_t* constraints,
  typelist_t* typeargs);

/**
 * TODO: not right
 *
 * Given a list of type parameters and a list of constraints, generate type
 * arguments that represent the upper bounds of the possible type arguments.
 * To handle F-bounded polymorphism, this first replaces instances of self
 * reference in each type parameter with Any, then reifies the result with
 * itself.
 */
typelist_t* typelist_fbounds(typelist_t* typeparams, typelist_t* constraints);

/**
 * If the type appears in the formal list, replace it with corresponding type in
 * the actual list. Otherwise, reify the type with the same formal and actual
 * list. So if we have [A] and [Wombat], we reify A as Wombat and List[A] as
 * List[Wombat].
 */
typelist_t* typelist_reify(typelist_t* list,
  typelist_t* formal, typelist_t* actual);

/**
 * A type_t for an unqualified nominal type in the specified scope.
 */
type_t* type_name(ast_t* scope, const char* name, cap_id cap);

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
 * Qualify a nominal type with type arguments. The type arguments must be
 * subtypes of the constraints on the type parameters. The result is a nominal
 * type with no type parameters.
 */
type_t* type_qualify(type_t* type, typelist_t* typeargs);

/**
 * Reify a type by replacing occurrences of types in the formal list with the
 * corresponding type from the actual list.
 */
type_t* type_reify(type_t* type, typelist_t* typeparams, typelist_t* typeargs);

/**
 * Generate a hash of the type. Exported for use in hashing a function_t.
 */
uint64_t type_hash(type_t* t);

/**
 * Clean up the global type table.
 */
void type_done();

#endif

#ifndef FUNCTION_H
#define FUNCTION_H

#include "types.h"
#include "../ast/ast.h"
#include "../ds/list.h"
#include "../ds/table.h"

typedef struct function_t function_t;

/**
 * A funlist_t is a list of function_t.
 */
DECLARE_LIST(funlist, function_t);

/**
 * Does the list of functions in A represent a subtype of the list of functions
 * in B? This requires a subtype of every function in B to be present in A.
 */
bool funlist_sub(funlist_t* a, funlist_t* b);

/**
 * Reify a list of functions using function_reify.
 */
funlist_t* funlist_reify(funlist_t* list,
  typelist_t* formal, typelist_t* actual);

/**
 * function_t from a TK_NEW, TK_BE, TK_FUN, TK_NEWTYPE, TK_BETYPE, TK_FUNTYPE
 */
function_t* function_create(ast_t* ast);

/**
 * Function equality test.
 */
bool function_eq(function_t* a, function_t* b);

/**
 * Is function A a subtype of function B?
 */
bool function_sub(function_t* a, function_t* b);

/**
 * Qualify a function with type arguments. The type arguments must be subtypes
 * of the constraints on the type parameters. The result is a function with
 * no type parameters.
 */
function_t* function_qualify(function_t* f, typelist_t* typeargs);

/**
 * Reify a function with type arguments from an enclosing reified type.
 *
 * TODO: If we can mention enclosing type parameters in our own type parameters,
 * check that the argument is within our constraint. Would allow a function
 * to exist on a type only when a type parameter is within a given constraint.
 */
function_t* function_reify(function_t* f,
  typelist_t* typeparams, typelist_t* typeargs);

/**
 * TODO: infer qualifying type arguments from arguments
 */

/**
 * Clean up the global function table.
 */
void function_done();

#endif

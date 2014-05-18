#ifndef FUNCTION_H
#define FUNCTION_H

#include "../ds/list.h"
#include "../ds/table.h"
#include "../ast/ast.h"

typedef struct function_t function_t;

/**
 * A funlist_t is a list of function_t.
 */
DECLARE_LIST(funlist, function_t);

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
 * Does the list of functions in A represent a subtype of the list of functions
 * in B? This requires a subtype of every function in B to be present in A.
 */
bool funlist_sub(funlist_t* a, funlist_t* b);

/**
 * Clean up the global function table.
 */
void function_done();

#endif

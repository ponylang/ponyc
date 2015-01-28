#ifndef REACH_H
#define REACH_H

#include "../ast/ast.h"
#include "../../libponyrt/ds/hash.h"

typedef struct reified_method_t
{
  const char* name;
  ast_t* typeargs;
  ast_t* r_fun;
} reified_method_t;

DECLARE_HASHMAP(reified_methods, reified_method_t);

typedef struct method_t
{
  const char* name;
  reified_methods_t r_methods;
} method_t;

DECLARE_HASHMAP(methods, method_t);

typedef struct reified_type_t
{
  const char* name;
  ast_t* type;
  methods_t methods;
} reified_type_t;

DECLARE_HASHMAP(reified_types, reified_type_t);

/** Determine code reachability for a method in a type.
 *
 * The type should be a nominal, including any typeargs. The supplied method
 * typeargs can be NULL if there are none.
 */
void reach_method(reified_types_t* r, ast_t* type, const char* name,
  ast_t* typeargs);

#endif

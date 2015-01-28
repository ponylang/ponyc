#ifndef REACH_H
#define REACH_H

#include "../codegen/codegen.h"

/** Hande code reachability for a method in a reified type.
 *
 * The type should be a nominal, including any typeargs. The method should be
 * reified with the type-level typeargs, but not with its own typeargs.
 */
void reach_method(compile_t* c, ast_t* type, const char* name,
  ast_t* typeargs);

#endif

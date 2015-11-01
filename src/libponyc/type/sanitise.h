#ifndef SANITISE_H
#define SANITISE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN


/** Collect all type parameters that are in scope at the given node, for use in
 * an internally created anonymous type.
 *
 * We want the programmer to be able to use in scope type parameters in object
 * literals and associated constructs. However, the corresponding anonymous
 * type is inherently in a different context, so the type parameters are no
 * longer valid.
 *
 * To fix this we automatically add type parameters to the anonymous type
 * corresponding to all type parameters in scope at the object literal
 * definition. Clearly we won't always need all these type parameters, but it
 * is much easier to just add all of them, rather than trying to work out which
 * are needed.
 *
 * In this process we produce both the type parameters to add to the anonymous
 * type and the type arguments to use on the create() call. The parameters are
 * rooted at a TK_TYPEPARAMS node and the arguments at a TK_TYPEARGS node. If
 * there are no type parameters in scope, then both produced nodes will be
 * TK_NONEs, which keeps our AST consistent.
 */
void collect_type_params(ast_t* ast, ast_t** out_params, ast_t** out_args);

/** Produce a copy of the given type that is safe to use in an internally
* created AST that will be run through passes again.
*
* If a type AST goes through the passes twice it is important to sanitise it
* between goes. If this is not done type parameter references can point at the
* wrong definitions.
*/
ast_t* sanitise_type(ast_t* type);


PONY_EXTERN_C_END

#endif

#ifndef SANITISE_H
#define SANITISE_H

#include <platform.h>
#include "../ast/ast.h"

typedef struct pass_opt_t pass_opt_t;

PONY_EXTERN_C_BEGIN

/** Point the type parameter references in `ast` whose parameter was collected
 * into `params` (see collect_type_params) at the collected parameter.
 */
void remap_collected_typeparams(ast_t* ast, ast_t* params);

/** Make each collected parameter its own type parameter, no longer bound to
 * the one it was made from. Call this once every reference that should name
 * a collected parameter has been pointed at it.
 */
void own_collected_typeparams(ast_t* params);


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
 * definition. The literal's class later drops the ones it doesn't mention.
 *
 * In this process we produce both the type parameters to add to the anonymous
 * type and the type arguments to use on the create() call. The parameters are
 * rooted at a TK_TYPEPARAMS node and the arguments at a TK_TYPEARGS node. If
 * there are no type parameters in scope, then both produced nodes will be
 * TK_NONEs, which keeps our AST consistent.
 */
void collect_type_params(ast_t* ast, ast_t** out_params, ast_t** out_args, pass_opt_t* opt);

/** Produce a copy of the given type that is safe to use in an internally
* created AST that will be run through passes again: the copy's pass flags are
* cleared. Type parameter and type alias references keep the definitions they
* refer to; an anonymous type that receives the copy points the type parameter
* references at its own parameters.
*/
ast_t* sanitise_type(ast_t* type, pass_opt_t* opt);


PONY_EXTERN_C_END

#endif

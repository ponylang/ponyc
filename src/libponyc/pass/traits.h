#ifndef PASS_TRAITS_H
#define PASS_TRAITS_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/** Process traits provided by each actor, class and data class, adding the
 * required methods.
 */
ast_result_t pass_traits(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif

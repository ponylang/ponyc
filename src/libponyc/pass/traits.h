#ifndef PASS_TRAITS_H
#define PASS_TRAITS_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/** Process traits provided by each actor, class and data class, adding the
 * required methods.
 */
ast_result_t pass_traits(ast_t** astp);

PONY_EXTERN_C_END

#endif

#ifndef PASS_TRAITS_H
#define PASS_TRAITS_H

#include "../ast/ast.h"

/** Process traits provided by each actor, class and data class, adding the
 * required methods.
 */
ast_result_t pass_traits(ast_t** astp);

#endif

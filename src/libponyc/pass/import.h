#ifndef PASS_IMPORT_H
#define PASS_IMPORT_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Imports packages, either with qualifying names or by merging them into the
 * current scope.
 */
ast_result_t pass_import(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif

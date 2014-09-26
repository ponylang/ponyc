#ifndef PASS_SCOPE_H
#define PASS_SCOPE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/**
 * Inserts entries in scopes. When this is complete, all scopes are fully
 * populated, including package imports, type names, type parameters in types,
 * field names, method names, type parameters in methods, parameters in methods,
 * and local variables.
 */
ast_result_t pass_scope(ast_t** astp, pass_opt_t* options);

ast_result_t pass_scope2(ast_t** astp, pass_opt_t* options);

/**
* Import a package, either with a qualifying name or by merging it into the
* current scope.
*/
bool use_package(ast_t* ast, const char* path, ast_t* name,
  pass_opt_t* options);

PONY_EXTERN_C_END

#endif

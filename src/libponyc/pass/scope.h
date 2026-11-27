#ifndef PASS_SCOPE_H
#define PASS_SCOPE_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/**
 * Inserts entries in scopes. When this is complete, all scopes are fully
 * populated, including module imports, type names, type parameters in types,
 * field names, method names, type parameters in methods, parameters in methods,
 * and local variables.
 */
ast_result_t pass_scope(ast_t** astp, pass_opt_t* options);

/**
 * Build the narrowed type parameters of an iftype in the text of an object
 * literal or lambda, where that text is written. The scope pass runs on that
 * text again when the literal is expanded and keeps what this builds.
 */
bool scope_written_iftype(pass_opt_t* opt, ast_t* ast);

/**
* Import a module, either with a qualifying name or by merging it into the
* current scope.
*/
bool use_module(ast_t* ast, const char* path, ast_t* name,
  pass_opt_t* options);

PONY_EXTERN_C_END

#endif

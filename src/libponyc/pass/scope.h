#ifndef PASS_SCOPE_H
#define PASS_SCOPE_H

#include "../ast/ast.h"

/**
 * Inserts entries in scopes. When this is complete, all scopes are fully
 * populated, including package imports, type names, type parameters in types,
 * field names, method names, type parameters in methods, parameters in methods,
 * and local variables.
 */
ast_result_t pass_scope(ast_t* ast, int verbose);

#endif

#ifndef TREE_CHECK_H
#define TREE_CHECK_H

#include <platform.h>
#include "ast.h"

PONY_EXTERN_C_BEGIN

// Check that the given AST is well formed.
// Does nothing in release builds of the compiler.
void check_tree(ast_t* tree, errors_t* errors);

PONY_EXTERN_C_END

#endif

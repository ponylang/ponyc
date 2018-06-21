#ifndef TREE_CHECK_H
#define TREE_CHECK_H

#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct ast_t ast_t;
typedef struct pass_opt_t pass_opt_t;

// Check that the given AST is well formed.
// Does nothing in release builds of the compiler.
void check_tree(ast_t* tree, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif

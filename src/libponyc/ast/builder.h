#ifndef BUILDER_H
#define BUILDER_H

#include "ast.h"
#include "source.h"
#include <stdbool.h>


/** Build an AST from the description in the given source.
 * Returns the resulting AST, which must be freed later, or NULL on error.
 */
ast_t* build_ast(source_t* source);

/// Compare the 2 given ASTs
bool build_compare_asts(ast_t* expected, ast_t* actual);

/// Compare the 2 given ASTs ignoring siblings of the top node
bool build_compare_asts_no_sibling(ast_t* expected, ast_t* actual);


#endif

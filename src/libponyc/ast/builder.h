#ifndef BUILDER_H
#define BUILDER_H

#include "ast.h"
#include <stdbool.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct builder_t builder_t;


/** Create an AST builder based on the given description.
 * @return Created builder, which must be freed later with builder_free() or
 *   NULL on error.
 */
builder_t* builder_create(const char* description);

/** Add a sub-tree based on the given description to an existing AST. The new
 * sub-tree is added as a new child to the existing node with the specified
 * {def} attribute.
 *
 * If an error occurs (eg due to lookup failure) the entire builder may no
 * longer be in a consistent state and should be freed.
 *
 * @return A pointer to the new sub-tree (which is also part of the whole AST)
 *   or NULL on error.
 */
ast_t* builder_add(builder_t* builder, const char* add_point,
  const char* description);

ast_t* builder_add_type(builder_t* builder, const char* type_of,
  const char* description);

/// Report the root AST node from the given builder
ast_t* builder_get_root(builder_t* builder);

/** Extract the root AST from the given builder.
 * Once extracted the AST will not be freed when the builder is.
 * This is intended for use is the AST is to be added to another, which will
 * then be freed together.
 * The builder must not be freed until after the AST, since is contains the
 * source the AST uses.
 */
//ast_t* builder_extract_root(builder_t* builder);

/** Find the sub-tree defined by the given string with a {def} attribute.
 * @return The specified sub-tree (which remains part of the main tree) or NULL
 *   if the name is not found.
 */
ast_t* builder_find_sub_tree(builder_t* builder, const char* name);

/// Freed the given builder
void builder_free(builder_t* builder);


/// Compare the 2 given ASTs
bool build_compare_asts(ast_t* expected, ast_t* actual);

/// Compare the 2 given ASTs ignoring siblings of the top node
bool build_compare_asts_no_sibling(ast_t* expected, ast_t* actual);

PONY_EXTERN_C_END

#endif

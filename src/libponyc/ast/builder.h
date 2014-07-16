#ifndef BUILDER_H
#define BUILDER_H

typedef struct ast_t ast_t;
typedef struct source_t source_t;


/** Build an AST from the description in the given source.
 * Returns the resulting AST, which must be freed later, or NULL on error.
 */
ast_t* build_ast(source_t* source);

/// Compare the 2 given ASTs
//bool build_compare_asts(ast_t* ast1, ast_t* ast2);


#endif

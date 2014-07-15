#ifndef BUILDER_H
#define BUILDER_H

typedef struct ast_t ast_t;
typedef struct source_t source_t;


ast_t* build_ast(source_t* source);

#endif

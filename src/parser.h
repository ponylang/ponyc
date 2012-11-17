#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "error.h"

#define AST_SLOTS 7

typedef struct ast_t
{
  token_t* t;
  struct ast_t* sibling;
  struct ast_t* child[AST_SLOTS];
} ast_t;

typedef struct parser_t parser_t;

parser_t* parser_open( const char* file );
ast_t* parser_ast( parser_t* parser );
errorlist_t* parser_errors( parser_t* parser );
void parser_close( parser_t* parser );

#endif
